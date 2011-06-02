#ifndef GPD_NODE_HPP
#define GPD_NODE_HPP
#include "atomic.hpp"
namespace gpd {
struct node
{
    explicit node(node * next = 0) : m_next(next) {}
    atomic<node*>  m_next;
};
}
#endif
