#ifndef GPD_SWITCH_PAIR_ACCESSOR_HPP
#define GPD_SWITCH_PAIR_ACCESSOR_HPP
#include "switch_base.hpp"
namespace gpd { namespace details {
struct switch_pair_accessor { 
    template<class X>
    static switch_pair pilfer(X&& x) { return x.pilfer(); }

};
}}
#endif
