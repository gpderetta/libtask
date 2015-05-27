#ifndef GPD_WAITER_HPP
#define GPD_WAITER_HPP
#include "futex_waiter.hpp" // default waiter

namespace gpd {

using default_waiter_t =  futex_waiter;
extern thread_local default_waiter_t default_waiter;

}
#endif
