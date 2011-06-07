#ifndef GPD_MPSC_QUEUE_HPP
#define GPD_MPSC_QUEUE_HPP
#include <utility>
#include "xassert.hpp"
#include "node.hpp"
#include "atomic.hpp"

namespace gpd {

// MP-SC queue_base
struct mpsc_queue_base
{
    mpsc_queue_base()  
        : m_head(&m_tail)
        , m_tail(0)
    {}

    void push(node* n) {
        n->m_next.store(0, memory_order_relaxed);
        node* prev = m_head.exchange(n);
        XASSERT(prev);
        store_release(prev->m_next, n);
    }

    node* pop()  {

        node* tail = load_acquire(m_tail.m_next);
        if (0 == tail)
            return 0;
        node* next = load_acquire(tail->m_next);

        if (next) {
            store_release(m_tail.m_next, next);
            return tail;
        }

        node* head = load_acquire(m_head);

        if (tail != head)
            return 0;
        store_release(m_tail.m_next, (node*)0);

        if (m_head.compare_exchange_strong(head,  &m_tail))  {
            return tail;
        }

        next = load_acquire(tail->m_next);
        if (next) {
            store_release(m_tail.m_next, next);
            return tail;
        }
        return 0;
    } 

    atomic<node*>  m_head;
    node           m_tail;
};

template<class Node>
struct mpsc_queue : mpsc_queue_base{
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
