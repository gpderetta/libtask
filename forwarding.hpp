#ifndef TOOLS_HPP
#define TOOLS_HPP
#include "tuple.hpp"

#include "macros.hpp"
namespace gpd {


namespace details {
template<int...> struct index_tuple{};

template<int I, typename IndexTuple, typename... Types>
struct make_indices_impl;

template<int I, int... Indices, typename T,
         typename... Types>
struct make_indices_impl<I, index_tuple<Indices...>, T, Types...>
{
    typedef typename make_indices_impl<I + 1, index_tuple<Indices...,
                                                          I>, Types...>::type type;
};

template<int I, int... Indices>
struct make_indices_impl<I, index_tuple<Indices...> >
{
    typedef index_tuple<Indices...> type;
};

template<class... Types>
struct make_indices : make_indices_impl<0, index_tuple<>, Types...> {};

template<class ...Types>
auto indices(std::tuple<Types...> const&) 
    as (typename make_indices<Types...>::type());

template <class F, class Tuple, int... Indices>
auto forward0(F f, index_tuple<Indices...>, Tuple&& args)
    as ( f(forward_get<Indices>(std::forward<Tuple>(args))...) );

template<class F, class Tuple, int ... Indices>
auto transform0(F f, index_tuple<Indices...> idx, Tuple&& t) 
    as ( pack (f(forward_get<Indices>(t))...) );

template<class... T>
struct replacer_t {
 
    std::tuple<T&&...> packed_args;
    template<class Arg>
    auto operator()(Arg&& x) as (std::forward<Arg>(x));

    template<int I>
    auto operator()(placeholder<I>) as (forward_get<I>(packed_args));

};

template<class ...T>
auto replacer(T&&... t) id(replacer_t<T...>, replacer_t<T...>{ pack(std::forward<T>(t)...)});

}

    
template <class F, class Tuple>
auto forward(F f, Tuple tuple)
    as ( details::forward0(f, details::indices(tuple), tuple) ) ;

template<class F, class Tuple>
auto transform(F f, Tuple&& t) 
    as( details::transform0(f, details::indices(t), std::forward<Tuple>(t)) );

template<class Tuple, class ...T1>
auto replace_placeholders(Tuple&& closure, T1&&... args) 
    as(transform(details::replacer(std::forward<T1>(args)...), std::forward<Tuple>(closure)));



}
#include "macros.hpp"
#endif
