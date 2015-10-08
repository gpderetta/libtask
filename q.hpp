#ifndef GPD_Q_HPP
#define GPD_Q_HPP
#include <utility>
#include "match.hpp"
#include <iostream>

namespace gpd {

template<template<class...> class M, class... T>
M<std::decay_t<T>...> make(T&&...x) {
    return M<std::decay_t<T>...> {std::forward<T>(x)...};
}

struct constexpr_str
{
    constexpr constexpr_str(const char * s) 
        : begin(s), len(0)
    {
        while(begin[len++])
            ;
    }
    const char * begin;
    std::size_t len;
    constexpr std::size_t size() const { return len; }

    constexpr bool operator==(constexpr_str rhs) const
    {
        if (len != rhs.len) return false;
        for(std::size_t i = 0; i < len; ++i)
            if (begin[i] != rhs.begin[i]) return false;
        return true;
    }
};

struct probe { template<class T> constexpr operator T() const { return {}; } };

template<class T> struct init : T {  constexpr init() : T(*this){}; };
template<class Fun, class MemFn, class Member, class Forward, class Name>
struct quoted : Fun, MemFn, Member {
    using Fun::operator();
    using MemFn::operator();
    using Member::operator();

    template<class T>
    auto operator=(T&& x) const {
        return Forward{}(*this, std::forward<T>(x));
    }

    static constexpr constexpr_str name() {
        decltype(Name{}()) meta{};
        return { meta.name() };
    }
};


template<class T>
auto capture(T&& x) -> decltype(x.$bind())  { return x.$bind(); }

template<class T>
using capture_t = typename T::$bind_t;

template<class... M>
struct named_tuple : capture_t<M> ... {
    named_tuple(M... t)
        : capture_t<M>(capture(t))...
    { }
    
    template<typename V>
        auto visit(V v) const {
        return v(static_cast<capture_t<M> const& >(*this).$meta() =
                 static_cast<capture_t<M> const& >(*this).$get()...);
    }

    template<typename V>
        auto visit(V v) {
        return v(static_cast<capture_t<M>&>(*this).$meta() =
                 static_cast<capture_t<M>&>(*this).$get()...);
    }

};

template<class... M>
std::ostream& operator<<(std::ostream& s, named_tuple<M...> const& t)
{
    s << "{";
                    
    t.visit([&s](auto... t)
            {
                bool first = true;
                bool _ [] = {
                    (s << (std::exchange(first, false) ? "" : ", ") << t.$meta().name().begin << ": " << t.$get(), 0)... };
                (void)_;
            });
    return s << "}";
}

template<class... T, class F>
constexpr auto callable(F f) -> decltype(f(std::declval<T>()...), true) {
    (void) f; return true;
}

template<class...>
constexpr auto callable(...) -> bool { return false; }


template<class... T>
named_tuple<T...> tup(T... t)
{
    return {std::forward<T>(t)...};
};

template<class... F>
::gpd::quoted< ::gpd::init<F>...>
mkquoted(F... ) {
    return {};
    static_assert(sizeof(::gpd::quoted< ::gpd::init<F>...>) == 1, "not small");
}
}

#define $forward($s) std::forward<decltype($s)>($s)
#define $($s)                                                           \
    (false?                                                             \
    ::gpd::mkquoted(                                                    \
        [](auto&&... args)                                              \
        noexcept(noexcept($s($forward(args)...)))                       \
        -> decltype($s($forward(args)...)) {                            \
            return $s($forward(args)...);                               \
        },                                                              \
        [](auto&& h, auto&&... args)                                    \
        noexcept(noexcept($forward(h).$s($forward(args)...)))           \
        -> decltype($forward(h).$s($forward(args)...)) {                \
            return $forward(h).$s($forward(args)...);                   \
        },                                                              \
        [](auto&& h)                                                    \
        noexcept(noexcept(h.$s))                                        \
        -> decltype(($forward(h).$s)) {                                 \
            return ($forward(h).$s);                                    \
        },                                                              \
        /* forward */                                                   \
        [](auto base, auto&& x) noexcept                                \
        {                                                               \
            struct C {                                                  \
                using $type = std::decay_t<decltype(x)>;                \
                $type $s;                                               \
                $type const& $get() const& { return $s; }               \
                $type& $get() & { return $s; }                          \
                $type&& $get() && { return std::move($s); }             \
                typedef decltype(base) $meta_t;                         \
                $meta_t  $meta() const { return{}; }                    \
                                                                        \
            };                                                          \
            struct F  {                                                 \
                using $type = decltype($forward(x));                    \
                $type $s;                                               \
                $type $get() const { return $forward($s); }             \
                typedef C   $bind_t;                                    \
                $bind_t $bind() const { return { $forward($s)}; }       \
                typedef decltype(base) $meta_t;                         \
                $meta_t  $meta() const { return{}; }                    \
            };                                                          \
            return F{ $forward(x) };                                    \
        },                                                              \
        [] {                                                            \
            struct R {                                                  \
                constexpr const char* name() const { return #$s; }      \
            };                                                          \
            return R{};                                                 \
        }                                                               \
        ): ::gpd::probe{})                                              \
          /**/


#else
#undef GPD_Q_HPP
#undef $
#undef $forward

#endif
