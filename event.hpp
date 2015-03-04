#ifndef GPD_EVENT_HPP
#define GPD_EVENT_HPP
#include <atomic>
#include <memory>
#include <cassert>

namespace gpd {
struct event;
using event_ptr = std::unique_ptr<event>;
struct waiter {
    // this might be destroyed after calling 'signal'
    virtual void signal(event_ptr) = 0;
    virtual ~waiter() {}
};

struct delete_waiter_t : waiter {
    virtual void signal(event_ptr p) override { (void)p; };
    virtual ~delete_waiter_t() override {}
};

extern delete_waiter_t delete_waiter;

#define GPD_EVENT_IMPL_WAITFREE 1
#define GPD_EVENT_IMPL_DEKKER_LIKE 2
#define GPD_EVENT_IMPL_MIXED_ATOMICS 3

#define GPD_EVENT_IMPL GPD_EVENT_IMPL_WAITFREE

// Synchronize a producer and a consumer via a continuation.
//
// The producer invokes ''signal'' when it wants to noify the
// consumer. Signal may or may not synchronously invoke the consumer
// registered continuation. 
//
// The consumer invokes 'wait' (or try_then) to register a callback to
// listen for the producer event. A registered callback can be unregistered .
//
// The event can be in three states: empty, waited, signaled.
//
// Note: all RMW are seq_cst (because x86), but should be possible to
// downgrade them to acq_rel

#if GPD_EVENT_IMPL == GPD_EVENT_IMPL_WAITFREE
/// This is the straight-forward implementation. Signal and then use a
/// stright exchange, while dismiss then uses a single strong CAS,
/// Assuming exchange and CAS are waitfree, all operations are also
/// waitfree.
struct event 
{
    event(event&) = delete;
    void operator=(event&&) = delete;
    event(bool empty = true) : state(empty ? 0 : state_t::signaled ) {}

    // Put the event in the signaled state. If the event was in the
    // waited state invoke the callback (and leave the event in the
    // signaled state).
    void signal()  {
        auto val = get_state();
        assert(val != state_t::signaled);
        
        val = state.exchange(state_t::signaled);

        if (val > state_t::signaled) {
            auto w = reinterpret_cast<waiter*>(val);
            w->signal(
                event_ptr (this));
        }
    }

    // Register a callback with the event. If the event is already
    // signaled, invoke the callback (and leave the event in the
    // signaled state), otherwise put the event to the waited state.
    //
    // Pre: state != waited
    void wait(waiter * w_) {
        if (!try_wait(w_))  
            w_->signal(event_ptr(this));
    }

    // If the event is in the signaled state, return false, otherwise
    // register a callback with the event and put the event in the
    // waited state, then return true.
    __attribute__((warn_unused_result))
    bool try_wait(waiter * w_) {
        assert(w_);
        auto val = get_state();
        auto w = reinterpret_cast<std::uintptr_t>(w_);
        return (val != state_t::signaled &&
                state.compare_exchange_strong(val, w));
    }
      
    // [begin, end) is a range of pointers to events.  For every
    // non-null pointer p in range, call p->try_wait(w) Return
    // (signaled, waited), where signaled is number of times
    // try_waited retuned false and waited is the number of times it
    // retuned true.
    template<class Iter>
    static std::pair<std::size_t, std::size_t> wait_many(waiter * w, Iter begin, Iter end) {
        std::size_t signaled = 0;
        std::size_t waited  = 0;
        for (auto  i = begin; i != end; ++i) {
            if (*i) {
                ((**i).try_wait(w) ? waited : signaled)++;
            }
        }
        return std::make_pair(signaled, waited);

    }
    
    // If the event is in the waited or empty state, put it into the
    // empty state and return true, otherwise return false and leave
    // it in the signaled state.
    __attribute__((warn_unused_result))
    bool dismiss_wait(waiter* ) {
        auto val = get_state();
        return val == 0 ||
            ( val != state_t::signaled &&
              state.compare_exchange_strong(val, state_t::empty));
    }

    // [begin, end) is a range of pointers to events.
    // For every non-null pointer p in range, call p->try_wait(w)
    // Return the number of successfull dismissals.
    template<class Iter>
    static std::size_t dismiss_wait_many(waiter * w, Iter begin, Iter end) {
        std::size_t count = 0;
        for (auto  i = begin; i != end; ++i)
            if (*i) count += (**i).dismiss_wait(w);
        return count;
    }

    virtual ~event()  {}
private:
    std::uintptr_t get_state() const { return state.load(std::memory_order_acquire); }
    // msb is the signaled bit
    struct state_t {
        static const constexpr std::uintptr_t empty = 0;
        static const constexpr std::uintptr_t signaled = 1;
        // waited is any value non empty non signaled
    };
    std::atomic<std::uintptr_t> state;
                             
};

#elif GPD_EVENT_IMPL == GPD_EVENT_IMPL_DEKKER_LIKE

/// This implementation uses a dekker-like mutual exclusion mechanism
/// between signal and both then and dismiss_then. We trade an RMW
/// with a store+fence+load and the possiblity of spinning, but we
/// gain the ability to amortize the cost of the fence on multi event
/// ops.
struct event 
{
    event(event&) = delete;
    void operator=(event&&) = delete;
    event(bool shared = true) : signaled(shared ? state_t::unset : state_t::signaled) {}

    // Notify the consumer thread and relinquish ownership of the
    // event.
    void signal()  {

        signaled.store(state_t::critical, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        auto w = waited.load(std::memory_order_acquire);
        if (!w)
            signaled.store(state_t::signaled, std::memory_order_release);
        else {
            signaled.store(state_t::fired, std::memory_order_release);
            w->signal(event_ptr (this));
        }
    }

    
    
    // Register a consumer callback with the event. Pre: no
    // dismissiable wait must be pending.
    void then(waiter * w) {
        assert(w);
        then_1(w);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        then_2(w);
    }

    template<class Iter>
    static  bool then(waiter * w, Iter begin, Iter end) {
        bool waited = false;
        for (auto i = begin; i != end; ++i)
            if (*i) { waited = true; (**i).then_1(w); }
        std::atomic_thread_fence(std::memory_order_seq_cst);
        for (auto i = begin; i != end; ++i)
            if (*i) { (**i).then_2(w); }
        return waited;
    }
    
    // Dismiss a previous then(p) or successful try_then.  Returns
    // true if the dismissal was successful. Otherwise the producer is
    // in the process of inoking the callback; the consumer must wait
    // for the signal and now owns the event.  Pre: last prepare_wait
    // returned true and no retire_wait has been called since.
    __attribute__((warn_unused_result))
    bool dismiss_then(waiter* w) {
        dismiss_then_1(w);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return dismiss_then_2(w);
    }

    template<class Iter>
    static std::size_t dismiss_then(waiter * w, Iter begin, Iter end) {
        for (auto  i = begin; i != end; ++i)
            if (*i) (**i).dismiss_then_1(w);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::size_t count = 0;
        for (auto  i = begin; i != end; ++i)
            if (*i) count += !(**i).dismiss_then_2(w);
        return count;
    }

    virtual ~event()  {}
private:
    void then_1(waiter * w) { waited.store(w, std::memory_order_release); }
    void then_2(waiter * w) {
        auto s = signaled.load(std::memory_order_acquire);
        while (s == state_t::critical) {
            s = signaled.load(std::memory_order_acquire);
            __builtin_ia32_pause();
        }
        if (s == state_t::signaled) {
            w->signal(event_ptr(this));
            signaled.store(state_t::fired, std::memory_order_relaxed);
        }

    }

    void dismiss_then_1(waiter*) { waited.store(0, std::memory_order_release); }
    bool dismiss_then_2(waiter*) {
        auto s = signaled.load(std::memory_order_acquire);
        while (s == state_t::critical) {
            s = signaled.load(std::memory_order_acquire);
            __builtin_ia32_pause();
        }
        return s != state_t::fired;
    }

    
    enum class state_t { unset, critical, signaled, fired };
    std::atomic<waiter*> waited = { 0 };
    std::atomic<state_t> signaled = { state_t::unset };
                             
} ;

#elif GPD_EVENT_IMPL == GPD_EVENT_IMPL_MIXED_ATOMICS

/// This is an hibrid between the dekker like implementation and the
/// waitfree one. It relies on a DCAS being able to synchronize with
/// adjacent word-sized load and stores. The advantage is that no
/// spinning is required as there is no signal side critical
/// section. C++11 gives no guarantee, but depending on the underlying
/// hardware it should work fine. Signal is lock-free, then and
/// dismiss_then wait-free
///
struct event 
{
    event(event&) = delete;
    void operator=(event&&) = delete;
    event(bool shared = true)
        : atomic_pair { pair_t{ 0, shared ? state_t::unset : state_t::signaled } }
    {  }
    
    // Notify the consumer thread and relinquish ownership of the
    // event.
    void signal()  {
        auto w  = pair.waited.load(std::memory_order_relaxed);
        pair_t old { w, state_t::unset };
        while (! atomic_pair.compare_exchange_strong(
                   old, 
                   pair_t{ old.waited,
                           old.waited ? state_t::fired : state_t:: signaled }))
               ;         
        if (old.waited) old.waited->signal(event_ptr (this));
    }   
    
    // Register a consumer callback with the event. Pre: no
    // dismissiable wait must be pending.
    void then(waiter * w) {
        assert(w);
        then_1(w);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        then_2(w);
    }

    template<class Iter>
    static  bool then(waiter * w, Iter begin, Iter end) {
        bool waited = false;
        for (auto i = begin; i != end; ++i)
            if (*i) { waited = true; (**i).then_1(w); }
        std::atomic_thread_fence(std::memory_order_seq_cst);
        for (auto i = begin; i != end; ++i)
            if (*i) { (**i).then_2(w); }
        return waited;
    }
    
    // Dismiss a previous then(p) or successful try_then.  Returns
    // true if the dismissal was successful. Otherwise the producer is
    // in the process of inoking the callback; the consumer must wait
    // for the signal and now owns the event.  Pre: last prepare_wait
    // returned true and no retire_wait has been called since.
    __attribute__((warn_unused_result))
    bool dismiss_then(waiter* w) {
        dismiss_then_1(w);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        return dismiss_then_2(w);
    }

    template<class Iter>
    static std::size_t dismiss_then(waiter * w, Iter begin, Iter end) {
        for (auto  i = begin; i != end; ++i)
            if (*i) (**i).dismiss_then_1(w);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::size_t count = 0;
        for (auto  i = begin; i != end; ++i)
            if (*i) count += !(**i).dismiss_then_2(w);
        return count;
    }

    virtual ~event()  {}
private:
    void then_1(waiter * w) { pair.waited.store(w, std::memory_order_release); }
    void then_2(waiter * w) {
        if (pair.signaled.load(std::memory_order_acquire) == state_t::signaled) {
            w->signal(event_ptr(this));
            pair.signaled.store(state_t::fired, std::memory_order_relaxed);
        }
    }

    void dismiss_then_1(waiter*) { pair.waited.store(0, std::memory_order_release); }
    bool dismiss_then_2(waiter*) {
        return pair.signaled.load(std::memory_order_acquire) != state_t::fired;
    }

    
    enum class state_t { unset, signaled, fired  };

    struct pair_t {
        waiter* waited; 
        state_t signaled;
    };

    struct atomic_pair_t {
        std::atomic<waiter*> waited = { 0 };
        std::atomic<state_t> signaled = { state_t::unset };
    };

     union alignas(16) {
        std::atomic<pair_t> atomic_pair;
        atomic_pair_t pair;
    };
};                         

#endif

// ADL customization point. Given a "Waitable", returns an event
// object. The lifetime of the result is the same as for 'x'. Only
// prepare_wait and retire_wait should be called on the result as x
// retains ownership of the object.
template<class T>
event* get_event(T& x);

/// ADL customization points for Waiters
///
/// The default implementations expect the wait strategy to match the
/// CountdownLatch concept.
///
///@{

/// Note: this is not really an actual type but just a concept
/// documentation.
///
/// A synchronization object modeling the CountdownLatch concept keeps
/// track of the number of signaling operations since the last
/// reset. Each inocation of signal increases the internal count,
/// while each inocation of wait(n) consumes n previous invocations
/// and decreases the count. Wait will not return untill count is at
/// least n.
struct CountdownLatch : waiter {
    /// set count to 0. Not thread safe,
    void reset();

    /// call p.release(); increase count by 1. Wakeup waiter if //
    /// target reached. Can be called concurrently with other calls to
    /// signal and wait
    void signal(event_ptr p);

    /// Wait untill count == 'target', then decrease it by count. Can
    /// be called concurrently with other calls to signal, but not
    /// wait
    void wait(std::uint32_t  target = 1);
};


template<class, class T = void>
using void_t = T;

template<class CountdownLatch, class Waitable>
auto wait_adl(CountdownLatch& latch, Waitable& e) ->
    void_t<decltype(get_event(e))>{
    latch.reset();
    get_event(e)->wait(&latch);
    latch.wait();
}

template<class CountdownLatch, class... Waitable>
void wait_all_adl(CountdownLatch& latch, Waitable&... e) {
    event * events[] = {get_event(e)...};
    latch.reset();
    std::size_t waited =
        event::wait_many(&latch, std::begin(events), std::end(events)).second;
    if (waited)
        latch.wait(waited);
}

template<class CountdownLatch, class... Waitable>
void wait_any_adl(CountdownLatch& latch, Waitable&... e) {
    event * events[] = {get_event(e)...};

    latch.reset();
    std::size_t signaled;
    std::size_t waited;
    std::tie(signaled, waited) =
        event::wait_many(&latch, std::begin(events), std::end(events));

    if (signaled == 0) 
        latch.wait();
        
    const std::size_t dismissed =
        event::dismiss_wait_many(&latch, std::begin(events), std::end(events));
    
    std::size_t pending = waited - dismissed;
    assert( signaled != 0 || pending >= 1 );
    if (signaled == 0) {
        assert(pending >= 1);
        pending -= 1;
    }
    
    if (pending)
        latch.wait(pending);

}
/// @} 


/// @} 


/// Wait entry points; simply defer to the customization points
/// @{
template<class WaitStrategy, class Waitable>
void wait(WaitStrategy& how, Waitable& w) noexcept {
    wait_adl(how, w);
}

template<class WaitStrategy, class... Waitable>
void wait_all(WaitStrategy& how, Waitable&... w) noexcept {
    wait_all_adl(how, w...);
}

template<class WaitStrategy, class... Waitable>
void wait_any(WaitStrategy& to, Waitable&... w) noexcept {
    wait_any_adl(to, w...);
}

/// @}
}
#endif
