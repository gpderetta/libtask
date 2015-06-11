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

template<class T>
using future_storage = variant<T, std::exception_ptr>;
template<class T>
class shared_state : public event
{
    future_storage<T> storage_;
public:
    shared_state() {}
    shared_state(T value) 
        : event(true), storage_(std::move(value)) { }
    shared_state(std::exception_ptr except) 
        : event(true), storage_(std::move(except)) { }
    
    shared_state(shared_state&&) = default;
    shared_state& operator=(shared_state&&) = default;
    
    using event::ready;
    bool has_exception() const { return ready() && storage_.is(ptr<std::exception_ptr>{}); }
    bool has_value() const { return storage_.is(ptr<T>{}); }

    auto storage() && { return std::move(storage_); }
    
    T& get() {
        assert(ready());
        if (has_exception())
            std::rethrow_exception(get_exception());
        return storage_.get(ptr<T>{});
    }

    std::exception_ptr& get_exception() {
        assert(ready());
        return storage_.get(ptr<std::exception_ptr>{});
    }

    void set_value(T&& x) {
        storage_ = std::move(x);
    }

    void set_exception(std::exception_ptr&& e) {
        storage_ = std::move(e);
    }

    void set_value(const T&& x) {
        storage = x;
    }

    void set_exception(const std::exception_ptr& e) {
        storage = e;
    }
        
    virtual ~shared_state() override { }
};

struct shared_state_deleter {
    void operator()(event* e) {
        e->wait(&delete_waiter);
    }
};
template<class T>
using shared_state_ptr = std::unique_ptr<shared_state<T>, shared_state_deleter>;

template<class T>
class future {
    using shared_state = gpd::shared_state<T>;
    shared_state_ptr<T> state;

public:

    explicit future(shared_state_ptr<T> state) : state(std::move(state)) {}

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
        return future{ shared_state_ptr<T>{state}  } ;
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
    return future<T>(shared_state_ptr<T>{p.release()});
}



}
#endif
