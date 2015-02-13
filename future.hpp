#ifndef GPD_FUTURE_HPP
#define GPD_FUTURE_HPP
#include <atomic>
#include <future>
#include <cassert>
#include <deque>
#include "event.hpp"
#include "cv_waiter.hpp" // default waiter
namespace gpd {

template<class T, bool atomic = true>
struct shared_state_union {
    enum state_t { unset, value_set, except_set };
    typename std::conditional<
        atomic, 
        std::atomic<state_t>,
        state_t>::type state = { unset };
    union {
        T value;
        std::exception_ptr except;
    };

    shared_state_union() {}
    
    template<bool x>
    shared_state_union(shared_state_union<T, x>&& rhs) {
        auto s = rhs.get_state();
        if (s == value_set)
            set_value(std::move(rhs.value));
        else if (s == except_set)
            set_exception(std::move(rhs.except));
    }

    static state_t do_get_state(state_t const& state)  {
        return state;
    }

    static void do_set_state(state_t& state, state_t s)  {
        state = s;
    }

    static state_t do_get_state(std::atomic<state_t> const& state)  {
        return state.load(std::memory_order_acquire);
    }

    static void do_set_state(std::atomic<state_t>& state, state_t s) {
        state.store(s, std::memory_order_release);
    }

    state_t get_state() const {
        return do_get_state(state);
    }

    void set_state(state_t s) {
        do_set_state(state, s);
    }

    bool is_empty() const { return get_state() == unset; }
    bool has_value() const { return get_state() == value_set; }
    bool has_except() const { return get_state() == except_set; }
    
    T& get() {
        auto s = get_state();
        if  (s == except_set)
            std::rethrow_exception(except);
        assert(s = value_set);
        return value;
    }

    T&& get_move() {
        return std::move(get());
    }
    
    T& get_value() {
        return value;
    }

    std::exception_ptr& get_except() {
        return except;
    }

    void set_value(T&& x) {
        new(&value) T (std::move(x));
        set_state(value_set);
    }

    void set_except(std::exception_ptr&& e) {
        new(&except) std::exception_ptr  {std::move(e) };
        set_state(except_set);
    }

    void set_value(const T&& x) {
        new(&value) T {x};
        set_state(value_set);
    }

    void set_except(const std::exception_ptr& e) {
        new(&except) std::exception_ptr {e} ;
        set_state(except_set);
    }

    ~shared_state_union() {
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

template<class T>
class future {
public:
    using shared_state = gpd::shared_state<T>;
    using promise = gpd::promise<T>;
    friend class gpd::promise<T>;
    future(shared_state* state) : state(state) {}

    shared_state * get_shared_state() { return std::exchange(state, nullptr);}

    // hybrid state/waiter/promise for 'then' chaining
    template<class Fn, class T2>
    struct fn_adapter : waiter, gpd::shared_state<T2> {
        using future_type = future<T2>;
        
        ~fn_adapter() override {}

        Fn fn;
        
        fn_adapter(Fn&& fn)
            : fn(std::move(fn)) {}
                
        fn_adapter(const Fn& fn)
            : fn(fn) {}
    };

    static auto as_union(event_ptr p) {
        return std::unique_ptr<shared_state_union<T> >(
            static_cast<shared_state_union<T>*>(p.release()));
    }

    // hybrid state/waiter/promise for 'then' chaining
    template<class Fn, class T2 = std::result_of_t<Fn(T)> >
    struct next_adapter : fn_adapter<Fn, T2> {
        using fn_adapter<Fn, T2>::fn_adapter;
        void signal(event_ptr p) override {
            auto other = as_union(std::move(p));
            assert(other && !other->is_empty());
            if (other->has_except())
                this->set_exception(std::move(other->get_except()));
            else try {
                    this->set_value(fn(other->get_move()));
                } catch(...) {
                    this->set_except(std::current_exception());
                }                           
            event::signal(); // may delete this
        }
    };

    template<class Fn, class T2 = std::result_of_t<Fn(future<T>)> >
    struct then_adapter : fn_adapter<Fn, T2> {
        using fn_adapter<Fn, T2>::fn_adapter;
        void signal(event_ptr p) override {
            auto other = as_union(std::move(p));
            assert(other && !other->is_empty());
            try {
                this->set_value(fn(std::move(future<T>(p.release()))));
            } catch(...) {
                this->set_except(std::current_exception());
            }                           
            shared_state::signal(); // may delete this
        }
    };

    shared_state * state;

    shared_state * steal() { return std::exchange(state, nullptr); }
public:
    using default_waiter = cv_waiter;
    // a ready future
    future(T value) : state(new shared_state{std::move(value)}) {}
    future() : state(0) {}
    future(const future&) = delete;
    future(future&& x) : state(x.steal()) {  }
    future& operator=(const future&) = delete;
    future& operator=(future&& x) { std::swap(state, x.state); return *this; }

    friend event* get_event(future& x) { return x.state;  }

    ~future() {  if (state) state->then(&event::nop_waiter);   }

    template<class WaitStrategy=default_waiter>
    T get(WaitStrategy&& strategy = WaitStrategy{}) {
        using gpd::wait;
        wait(strategy, *this);
        return std::unique_ptr<shared_state>(steal())->get_move();
    }

    bool valid()    const    { return state; }
    bool ready() const { return state && !state->is_empty(); }

    
    template<class WaitStrategy=default_waiter>
    void wait(WaitStrategy&& strategy=WaitStrategy{}) {
        using gpd::wait;
        assert(valid());
        wait(strategy, *this);
    }

    template<class F>
    auto then(F&&f) {
        assert(valid());
        using adapter = then_adapter<std::decay_t<F> >;
        using future  = typename adapter::future_type;

        if (!state->is_empty())
            return future(f(*this));

        future result (new adapter { std::forward<F>(f) });
        std::exchange(state, nullptr)->then(result.state); 
        return result;            
    }

    template<class F>
    auto next(F&&f) {
        // next can be implemented as a thin wrapper over then', but
        // this can be more efficient 
        assert(valid());
        using adapter = next_adapter<std::decay_t<F> >;
        using future = typename adapter::future_type;
        
        if (state->has_value())
            return future(f(std::move(state->get_value())));
        if (state->has_except())
            return future(std::move(state->get_except()));

        future result( new adapter { std::forward<F>(f) });
        std::exchange(state, nullptr)->then(result.state); 
        return result;            
    }

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
        //std::cerr << "get_future: " << ::pthread_self() << ' ' <<this << ' ' << state << '\n';
        return { state };
    }
    
    void set_value(T x) {
        //std::cerr << "set_value: " << ::pthread_self() << ' ' << this << ' ' << state << '\n';
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
        tstate->set_except(std::make_exception_ptr(std::forward<E>(e)));
        tstate->signal();
    }

    ~promise() {
        if (state)
            set_exception(std::future_error(std::future_errc::broken_promise));
    }
private:
    shared_state * state;
};

}
#endif
