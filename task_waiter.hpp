#ifndef GPD_TASK_WAITER_HPP
#define GPD_TASK_WAITER_HPP
#include "continuation.hpp"
#include <pthread.h>
namespace gpd {
using task_t = continuation<void()>;

template<int children_count>
struct task_waiter {
    struct task_latch : waiter {
        task_latch(const task_latch&) = delete;
        task_latch(task_latch&& rhs)
            : ev(rhs.ev), parent(rhs.parent.load(std::memory_order_relaxed))
        {}
        
        event* ev;
        std::atomic<task_waiter *> parent;
        task_latch(event* ev, task_waiter* parent)
            : ev(ev), parent(parent) {}
            
        void signal(event_ptr p) override {
            p.release();
            parent.load(std::memory_order_relaxed)->signal(this);
        }
            
        bool arm() {
            return !ev || ev->try_then(this);
        }
            
        void disarm() {
            if (ev && ! ev->dismiss_then(this))
                while(parent.load(std::memory_order_relaxed))
                    __builtin_ia32_pause();
        }
            
    };
    task_latch children[children_count];

    // -1 not set, -2 stolen, otherwise index of first not waited child
    std::atomic<int> index = {-1};
    task_t next;

    void wait(task_t& to) {
        to = callcc
            (std::move(to),
             [this](task_t c) {
                next = std::move(c);
                int i;
                for (i = 0; i < children_count; ++i)
                    if (!children[i].arm()) { break; }
                index.store(i, std::memory_order_release);
                if (i != children_count) 
                    signal(children + i);

                return c;
            });
    }

    void signal(task_latch* child) {

        // spin waiting for 'wait' to complete
        while(index.load(std::memory_order_relaxed) == -1)
            __builtin_ia32_pause();

        int max;
        if ((max = index.load(std::memory_order_relaxed)) == -2 ||
            (max = index.exchange(-2)) == -2) {
            child->parent.store(nullptr, std::memory_order_relaxed);
            return;
        }
            
        child->parent.store(nullptr, std::memory_order_relaxed);
        auto next = std::move(this->next);
            
        for (int i = 0; i < max; ++i)
            children[i].disarm();
        next();
    }
        
    template<class... Waitable>
    task_waiter(Waitable&... w)
        : children{ { get_event(w), this}...  }  {}
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
    for (bool x : { get_event(w)... })
        if (x) return task_waiter<sizeof...(w)>{ w... }.wait(to);
}

}
#endif
