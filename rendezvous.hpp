#ifndef GPD_RENDEZVOUS_HPP
#define GPD_RENDEZVOUS_HPP
#include "continuation.hpp"
#include "mpsc_queue.hpp"
#include "details/switch_pair_accessor.hpp"
#include <atomic>

namespace gpd
{

#define force_inline __attribute__((force_inline))



template<class WaitList, Predicate, class ReadyList>
bool wait(WaitList& wait, const Predicate& pred, ReadyList& rl)
{
    // put current task at end of ready list. This has the semantics
    // of an std::atomic_thread_fence(std::memory_order_seq_cst);
    rl.yield();

    
    if (!pred())
    {
        taks_t consumer = callcc(
            taks_t(cnext),
            [&](task_t c) {
                cont x = details::switch_pair_accessor::pilfer(c).cont;
                wait.push(cnext);
                return
            });       

    }
}

void rendezvous(exchange& exch, void * token, task_t& to)
{

    cont w = exch.consumer.load(std::memory_order_acquire);
    if (w) 
    {
        consumer_t consumer (switch_pair{w});
        consumer(token);
        return;
    }
    else
    {
    }

}



static std::atomic<task_node*> waiters[1024*256];

constexpr task_node* signaled = reinterpret_cast<task_node*>(0x1);


template<class Predicate>
struct wait_queue
{
    Predicate p;
    mpsc_queue<task_node*> waiters;
};

struct wait_node {
    std::atomic<task_node*> waiter;
    std::atomic<std::uint64_t> generation;
};

bool rendezvous(wait_node& node, std::uint64_t expected_generation, scheduler& sched)
{
    auto& w = node.waiter;
    auto& generation = node.generation;
    if (generation.load(std::memory_order_acquire) != expected_generation)
        return;

    auto waiter = w.load(std::memory_order_acquire);
    
    if (waiter)
        waiter = w.exchange(0); // grab the node
    
    if (waiter == 0) 
    {
        task_t w = callcc(
            sched.pop_next(),
            [&](task_t c) {
                task_node * waiter = w.exchange(details::switch_pair_accessor::pilfer(c).cont);
                if (waiter)
                    sched.queue_many(waiter);
                return task_t();
            });       
        XASSERT(!w);
        return;
    }
    else
    {
        if (generation.load(std::memory_order_acquire) != expected_generation)
        {
            // we stole someone else partner, we need to 

        }

        auto next = waiter->m_next.load(std::memory_order_acquire);
        if (next)
            sched.queue_many(next);
            
    }
}
}

   


}
