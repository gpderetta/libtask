#ifndef GPD_RANGE_HPP
#define GPD_RANGE_HPP
namespace gpd {

template<class T>
struct optional
{
    bool set = false;
    alignas(T) char buf [sizeof(T)];

    explicit operator bool() const { return set; }
    
    T& get() { assert(set); return *reinterpret_cast<T*>(buf); }
    const T& get() const { assert(set); return *reinterpret_cast<const T*>(buf); }
    template<class T2>
    void emplace(T2&&x) { reset(); set = true; new (buf) T(std::forward<T2>(x)); }
    void reset() { if (set) { get().~T(); set = false; }  }
    optional() {}
    optional(const optional& rhs) { if(rhs.set) { emplace(rhs.get()); } }
    optional(optional&& rhs) { if(rhs.set) { emplace(std::move(rhs.get())); } }
    optional& operator=(optional rhs) { emplace(std::move(rhs.get())); return *this; }
    optional& operator=(T rhs) { emplace(std::move(rhs)); return *this; }

    bool operator==(const optional& rhs) const { return set == rhs.set && (!set || get() == rhs.get()); }

    bool operator==(const T& rhs) const { return !set || get() == rhs; }

    ~optional() { reset(); }
};

template<class T>
struct optional<T&>
{
    T * p = 0;
    
    T& get() const { return *p; }
    void emplace(T& x) { p = &x; }
    void reset() { p = 0; }
    optional() {}
    optional(const optional& rhs) : p(rhs.p) {  }
    optional& operator=(optional rhs) { p = rhs.p; return *this; }
    optional& operator=(T& rhs) { p = &rhs; return *this; }

    ~optional() { }
};



template<class R, class T>
typename std::enable_if<!std::is_same<R, T>::value, R>::type
make(T&& x)
{
    return R( x.begin(), x.end() );
}

template<class R>
R make(R&& x)
{
    return R(std::forward<R&&>(x));
}

template<class T>
using decay_t = typename std::decay<T>::type;

template <class R>
R copy(R&& r) {
return std::forward<R>(r);
}

template<template<class> class Base>
struct iter_facade : Base<iter_facade<Base> >
{
    using Base::Base;
    using iterator_category = std::forward_iterator_tag;
    using value_type = decay_t<decltype(this->get())>;
    using reference = value_type&;
    using difference_type = std::size_t;
    using pointer = value_type*;

    reference operator*()  { return this->get(self); };
    iterator& operator++() { iter++; this->next(self); return *this; }
    iterator operator++(int) { auto x = *this; operator++(); return x; }
    bool operator==(iterator rhs) const { return iter == rhs.iter; }
    bool operator!=(iterator rhs) const { return iter != rhs.iter; }

};

template<class X, template<class> class Y>
X& derived(Y<X>* self) { return static_cast<X&>(*self); }

template<class... R>
struct radapter {
    R... r;
    
};

template<class R, class P>
struct rfilter
{
    R r;
    P pred;

    template<class Derived>
    struct iterator  {
        self_t* self;
        using riter = decltype(std::begin(self->r));
        riter iter;

        iterator(self_t * self, riter iter) : self(self), iter(iter) { next();  }

        void next() {
            auto end = std::end(self->r);
            auto pred = self->pred;
            while(iter != end && !pred(*iter)) ++iter;
        }

        reference get() as (*iter);
    };

    iterator<rfilter> begin() { return {this, std::begin(r) }; }
    iterator<rfilter> end()   { return {this, std::end(r) }; }
    iterator<const rfilter> begin() const { return {this, std::begin(r) }; }
    iterator<const rfilter> end()   const { return {this, std::end(r) }; }

};

template<class R, class Pred>
rfilter<R&&, Pred> filter(R&& r, Pred pred) {
    return { std::forward<R&&>(r), pred };
}

template<class R>
struct rflatten
{
    R r;

    template<class self_t>
    struct iterator {
        self_t* self;
        using riter = decltype(std::begin(self->r));
        riter iter;
        using niter = decltype(std::begin(*iter));
        optional<niter> iter2;

        //iterator() {}
        iterator(self_t * self, riter iter_) : self(self), iter(iter_) { 
            
            auto end = std::end(self->r);
            if (iter != end)
            {
                iter2 = std::begin(*iter);
                next();
            }
        }

        void next() {
            std::cerr <<"x\n";
            auto end = std::end(self->r);

            while(!(iter2 == std::end(*iter)))
            {
                std::cerr <<"y\n";
                if (++iter == end)
                    iter2.reset();
                else
                    iter2 = std::begin(*iter);
            }
        }

        using iterator_category = std::forward_iterator_tag;
        using value_type = decay_t<decltype(*iter2.get())>;
        using reference = value_type&;;
        using difference_type = std::size_t;
        using pointer = value_type*;

        reference operator*()  { return (*iter2.get()); }

        iterator& operator++() { iter2.get()++; next(); return *this; }
        iterator operator++(int) { auto x = *this; operator++(); return x; }
        bool operator==(iterator rhs) const { return iter == rhs.iter && iter2 == rhs.iter2; }
        bool operator!=(iterator rhs) const { return !(*this == rhs); }
    };

    iterator<rflatten> begin() { return {this, std::begin(r) }; }
    iterator<rflatten> end()   { return {this, std::end(r) }; }
    iterator<const rflatten> begin() const { return {this, std::begin(r) }; }
    iterator<const rflatten> end()   const { return {this, std::end(r) }; }

};



template<class R>
rflatten<R&&> flatten(R&& r) {
    return { std::forward<R&&>(r) };
}



template<class R, class T>
struct rmap
{
    R r;
    using Range = decay_t<R>;
    T map;

    template<class self_t>
    struct iterator {
        self_t* self;
        using riter = decltype(std::begin(self->r));
        riter iter;
        iterator() {}
        iterator(self_t * self, riter iter) : self(self), iter(iter) { next() ;}

        void next() { if (iter != std::end(self->r)) x = self->map(*iter); }

        using iterator_category = std::forward_iterator_tag;
        using value_type = decay_t<decltype(self->map(*iter))>;
        using reference = value_type&;
        using difference_type = std::size_t;
        using pointer = value_type*;
     
        optional<decltype(self->map(*iter))> x;

        reference operator*() { return x.get(); }            

        iterator& operator++() { iter++; next() ; return *this; }
        iterator operator++(int) { auto tmp = *this; operator++(); return tmp; }
        bool operator==(iterator rhs) const { return iter == rhs.iter; }
        bool operator!=(iterator rhs) const { return iter != rhs.iter; }
    };

    iterator<rmap> begin()  { return {this, std::begin(r) }; }
    iterator<rmap> end()    { return {this, std::end(r) }; }
    iterator<const rmap> begin()  const { return {this, std::begin(r) }; }
    iterator<const rmap> end()    const { return {this, std::end(r) }; }

};


template<class R, class T>
rmap<R&&, T> map(R&& r, T t) {
    return { std::forward<R&&>(r), t };
}





template<class R1, class R2>
struct rzip
{
    R1 r1;
    R2 r2;

    template<class self_t>
    struct iterator {

        self_t* self;
        using riter1 = decltype(std::begin(self->r1));
        using riter2 = decltype(std::begin(self->r2));

        riter1 iter1;
        riter2 iter2;

        iterator(self_t* self, riter1 iter1, riter2 iter2) 
            : self(self)
            , iter1(iter1)
            , iter2(iter2) { }

        using iterator_category = std::forward_iterator_tag;

        using value_type = decltype(std::tie(*iter1, *iter2));
        using reference = value_type&;
        using difference_type = std::size_t;
        using pointer = value_type*;
     
        auto operator*() as ( std::tie(*iter1, *iter2) );

        iterator& operator++() { iter1++; iter2++; return *this; }
        iterator operator++(int) { auto x = *this; operator++(); return x; }
        bool operator==(iterator rhs) const { return iter1 == rhs.iter1 && iter2 == rhs.iter2; }
        bool operator!=(iterator rhs) const { return !(*this == rhs); }
    };

    iterator<rzip> begin()  { return {this, std::begin(r1), std::begin(r2) }; }
    iterator<rzip> end()    { return {this, std::end(r1), std::end(r2) }; }
    iterator<const rzip> begin()  const { return {this, std::begin(r1), std::begin(r2) }; }
    iterator<const rzip> end()    const { return {this, std::end(r1), std::end(r2) }; }

};


template<class R1, class R2>
rzip<R1&&, R2&&> zip(R1&& r1, R2&& r2) {
    return { std::forward<R1&&>(r1), std::forward<R2&&>(r2) };
}


}
#endif
