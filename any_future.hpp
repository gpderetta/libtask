#ifndef GPD_ANY_FUTURE_HPP
#define GPD_ANY_FUTURE_HPP
#include "dynamic.hpp"
#include "future.hpp"
namespace gpd {

template<class T>
struct future_interface {
    virtual bool valid() const = 0;
    virtual bool ready() const = 0;
    virtual T get() = 0;

    template<class S> struct wrap : S {
        wrap(const wrap&) = delete;
        wrap() = default;
        bool valid() const { return this->target().valid();  }
        bool ready() const { return this->target().ready(); }

        template<class WaitStrategy>
        T get(WaitStrategy&& strategy) {
            this->wait(strategy);
            return this->get();
        }
            
        
        T get() { return this->target().get(); }

        template<class WaitStrategy=default_waiter_t&>
        void wait(WaitStrategy&& strategy=default_waiter) {
            assert(this->valid());
            if (!this->ready())
                gpd::wait(strategy, *this);
        } 

        template<class F>
        auto then(F&&f) {
            return gpd::then(std::move(this->self()), std::forward<F>(f));
        }

        friend event * get_event(typename S::self_t& x) { return x.do_get_event(); }
                
    protected:
        event* do_get_event() { return get_event(this->target()); }

    };
protected:
    friend event* get_event(future_interface&x) { return x.do_get_event();}
    virtual event* do_get_event() = 0;
};

template<class T>
using any_future = dynamic<future_interface<T> >;


}
#endif
