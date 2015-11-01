// template<class T, class F>
// struct deferred_future {
//     enum state { unset, thunk_set };
//     state_t state = {unset};
//     union { F thunk };

//     friend event * get_event(any_future& x) {       
//     case thunk_set:  x = F(std::move(x.thunk))(); return &always_ready;
//         case unset:      return 0;
//         };
//         assert(false);
//     }

// };
#if 0
template<class T>
class any_future;

template<class F, class T>
any_future<T> vtable_impl(void * x, bool call) {
    if (call) { return (*static_cast<F*>(x))(); }
    else {
        static_cast<F*>(x)->~F();
        return any_future<T>();
    }
}

template<class T>
class any_future {
    enum state_t { unset, thunk_set,
                   value_set, except_set, async_set };
    state_t state = { unset };
    using thunk_t = future_thunk<T>;
    union {
        thunk_t thunk;
        T value;
        std::exception_ptr except;
        future<T> async;
    };

    friend event * get_event(any_future& x) {       
        switch(x.state) {
        case value_set: 
        case except_set:
            return &always_ready;
        case thunk_set:  x = thunk_t(std::move(x.thunk))(); return &always_ready;
        case async_set:  return get_event(x.async);
        case unset:      return 0;
        };
        assert(false);
    }

    struct resetter { any_future* self; ~resetter() {self->reset();} };
public:

    any_future() {}

    any_future(any_future<T>&& rhs) {
        resetter _ {&rhs};
        switch(rhs.state) {
            break; case value_set:  set_value(std::move(rhs.value));
            break; case except_set: set_exception(std::move(rhs.except));
            break; case thunk_set:  set_thunk(std::move(rhs.thunk));
            break; case async_set:  set_async(std::move(rhs.async));
            break; case unset: ;
        }
    }

    template<class F2>
    any_future(deferred_t, F2&& f) { set_thunk(std::forward<F2>(f)); }
    
    template<class F2, class... Args>
    any_future(evalinto_t, F2&& f, Args&&... args) {
        eval_into(*this, std::forward<F2>(f), std::forward<Args>(args)...);
    }

    template<class T2,
             class = std::enable_if_t<std::is_convertible<T2, T>::value > >
    any_future(T2&& x) { set_value(std::forward<T2>(x)); }

    any_future(future<T> async) { set_async(std::move(async)); }

    any_future(std::exception_ptr e) { set_exception(e); }

    any_future(shared_state<T> * s) : any_future(future<T>(s)) {}

    any_future& operator=(any_future<T>&& rhs) {
        resetter _ {&rhs};
        assert(state == unset);
        switch(rhs.state){
            break; case value_set:  set_value(std::move(rhs.value));
            break; case except_set: set_exception(std::move(rhs.except));
            break; case thunk_set:  set_thunk(std::move(rhs.thunk));
            break; case async_set:  set_async(std::move(rhs.async));
            break; case unset: ;
        }
        return *this;
    }

    bool valid() const {
        return (state != async_set || async.valid() ) && state != unset;
    }

    bool ready() const {
        return (state != async_set || async.ready() ) && state != unset;
    }


    template<class WaitStrategy=default_waiter_t&>
    T get(WaitStrategy&& strategy = default_waiter) {
        switch(state) {
        case value_set:  return resetter{this},  std::move(value);
        case except_set: resetter{this},  std::rethrow_exception(except);
        case thunk_set:  return resetter{this},
                thunk_t(std::move(thunk))().get(strategy); 
        case async_set:  return resetter{this}, async.get(strategy);
        case unset:      assert(false);
        };
        assert(false);
    }

    template<class WaitStrategy=default_waiter_t&>
    void wait(WaitStrategy&& strategy=default_waiter) {
        assert(state != unset);
        gpd::wait(strategy, *this);
    }

    template<class F2>
    auto then(F2&& f2) {
        using Ret = decltype(
            make_deferred_future(combine(std::forward<F2>(f2), std::move(*this))));
        switch(state) {
        case value_set:
        case except_set:
            return Ret{evalinto, std::forward<F2>(f2), std::move(*this)};
        case thunk_set:
            return make_deferred_future(combine(std::forward<F2>(f2), std::move(*this)));
        case async_set:
            return Ret{gpd::then(std::move(*this), std::forward<F2>(f2))};
        case unset:      assert(false);
        };
        assert(false);
    }


    void set_exception(std::exception_ptr e) {
        reset();
        new(&except) std::exception_ptr  {std::move(e) };
        state = except_set;
    }

    template<class T2,
             class = std::enable_if_t<std::is_convertible<T2, T>::value > >
    void set_value(T2&& x) {
        reset();
        new(&value) T {std::forward<T2>(x)};
        state = value_set;
    }

    template<class F2>
    void set_thunk(F2&& f) {
        reset();
        new(&value) thunk_t (std::forward<F2>(f));
        state = thunk_set;
    }

    void set_async(future<T> async) {
        reset();
        new(&this->async) future<T>(std::move(async));
        state = async_set;
    }

    ~any_future() { reset(); }

    void reset() noexcept {
        switch(state) {
            break; case value_set:  value.~T();
            break; case except_set: except.~exception_ptr();
            break; case thunk_set:  thunk.~thunk_t();
            break; case async_set:  async.~future<T>();
            break; case unset:      ;
        }
        state = unset;
    }
    
};
#endif

struct evalinto_t {};
struct deferred_t {};
constexpr deferred_t deferred;
constexpr evalinto_t evalinto;

template<class F, class... Args>
auto make_into_future(F&& f, Args&&... args);

template<class F>
auto make_deferred_future(F&&f);

template<class F, class... Args>
struct combiner;
template<class F, class... Args>
combiner<F, Args...> combine(F&& f, Args&&... args);

template<class F, class... Args>
auto make_into_future(F&& f, Args&&... args)
{
    using T = decltype(f(std::declval<Args>()...));
    return any_future<T>(evalinto, std::forward<F>(f), std::forward<Args>(args)...);
}

template<class F>
struct defer {
    std::decay_t<F> f;
    auto operator()() {
        return make_into_future(std::move(f));
    }
};

template<class F>
auto make_deferred_future(F&&f)
{
    using T = decltype(f());
    return any_future<T>(deferred, defer<F>{std::forward<F>(f)});
}

template<class F, class... Args>
struct combiner {
    template<class F2, class... Args2>
    combiner(F2&& f, Args2&&... args)
        : f(std::forward<F2>(f))
        , args(std::make_tuple(std::forward<Args2>(args)...)) {}
    std::decay_t<F> f;
    std::tuple<std::decay_t<Args>...> args;
    
    auto operator()() {
        return gpd::forward(std::move(f), std::move(args));
    }
};

template<class F, class Arg>
struct combiner<F, Arg> {
    std::decay_t<F> f;
    std::decay_t<Arg> arg;

    template<class F2, class Arg2>
    combiner(F2&& f, Arg2&& arg)
        : f(std::forward<F2>(f))
        , arg(std::forward<Arg2>(arg)){}

    auto operator()() {
        return f(std::move(arg));
    }
};

template<class F, class... Args>
combiner<F, Args...>  combine(F&& f, Args&&... args)
{
    return combiner<F, Args...>{
        std::forward<F>(f), std::forward<Args>(args)...
            };
    
}


struct default_evaluator {
    template<class F>
    auto operator()(F&&f) {
        using T = decltype(f());
        return future<T>(evalinto, f);
    }
};

template<class F>
auto async(deferred_t, F&& f)
{    
    return make_deferred_future(std::forward<F>(f));
}

