#ifndef GPD_MPSC_QUEUE_HPP
#define GPD_MPSC_QUEUE_HPP
#include <utility>
#include "xassert.hpp"
#include "node.hpp"
#include <atomic>

namespace gpd {


typedef char padding_t[64];
/**
 * Thread safe, Multiple producers single consumer queue, based on an algorithm by
 * Dmitriy V'yukov/
 *
 * Untyped variant.
 **/
struct mpsc_queue_base
{
    mpsc_queue_base()  
        : m_head(&m_tail)
        , m_tail(0)
    {}

    void push_unlocked(node* n) {
        n->m_next.store(0, std::memory_order_relaxed);
        node* prev = m_head.load(std::memory_order_relaxed);
        XASSERT(prev);
        m_head.store(n, std::memory_order_relaxed);
        prev->m_next.store(n, std::memory_order_relaxed);
    }

    void push(node* n) {
        n->m_next.store(0, std::memory_order_relaxed);
        node* prev = m_head.exchange(n);
        XASSERT(prev);
        prev->m_next.store(n, std::memory_order_release);
    }

    node * pop_unlocked() {
        auto tail = m_tail.m_next.load(std::memory_order_relaxed);
        if (tail == 0)
            return 0;

        node* next = tail->m_next.load(std::memory_order_relaxed);

        m_tail.m_next.store(next, std::memory_order_relaxed);

        if (!next) {
            // tail is the last elment, update head
            XASSERT(tail == m_head.load(std::memory_order_relaxed));
            m_head.store(&m_tail, std::memory_order_relaxed);  
        }
        
        return tail;
        
    }
   
    // returns the next element
    node * pop() {
        node* tail = m_tail.m_next.load(std::memory_order_acquire);
        if (tail == 0)
            return 0;

        node* next = tail->m_next.load(std::memory_order_acquire);

        if (next) {
            m_tail.m_next.store(next, std::memory_order_relaxed);
            return tail;
        }
        // caught up with the producers, need to exchange 
        
        node* head = m_head.load(std::memory_order_acquire);

        if (tail != head)
        {   
            // producer preempted? we are stuck untill it finishes
            return 0;
        }

        m_tail.m_next.store(0, std::memory_order_release);

        if (m_head.compare_exchange_strong(head,  &m_tail))  {
            return tail;
        }

        // cas failed, a node was pushed
        next = tail->m_next.load(std::memory_order_acquire);
        if (next) {
            m_tail.m_next.store(next, std::memory_order_release);
            return tail;
        }
        return 0;
    }

    node * peek() {
        node* tail = m_tail.m_next.load(std::memory_order_acquire);
        if (tail == 0)
            return 0;

        node* next = tail->m_next.load(std::memory_order_relaxed);

        return next ? tail : 0;
    }

    std::atomic<node*>  m_head;
    padding_t _;
    node           m_tail;
};

/**
 * Thread safe, Multiple producers single consumer queue, based on an algorithm by
 * Dmitriy V'yukov/
 *
 * Generic type safe variant.
 *
 * Node must be derived from gpd::node.
 **/
template<class Node>
struct mpsc_queue : mpsc_queue_base {
    mpsc_queue() : mpsc_queue_base() {}

    typedef Node node;

    static_assert(std::is_base_of< ::gpd::node, node>::value, 
                  "Node must derive from gpd::node");
 
    void push(node* n) {
        mpsc_queue_base::push(n);
    }

    void push_unlocked(node* n) {
        mpsc_queue_base::push_unlocked(n);
    }

    node *pop() {
        auto p = mpsc_queue_base::pop();
        XASSERT(p != & m_tail);
        return static_cast<node*>(p);
    }

    node *pop_unlocked() 
    {
        auto p = mpsc_queue_base::pop_unlocked();
        XASSERT(p != & m_tail);
        return static_cast<node*>(p);
    }
};

}
#endif
