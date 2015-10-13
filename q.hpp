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

template<class Field>
struct meta_of_ { using type = typename Field::__symbol_type; };

template<class T>
using meta_of_t = typename meta_of_<T>::type;

template<class T>
constexpr auto meta = meta_of_t<T>{};

template<class T>
constexpr auto meta_of(T&&) { return meta<std::decay_t<T> >; }

template<class T>
constexpr auto get(T&& x) -> decltype((meta<std::decay_t<T> >(std::forward<T>(x)))) {
    return meta<std::decay_t<T> >(std::forward<T>(x));
}

template<class T, class Meta>
using rebind_field_t = typename Meta::template field<T>;

template<class Field, class Meta = meta_of_t<std::decay_t<Field> > >
auto capture(Field&& x, Meta meta = Meta{}) ->
    rebind_field_t<std::decay_t<decltype(meta(std::forward<Field>(x)))>, Meta>
{
    return { meta(std::forward<Field>(x)) };
}

template<class T>
constexpr constexpr_str name() {
    decltype(std::declval<T>()()) x; return { x.name() };
}

template<class T>
struct gen { using type = T; };

template<class T> T xgen() { return *(T*)0; }

template<class T> struct init : T {  constexpr init() : T(*this){}; };
template<class M>
struct symbol : M::fun, M::memfn, M::member {
    using M::fun::operator();
    using M::memfn::operator();
    using M::member::operator();

    template< class T>
    using field = std::result_of_t<typename M::field(gen<T>)>;
    
    template<class T>
    auto operator=(T&& x) const -> field<T>  { return {std::forward<T>(x)};  }

    template<class T>
    friend auto operator<(T&& x, symbol) {
        return {std::forward<T>(x)};
    }

    constexpr constexpr_str name() const { return M{}.name(); }
    
};


template<class T>
using capture_t = decltype(gpd::capture(std::declval<T>()));

template<class... F>
struct named_tuple : capture_t<F> ... {
    named_tuple(F... t)
        : capture_t<F>{capture(t)}...
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

struct eval { template<class... T> eval(T&&...){}};

template<class... M>
std::ostream& operator<<(std::ostream& s, named_tuple<M...> const& t)
{
    s << "{";
                    
    t.visit([&s](auto h, auto... t)
            {
                s << meta_of(h).name() << ": " << get(h);
                eval{s << ", " << meta_of(t).name() << ": " << get(t)...};
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


}

#define $forward($s) std::forward<decltype($s)>($s)
#define $($s)                                                       \
    (false?                                                         \
     [] {                                                           \
        struct meta;                                                \
        using self_t = ::gpd::symbol<meta>;                         \
        auto fun_ = [](auto&&... args)                              \
            noexcept(noexcept($s($forward(args)...)))               \
            -> decltype($s($forward(args)...))                      \
            { return $s($forward(args)...); };                      \
        auto memfn_ = [](auto&& h, auto&&... args)                  \
            noexcept(noexcept($forward(h).$s($forward(args)...)))   \
            -> decltype($forward(h).$s($forward(args)...))          \
            { return $forward(h).$s($forward(args)...); } ;         \
        auto member_ = [](auto&& h)                                 \
            noexcept(noexcept($forward(h).$s))                      \
            -> decltype(($forward(h).$s))                           \
            { return ($forward(h).$s); };                           \
                                                                    \
        auto field_ = [](auto f) noexcept {                         \
            struct field {                                          \
                using __symbol_type = self_t;            \
                typename decltype(f)::type $s;                  \
                /*decltype(f) $x;*/                                 \
            };                                                      \
            return gpd::xgen<field>();                              \
        };                                                          \
        struct meta {                                               \
            constexpr auto name() const { return #$s; }             \
            using fun = ::gpd::init<decltype(fun_)>;                \
            using memfn = ::gpd::init<decltype(memfn_)>;            \
            using member = ::gpd::init<decltype(member_)>;          \
            using field = ::gpd::init<decltype(field_)>;            \
        } ; return gpd::symbol<meta>{}; }():                        \
     ::gpd::probe{})                                                \
     /**/


#else
#undef GPD_Q_HPP
#undef $
#undef $forward

#endif
