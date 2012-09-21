#ifndef GPD_FUTURE_HPP
#define GPD_FUTURE_HPP
#include "switch.hpp"
namespace gpd {
typedef continuation<void()> task_t;

namespace details {
struct waiter_t {
    int count;
    task_t task;
};

static waiter_t *unbound = (waiter_t*)0;
static waiter_t *armed = (waiter_t*)1;
static waiter_t *set = (waiter_t*)2;

}

template<class T> class promise;

template<class T>
struct future 
{
    future(const future&) = delete;
    future(future&& rhs) : pimpl(rhs) {}
    future& operator=(future rhs) { swap(*this, rhs); return *this; }

    T& operator*() const {
        assert(!!*this);
        return pimpl->value;
    }

    explicit operator bool () const { 
        return pimpl->waiter == details::set; 
    }

    friend void swap(future& lhs, future& rhs) { std::swap(lhs.pimpl, rhs.pimpl);}


    future() : pimpl(new cb()) {  }

    ~future() 
    {
        assert(!armed());
    }

    bool armed() const { return pimpl->waiter == details::armed; }


    friend class promise<T>;

    template<class T2>
    friend void wait(task_t& to, future<T2>& f);

    template<class... Futures>
    friend void wait_any(task_t& to, Futures&... f) ;

    template<class... Futures>
    friend void wait_all(task_t& to, Futures&... f) ;

    void reset() {
        pimpl->reset();
    }

    gpd::promise<T> promise();
private:
            
    struct cb  { 
        cb() : waiter(details::unbound) {}
        details::waiter_t * waiter; union { T value; }; 
        ~cb() { reset(); } 

        void reset() { 
            assert(waiter == details::set || waiter == details::unbound);
            if(waiter == details::set) {
                waiter = details::unbound; value.~T();
            }
        }
    };
    std::unique_ptr<cb> pimpl;
};

template<class T>
void wait(task_t& to, future<T>& f) {
    if(!f) {
        details::waiter_t w { 1, {}};
        to = splicecc
            (std::move(to),
             [&](task_t c){    
                w.task = std::move(c);
                f.pimpl->waiter = &w;
                return c; // now empty
            });

    }
    assert(!!f);
}

template<class... Futures>
void wait_any(task_t& to, Futures&... f) {
    // GCC will barf in lambda capture if we omit the size here
    details::waiter_t** cbs[sizeof...(f)] = { &f.pimpl.get()->waiter... }; 
    details::waiter_t w { 1, {} };
    to = splicecc
        (std::move(to),
         [&](task_t c){  
            w.task = std::move(c);
            bool has_waiter = false;
            for(auto& x : cbs) {
                has_waiter |= (*x != details::set);
                if(*x != details::set)  *x = &w; 
            }
            if(!has_waiter) w.task();
            return c ;
        });
}

template<class... Futures>
void wait_all(task_t& to, Futures&... f) {
    // GCC will barf in lambda capture if we omit the size here
    details::waiter_t** cbs[sizeof...(f)] = { &f.pimpl.get()->waiter... }; 
    int count =0;
    for(auto& x : cbs) {
        count+= (*x == details::armed);
    }
    details::waiter_t w { count, {} };

    to = splicecc
        (std::move(to),
         [&](task_t c){  
            w.task = std::move(c);
            bool has_waiter = false;
            for(auto& x : cbs) {
                has_waiter |= (*x != details::set);
                if(*x != details::set)  *x = &w; 
            }
            if(!has_waiter) {  
                assert(w.task); 
                w.task();
            }
            return c ;
        });
}


template<class T>
struct promise {
    explicit promise(future<T>& f) : cb(f.pimpl.get()) {
        assert(cb->waiter == details::unbound);
        cb->waiter = details::armed;
    }

    template<class... Args>
    void operator()(Args&&... args) const {
        assert(cb);
        new(&cb->value) T{std::move(args)...};
        auto w = details::set;
        std::swap(w, cb->waiter);
        if(w > details::set && (--w->count <= 0))
        {
            auto task = std::move(w->task);
            assert(task);
            task();
            assert(!task);
        }       
    }

    promise() : cb() {}

private:
    typename future<T>::cb* cb;
};

template<class T>
inline
gpd::promise<T> future<T>::promise() { return gpd::promise<T>(*this); }


}
#endif
