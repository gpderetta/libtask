#ifndef GPD_PIPE_HPP
#define GPD_PIPE_HPP

#include "continuation.hpp"
#include <utility>

#include "macros.hpp"
namespace gpd {



namespace details {
template<class F>
struct pipeable : F { 
    pipeable(F&& f): F(f) {} 
    pipeable(pipeable&&) = default;
    pipeable(const pipeable&) = delete;
};

template<class F>
pipeable<F> make_pipeable(F&& f) { return {std::forward<F>(f)}; }

template<class Arg, class F>
auto operator|(Arg&& x, details::pipeable<F>&& f) as (f(std::forward<Arg>(x)));

struct plumb_impl {
    template<class... Args>
    auto operator()(Args&&... args) const 
        as (gpd::callcc(std::forward<Args>(args)...));    
};

}

template<class T>
struct opipe : continuation<void(T)> {
    typedef continuation<void(T)> _base;
    opipe(opipe&&) = default;
    opipe(_base rhs) : _base(std::move(rhs)) {}
    opipe& operator=(_base rhs) { 
        _base(*this)=std::move(rhs);
        return *this;
    }
};

// TODO: The preferred syntax to declare {,i,o}pipe would be:
// template<class T>
// using opipe = continuation<void(T)>;


template<class T>
struct ipipe : continuation<T()> {
    typedef continuation<T()> _base;
    ipipe(ipipe&&) = default;
    ipipe(_base rhs) : _base(std::move(rhs)) {}
    ipipe& operator=(_base rhs) { 
        _base(*this)=std::move(rhs);
        return *this;
    }
};

template<class T, class T2=T>
struct pipe : continuation<T(T2)> {
    typedef continuation<T(T2)> _base;
    pipe(pipe&&) = default;
    pipe(_base rhs) : _base(std::move(rhs)) {}
    pipe& operator=(_base rhs) { 
        _base(*this)=std::move(rhs);
        return *this;
    }
};



template<class F, class... Args>
auto stage(F f, Args&&... args) as 
    (details::make_pipeable
     (gpd::bind(f, std::forward<Args>(args)..., gpd::placeholder<0>())));    


template<class... Args>
auto plumb(Args&&... args) as 
        (details::make_pipeable
         (gpd::bind(details::plumb_impl(), std::forward<Args>(args)..., 
                    gpd::placeholder<0>())));    
    




}

#include "macros.hpp"
#endif
