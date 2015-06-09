#ifndef GPD_VARIANT_HPP
#define GPD_VARIANT_HPP
#include "match.hpp"
#include "details/variant_details.hpp"
#include <cassert>

namespace gpd {

template<class T>
struct static_ { static T value; };

template<class T>
T static_<T>::value ={};
struct empty_t {

    constexpr bool operator==(empty_t) { return true; }
    constexpr bool operator<(empty_t) { return false; }
};

namespace { constexpr const empty_t& empty = static_<empty_t>::value; }
template<class>
struct is_variant : std::false_type {};
template<class...T>
struct is_variant<variant<T...>> : std::true_type {};
 
template<class... T>
struct variant : private details::assigner<0, empty_t, T...>
{
    variant() : _discriminant(0) {}

    ~variant() { reset(); }
    
    template<class T2,
             class = std::enable_if_t<!is_variant<std::decay_t<T2> >::value> >
    variant(T2&& x) : _discriminant(0) { unsafe_assign(std::forward<T2>(x)); }

    template<class... T2>
    variant(variant<T2...>&& rhs) : _discriminant(0) {
        rhs.visit([&](auto& x){ this->unsafe_assign(std::move(x)); } );
    }

    template<class... T2>
    variant(const variant<T2...>& rhs) : _discriminant(0) {
        rhs.visit([&](auto& x){ this->unsafe_assign(x); } );
    }

    variant(const variant& rhs) : _discriminant(0) {
        rhs.visit([&](auto& x){ this->unsafe_assign(x); } );
    }

    template<class T2>
    static int constexpr index_of(T2* = 0) {
        return details::assigner<0, empty_t, T...>::get_index((T2*)0);
    }

    template<class T2>
    bool is(T2* = 0) const {
        return index_of<T2>() == index();
    }

    bool empty() const { return _discriminant == 0; }

    int index() const { return _discriminant; }

    template<class T2>
    T2& get(const T2* = 0) 
    { 
        assert(index_of<T2>() == _discriminant);
        return *static_cast<T2*>(this->buffer());
    }

    template<class T2>
    const T2& get(const T2* = 0) const 
    { 
        assert(index_of<T2>() == _discriminant);
        return *static_cast<const T2*>(this->buffer());
    }

    template<class T2, class = std::enable_if_t<!is_variant<std::decay_t<T2>>::value> >
    variant& operator=(T2&& rhs)
    {
        if (index_of<std::decay_t<T2>>() == _discriminant)
            get<T2>() = std::forward<T2>(rhs); 
        else
            assign(std::forward<T2>(rhs)); 
        return *this;  
    }

    template<class... T2>
    variant& operator=(variant<T2...> const& rhs)
    {
        rhs.visit([&] (auto&& x){ this->assign(x); });
        return *this;  
    }

    template<class... T2>
    variant& operator=(variant<T...> && rhs)
    {
        rhs.visit([&] (auto&& x){ this->assign(x); });
        return *this;  
    }

    variant& operator=(const variant& rhs)
    {
        if (rhs._discriminant == _discriminant)
            rhs.visit([&](auto& x){ this->get(&x) = x; } );
        else {
            // absolutely not exception safe
            reset();
            rhs.visit([&](auto& x){ this->unsafe_assign(x); } );
        }
        return *this;
    }

    variant& operator=(variant&& rhs)
    {
        if (rhs._discriminant == _discriminant)
            rhs.visit([&](auto& x){ this->get(&x) = std::move(x); } );
        else {
            reset();
            rhs.visit([&](auto& x){ this->unsafe_assign(std::move(x)); } );
        }
        return *this;
    }
        
    template<class T2>
    void assign(T2&& x) 
    {
        reset();
        unsafe_assign(std::forward<T2>(x));
    }

    template<class V>
    decltype(auto) visit(V&& v)
    {
        using R = std::common_type_t<decltype(v(std::declval<empty_t&>())), decltype(v(std::declval<T&>()))...>;
        return details::do_visit<R>(_discriminant, *this, v);
    }

    template<class V>
    decltype(auto) visit(V&& v) const
    {
        using R = std::common_type_t<decltype(v(std::declval<const empty_t&>())), decltype(v(std::declval<const T&>()))...>;
        return details::do_visit<R>(_discriminant, *this, v);
    }

    template<class... V>
    decltype(auto) visit(V&&... v) { return visit(match(std::forward<V>(v)...)); }

    template<class... V>
    decltype(auto) visit(V&&... v) const { return visit(match(std::forward<V>(v)...)); }

    template<class V>
    decltype(auto) map(V&& v)
    {
        using R = details::to_variant<decltype(v(std::declval<empty_t&>())),
                                      decltype(v(std::declval<T&>()))...>;
        return details::do_visit<R>(_discriminant, *this, v);
    }

    template<class V>
    decltype(auto) map(V&& v) const
    {
        using R = details::to_variant<decltype(v(std::declval<const empty_t&>())),
                                      decltype(v(std::declval<const T&>()))...>;
        return details::do_visit<R>(_discriminant, this, v);
    }

    template<class... V>
    decltype(auto) map(V&&... v) { return map(match(std::forward<V>(v)...)); }

    template<class... V>
    decltype(auto) map(V&&... v) const { return map(match(std::forward<V>(v)...)); }

    friend bool operator==(variant const& lhs, variant const& rhs)
    {
        return lhs._discriminant == rhs._discriminant &&
            lhs.visit([&](auto const& x) { rhs.get(&x) == x; });
    }

    friend bool operator!=(variant const& lhs, variant const& rhs)
    {
        return !(lhs == rhs);
    }

    void reset() noexcept { 
        visit([](auto& x) { using T3 = std::decay_t<decltype(x)>; x.~T3(); });
        _discriminant = 0;
    }

private:
    void * buffer() { return _buffer; }
    const void * buffer() const { return _buffer; }

    int _discriminant;        
    enum { size = details::const_max(sizeof(T)...) };
    enum { alignment = details::const_max(alignof(T)...) };
    alignas(alignment) char _buffer[size]; 

    // reset must be called or the discriminant must be otherwise
    // 0. On exception the discriminant will be 0.
    template<class T2>
    void unsafe_assign(T2&& x) {
        assert(_discriminant == 0);
        _discriminant = this->do_assign(_buffer, std::forward<T2>(x));
    }

    
};

}
#endif
