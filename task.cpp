#include "task.hpp"
#include "mpsc_queue.hpp"
#include "fd_waiter.hpp"
#include <mutex>
#include <set>
namespace gpd {
namespace {

thread_local scheduler * scheduler_ptr = 0;

struct scheduler_saver {
    scheduler * saved;
    scheduler_saver(scheduler& sched)
        : saved(std::exchange(scheduler_ptr, &sched))
    {}
    ~scheduler_saver() { scheduler_ptr = saved; }
};

}


struct scheduler {
    scheduler(const scheduler&) = delete;
    scheduler() {}
    friend void idle(scheduler&);
    typedef details::scheduler_node node;

    node* pop() {
        std::uint64_t pri[] = {
            get_pri(pinned_tasks),
            get_pri(tasks),
            get_pri(remote_tasks)};
        return
            pri[0] <=  pri[1] && pri[0] <= pri[2] ?
            pinned_tasks.pop_unlocked() :
            pri[1] <= pri[2] ?
            tasks.pop_unlocked() :
            remote_tasks.pop();
    }

    void push(node* n) {
        int pri = generation.load(std::memory_order_relaxed) + 1;
        n->pri = pri;
        if (scheduler_ptr == this) {
            generation.store(pri,std::memory_order_relaxed);                
            (pinned ? pinned_tasks : tasks).push_unlocked(n);
        } else {
            remote_tasks.push(n); // seq_cst
            if (waiting)
                waiter.signal({});
        }
    }
    
    bool pinned = false;
private:

    static std::uint64_t get_pri(mpsc_queue<node>& q) {
        node * n = static_cast<node*>(q.peek());
        return n ? n->pri : std::uint64_t(-1);
    }
    
    std::atomic<std::uint64_t> generation;
    mpsc_queue<node> pinned_tasks;
    mpsc_queue<node> tasks;
    mpsc_queue<node> remote_tasks;

    std::atomic<bool> waiting;
    fd_waiter waiter;
};


namespace details {

scheduler_node::scheduler_node()
    : pri(0)
    , sched(scheduler_ptr)
    , pinned(sched && sched->pinned)
{}

scheduler_node::~scheduler_node() {
    if(sched) std::exchange(sched->pinned, pinned);
}

bool scheduler_node::stolen() const {
    return sched != scheduler_ptr;
}

scheduler& scheduler_get_local() {
    assert(scheduler_ptr);
    return *scheduler_ptr;
}

void scheduler_post(details::scheduler_node& n) {
    assert(n.sched || scheduler_ptr);
    assert(!n.pinned || n.sched);
    if ( (n.pinned && n.stolen()) || !scheduler_ptr)
        n.sched->push(&n);
    else
        scheduler_ptr->push(&n);
}

task_t scheduler_pop() {
    auto * next = scheduler_get_local().pop();
    assert(next);
    return std::move(next->task);
}

}


void idle(scheduler& sched) {
    scheduler_saver _ (sched);

    auto next = sched.pop();
    if (next == 0) {
        sched.waiting.exchange(true);
        sched.waiter.reset();
        while ((next = sched.pop()) == 0)
            sched.waiter.wait();
        sched.waiting.store(0, std::memory_order_relaxed);
    }

    scheduler::node self;
    auto old = callcc(
        std::move(next->task),
        [&](task_t task) {
            self.task = std::move(task);
            sched.push(&self);
            return task;
        });
    assert(!old);
}

future<scheduler*> start_background_scheduler() {
    promise<scheduler*> result;
    auto future = result.get_future();
    std::thread th([result = std::move(result)] () mutable {
            scheduler sched;
            result.set_value(&sched);
            while(true)
                idle(sched);
        });
    th.detach();
    return future;
}



void yield(scheduler& target, task_t next) {
    scheduler::node self;
    auto old = callcc(
        std::move(next),
        [&](task_t task) {
            self.task = std::move(task);
            target.push(&self);
            return task;
        });
    assert(!old);
}

void yield(scheduler& target) {
    yield(target, details::scheduler_pop());
}

void yield(task_t next) {
    yield(details::scheduler_get_local(), std::move(next));
}

void yield() {
    yield(details::scheduler_get_local(), details::scheduler_pop());
}

void details::scheduler_waiter::signal(event_ptr p) {
    p.release();
    if (--signal_counter == 0) 
        details::scheduler_post(*this);    
}

void details::scheduler_waiter::wait(std::uint32_t count) {
    auto to = callcc
        (details::scheduler_pop(),
         [&](task_t c) {
            task = std::move(c);
            if ((signal_counter += count) <= 0)
                details::scheduler_post(*this);
            return c;
        });
    assert(!to);
}

}
