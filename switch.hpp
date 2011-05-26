#ifndef GPD_SWITCH_HPP
#define GPD_SWITCH_HPP
#include "forwarding.hpp"
#include "guard.hpp"
#include "bitcast.hpp"
#include "switch_base.hpp"
#include "tuple.hpp"
#include <exception>
#include <cstring>
#include <iterator>
#include <stdlib.h>
#include <memory>
#include "macros.hpp"
namespace gpd {

template<class Signature = void()>
class continuation;

template<class Result, class... Args>
struct continuation<Result(Args...)> {
    typedef Result signature (Args...);
    typedef Result result_type;

    typedef continuation<result_type(Args...)> self;

    continuation(const continuation&) = delete;

    continuation(continuation&& rhs) 
        : pair(rhs.pilfer()) { }

    explicit continuation(switch_pair pair) : pair(pair) {}

    self& operator=(continuation rhs) {
        pair = rhs.pilfer();
        return *this;
    }

    self& operator() (Args... args) 
    {
        assert(!terminated());
        switch_pair cpair = pilfer(); 
        auto p = std::tuple<Args...>(std::forward<Args>(args)...);
        pair = stack_switch
            (cpair.sp, &p); 
        return *this;
    }

    result_type get() const {
        assert(has_data());
        return *static_cast<result_type*>(pair.parm);
    }        

    result_type operator*() const {
        return get();
    }

    self& operator++()  {
        return this->operator()();
    }

    // This is is arguably wrong
    self& operator++(int)  {
        return this->operator()();
    }

    explicit operator bool() const {
        return has_data();
    }

    bool has_data() const {
        return pair.parm;
    }

    bool terminated() const {
        return !pair.sp;
    }

    switch_pair pilfer() {
        switch_pair result = pair;
        pair = {{0},0};
        return result;
    }

    ~continuation() {
        assert(!pair.sp);
    }

private:
    switch_pair pair;
};

struct exit_continuation {
    exit_continuation(const exit_continuation&) = delete;

    explicit exit_continuation(switch_pair pair) : pair(pair) {}

    exit_continuation(exit_continuation&& rhs) 
        : pair(rhs.pilfer()) { }
    
    template<class Signature>
    explicit exit_continuation(continuation<Signature>&& rhs) 
        : pair(rhs.pilfer()) { }

    exit_continuation& operator=(exit_continuation rhs) {
        pair = rhs.pilfer();
        return *this;
    }

    switch_pair pilfer() {
        switch_pair result = pair;
        pair = {{0}, 0};
        return result;
    }

    bool terminated() const {
        return !pair.sp;
    }

    ~exit_continuation() {
        assert(!pair.sp);
    }
private:
    switch_pair pair;
};

struct exit_exception 
    : std::exception {
    explicit exit_exception(exit_continuation exit_to)
        : exit_to(std::move(exit_to)) {}
    
    exit_continuation exit_to;
    ~exit_exception() throw() {}
};

struct abnormal_exit_exception 
    : exit_exception, std::nested_exception {
    explicit abnormal_exit_exception(exit_continuation exit_to)
        : exit_exception(std::move(exit_to)) {}
    ~abnormal_exit_exception() throw() {}
};

template<class F, class Continuation>
auto with_escape_continuation(F &&f, Continuation&& c) -> decltype(F()) {
    try {
        f();
    } catch(...) {
        throw abnormal_exit_exception(std::move(c));
    }
}

namespace details {

void exit_to_trampoline(cont from, parm_t) {
    throw exit_exception(exit_continuation({from,0}));
}

template<class Deleter>
struct cleanup_trampoline_args {
    Deleter deleter; // Remove if is_empty<Deleter>
    std::exception_ptr excp;
};

template<class Deleter>
switch_pair cleanup_trampoline(parm_t arg, cont)  {
    auto argsp = static_cast<cleanup_trampoline_args<Deleter> *>(arg);
    auto g = guard
        ([&]{ 
            Deleter d(std::move(argsp->deleter)); // Must not throw, othwise we leak memory
            argsp->~cleanup_trampoline_args<Deleter>();
            d();
        });
    if (argsp->excp) std::rethrow_exception(argsp->excp);
    return switch_pair{{0}, 0};
}

template<class F, class Deleter>
struct startup_trampoline_args { // POD
    startup_trampoline_args(startup_trampoline_args const&) = default;
    F functor;
    cleanup_trampoline_args<Deleter> cleanup_args;
};

template<class Continuation, class F, class Deleter>
switch_pair startup_trampoline(parm_t arg, cont sp) {
    auto argsp = static_cast<startup_trampoline_args<F, Deleter>*>(arg);
    auto cleanup_args = std::move(argsp->cleanup_args);
    try {
        F f(std::forward<F>(argsp->functor)); 
        switch_pair pair = {sp,0};
        sp = f(Continuation(pair)).pilfer().sp;
    } catch(abnormal_exit_exception& e) {
        sp = e.exit_to.pilfer().sp;
        cleanup_args.excp = e.nested_ptr();
    } catch(exit_exception& e) {
        sp = e.exit_to.pilfer().sp;
    } 
    return execute_into(&cleanup_args, sp, &cleanup_trampoline<Deleter>); 
}
 
// Deleter must be nothrow move constructible 
template<class Continuation, class F, class Deleter>
switch_pair
run_startup_trampoline_into(cont cs, F && f, Deleter d) {
    startup_trampoline_args<F, Deleter> args{ 
        std::forward<F>(f),
        { std::move(d), 0 }
    };
    return execute_into(&args, cs, &startup_trampoline<Continuation, F, Deleter>);
}

struct default_stack_allocator {
    static const size_t alignment = 16;

    static void * allocate(size_t size) {
        void * result = 0;
        int ret = ::posix_memalign(&result, alignment, size);
        assert(ret != EINVAL);
        if(ret == ENOMEM) 
            throw  std::bad_alloc();
        return result;
    }

    static void deallocate(void * ptr) throw() {
        free(ptr);
    }
};

template<class Signature>
void signal_exit_to(continuation<Signature> c) {
    execute_into(0, c.sp, &details::exit_to_trampoline);
}

template<class Signature>
struct reverse_signature; 

template<class... Ret, class... Args>
struct reverse_signature<std::tuple<Ret...>(Args...)> {
    typedef std::tuple<Args...> type (Ret...);
};

template<class... Args>
struct reverse_signature<void(Args...)> {
    typedef std::tuple<Args...> type ();
};

template<class... Ret>
struct reverse_signature<std::tuple<Ret...>()> {
    typedef void type (Ret...);
};

template<>
struct reverse_signature<void()> {
    typedef void type ();
};


}

template<class Signature, 
         class F,
         class StackAlloc = details::default_stack_allocator> 
continuation<Signature> create_context(F&& f, size_t stack_size = 1024*1024, 
                                       StackAlloc alloc = StackAlloc()) 
{
    void * stackp = alloc.allocate(stack_size);
    cont cs { stack_bottom(stackp, stack_size) };
        
    auto deleter =  [=] { alloc.deallocate(stackp); };

    typedef typename details::reverse_signature<Signature>::type reverse_signature;
    return continuation<Signature>
    { details::run_startup_trampoline_into<continuation<reverse_signature> >
            (cs, std::forward<F>(f), deleter) };
}

namespace details {

template<class Sig> struct extract_signature;

template<class T, class Ret, class... Args>
struct extract_signature<Ret(T::*)(Args...)> { typedef Ret type (Args...); };
template<class T, class Ret, class... Args>
struct extract_signature<Ret(T::*)(Args...) const> { typedef Ret type (Args...); };

template<class Sig>
struct parm0;

template<class Ret, class Arg0, class... Args>
struct parm0<Ret(Arg0, Args...)> { typedef Arg0 type; };

}

template<class F, 
         class FSig = typename details::extract_signature<decltype(&F::operator())>::type,
         class Parm = typename details::parm0<FSig>::type,
         class RSig = typename Parm::signature,
         class Sig  = typename details::reverse_signature<RSig>::type
 >
continuation<Sig> callcc(F f) {
    return create_context<Sig>(f);
}



}
#include "macros.hpp"
#endif
