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
    shared_state(shared_state&&) = delete;
    shared_state(const shared_state&) = delete;
    void operator=(shared_state&&) = delete;
    void operator=(const shared_state&) = delete;
    using type = T;
    shared_state() {}
    shared_state(type value) 
        : event(true), storage_(std::move(value)) { }
    shared_state(std::exception_ptr except) 
        : event(true), storage_(std::move(except)) { }
        
    using event::ready;
    bool has_exception() const { return ready() && storage_.is(ptr<std::exception_ptr>{}); }
    bool has_value() const { return ready() && storage_.is(ptr<type>{}); }

    auto get_storage() && { return std::move(storage_); }

    type& get() {
        assert(ready());
        if (storage_.is(ptr<std::exception_ptr>{}))
            std::rethrow_exception(get_exception());
        return storage_.get(ptr<type>{});
    }

    std::exception_ptr get_exception() {
        assert(ready());
        return std::move(storage_.get(ptr<std::exception_ptr>{}));
    }

    type get_value() {
        assert(ready());
        return std::move(storage_.get(ptr<type>{}));
    }


    void set_value(type&& x) {
        storage_ = std::move(x);
    }

    void set_exception(std::exception_ptr&& e) {
        storage_ = std::move(e);
    }

    void set_value(const type&& x) {
        storage_ = x;
    }

    void set_exception(const std::exception_ptr& e) {
        storage_ = e;
    }

    void set_storage(future_storage<T>&& storage) {
        storage_ = storage;
    }
    
    virtual ~shared_state() override { }
};

struct event_deleter {
    void operator()(event* e) {
        assert(e);
        if (e->ready())
            delete e;
        else
            e->wait(&delete_waiter);
    }
};

template<class T>
using shared_state_ptr = std::unique_ptr<shared_state<T>, event_deleter>;

template<class T>
class future {
    using shared_state = gpd::shared_state<T>;
    shared_state_ptr<T> state;

public:
    using type = T;
    explicit future(shared_state_ptr<type> state) : state(std::move(state)) {}

    future() {}
    future(type value) : state(new shared_state{std::move(value)}) {}

    template<class WaitStrategy=default_waiter_t&>
    type get(WaitStrategy&& strategy = default_waiter) {
        wait(strategy);
        auto tstate = std::move(state);
        return static_cast<type&&>(tstate->get());
    }

    bool valid() const { return !!state; }
    bool ready() const { return valid() && state->ready(); }
    bool has_exception() const { return valid() && state->has_exception(); }
    bool has_value() const { return valid() && state->has_value(); }
    type get_value() const { assert(ready()); return state->get_value(); }
    std::exception_ptr get_exception() const { assert(ready()); return state->get_exception(); }
    
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

    shared_future<type> share();
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
        void signal(event_ptr p) override final {
            p.release();
            eval_into(*this, f, std::move(w));
            shared_state<T>::signal();
        }

        ~state() override final {}
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
