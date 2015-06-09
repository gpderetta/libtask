#ifndef GPD_VARIANT_DETAILS_HPP
#define GPD_VARIANT_DETAILS_HPP
#include <type_traits>
#include <utility>
namespace gpd {
template<class...>
struct variant;
struct empty_t;

namespace details {

template<class H, class T>
struct cons : T {
    using T::test;
    static std::true_type test(H*);
};

struct base {
    template<class T>
    static std::false_type test(T*);
    static std::true_type test(empty_t*);
};

template<class H, class... X>
struct deduplicate {
    using tail = typename deduplicate<X...>::type;
    using present = decltype(tail::test((H*)0));
    using type = std::conditional_t<present::value, tail, cons<H, tail> >;
};

template<class H>
struct deduplicate<H> {
    using type = cons<H, base>;
};


template<class L, class... X>
struct cons2variant { using type = variant<X...>; };

template<class H, class T, class... X>
struct cons2variant<cons<H, T>, X...> : cons2variant<T, X..., H> {};


template<class... T>
using to_variant = typename cons2variant<typename deduplicate<T...>::type>::type;

static_assert(std::is_same<to_variant<double, int, int, double, int, float>,
              variant<double, int, float> >::value, "");



constexpr auto const_max(std::size_t h) 
{
    return h;
}

template<class... T>
constexpr auto const_max(std::size_t x, std::size_t h, T... t) 
{
    return const_max(x > h ? x : h, t...);
} 

template<int i, class T, class... R>
struct assigner : assigner<i + 1, R...> {
    using assigner<i + 1, R...>::do_assign;
    using assigner<i + 1, R...>::get_index;
    constexpr int do_assign(void * p, T&& x)
    {
        return new (p) T(std::move(x)), i;
    }
    constexpr int do_assign(void * p, const T& x)
    {
        return new (p) T(x), i;
    }
    static constexpr int get_index(const T*) 
    {
        return i;
    }
};

template<int i, class T>
struct assigner<i, T> {
    constexpr int do_assign(void * p, T&& x)
    {
        return new (p) T(std::move(x)), i;
    }
    constexpr int do_assign(void * p, const T& x)
    {
        return new (p) T(x), i;
    }
    static constexpr int get_index(const T*) 
    {
        return i;
    }
};

template<class Variant, class R, class T, class F>
R evaluate(Variant&& v, F&& f) {
    return f(v.template get<T>());
}

template<class R, class... T, class F>
R do_visit(int discriminant, variant<T...>&v , F&&f) {
    using J = R(variant<T...>&, F&&);
    static constexpr J * table[] = {
        &evaluate<variant<T...>&, R, empty_t, F>,
        &evaluate<variant<T...>&, R, T, F>...
    };
    return table[discriminant](v, f);
}

template<class R, class... T, class F>
R do_visit(int discriminant, const variant<T...>&v , F&&f) {
    using J = R(const variant<T...>&, F&&);
    static constexpr J * table[] = {
        &evaluate<const variant<T...>&, R, empty_t, F>,
        &evaluate<const variant<T...>&, R, T, F>...
    };
    return table[discriminant](v, f);
}

}

}
#endif
