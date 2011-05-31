#ifndef GPD_QUEUE_HPP
#define GPD_QUEUE_HPP
#include <utility>
#include <cassert>
namespace gpd {

void cbarrier() {
    asm volatile("":::"memory");
}

template<class T>
T load_acquire(T volatile &x) {
    cbarrier();
    auto ret = x;
    return ret;
}

template<class T, class T2>
void store_release(T volatile &x, T2 value) {
    x = value;
    cbarrier();
}

template<class T>
T exchange(T volatile& x, T value) {
    auto ret = __sync_lock_test_and_set(&x, value);
    cbarrier();
    return ret;
}

template<class T>
bool bool_cas(T volatile& x, T oldval, T newval) {
    return __sync_bool_compare_and_swap(&x, oldval, newval);
}

struct node_base
{
    explicit node_base(node_base * next = 0) : m_next(next) {}
    node_base* volatile  m_next;
};

// MP-SC queue_base
struct queue_base
{
    queue_base()  
        : m_head(&m_tail)
        , m_tail(0)
    {}

    void push(node_base* n) {
        n->m_next = 0;
        node_base* prev = exchange(m_head, n);
        assert(prev);
        store_release(prev->m_next, n);
    }

    node_base* pop()  {

        node_base* tail = load_acquire(m_tail.m_next);
        if (0 == tail)
            return 0;
        node_base* next = load_acquire(tail->m_next);

        if (next) {
            store_release(m_tail.m_next, next);
            return tail;
        }

        node_base* head = load_acquire(m_head);

        if (tail != head)
            return 0;
        store_release(m_tail.m_next, nullptr);

        if (bool_cas(m_head, head,  &m_tail))  {
            return tail;
        }

        next = load_acquire(tail->m_next);
        if (next)
        {
            store_release(m_tail.m_next, next);
            return tail;
        }
        return 0;
    } 

    node_base*  volatile m_head;
    node_base            m_tail;
};

template<class Node>
struct queue : queue_base{
    queue() : queue_base() {}

    typedef Node node;

    static_assert(std::is_base_of<node_base, Node>::value, 
                  "Node must derive from queue_base::node_base");


    void push(node* n) {
        queue_base::push(n);
    }

    node *pop() {
        auto p = queue_base::pop();
        assert(p != & m_tail);
        return static_cast<node*>(p);
    }
};

#define MPSCQ_STATIC_INIT(self) {&self.tail, 0}


}
#endif
