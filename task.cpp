#include "task.hpp"
#include "mpsc_queue.hpp"
#include "fd_waiter.hpp"
#include <mutex>
#include <set>
namespace gpd {

namespace {

struct scheduler_t {

    struct node : gpd::node {
        std::uint64_t pri = 0;
        bool pinned  = scheduler_t::get_pinned();
        scheduler_t & scheduler = scheduler_t::sched;
        task_t task;

        bool stolen() const { return &scheduler != &scheduler_t::sched; }
        
        ~node() { scheduler_t::set_pinned(pinned); }
    };


    static bool set_pinned(bool pinned) {
        return std::exchange(sched.pinned, pinned);
    }

    static bool get_pinned() { return sched.pinned; }
    static node * pop() { return sched.do_pop(); }
    static void post(node& n) {
        if (n.pinned && n.stolen())
            n.scheduler.push(&n);
        else
            sched.push(&n);
    }

    node* do_pop() {
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
        if (&sched == this) {
            generation.store(pri,std::memory_order_relaxed);                
            (pinned ? pinned_tasks : tasks).push_unlocked(n);
        } else {
            remote_tasks.push(n); // seq_cst
            if (waiting)
                waiter.signal({});
        }
    }

    static thread_local scheduler_t sched;

private:

    static std::uint64_t get_pri(mpsc_queue<node>& q) {
        node * n = static_cast<node*>(q.peek());
        return n ? n->pri : std::uint64_t(-1);
    }
    
    std::atomic<std::uint64_t> generation;
    mpsc_queue<node> pinned_tasks;
    mpsc_queue<node> tasks;
    mpsc_queue<node> remote_tasks;
    bool pinned = false;
    std::atomic<bool> waiting;
    fd_waiter waiter;
};


constexpr struct scheduler_tag {} scheduler;

void yield(scheduler_t& target = scheduler_t::sched) {
    auto next = scheduler_t::pop();
    assert(next);
    scheduler_t::node self;
    auto old = callcc(
        std::move(next->task),
        [&](task_t task) {
            self.task = std::move(task);
            target.push(&self);
        });
    assert(!old);
}

struct scheduler_waiter : waiter, scheduler_t::node {

    std::uint32_t wait_counter = { 0 };
    char padding[60];
    std::atomic<std::uint32_t> signal_counter = { 0 };

    void reset() {
        signal_counter.store(0, std::memory_order_relaxed);
        wait_counter = 0;
    }
    
    void signal(event_ptr p) override {
        p.release();
        if (--signal_counter == 0) 
            scheduler_t::post(*this);
    }

    void wait(std::uint32_t count = 1) {
        auto next = scheduler_t::pop();

        auto to = callcc
            (std::move(next->task),
             [&](task_t c) {
                task = std::move(c);
                auto delta = count - wait_counter;
                wait_counter += delta;
                if ((signal_counter += delta) == 0)
                    scheduler_t::post(*this);
                return c;
            });
        assert(!to);
    }
};

template<class... Waitable>
void wait_any_adl(scheduler_tag, Waitable&... w) {
    scheduler_waiter waiter;
    gpd::wait_any(waiter, w...);
}

template<class... Waitable>
void wait_all_adl(scheduler_tag, Waitable&... w) {
    scheduler_waiter waiter;
    gpd::wait_all(waiter, w...);
}

template<class Waitable>
void wait_adl(scheduler_tag, Waitable& w) {
    if (get_event(w) == 0) return;
    
    auto next = scheduler_t::pop();
    assert(next);
    struct task_latch : waiter, scheduler_t::node {
        void signal(event_ptr p) override {
            p.release();
            scheduler_t::post(*this);
        }
    } waiter;

    auto to = callcc
        (std::move(next->task),
         [&](task_t c) {
            waiter.task = std::move(c);
            auto state = get_event(w);
            assert(state);
            state->then(&waiter);
            return c;
        });
    assert(!to);
}

struct schedulers {
    void add(scheduler_t*x) { std::lock_guard<std::mutex> {mux}, lists.insert(x); }
    void remove(scheduler_t*x) { std::lock_guard<std::mutex>{mux}, lists.erase(x); }
private:
    std::mutex mux;
    std::set<scheduler_t*> lists;

    
};
}

}
