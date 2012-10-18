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

struct proc_impl {
    template<class... Args>
    auto operator()(Args&&... args) const 
        as (gpd::callcc(std::forward<Args>(args)...));    
};

}



template<class F, class... Args>
auto stage(F f, Args&&... args) as 
    (details::make_pipeable
     (gpd::bind(f, std::forward<Args>(args)..., gpd::placeholder<0>())));    


template<class... Args>
auto proc(Args&&... args) as 
        (details::make_pipeable
         (gpd::bind(details::proc_impl(), std::forward<Args>(args)..., 
                    gpd::placeholder<0>())));    
    




}

#include "macros.hpp"
#endif
