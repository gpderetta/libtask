#ifndef TUPLE_HPP
#define TUPLE_HPP
#include <tuple>

#include "macros.hpp"
namespace gpd {

template<class T> struct identity { typedef T type; };

template<int> struct placeholder {};
namespace details {

template<class T, class Disable=void>
struct pack_arg : identity<T&&> {};

template<int I>
struct pack_arg<placeholder<I>> : identity<placeholder<I>> {};

template<int I>
struct pack_arg<placeholder<I>&> : identity<placeholder<I>> {};
template<int I>
struct pack_arg<const placeholder<I>&> : identity<placeholder<I>> {};
}

template<class ...Args>
auto pack(Args&&... args) -> std::tuple<typename details::pack_arg<Args>::type...>
{
    return std::tuple<typename details::pack_arg<Args>::type...>{ std::forward<Args>(args)... };
}

template<int i, class Tuple>
auto forward_get(Tuple&& t) as
    (std::forward<typename std::tuple_element<i, Tuple>::type>(std::get<i>(t)));

template<int i, class Tuple>
auto forward_get(Tuple& t) as
    (std::forward<typename std::tuple_element<i, Tuple>::type>(std::get<i>(t)));


template<int i, class Tuple>
auto xget(Tuple&& t) as
    (static_cast<typename std::tuple_element<i, Tuple>::type>(std::get<i>(t)));


}
#include "macros.hpp"
#endif 
