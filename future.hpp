#ifndef GPD_FUTURE_HPP
#define GPD_FUTURE_HPP
#include <atomic>
#include <future>
#include <cassert>
#include <deque>
#include <thread>
#include "event.hpp"
#include "waiter.hpp"
#include "forwarding.hpp"
#include "variant.hpp"
namespace gpd {


template<class W, class F, class... Args>
void eval_into(W& w, F&& f, Args&&... args) {
    try {
        w.set_value(std::forward<F>(f)(std::forward<Args>(args)...));
    } catch(...) {
        w.set_exception(std::current_exception());
    }
}

template<class> class shared_state;
template<class> class promise;
template<class T> class future;
template<class> class shared_future;

/// Generic 'then' implementation.
///
/// Given a waitable w and a function f, when the waitable becomes
/// ready, evaluate f(w).
///
/// Concept Requirements:
/// 
///   event * e = get_value(w)
///   using T = decltype(f(std::move(w)))
///
/// Returns a future<T> representing the delayed
/// evaluation.
template<class Waitable, class F>
auto then(Waitable w, F&& f);

template<class T>
using ptr = T*;

/// This is not (yet) like the proposed std::experimental::expected,
/// but it might in the future.
template<class T>
struct expected {
    variant<empty, T, std::exception_ptr > value;
private:

public:
    expected() {}

    expected(expected<T>&& rhs)
        : value(std::move(rhs.value))
    { }

    expected& operator=(expected<T>&& rhs) {
        value = (std::move(rhs.value));
        return *this;
    }

    bool ready() const { return !value.is(ptr<empty>{}); }
    
    T& get() {
        assert(ready());
        if (value.is(ptr<std::exception_ptr>{}))
            std::rethrow_exception(value.get(ptr<std::exception_ptr>{}));
        return value.get(ptr<T>{});
    }

    void set_value(T&& x) {
        value = std::move(x);
    }

    void set_exception(std::exception_ptr&& e) {
        value = std::move(e);
    }

    void set_value(const T&& x) {
        value = x;
    }

    void set_exception(const std::exception_ptr& e) {
        value = e;
    }
};

template<class T>
struct shared_state : event, expected<T>
{
    shared_state()  {}
    shared_state(T value) 
        : event(true), expected<T>(std::move(value)) { }

    bool ready() const { return event::ready(); }
    shared_state(std::exception_ptr except)
        : event(true), expected<T>(std::move(except)) { }
    
    virtual ~shared_state() override { }
};

template<class T>
class future {
    struct shared_state_deleter {
        void operator()(event* e) {
            e->wait(&delete_waiter);
        }
    };
    using shared_state = gpd::shared_state<T>;
    using shared_state_ptr = std::unique_ptr<shared_state, shared_state_deleter>;
    shared_state_ptr state;

public:

    explicit future(shared_state * state) : state(state) {}

    future() {}
    future(T value) : state(new shared_state{std::move(value)}) {}

    template<class WaitStrategy=default_waiter_t&>
    T get(WaitStrategy&& strategy = default_waiter) {
        wait(strategy);
        auto tstate = std::move(state);
        return static_cast<T&&>(tstate->get());
    }

    bool valid() const { return !!state; }
    bool ready() const { return valid() && state->ready(); }

    template<class WaitStrategy=default_waiter_t&>
    void wait(WaitStrategy&& strategy=default_waiter) {
        assert(valid());
        if (!ready())
            gpd::wait(strategy, *this);
    }


    template<class F>
    auto then(F&&f) {
        return gpd::then(std::move(*this), std::forward<F>(f));
    }

    friend event* get_event(future&x) { return x.state.get(); }

    shared_future<T> share();
};

template<class T>
struct promise
{
public:
    using shared_state = gpd::shared_state<T>;
    using future = gpd::future<T>;
    promise() : state(new shared_state) {}
    promise(const promise&) = delete;
    promise(promise&& x) : state(x.state) { x.state=0; }
    promise& operator=(const promise&) = delete;
    promise& operator=(promise&& x) { std::swap(state, x.state); return *this; }
    
    future get_future() {
        assert(state);
        return future{ state } ;
    }
    
    void set_value(T x) {
        // we can't distinguish the ''no state'' state
        if (!state)
            throw std::future_error (std::future_errc::promise_already_satisfied);

        auto tstate = std::exchange(state, nullptr);
        tstate->set_value(std::move(x));
        tstate->signal();
    }
    
    template<class E>
    void set_exception(E&& e) {
        if (!state)
            throw std::future_error (std::future_errc::promise_already_satisfied);

        auto tstate = std::exchange(state, nullptr);
        tstate->set_exception(std::make_exception_ptr(std::forward<E>(e)));
        tstate->signal();
    }

    ~promise() {
        if (state)
            set_exception(std::future_error(std::future_errc::broken_promise));
    }
private:
    shared_state * state;
};


template<class F>
auto async(F&& f)
{    
    struct {
        std::decay_t<F> f;
        gpd::promise<decltype(f())> promise;
        void operator()() { eval_into(promise, f);  }
    } run { std::forward<F>(f), {} };
    
    auto future = run.promise.get_future();
    std::thread th (std::move(run));
    th.detach();
    return future;
}

template<class Waitable, class F>
auto then(Waitable w, F&& f)
{
    using T = decltype(f(std::move(w)));
    struct state : waiter, shared_state<T> {
        state(Waitable&& w, F&& f)
            : w(std::move(w)), f(std::forward<F>(f)){}
        Waitable w;
        std::decay_t<F> f;
        void signal(event_ptr p) override {
            p.release();
            eval_into(*this, f, std::move(w));
            shared_state<T>::signal();
        }
    };

    std::unique_ptr<state> p {
        new state {std::move(w), std::forward<F>(f)}};

    auto e = get_event(p->w);
    assert(e);
    e->wait(p.get());
    return future<T>(p.release());
}



}
#endif
