#ifndef GPD_TASK_WAITER_HPP
#define GPD_TASK_WAITER_HPP
#include "continuation.hpp"
#include "event.hpp"
#include <pthread.h>
namespace gpd {
using task_t = continuation<void()>;

struct task_waiter : waiter {
    task_t next;
    task_t get() { return std::move(next); }

    char padding[60];
    std::atomic<std::uint32_t> signal_counter = { 0 };

    task_waiter(task_t task) : next(std::move(task)) {}
    
    void reset() {
        signal_counter.store(0, std::memory_order_relaxed);
    }
    
    void signal(event_ptr p) override {
        p.release();
        if (--signal_counter == 0) 
            get()();
    }

    void wait(std::uint32_t count = 1) {
        next = callcc
        (get(),
         [&](task_t c) {
            next = std::move(c);
            if ((signal_counter += count) == 0)
                get()();
            return c;
        });

    }
};


template<class... Waitable>
void wait_any_adl(task_t& to, Waitable&... w) {
    task_waiter waiter(std::move(to));
    gpd::wait_any(waiter, w...);
    to = waiter.get();
}

template<class... Waitable>
void wait_all_adl(task_t& to, Waitable&... w) {
    task_waiter waiter(std::move(to));
    gpd::wait_all(waiter, w...);
    to = waiter.get();
}


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

    to = callcc
        (std::move(to),
         [&](task_t c) {
            waiter.next = std::move(c);
            get_event(w)->wait(&waiter);
            return c;
        });
}

}
#endif
