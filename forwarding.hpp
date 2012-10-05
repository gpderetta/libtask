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

template <class F, class Tuple, int... Indices, class... Args>
auto forward0(F&& f, index_tuple<Indices...>, Tuple&& tup, Args&&... args)
    as ( f(forward_get<Indices>(tup)..., std::forward<Args>(args)...) );

template<class F, class Tuple, int ... Indices>
auto transform0(F&& f, index_tuple<Indices...>, Tuple&& t) 
    id(std::tuple<decltype(f(forward_get<Indices>(t)))...>,
       result(f(forward_get<Indices>(t))...) );

template<class Pack>
struct replacer_t {
    Pack packed_args;
    template<class Arg> 
    auto operator()(Arg&& x) as (std::forward<Arg>(x));

    template<int I>
    auto operator()(placeholder<I>) as (forward_get<I>(packed_args));
};

template<class Pack, class XPack = typename std::remove_reference<Pack>::type >
auto replacer(Pack&& p) ->
    replacer_t<XPack>
{ return { std::forward<Pack>(p) }; }
 
}
   
template <class F, class Tuple, class... Args>
auto forward(F&& f, Tuple&& tuple, Args&&... args)
    as ( details::forward0(std::forward<F>(f), 
                           details::indices(tuple), 
                           std::forward<Tuple>(tuple), 
                           std::forward<Args>(args)...) ) ;

template<class F, class Tuple>
auto transform(F&& f, Tuple&& t) 
    as( details::transform0(std::forward<F>(f), 
                            details::indices(t), 
                            std::forward<Tuple>(t)) );

template<class Tuple, class ...Args>
auto replace_placeholders(Tuple&& closure, Args&&... args) 
    as( transform(details::replacer(pack(std::forward<Args>(args)...)), 
                  std::forward<Tuple>(closure)) );

namespace details {
template<class F, class Closure>
struct binder_t {
    F f; Closure closure;

    binder_t(F&& f, Closure&& closure)
        : f(std::move(f)), closure(std::move(closure)) {}

    template<class... Args>
    auto operator()(Args&&... args)
        as( forward(f, replace_placeholders
                    (closure, std::forward<Args>(args)...)) );
};

template<class F, class Closure>
auto binder(F&& f, Closure&& closure) -> binder_t<F, Closure> {
    return { std::forward<F>(f),  std::forward<Closure>(closure) };
}


}

template<class F, class... Args>
auto bind(F&& f,  Args&&... args) 
    as( details::binder(std::forward<F>(f),
                        std::make_tuple(std::forward<Args>(args)...)) );

}
#include "macros.hpp"
#endif
