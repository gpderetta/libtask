#ifndef GPD_MATCH_HPP
#define GPD_MATCH_HPP
namespace gpd {
namespace details {


template<class...>
struct xoverload;

template<>
struct xoverload<>  {
    struct uncallable{};
    void operator()(uncallable){}
};


template<class Head, class... Tail>
struct xoverload<Head, Tail ...> : Head, xoverload<Tail...> {
    xoverload(Head h, Tail... t) : Head(h), xoverload<Tail...>(t...) {}
    using xoverload<Tail...>::operator();
    using Head::operator();
};

template<class... F>
struct xoverload_set 
    : xoverload<F...>
{
    //using F::operator()...;
    xoverload_set(F... f)
        : xoverload<F...>(f...)
    {}
};

}
template<class... F>
details::xoverload_set<F...> 
match(F... f) {
    return {f...};

}
}
#endif
