#ifndef GPD_TASK_WAITER_HPP
#define GPD_TASK_WAITER_HPP
#include "continuation.hpp"
#include "event.hpp"
#include <pthread.h>
namespace gpd {
using task_t = continuation<void()>;

template<class Waitable>
void wait_adl(task_t& to, Waitable& w) {
    if (get_event(w) == 0) return;
    struct task_latch : waiter {
        task_t next;
        void signal(event_ptr p) override {
            p.release();
            auto next = std::move(this->next);
            next();
        }
        ~task_latch() { }
    } waiter;

    assert(to);

    to = callcc
        (std::move(to),
         [&](task_t c) {
            waiter.next = std::move(c);
            auto state = get_event(w);
            assert(state);
            state->then(&waiter);
            return c;
        });
}

template<class... Waitable>
void wait_any_adl(task_t& to, Waitable&... w) {
    event* children[] = { get_event(w)... };

    struct task_latch : waiter {
        task_t next;
        // whoever increments critical from 0 to 1 gets to call do_signal
        // Starts at 1 to prevent signaling while we setup the waiters
        std::atomic<std::size_t> critical = { 1 };

        void signal(event_ptr p) override {
            p.release();
            if (critical++ == 0) {
                auto next = std::move(this->next);
                next();
            }
        }
    } waiter;

    if (!event::then(&waiter, std::begin(children), std::end(children)))
        return;
    
    to = callcc
        (std::move(to),
         [&](task_t c) {
            waiter.next = std::move(c);       
                    
            // attempt to set critical to 0. On success, we are done
            if ( waiter.critical-- != 1) {
                // signal was called at least once and critcal
                // incremented, but the starting non-zero value
                // prevented the signaler from calling
                // do_signal. We do it here instead.
                auto next = std::move(waiter.next);
                next();
            }
            return c;
        });
    
    std::size_t count =
        event::dismiss_then(&waiter, std::begin(children), std::end(children));
    
    while(waiter.critical != count)
        __builtin_ia32_pause();
        

}

}
#endif
