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
struct event //: waiter
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

    struct nop_waiter_t : waiter {
        virtual void signal(event_ptr) override {};
        virtual ~nop_waiter_t() override {}
    };

    static nop_waiter_t nop_waiter;

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
