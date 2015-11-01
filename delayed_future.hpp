#ifndef GPD_DELAYED_HPP
#define GPD_DELAYED_HPP

#include "future.hpp"
#include "forwarding_hpp"

namespace gpd{
struct delay_t{};
struct evalinto_t {};

constexpr delay_t delay;
constexpr evalinto_t evalinto;





template<class T, class State>
class future {
private:
    using shared_state = gpd::shared_state<T>;
    using promise = gpd::promise<T>;
    friend class gpd::promise<T>;

    State state;

public:
    using default_waiter = typename future<T>::default_waiter;

    future()  {}
    future(State&& state) : state(std::move(state)) {}
    future(const future&) = delete;
    future(future&& x) : state(std::move(x.state)) {  }
    future& operator=(const future&) = delete;
    future& operator=(future&& x) { state = std::move(x.state); return *this; }

    friend event* get_event(future& x) { return get_event(x.state);  }

    ~future() {   }

    template<class WaitStrategy=default_waiter>
    T get(WaitStrategy&& strategy = WaitStrategy{}) {
        return state.get(strategy);
    }

    bool valid() const { return state.valid(); }
    bool ready() const { return state.ready(); }
    
    template<class WaitStrategy=default_waiter>
    void wait(WaitStrategy&& strategy=WaitStrategy{}) {
        state.wait(strategy);
    }

    template<class F>
    auto then(F&&f) {
        return state.then(std::forward<F>(f));
    }

    shared_future<T> share();
};

}

#endif
