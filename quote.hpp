#ifndef GPD_QUOTE_HPP
#define GPD_QUOTE_HPP
#include <utility>
#include "macros.hpp"
//#include <boost/preprocessor/seq.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>

namespace gpd { 
namespace details {

template<int i>
struct quoted_op;

#define GPD_QUOTE_binops                                \
    (||)(&&)(/)(%)(|)(^)(<<)(>>)(<)(>)(=)(->*)          \
        (==)(<=)(>=)(!=)(*=)(/=)(+=)(-=)(&=)(^=)(|=)    \
    /**/
        
#define GPD_QUOTE_prebinops (+)(-)(*)(&)
#define GPD_QUOTE_preops (!)(~)(++)(--)

struct quote_probe {
    void operator[](quote_probe) {}
    void operator()(quote_probe) {}
#define GPD_QUOTE_binop_expand(r, data, op) \
    void operator op (quote_probe) {}
BOOST_PP_SEQ_FOR_EACH(GPD_QUOTE_binop_expand, ~, 
                      GPD_QUOTE_binops GPD_QUOTE_prebinops);
#undef GPD_QUOTE_binop_expand

#define GPD_QUOTE_op_expand(r, data, op) void operator op () {}
BOOST_PP_SEQ_FOR_EACH(GPD_QUOTE_op_expand, ~, GPD_QUOTE_preops);
#undef GPD_QUOTE_op_expand
};

struct unary {
    template<void (quote_probe::*)()>
    struct apply {};
};

#define GPD_QUOTE_preop_xspecialize(r, base,op)                          \
    template<>                                                          \
    struct unary::apply<&quote_probe::operator op > {                    \
        template<class Arg>                                             \
            constexpr auto operator()(Arg&& arg)                        \
            as((op std::forward<Arg>(arg)));                            \
    };                                                                  \
/**/

BOOST_PP_SEQ_FOR_EACH(GPD_QUOTE_preop_xspecialize, 200, GPD_QUOTE_preops)
#undef GPD_QUOTE_preop_xspecialize

inline constexpr unary xprobe(...){ return {}; }

struct binary{
    template<void (quote_probe::*)(quote_probe)>                 
    struct apply {};                                                    
};
#define GPD_QUOTE_binop_xspecialize(r, base, i, op)                     \
    template<>                                                          \
    struct binary::apply<&quote_probe::operator op > {                   \
        template<class Lhs, class Rhs>                                  \
        constexpr auto operator()(Lhs&& lhs, Rhs&& rhs)                 \
            as((std::forward<Lhs>(lhs) op std::forward<Rhs>(rhs)));     \
    };                                                                  \
/**/
BOOST_PP_SEQ_FOR_EACH_I(GPD_QUOTE_binop_xspecialize, 0, GPD_QUOTE_binops)
#undef GPD_QUOTE_binop_xspecialize

template<>                                                              \
struct binary::apply<&quote_probe::operator [] > {                      \
    template<class Lhs, class Rhs>                                      \
    constexpr auto operator()(Lhs&& lhs, Rhs&& rhs)                     \
        as(std::forward<Lhs>(lhs)[std::forward<Rhs>(rhs)]);             \
};                                                                      \

template<>                                                              \
struct binary::apply<&quote_probe::operator () > {                      \
    template<class F, class... Args>                                     \
    constexpr auto operator()(F&& f, Args&&... args)                     \
        as(std::forward<F>(f)(std::forward<Args>(args)...));            \
};                                                                      \


#define GPD_QUOTE_prebinop_xspecialize(r, base, i, op)                  \
    template<>                                                          \
    struct binary::apply<&quote_probe::operator op > {                   \
        template<class Lhs, class Rhs>                                  \
        constexpr auto operator()(Lhs&& lhs, Rhs&& rhs)                 \
            as((std::forward<Lhs>(lhs) op std::forward<Rhs>(rhs)));     \
        template<class Arg>                                             \
        constexpr auto operator()(Arg&& arg)                            \
            as((op std::forward<Arg>(arg)));                            \
    };                                                                  \
/**/
BOOST_PP_SEQ_FOR_EACH_I(GPD_QUOTE_prebinop_xspecialize, 100, GPD_QUOTE_prebinops)
#undef GPD_QUOTE_prebinop_xspecialize
inline constexpr binary xprobe(void (quote_probe::*)(quote_probe)) { return {}; }


#undef GPD_QUOTE_preops
#undef GPD_QUOTE_binops
#undef GPD_QUOTE_prebinops
} }
#include "macros.hpp"

#endif

#ifndef GPD_QUOTE
#define GPD_QUOTE
#include "macros.hpp"

#define quotable(name)                                                  \
    struct unusable;                                                    \
    void name(unusable);                                                \
                                                                        \
    struct quoted_##name {                                              \
        template<class T0, class... Rest>                               \
        constexpr auto apply(long, T0&& t0, Rest&&... x)                \
            as(name(std::forward<T0>(t0), std::forward<Rest>(x)...));   \
                                                                        \
        template<class T>                                               \
        constexpr auto apply(int, T&& x) as(x.name);                    \
                                                                        \
        template<class T, class... Args>                                \
        constexpr auto apply(int, T&& x, Args&&... args)                \
            as(std::forward<T>(x).name(std::forward<Args>(args)...));   \
                                                                        \
        template<class... Args>                                         \
        constexpr auto operator()(Args&&...args)                        \
            as(apply(0L, std::forward<Args>(args)...));                 \
    };                                                                  \
    constexpr quoted_##name  $##name{};                                 \
/**/

//#define $(op) ::gpd::details::quoted_op< ::gpd::details::probe(&::gpd::details::quote_probe::operator op)>()

template<class T>
using identity = T;

#define $(op) identity<decltype(::gpd::details::xprobe(&::gpd::details::quote_probe::operator op))>::apply<&::gpd::details::quote_probe::operator op >()

#else
#include "macros.hpp"

#undef GPD_QUOTE
#undef quotable
#undef $
#endif
