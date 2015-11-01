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

struct expand{ template<class... T> expand(T&&...){}};


struct probe { template<class T> constexpr operator T() const { return {}; } };

template<class Field>
struct meta_of_ { using type = typename Field::__symbol_type; };

template<class T>
using meta_of_t = typename meta_of_<T>::type;

template<class Field>
struct type_of_ { using type = typename Field::__value_type; };

template<class T>
using type_of_t = typename type_of_<T>::type;

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

#define GPD_expand(e) e
#define GPD_fn(decl, expr)                      \
    constexpr decltype(auto)                    \
        GPD_expand decl ->decltype(GPD_expand expr)                   \
        noexcept(noexcept(GPD_expand expr))                \
    { return GPD_expand expr; }                            \
    /**/


#define GPD_make_binary_op(name, symbol)                                \
    namespace operators {                                               \
    struct name {                                                       \
        template<class Lhs, class Rhs>                                  \
        GPD_fn((operator()(Lhs&& lhs, Rhs&&rhs) const),                 \
               (std::forward<Lhs>(lhs) symbol std::forward<Rhs>(rhs))); \
    };                                                                  \
    }                                                                   \
    template<class Lhs, class Rhs>                                      \
    struct expression<operators::name, Lhs, Rhs>                        \
    {                                                                   \
        Lhs lhs; Rhs rhs;                                               \
        template<class V>                                               \
        friend GPD_fn((visit(V v, expression e)), (v(operators::name{}, e.lhs, e.rhs))); \
                                                                        \
    }                                                                   \
                                                                        \
    /**/

#define GPD_make_unary_op_prefix(name, op)                              \
    namespace operators {                                               \
    struct name {                                                       \
        template<class T>                                               \
        GPD_fn((operator()(T&& x) const), (op std::forward<T>(x)));     \
    };                                                                  \
    template<class T>                                                   \
    struct expression<operators::name, T>                               \
    {                                                                   \
        T value;                                                        \
        template<class V>                                               \
            friend GPD_fn((visit(V v, expression e)), (v(operators::name{}, e.value))); \
    }                                                                   \
                                                                        \
    /**/

//+ - * / % ˆ & | ~ ! = < > += -= *= /= %= ˆ= &= |= << >> >>= <<= == != <= >= && || ++ -- , ->* -> ( ) [ ]
GPD_make_binary_op(eq, ==);
GPD_make_binary_op(neq, !=);
GPD_make_binary_op(lt, <);
GPD_make_binary_op(gt, >);
GPD_make_binary_op(lte, <=);
GPD_make_binary_op(gte, >=);

GPD_make_binary_op(plus_eq, +=);
GPD_make_binary_op(minus_eq, -=);
GPD_make_binary_op(times_eq, *=);
GPD_make_binary_op(div_eq, /=);
GPD_make_binary_op(mod_eq, %=);
GPD_make_binary_op(bitxor_eq, ^=);
GPD_make_binary_op(bitand_eq, &=);
GPD_make_binary_op(bitor_eq, |=);
GPD_make_binary_op(lshift_eq, <<=);
GPD_make_binary_op(rshift_eq, >>=);

GPD_make_binary_op(plus, +);
GPD_make_binary_op(minus, -);
GPD_make_binary_op(times, *);
GPD_make_binary_op(div, /);
GPD_make_binary_op(mod, %);
GPD_make_binary_op(bitxor, ^);
GPD_make_binary_op(bitand_, &);
GPD_make_binary_op(bitor_, |);
GPD_make_binary_op(comp, ~);
GPD_make_binary_op(lshift, <<);
GPD_make_binary_op(rshift, >>);

GPD_make_binary_op(and_, &&);
GPD_make_binary_op(or_, ||);
GPD_make_binary_op(_, &&);

GPD_make_binary_op(assign, =);




template<class Tag, class...> struct expression;

template<class Lhs, class Rhs>
struct expression<operators::assign, Lhs, Rhs>
{
    Lhs lhs; Rhs rhs;
};

template<class Lhs, class Rhs>
struct expression<operators::lte, Lhs, Rhs>
{
    Lhs lhs; Rhs rhs;
};

template<class Lhs, class Rhs>
struct expression<operators::gte, Lhs, Rhs>
{
    Lhs lhs; Rhs rhs;
};

template<class Lhs, class Rhs>
struct expression<operators::gte, Lhs, Rhs>
{
    Lhs lhs; Rhs rhs;
};


template<class T>
struct terminal
{
    T value;
    friend constexpr auto eval(terminal& x) { return x.value; }
    friend constexpr auto eval(const terminal& x) { return x.value; }
    friend constexpr auto eval(terminal&& x) { return x.value; }
};

template<class M> struct symbol;

template<class M, class T>
using param = expression<operators::assign, terminal<symbol<M> >, terminal<T> >;

template<class M, class T>
auto as_field(param<M, T> expr) ->
    rebind_field_t<std::decay_t<T>, symbol<M> >
{
    return { std::forward<T>(expr.rhs.value) };
}

template<class T>
using as_field_t = decltype(gpd::as_field(std::declval<T>()));

template<class M, class T>
auto as_ref_field(param<M,T> expr) ->
    rebind_field_t<T, symbol<M> >
{
    return { std::forward<T>(expr.rhs.value) };
}

template<class T>
using as_ref_field_t = decltype(gpd::as_ref_field(std::declval<T>()));

template<class T>
constexpr constexpr_str name() {
    decltype(std::declval<T>()()) x; return { x.name() };
}

template<class T>
struct gen { using type = T; };

template<class T> T xgen() { return *(T*)0; }

template<class T> struct init : T {  constexpr init() : T(*this){}; };

template<class M>
struct symbol : M::fun, M::memfn, M::member, M::dnu {
    using M::fun::operator();
    using M::memfn::operator();
    using M::member::operator();
    using M::dnu::operator();

    template< class T>
    using field = std::result_of_t<typename M::field(gen<T>)>;
    
    template<class T>
    auto operator=(T&& x) const -> param<M, T>
    { return {{*this}, {std::forward<T>(x)} };  }

    template<class T>
    friend auto operator<(T&& x, symbol) {
        return {std::forward<T>(x)};
    }

    constexpr constexpr_str name() const { return M{}.name(); }
    
};

template<class... T, class F>
constexpr auto callable(F f) -> decltype(f(std::declval<T>()...), true) {
    (void) f; return true;
}

template<class...>
constexpr auto callable(...) -> bool { return false; }


template<class R, class M, class X>
auto get_with_default(symbol<M> m, X&& x, R&&) -> decltype(m(std::forward<X>(x)))
{
    return m(std::forward<X>(x));
}

template<class, class M, class X>
auto get_with_default(symbol<M> m, X&& x) -> decltype(m(std::forward<X>(x)))
{
    return m(std::forward<X>(x));
}

template<class R, class M, class X>
auto get_with_default(symbol<M>, X&&, R&& r = {}) ->
    std::enable_if_t<!callable<X>(symbol<M>{}), std::decay_t<R> >
{
    return std::forward<R>(r);
}

template<class X>
struct binder;

static constexpr struct elementwise_tag {} elementwise = {};
template<class... F>
struct named_tuple : F ... {

    template<class... Args>
        named_tuple(elementwise_tag, Args&& ... args)
        : F{std::forward<Args>(args)}...
    { }
    
    template<class... M, class... T>
        named_tuple(param<M, T>...)
      ;

    template<class... F2>
        named_tuple(named_tuple<F2...>&& x)
        : F{ meta<F>(x) }...
    { }

    template<class... F2>
        named_tuple(named_tuple<F2...>& x)
        : F{ meta<F>(x) }...
    { }

    template<class... F2>
        named_tuple(const named_tuple<F2...>& x)
        : F{ meta<F>(x) }...
    { }

    named_tuple(named_tuple&& x) = default;
    named_tuple(named_tuple& x)  = default;
    named_tuple(const named_tuple& x) = default;

    template<class Rhs>
        explicit named_tuple(binder<Rhs> x);
    
    template<typename V>
        auto visit(V v) const {
        return v((static_cast<const F&>(*this))...);
    }

    template<typename V>
        auto visit(V v) {
        return v((static_cast<F&>(*this))...);
    }

    template<class Rhs>
    named_tuple& operator=(Rhs&& rhs)
        {
            expand{get(static_cast<F&>(*this)) =
                    std::move(meta<F>(std::forward<Rhs>(rhs)))...};
            return *this;
        }

    template<class Rhs>
        named_tuple& operator=(binder<Rhs> rhs);

    template<class T>
        explicit operator T() const;
};

template<class X>
struct binder {
    X value;
    template<class... F>
    X operator=(named_tuple<F...> const& x)
    {
        expand{meta<F>(value) = get(static_cast<F const&>(x))...};
        return value;
    }
    template<class... F>
    X operator=(named_tuple<F...> && x)
    {
        expand{meta<F>(value) = std::move(get(static_cast<F&&>(x)))...};
        return value;
    }

};

template<class X>
binder<X&> to(X& x) { return {x}; }
template<class X>
binder<X> from(X&& x) { return {std::forward<X>(x)}; }

template<class... M, class... T>
named_tuple<as_field_t<param<M, T> >... >
tup(param<M, T>... expr)
{
    return {elementwise, std::forward<T>(expr.rhs.value)...};
};

template<class... M, class... T>
named_tuple<as_ref_field_t<param<M,T> >...>
ref(param<M, T>... expr)
{
    return {elementwise, expr.rhs.value...};
};

template<class... F>
template<class... M, class... T>
named_tuple<F...>::named_tuple(param<M, T>... args)
    : named_tuple(from(ref(args...)))
{
}


template<class... F>
template<class Rhs>
named_tuple<F...>::named_tuple(binder<Rhs> x)
    : F{ get_with_default<type_of_t<F> >(meta<F>, std::forward<Rhs>(x.value)) }...
    { }

template<class... F>
template<class Rhs>
named_tuple<F...>& named_tuple<F...>::operator=(binder<Rhs> x)
{
    this = named_tuple{x};
    return *this;
}

template<class... F>
template<class T>
named_tuple<F...>::operator T() const
{
    T x;
    to(x) = *this;
    return x;
}


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
        auto dnu_ = [](auto&& h)                                    \
            noexcept(noexcept(does_not_understand(self_t{}, $forward(h)))) \
            -> decltype(does_not_understand(self_t{}, $forward(h)))     \
            { return does_not_understand(self_t{}, $forward(h)); };     \
                                                                    \
        auto field_ = [](auto f) noexcept {                         \
            struct field {                                          \
                using __symbol_type = self_t;                       \
                using __value_type = typename decltype(f)::type;        \
                __value_type $s;                                        \
                /*decltype(f) $x;*/                                 \
            };                                                      \
            return gpd::xgen<field>();                              \
        };                                                          \
        struct meta {                                               \
            constexpr auto name() const { return #$s; }             \
            using fun = ::gpd::init<decltype(fun_)>;                \
            using memfn = ::gpd::init<decltype(memfn_)>;            \
            using member = ::gpd::init<decltype(member_)>;          \
            using dnu = ::gpd::init<decltype(dnu_)>;          \
            using field = ::gpd::init<decltype(field_)>;            \
        } ; return gpd::symbol<meta>{}; }():                        \
     ::gpd::probe{})                                                \
     /**/


#else
#undef GPD_Q_HPP
#undef $
#undef $forward

#endif
