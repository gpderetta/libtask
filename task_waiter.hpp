#ifndef GPD_TASK_WAITER_HPP
#define GPD_TASK_WAITER_HPP
#include "continuation.hpp"
#include "event.hpp"
#include <pthread.h>
namespace gpd {
using task_t = continuation<void()>;

struct task_waiter_base : waiter {
    event **children_begin, **children_end;
    task_t next;

    // whoever increments critical from 0 to 1 gets to call do_signal
    // Starts at 1 to prevent signaling while we setup the waiters
    std::atomic<unsigned> critical = { 1 };

    void wait(task_t& to) {
        bool waited = false;
        for (auto i = children_begin; i != children_end; ++i)
            if (*i) { waited = true; (**i).then(this); }

        if (waited)
            to = callcc
                (std::move(to),
                 [this](task_t c) {
                    next = std::move(c);       
                    
                    // attempt to set critical to 0. On success, we are done
                    if ( critical-- != 1)
                        // signal was called at least once and critcal
                        // incremented, but the starting non-zero value
                        // prevented the signaler from calling
                        // do_signal. We do it here instead.
                        do_signal();
                    return c;
                });
    }

    void do_signal() {
        unsigned count = 0;
        for (auto  i = children_begin; i != children_end; ++i)
            if (*i && ! (**i).dismiss_then(this))
                count++; // failed to dismiss, signal in progress

        // 'count' signalers got in before we could disengage their
        // event. We need to wait them to signal that they are out of
        // the critical section by each increasing the critical count by one.
        //
        // note that the signal side critical session is tiny.
        
        while(critical != count)
              __builtin_ia32_pause();
        
        auto next = std::move(this->next);
        next();   
    }
    
    void signal(event_ptr p) override {
        p.release();
        if (critical++ == 0)
            // critical was exactly 0, we are in charge
            do_signal();
    }

    task_waiter_base(event** b, event** e) : children_begin(b), children_end(e) {}
};

template<int children_count>
struct task_waiter : task_waiter_base {
    event* children[children_count];
    template<class... Waitable>
    task_waiter(Waitable&... w)
        : task_waiter_base(children, children + children_count)
        , children{ get_event(w)...  }  {}
};

template<class Waitable>
void wait_adl(task_t& to, Waitable& w) {
    if (get_event(w) == 0) return;
    struct task_latch : waiter {
        task_t next;
        unsigned canary = 0xdeadbeef;
        void signal(event_ptr p) override {
            p.release();
            auto next = std::move(this->next);
            canary = 0;
            next();
        }
        ~task_latch() { assert(!canary);}
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
    task_waiter<sizeof...(w)>{ w... }.wait(to);
}

}
#endif
