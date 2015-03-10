#ifndef GPD_SHARED_FUTURE_HPP
#define GPD_SHARED_FUTURE_HPP
#include "future.hpp"

    
namespace gpd {
template<class T>
struct shared_state_multiplexer : waiter, shared_state_union<T> {

    std::mutex mux;
    std::deque<promise<T*> > listeners;
    
    shared_state_multiplexer(future<T>&& future) {
        future.get_shared_state()->wait(this);
    }   
    
    auto lock() { return std::unique_lock<std::mutex> (mux);}
    
    future<T*> add_listener() {
        return lock(),  listeners.emplace_back(), listeners.back().get_future();
    }

    static auto as_shared_state(event_ptr p) {
        return std::unique_ptr<gpd::shared_state_union<T> >(
            static_cast<gpd::shared_state<T>*>(p.release()));
    }
    
    void signal(event_ptr other) override {
        auto original = as_shared_state(std::move(other)); // deleted at end of scope
        assert(original);
        *static_cast<shared_state_union<T>*>(this) = std::move(*original);
        original.reset();
        auto tmp_listeners = (lock(), std::exchange(listeners, {}));
        if (this->has_value())
            for(auto&& p : tmp_listeners)
                p.set_value(&this->value);
        else
            for(auto&& p : tmp_listeners)
                p.set_exception(&this->except);
    }
};

template<class T>
class shared_future {
    std::shared_ptr<shared_state_multiplexer<T> > state;
    future<T*> listener;
    friend class future<T>;

    std::shared_ptr<shared_state_multiplexer<T> > steal() { return std::move(state); }
public:
    friend event& get_event(shared_future& x) { assert(x->state); return x->state; }
    shared_future(future<T>&& future)
        : state(new shared_state_multiplexer<T>(std::move(future)) )
        , listener(state->add_listener())
    {
        assert(listener.valid());
    }
      
    shared_future() : state(0) {}
    shared_future(const shared_future& rhs)
        : state(rhs.state)
        , listener(state ? state->add_listener() : future<T*>())
    {
        assert(listener.valid());
    }

    shared_future(shared_future&& rhs)
        : state(rhs.steal()), listener(std::move(rhs.listener))
    {}
    
    shared_future& operator=(shared_future rhs) {
        state = rhs.steal();
        listener = std::move(rhs.listener);
        return *this;
    }      
    ~shared_future() {  }
    
    T& get() {
        this->wait();
        assert(state);
        return state->get();
    }

    bool valid() const { return state && listener.valid(); }
    bool is_ready() const { return state && !state->is_empty(); }

    template<class WaitStrategy=cv_waiter>
    void wait(WaitStrategy&& strategy=WaitStrategy{}) {
        listener.wait(strategy);
    }

    template<class F>
    auto then(F&&f) {
        listener.then(std::forward<F>(f));
    }

};

template<class T>
shared_future<T> future<T>::share() { return { std::move(*this) }; }
}
#endif
