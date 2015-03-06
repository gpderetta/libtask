#ifndef GPD_TASK_HPP
#define GPD_TASK_HPP
#include "continuation.hpp"
#include "future.hpp"
#include "node.hpp"
namespace gpd {

using task_t = continuation<void()>;

struct scheduler;


constexpr struct scheduler_tag {} pool;

namespace details {

struct scheduler_node : gpd::node {
    scheduler_node();
    ~scheduler_node();
    bool stolen() const;
    
    std::uint64_t pri;
    scheduler* sched;
    bool pinned;
    task_t task;
};

scheduler& scheduler_get_local();
void scheduler_post(scheduler_node& n);
task_t scheduler_pop();

struct scheduler_waiter : waiter, details::scheduler_node {
    std::atomic<std::uint32_t> signal_counter = { 0 };
    void reset() { signal_counter.store(0, std::memory_order_relaxed); }
    void signal(event_ptr p) override;
    void wait(std::uint32_t count = 1);
};
}

/// Asynchronously start a background thread and run a scheduler on
/// it. Return a future pointer to the scheduler.
future<scheduler*> start_background_scheduler();

/// Push current continuation at the back of target scheduler ready
/// queue and jump to 'next' continuation.
void yield(scheduler& target, task_t next);

/// Push current continuation at the back of target scheduler ready
/// queue and pop and jump to the continuation at front of the current
/// scheduler ready queue.
void yield(scheduler& target);

/// Push current continuation at the back of the current
/// scheduler ready queue and jump to 'next' continuation.
void yield(task_t next);

/// Push current continuation at the back of the current
/// scheduler ready queue and pop and jump to the continuation at
/// front of the current scheduler ready queue queue.
void yield();

template<class F>
auto async(scheduler& target, F&&f);

template<class F>
auto async(scheduler_tag, F&&f);


/// wait{,_any,_all} customization point for the scheduler
template<class... Waitable>
void wait_any_adl(scheduler_tag, Waitable&... w);

template<class... Waitable>
void wait_all_adl(scheduler_tag, Waitable&... w);

template<class Waitable>
void wait_adl(scheduler_tag, Waitable& w);
//// implementation

template<class F>
auto async(scheduler& target, F&&f)  {

    struct {
        scheduler& target;
        F f;
        gpd::promise<decltype(f())> promise;

        auto operator()(task_t caller) {
            yield(target, std::move(caller));
            try {
                promise.set_value(f());
            } catch(...)
            {
                promise.set_exception(std::current_exception());
            }
            return details::scheduler_pop();
        }
    } run { target, std::forward<F>(f), {} };
    
    auto future = run.promise.get_future();
    auto c = callcc(std::move(run));
    return future;
}

template<class F>
auto async(scheduler_tag, F&&f) {
    return async(details::scheduler_get_local(), std::forward<F>(f));
}


template<class... Waitable>
void wait_any_adl(scheduler_tag, Waitable&... w) {
    details::scheduler_waiter waiter;
    gpd::wait_any(waiter, w...);
}

template<class... Waitable>
void wait_all_adl(scheduler_tag, Waitable&... w) {
    details::scheduler_waiter waiter;
    gpd::wait_all(waiter, w...);
}

template<class Waitable>
void wait_adl(scheduler_tag, Waitable& w) {
    if (auto * event = get_event(w)) {    
        struct task_latch : waiter, details::scheduler_node {
            void signal(event_ptr p) override {
                p.release();
                details::scheduler_post(*this);
            }
        } waiter;

        auto to = callcc
            (details::scheduler_pop(),
             [&](task_t c) {
                waiter.task = std::move(c);
                event->wait(&waiter);
                return c;
            });
        assert(!to);
    }
}

}
#endif
