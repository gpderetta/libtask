#ifndef GPD_BITCAST_HPP
#define GPD_BITCAST_HPP
#include <type_traits>
#include <cstring>
namespace gpd {

/**
 * strict-aliasing safe version of reinterpret-cast Convert value 'b'
 * of type B to an value of type A, keeping the same bit
 * rapresentaiton.
 */
template<class A, class B>
A bitcast(B b) {
    static_assert(sizeof(A) == sizeof(b), "size mismatch");
    static_assert(std::is_pod<A>::value, "not a POD");
    static_assert(std::is_pod<B>::value, "not a POD");
    A ret;
    std::memcpy(&ret, &b, sizeof(b));
    return ret;
}

}
#endif
