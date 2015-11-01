#ifndef GPD_FUTURE_ALGO_HPP
#define GPD_FUTURE_ALGO_HPP
#include "event.hpp"
#include "waiter.hpp"
#include "future.hpp"
#include <utility>
namespace gpd {

template<class Future>
struct future_value_type { using type = typename Future::type; };
template<class Future>
using future_value_type_t = typename future_value_type<Future>::type;

struct delayed_t {};
constexpr delayed_t delayed = {};

template<class Waitable, class F>
auto then(delayed_t, Waitable w, F&& f);

template<class T>
struct value_future
{
    using type = T;

    value_future(T&& value)
        : value(std::move(value)) {}
    value_future(const T& f)
        : value(value) {}

    value_future(const value_future&) = delete;
    value_future(value_future&&) = default;
    value_future& operator=(const value_future&) = delete;
    value_future& operator=(value_future&&) = default;

    bool valid() const { return true; }
    bool ready() const { return true; }
    bool has_exception() const { return false; }
    bool has_value() const { return true; }
    std::exception_ptr get_exception()  { assert(false); __builtin_unreachable(); }
    type get_value()  { return std::move(value); }

    future_storage<type> get_storage() {
        return future_storage<type>{ get_value() };
    }
    
    template<class WaitStrategy = default_waiter_t&>
    type get(WaitStrategy&& strategy = default_waiter) {
        (void)strategy;
        return get_value();
    }
    
    template<class WaitStrategy=default_waiter_t&>
    void wait(WaitStrategy&& strategy=default_waiter) {
        assert(this->valid());
        (void)strategy;
    } 
    
    operator future<type>() && {
        return future<type>(get_value());
    }

    friend event * get_event(value_future&) { return &always_ready; }
    
    template<class F2>
    auto then(F2 && f) {
        return gpd::then(delayed, std::move(*this), std::forward<F2>(f));
    }

private:
    type value;
};

template<class T>
value_future<std::decay_t<T> > make_ready_future(T&& x) {
    return {std::move(x)};
}

template<class T>
value_future<std::decay_t<T> > make_ready_future(T const& x) {
    return {x};
}

template<class T>
struct exception_future
{
    using type = T;
    
    exception_future(std::exception_ptr except)
        : except(std::move(except)) {}
    exception_future(const exception_future&) = delete;
    exception_future(exception_future&&) = default;
    exception_future& operator=(const exception_future&) = delete;
    exception_future& operator=(exception_future&&) = default;

    bool valid() const { return true; }
    bool ready() const { return true; }
    bool has_exception() const { return true; }
    bool has_value() const { return false; }
    std::exception_ptr get_exception()  { return std::move(except); }
    type get_value()  { assert(false); }
    future_storage<type> get_storage() {
        return future_storage<type>{ get_exception() };
    }
    
    template<class WaitStrategy = default_waiter_t&>
    type get(WaitStrategy&& strategy = default_waiter) {
        (void)strategy;
        std::rethrow_exception(get_exception());
    }
    
    template<class WaitStrategy=default_waiter_t&>
    void wait(WaitStrategy&& strategy=default_waiter) {
        assert(this->valid());
        (void)strategy;
    } 
    
    operator future<type>() && {
        return future<type>(get_exception());
    }

    friend event * get_event(exception_future&) { return &always_ready; }
    
    template<class F2>
    auto then(F2 && f) {
        return gpd::then(delayed, std::move(*this), std::forward<F2>(f));
    }

private:
    std::exception_ptr except;
};


template<class F, class W>
using then_value_type = decltype(std::declval<F>()(std::declval<W>()));
template<class Waitable, class F>
struct then_future  {
    
    using type = then_value_type<F, Waitable>;

    then_future(Waitable&& w, F&& f)
        : w(std::move(w)), f(f){}
    then_future(Waitable&& w, const F& f)
        : w(std::move(w)), f(f){}
    
    bool valid() const { return w.valid(); }
    bool ready() const { return w.ready(); }
    bool has_exception() const { return w.has_exception(); }
    bool has_value() const { return w.has_value(); }
    std::exception_ptr get_exception()  { return w.get_exception();}
    type get_value()  { return f(std::move(w)); }
    future_storage<type> get_storage() {
        return has_value() ?
            future_storage<type>{ get_value() } :
        future_storage<type>{ get_exception() };
    }
    
    template<class WaitStrategy = default_waiter_t&>
    type get(WaitStrategy&& strategy = default_waiter) {
        w.wait(strategy);
        return f(std::move(w));
    }
    
    template<class WaitStrategy=default_waiter_t&>
    void wait(WaitStrategy&& strategy=default_waiter) {
        assert(this->valid());
        w.wait(strategy);
    } 
    
    operator future<type>() && {
        if (!ready()) {

            struct state : waiter, shared_state<type> {
                state(then_future&& w)
                    : w(std::move(w)){}
                then_future w;
                void signal(event_ptr p) override final {
                    p.release();
                    this->set_storage(w.get_storage());
                    shared_state<type>::signal();
                }

                ~state() override final {}
            };
        
            std::unique_ptr<state> p { new state {std::move(*this)} };
        
            auto e = get_event(p->w);
            assert(e);
            e->wait(p.get());
            return future<type>(shared_state_ptr<type>{p.release()});
        } else {
            return future<type>(get_storage());
        }

    }

    friend event * get_event(then_future& x) { return get_event(x.w); }
    
    template<class F2>
    auto then(F2 && f) {
        return gpd::then(delayed, std::move(*this), std::forward<F2>(f));
    }

private:

    Waitable w;
    std::decay_t<F> f;
};

template<class Waitable, class F>
auto then(delayed_t,  Waitable w, F&& f) {
    return then_future<Waitable, std::decay_t<F> >{std::move(w), std::forward<F>(f)};
}

#if 0
templ
using T = decltype(f(events));
struct state : waiter {
    state(WaitableRange r, F&& f)
        : r(std::move(r))
        , f(std::forward<F>(f))
        , signal_counter{1}
        , waited{0}
        , signaled{0}
        , dismiss{true} {

            std::tie(signaled, waited) =
                event::wait_many(this, std::begin(r), std::end(r));

            assert(signaled + waited <=
                   (std::size_t)std::distance(std::begin(r), std::end(r)));

            if (signaled)
                wait();
        }

    void call() {
        if (std::exchange(dismiss, false)) {
            const std::size_t dismissed =
                event::dismiss_wait_many(this, std::begin(r), std::end(r));
            assert(dismissed <= waited);
                
            std::ptrdiff_t pending = waited - dismissed;

            assert(pending >= 0);
            assert(signaled != 0 || pending >= 1 );

            if (signaled == 0) {
                assert(pending >= 1);
                pending -= 1;
            }
                
            if (pending) {
                wait(pending);
                return;
            }
        }
        eval_into(*this, f, std::move(r));
        shared_state<T>::signal();
    }
            
    void wait(std::size_t count = 1) {
        if ((signal_counter += count) <= 0)
            call();
    }

    void signal(event_ptr p) override {
        p.release();
        if (--signal_counter == 0)
            call();
    }

    ~then_future() override final {}
    
    WaitableRange r;
    std::decay_t<F> f;
    std::atomic<std::uint32_t> signal_counter;
    std::size_t waited;
    std::size_t signaled;
    bool dismissed;
};

template<class F, class WaitableRange>
auto when_any(F&& f, WaitableRange events) ->
    void_t<decltype(std::begin(events)), decltype(std::end(events))> {

    using T = decltype(f(events));
    struct state : waiter, shared_state<T> {
        state(WaitableRange r, F&& f)
            : r(std::move(r))
            , f(std::forward<F>(f))
            , signal_counter{1}
            , waited{0}
            , signaled{0}
            , dismiss{true} {

            std::tie(signaled, waited) =
                event::wait_many(this, std::begin(r), std::end(r));

            assert(signaled + waited <=
                   (std::size_t)std::distance(std::begin(r), std::end(r)));

            if (signaled)
                wait();
        }

        void call() {
            if (std::exchange(dismiss, false)) {
                const std::size_t dismissed =
                    event::dismiss_wait_many(this, std::begin(r), std::end(r));
                assert(dismissed <= waited);
                
                std::ptrdiff_t pending = waited - dismissed;

                assert(pending >= 0);
                assert(signaled != 0 || pending >= 1 );

                if (signaled == 0) {
                    assert(pending >= 1);
                    pending -= 1;
                }
                
                if (pending) {
                    wait(pending);
                    return;
                }
            }
            eval_into(*this, f, std::move(r));
            shared_state<T>::signal();
        }
            
        void wait(std::size_t count = 1) {
            if ((signal_counter += count) <= 0)
                call();
        }

        void signal(event_ptr p) override {
            p.release();
            if (--signal_counter == 0)
                call();
        }
        
        WaitableRange r;
        std::decay_t<F> f;
        std::atomic<std::uint32_t> signal_counter;
        std::size_t waited;
        std::size_t signaled;
        bool dismissed;
    };

    std::unique_ptr<state> p {
        new state {std::move(w), std::forward<F>(f)}};

    return future<T>(p.release());
}



#endif
}
#endif
