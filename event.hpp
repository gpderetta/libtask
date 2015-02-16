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
    virtual void signal(event_ptr) override {};
    virtual ~delete_waiter_t() override {}
};

extern delete_waiter_t delete_waiter;

#define GPD_EVENT_IMPL_WAITFREE 1
#define GPD_EVENT_IMPL_DEKKER_LIKE 2
#define GPD_EVENT_IMPL_MIXED_ATOMICS 3

#define GPD_EVENT_IMPL GPD_EVENT_IMPL_MIXED_ATOMICS

// Synchronize a producer and a consumer via a continuation.
//
// The producer invokes ''signal'' when it wants to noify the
// consumer. Signal may or may not synchronously invoke the consumer
// registered continuation. 
//
// The consumer invokes 'then' to register a callback to listen for
// the producer event. Once set with 'then' the callback cannot be unset.
//
// Alternatively the consumer can register a callback with 'wait'. In
// this case the calback can be dismissed with reset.
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
    event(bool shared = true) : state(shared ? state_t::shared : 0) {}

    // Notify the consumer thread and relinquish ownership of the
    // event.
    void signal()  {
        auto val = get_state();
        // not shared nor waited: can't happen
        assert(val != 0);
        
        val = state.exchange(0);

        if (val & state_t::waited) {
            auto w = reinterpret_cast<waiter*>(val & state_t::waited);
            w->signal(
                event_ptr (this));
        }
    }

    // Register a consumer callback with the event relinquish
    // ownership of the event. Pre: no dismissiable wait must be
    // pending.
    void then(waiter * w_) {
        assert(w_);
        auto val = get_state();

        // not already waited
        assert(~val & state_t::waited);

        auto w = reinterpret_cast<std::uintptr_t>(w_);
        // set the waiter and relinquish ownership
        // returns true was still shared
        if ( !(val != 0 && state.exchange(w)) )  {
            // we need to call this ourselves
            w_->signal(event_ptr(this));
        }
    }

    template<class Iter>
    static  bool then(waiter * w, Iter begin, Iter end) {
        bool waited = false;
        for (auto i = begin; i != end; ++i)
            if (*i) { waited = true; (**i).then(w); }
        return waited;
    }
    
    // Dismiss a previous then(p) or successful try_then.  Returns
    // true if the dismissal was successful. Otherwise the producer is
    // in the process of inoking the callback; the consumer must wait
    // for the signal and now owns the event.  Pre: last prepare_wait
    // returned true and no retire_wait has been called since.
    __attribute__((warn_unused_result))
    bool dismiss_then(waiter* ) {
        auto val = get_state();
        return val != 0 && state.compare_exchange_strong(val, state_t::shared);
    }

    template<class Iter>
    static std::size_t dismiss_then(waiter * w, Iter begin, Iter end) {
        std::size_t count = 0;
        for (auto  i = begin; i != end; ++i)
            if (*i) count += ! (**i).dismiss_then(w);
        return count;
    }

    virtual ~event()  {}
private:
    std::uintptr_t get_state() const { return state.load(std::memory_order_acquire); }
    // msb is the shared bit
    struct state_t {
        static const constexpr std::uintptr_t shared = ~(std::uintptr_t(-1)>>1);
        static const constexpr std::uintptr_t waited = ~shared;
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
/// section.C++11 gives no guarantee, but depending on the underlying
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
/// The default implementations expect the wait strategy to derive
/// from 'waiter' and additonally expose a wait member
///
/// @{

template<class, class T = void>
using void_t = T;

template<class WaitStrategy, class Waitable>
auto wait_adl(WaitStrategy& how, Waitable& what) ->
    void_t<decltype(get_event(what))>{
    get_event(what)->then(&how);
    how.wait().release();
}

template<class WaitStrategy, class... Waitable>
void wait_all_adl(WaitStrategy& how, Waitable&... w) {
    ([](...){})( (wait_adl(how, w), 0)... );
}

template<class WaitStrategy, class... Waitable>
void wait_any_adl(WaitStrategy& how, Waitable&... w) {
    ([](...){})( (wait_adl(how, w), 0)... );
}
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
