#ifndef GPD_FUTURE_HPP
#define GPD_FUTURE_HPP
#include <atomic>
#include <future>
#include <cassert>
#include <deque>
#include <thread>
#include "event.hpp"
#include "waiter.hpp"
namespace gpd {


template<class W, class F, class... Args>
void eval_into(W& w, F&& f, Args&&... args) {
    try {
        w.set_value(std::forward<F>(f)(std::forward<Args>(args)...));
    } catch(...) {
        w.set_exception(std::current_exception());
    }
}


template<class T>
struct shared_state_union {
private:
    enum state_t { unset, value_set, except_set };
    std::atomic<state_t> state = { unset };
    union {
        T value;
        std::exception_ptr except;
    };

    state_t get_state() const {
        return state.load(std::memory_order_acquire);
    }

    void set_state(state_t s) {       
        state.store(s, std::memory_order_release);
    }

public:
    shared_state_union() {}

    shared_state_union(shared_state_union<T>&& rhs) {
        auto s = rhs.get_state();
        if (s == value_set)
            set_value(std::move(rhs.value));
        else if (s == except_set)
            set_exception(std::move(rhs.except));
    }

    shared_state_union& operator=(shared_state_union<T>&& rhs) {
        auto s = rhs.get_state();
        if (s == value_set)
            set_value(std::move(rhs.value));
        else if (s == except_set)
            set_exception(std::move(rhs.except));
        return *this;
    }


    bool ready() const { return get_state() != unset; }
    
    T& get() {
        auto s = get_state();
        if  (s == except_set)
            std::rethrow_exception(except);
        assert(s == value_set);
        return value;
    }

    void set_value(T&& x) {
        new(&value) T (std::move(x));
        set_state(value_set);
    }

    void set_exception(std::exception_ptr&& e) {
        new(&except) std::exception_ptr  {std::move(e) };
        set_state(except_set);
    }

    void set_value(const T&& x) {
        new(&value) T {x};
        set_state(value_set);
    }

    void set_exception(const std::exception_ptr& e) {
        new(&except) std::exception_ptr {e} ;
        set_state(except_set);
    }

    virtual ~shared_state_union() {
        const auto x = get_state();
        if (x == value_set)
            value.~T();
        else if (x == except_set)
            except.~exception_ptr();
    }
};


template<class T>
struct shared_state : event, shared_state_union<T>
{
    shared_state()  {}
    shared_state(T value) : event(false) {
        this->set_value(std::move(value));
    }

    shared_state(std::exception_ptr except) : event(false) {
        this->set_exception(std::move(except));
    }
    
    virtual ~shared_state() override { }
};

template<class> class promise;
template<class> class shared_future;

/// Generic 'then' implementation.
///
/// Given a waitable w and a function f, when the waitable becomes
/// ready, evaluate f(w).
///
/// Returns a future<result_of_t<F(W)> > representing the delayed
/// evaluation.
template<class Waitable, class F>
auto then(Waitable w, F&& f);

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
