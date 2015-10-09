#ifndef GPD_Q_HPP
#define GPD_Q_HPP
#include <utility>
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

    friend constexpr bool operator==(constexpr_str lhs, constexpr_str rhs)
    {
        if (lhs.len != rhs.len) return false;
        for(std::size_t i = 0; i < lhs.len; ++i)
            if (lhs.begin[i] != rhs.begin[i]) return false;
        return true;
    }

    friend std::ostream& operator<<(std::ostream& s, constexpr_str str)
    {
        return s << str.begin;
    }
};

struct probe { template<class T> constexpr operator T() const { return {}; } };

template<class M, class T>
using rebind_field = typename M::template field<T>;

template<class M, class T, class D>
struct field;

template<class>
struct meta_of_;

template<class M, class T, class D>
struct meta_of_<field<M,T,D> > { using type = M; };

template<class T>
using meta_of_t = typename meta_of_<T>::type;

template<class T>
constexpr auto meta = meta_of_t<T>{};

template<class T>
constexpr auto meta_of(T&&) { return meta<std::decay_t<T> >; }

// template<class M, class T, class D>
// constexpr auto meta_of() { return M{}; }


template<class T>
constexpr auto get(T&& x) -> decltype((meta<std::decay_t<T> >(std::forward<T>(x)))) {
    return meta<std::decay_t<T> >(std::forward<T>(x));
}

template<class M, class T, class D>
struct field : D
{
    template<class T2>
    field(T2&& x) : D{ std::forward<T2>(x) } {}

    friend rebind_field<M, std::decay_t<T> > capture(const field& x) {
        return { get(x) };
    }

};

template<class T>
constexpr constexpr_str name() {
    decltype(std::declval<T>()()) x; return { x.name() };
}

template<class T>
using gen = T(*)();

template<class T> struct init : T {  constexpr init() : T(*this){}; };
template<class M>
struct symbol : M::fun, M::memfn, M::member {
    using M::fun::operator();
    using M::memfn::operator();
    using M::member::operator();

    template< class T>
    using field = decltype(typename M::field{}(symbol{}, gen<T>{}));

    template<class T>
    auto operator=(T&& x) const { return field<T>{std::forward<T>(x)};  }

    constexpr constexpr_str name() const { return gpd::name<typename M::name>(); }
    
};


template<class T>
using capture_t = decltype(capture(std::declval<T>()));

template<class... F>
struct named_tuple : capture_t<F> ... {
    named_tuple(F... t)
        : capture_t<F>(capture(t))...
    { }
    
    template<typename V>
        auto visit(V v) const {
        return v((meta<F> = meta<F>(*this))...);
    }

    template<typename V>
        auto visit(V v) {
        return v((meta<F> = meta<F>(*this))...);
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
                    (s << (std::exchange(first, false) ? "" : ", ")
                     << meta_of(t).name() << ": " << get(t), 0)... };
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

template<class Fun, class MemFn, class Member, class Field, class Name>
struct X {
    using fun = init<Fun>;
    using memfn = init<MemFn>;
    using member = init<Member>;
    using field = init<Field>;
    using name = Name;
};

template<class Fun, class MemFn, class Member, class Field, class Name>
auto make_symbol(Fun, MemFn, Member, Field, Name) {
    using X2 = X<Fun, MemFn, Member, Field, Name > ;
    return gpd::symbol< X2 > {};
    static_assert(sizeof(::gpd::symbol< X2 >) == 1, "not small");
}
}

#define $forward($s) std::forward<decltype($s)>($s)
#define $($s)                                                           \
    (false?                                                             \
     ::gpd::make_symbol(                                                \
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
        [](auto meta, auto g) noexcept {                                \
            using T = decltype(g());                                    \
            using M = decltype(meta);                                   \
            struct C { T $s; };                                         \
            return *(::gpd::field<M, T, C>*){};                        \
        },                                                              \
        [] {                                                            \
            struct { constexpr auto name() const { return #$s; } } x;   \
            return x;                                                   \
        }                                                               \
        ): ::gpd::probe{})                                              \
          /**/


#else
#undef GPD_Q_HPP
#undef $
#undef $forward

#endif
