#ifndef GPD_SHARED_FUTURE_HPP
#define GPD_SHARED_FUTURE_HPP
#include "future.hpp"

    
namespace gpd {
template<class T>
struct shared_state_multiplexer : waiter {
    gpd::future<T> future;
    std::mutex mux;
    std::deque<promise<bool> > listeners;
    expected<T> value;
    
    shared_state_multiplexer(gpd::future<T>&& f) : future(std::move(f)) {
        assert(future.valid());
        if (!future.ready())
            get_event(future)->wait(this);
        else
            do_set();
    }   

    void do_set() {
        try { value.set_value(future.get()); }
        catch(...) { value.set_exception(std::current_exception()); }
    }
    
    auto lock() { return std::unique_lock<std::mutex> (mux);}
    
    gpd::future<bool> add_listener() {
        return lock(),  listeners.emplace_back(), listeners.back().get_future();
    }

    void signal(event_ptr other) override {
        other.release();
        do_set();
        auto tmp_listeners = (lock(), std::exchange(listeners, {}));
        for(auto&& p : tmp_listeners)
            p.set_value(true);
    }
};

template<class T>
class shared_future {
    std::shared_ptr<shared_state_multiplexer<T> > state;
    future<bool> listener;
    friend class future<T>;

public:
    friend event * get_event(shared_future& x) { return get_event(x.listener); }
    shared_future(future<T>&& future)
        : state(new shared_state_multiplexer<T>(std::move(future)) )
        , listener(state->add_listener())
    {
        assert(listener.valid());
    }
      
    shared_future() : state(0) {}
    shared_future(const shared_future& rhs)
        : state(rhs.state)
        , listener(state ? state->add_listener() : future<bool>())
    {
        assert(listener.valid());
    }

    shared_future(shared_future&& rhs)
        : state(std::move(rhs.state)), listener(std::move(rhs.listener))
    {}
    
    shared_future& operator=(shared_future rhs) {
        state = std::move(rhs.state);
        listener = std::move(rhs.listener);
        return *this;
    }      
    ~shared_future() {  }

    template<class WaitStrategy>
    T& get(WaitStrategy&& strategy) {
        if (!ready())
            wait(strategy);
        return state->value.get();
    }

    T& get() {
        if (!ready())
            wait();
        return state->value.get();
    }

    bool valid() const { return listener.valid(); }
    bool ready() const { return listener.ready(); }

    template<class WaitStrategy>
    void wait(WaitStrategy&& strategy) {
        listener.wait(strategy);
    }

    void wait() {
        listener.wait();
    }

    template<class F>
    auto then(F&&f) {
        return gpd::then(std::move(*this), std::move(f));
    }

};

template<class T>
shared_future<T> future<T>::share() { return { std::move(*this) }; }
}
#endif
