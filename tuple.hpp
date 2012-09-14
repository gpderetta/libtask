#ifndef TUPLE_HPP
#define TUPLE_HPP
#include <boost/mpl/identity.hpp>
#include <tuple>

#include "macros.hpp"
namespace gpd {

template<int> struct placeholder {};
namespace details {
namespace mpl = boost::mpl;

template<class T, class Disable=void>
struct pack_arg : boost::mpl::identity<T&&> {};

template<int I>
struct pack_arg<placeholder<I>> : boost::mpl::identity<placeholder<I>> {};

template<int I>
struct pack_arg<placeholder<I>&> : boost::mpl::identity<placeholder<I>> {};
template<int I>
struct pack_arg<const placeholder<I>&> : boost::mpl::identity<placeholder<I>> {};
}

template<class ...Args>
auto pack(Args&&... args) -> std::tuple<typename details::pack_arg<Args>::type...>
{
    return std::tuple<typename details::pack_arg<Args>::type...>{ std::forward<Args>(args)... };
}

template<int i, class Tuple>
auto forward_get(Tuple& t) as
    (std::forward<typename std::tuple_element<i, Tuple>::type>(std::get<i>(t)));



}
#include "macros.hpp"
#endif 
