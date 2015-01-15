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

    void push(node* n) {
        n->m_next.store(0, std::memory_order_relaxed);
        node* prev = m_head.exchange(n);
        XASSERT(prev);
        prev->m_next.store(n, std::memory_order_release);
    }

    node* pop()  {

        node* tail = m_tail.m_next.load(std::memory_order_acquire);
        if (0 == tail)
            return 0;

        node* next = tail->m_next.load(std::memory_order_acquire);

        if (next) {
            m_tail.m_next.store(next, std::memory_order_relaxed);
            return tail;
        }

        node* head = m_head.load(std::memory_order_acquire);

        if (tail != head)
            return 0;
        m_tail.m_next.store(0, std::memory_order_release);

        if (m_head.compare_exchange_strong(head,  &m_tail))  {
            return tail;
        }

        next = tail->m_next.load(std::memory_order_acquire);
        if (next) {
            m_tail.m_next.store(next, std::memory_order_release);
            return tail;
        }
        return 0;
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

    node *pop() {
        auto p = mpsc_queue_base::pop();
        XASSERT(p != & m_tail);
        return static_cast<node*>(p);
    }
};

}
#endif
