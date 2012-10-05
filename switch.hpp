#ifndef GPD_SWITCH_HPP
#define GPD_SWITCH_HPP
#include "forwarding.hpp"
#include "guard.hpp"
#include "bitcast.hpp"
#include "switch_base.hpp"
#include "tuple.hpp"
#include "signature.hpp"
#include <exception>
#include <cstring>
#include <iterator>
#include <stdlib.h>
#include <memory>
#include <type_traits>
#include "macros.hpp"
#include <iterator>
namespace gpd {
struct static_stack_allocator {
    enum { stack_size = 1024*1024 };
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

struct debug_stack_allocator {
    static const size_t alignment = 16;
    enum { stack_size = static_stack_allocator::stack_size };
    void * allocate(size_t size) {
        void * ret = static_stack_allocator::allocate(size);
        count++;
        return ret;
    }

    void deallocate(void * ptr) throw() {
        count--;
        return static_stack_allocator::deallocate(ptr);
    }

    debug_stack_allocator(debug_stack_allocator&) = delete;
    
    int count;
    debug_stack_allocator() : count(0) {}
    debug_stack_allocator(debug_stack_allocator&& rhs) : count(rhs.count) {
        rhs.count = 0;
    }
    ~debug_stack_allocator() { assert(count == 0); }
};

#ifdef NDEBUG
typedef static_stack_allocator default_stack_allocator;
#else
typedef debug_stack_allocator default_stack_allocator;
#endif


namespace details {
/// Utility Metafunctions

template<class> class tag {};

template<class Ret>
struct result_traits {
    typedef Ret              result_type;
    typedef std::tuple<Ret>&   xresult_type;
};

template<class... Ret>
struct result_traits<std::tuple<Ret...> > {
    typedef std::tuple<Ret...>&& result_type;
    typedef std::tuple<Ret...>&  xresult_type;
};

template<>
struct result_traits<void> {
    typedef void result_type;
    typedef void xresult_type;
};

template<class...Ret>
struct result_pack { typedef std::tuple<Ret...> type; };

template<class Ret>
struct result_pack<Ret> { typedef Ret type; };

template<>
struct result_pack<> { typedef void type; };

template<class Ret, class... Parm>
struct compose_signature { typedef Ret type(Parm...); };

template<class Ret, class... Parms>
struct compose_signature<Ret, std::tuple<Parms...> > 
    : compose_signature<Ret, Parms...> { };

template<class Ret>
struct compose_signature<Ret, void> 
    : compose_signature<Ret> { };

template<class> struct reverse_signature;
template<class Ret, class... Args>
struct reverse_signature<Ret(Args...)> 
    : compose_signature<typename result_pack<Args...>::type, Ret>   {};

template<class Signature> struct make_signature; 
template<class Ret, class... Args>
struct make_signature<Ret(Args...)> 
    : result_traits<Ret> {
    typedef typename reverse_signature<Ret(Args...)>::type rsignature;
};


template<class Sig>
struct parm0;

template<class Ret, class Arg0, class... Args>
struct parm0<Ret(Arg0, Args...)> { typedef Arg0 type; };

}


/// Main continuation classes

template<class Signature = void()>
class continuation;

template<class Result, class... Args>
struct continuation<Result(Args...)> {
    typedef Result signature (Args...);
    typedef typename details::make_signature<signature>::rsignature   rsignature;
    typedef typename details::make_signature<signature>::result_type  result_type;
    typedef typename details::make_signature<signature>::xresult_type xresult_type;
    typedef continuation<rsignature> rcontinuation;

    continuation(const continuation&) = delete;

    continuation() : pair{{0},0} {}

    continuation(continuation&& rhs) : pair(rhs.pilfer()) {}

    explicit continuation(switch_pair pair) : pair(pair) {}

    continuation& operator=(continuation rhs) {
        assert(terminated());
        pair = rhs.pilfer();
        return *this;
    }

    continuation& operator() (Args... args) {
        assert(!terminated());
        switch_pair cpair = pilfer(); 
        std::tuple<Args...> p(std::forward<Args>(args)...);
        pair = stack_switch
            (cpair.sp, &p); 
        return *this; 
    }
    
    result_type get() const {
        assert(has_data());
        return get(details::tag<result_type>(), pair.parm);
    }
    
    explicit operator bool() const {
        return !terminated() && has_data();
    }

    bool has_data() const {
        return has_data(details::tag<result_type>(), pair.parm);
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
        assert(terminated());
    }

private:
    template<class T> 
    static bool has_data(details::tag<T>, void * parm)  {
        return parm;
    }

    static bool has_data(details::tag<void>, void*)  {
        return true;
    }

    template<class T> 
    static result_type get(details::tag<T>, void * parm)  {
        typedef std::tuple<T> tuple;
        return std::move(std::get<0>(static_cast<tuple&>(*static_cast<tuple*>(parm))));
    }


    template<class T> 
    static result_type get(details::tag<T&>, void * parm)  {
        typedef std::tuple<T&> tuple;
        return std::get<0>(static_cast<tuple&>(*static_cast<tuple*>(parm)));
    }
        
    template<class... T> 
    static result_type get(details::tag<std::tuple<T...>&&>, void*parm) {
        typedef std::tuple<T... > tuple;
        return std::move(*static_cast<tuple*>(parm));
    }        

    static void get(details::tag<void>, void*)  { }        

    switch_pair pair;
};


struct exit_exception 
    : std::exception {
    explicit exit_exception(switch_pair exit_to)
        : exit_to(exit_to) {}
    
    switch_pair exit_to;
    ~exit_exception() throw() {}
};

struct abnormal_exit_exception 
    : exit_exception {

    std::exception_ptr ptr;

    explicit abnormal_exit_exception(switch_pair exit_to)
        : exit_exception(exit_to)
        , ptr(std::current_exception()){}

    std::exception_ptr nested_ptr() const { 
        return ptr;
    }

    ~abnormal_exit_exception() throw() {}
};

namespace details { // Internal Trampolines 

template<class StackAlloc>
struct cleanup_trampoline_args {
    StackAlloc allocator; 
    void * stackp;
    std::exception_ptr excp;

    void operator()() { allocator.deallocate(stackp); }
    cleanup_trampoline_args(cleanup_trampoline_args&&) = default;
};

template<class StackAlloc>
switch_pair cleanup_trampoline(parm_t arg, cont)  {
    auto argsp = static_cast<cleanup_trampoline_args<StackAlloc> *>(arg);
    auto g = guard
        ([&]{ 
            auto alloc = std::move(argsp->allocator); // Must not throw, othwise we leak memory
            void * stackp = argsp->stackp;
            argsp->~cleanup_trampoline_args<StackAlloc>();
            alloc.deallocate(stackp);
        });
    if (argsp->excp) std::rethrow_exception(argsp->excp);
    return switch_pair{{0}, 0};
}

template<class F, class StackAlloc>
struct startup_trampoline_args { // POD
    startup_trampoline_args(startup_trampoline_args&) = default;
    F functor;
    StackAlloc allocator;
    void * stackp;
    startup_trampoline_args(startup_trampoline_args&&) = default;
};

template<class F, 
         class Continuation>
auto do_call(F& f, Continuation c) ->
    typename std::enable_if<std::is_void<decltype(f(std::move(c)))>::value,  
                            switch_pair>::type { 
    f(std::move(c)); 
    assert(false && "void function is not expected to return," 
           "throw exit_exception instead"); 
    abort();
 }

template<class F, 
         class Continuation>
auto do_call(F& f, Continuation c) ->
    typename std::enable_if<!std::is_void<decltype(f(std::move(c)))>::value,  
                            switch_pair>::type { 
    return f(std::move(c)).pilfer();
}

template<class Continuation, class F, class StackAlloc>
switch_pair startup_trampoline(parm_t arg, cont sp) {
    auto argsp = static_cast<startup_trampoline_args<F, StackAlloc>*>(arg);
    cleanup_trampoline_args<StackAlloc> cleanup_args
    { std::move(argsp->allocator), argsp->stackp, 0 };
    try {
        auto f(std::move(argsp->functor)); 
        switch_pair pair = {sp,0};
        sp = do_call(f, Continuation(pair)).sp;
    } catch(abnormal_exit_exception& e) {
        sp = e.exit_to.sp;
        cleanup_args.excp = e.nested_ptr();
    } catch(exit_exception& e) {
        sp = e.exit_to.sp;
    } 
    assert(sp && "invalid target stack");
    return execute_into(&cleanup_args, sp, &cleanup_trampoline<StackAlloc>); 
}

template<class FromSignature, class F>
switch_pair interrupt_trampoline(parm_t  p, cont from) {
    typedef continuation<FromSignature> from_cont;
    typename std::remove_reference<F>::type f
        = std::move(*static_cast<F*>(p));
    switch_pair r {from, 0};
    return do_call(f, from_cont(r));
}

 
// Helpers
template<class Signature, 
         class F,
         class StackAlloc = default_stack_allocator>
continuation<Signature> 
create_continuation(F f, StackAlloc alloc = StackAlloc(), 
                    size_t stack_size = StackAlloc::stack_size)  {
    void * stackp = alloc.allocate(stack_size);
    cont cs { stack_bottom(stackp, stack_size) };

    typedef typename continuation<Signature>::rsignature rsignature;

    startup_trampoline_args<F, StackAlloc> xargs{ 
        std::move(f), std::move(alloc), stackp
    };
    return continuation<Signature>
        (execute_into(&xargs, cs, &startup_trampoline<continuation<rsignature>, 
                                                    F, StackAlloc>));
}

template<class NewIntoSignature, class IntoSignature, class F>
continuation<NewIntoSignature> 
interrupt_continuation(continuation<IntoSignature> c, F f) {
    typedef typename make_signature<NewIntoSignature>::rsignature 
        new_from_signature;

    return continuation<NewIntoSignature>
        (execute_into(&f, c.pilfer().sp, 
                      &interrupt_trampoline<new_from_signature, F>));
}

template<class F, 
         class FSig = typename signature<F>::type,
         class Parm = typename parm0<FSig>::type,
         class RSig = typename Parm::signature,
         class Sig  = typename make_signature<RSig>::rsignature
         > 
struct deduce_signature { typedef Sig type; };

}

///// Public API
template<class F, 
         class... Args,
         class Sig = typename details::deduce_signature<F>::type>
continuation<Sig> callcc(F f, Args&&... args) {
    return details::create_continuation<Sig> 
        (gpd::bind(std::move(f), placeholder<0>(), 
                   std::forward<Args>(args)...)); 
}


template<class Sig, 
         class F, 
         class... Args>
continuation<Sig> callcc(F f,  Args&&... args) {
    return details::create_continuation<Sig>
        (gpd::bind(std::move(f), placeholder<0>(), 
                   std::forward<Args>(args)...));
}


// execute f on existing continuation c, passing to f the current
// continuation. When f returns, c is resumed; F must return a continuation to c.
//
// returns the new continuation of c.
//
// Note that a new signature can be specified for the new continuation
// of c and consequently for the continuation passed to f
template<class NewIntoSignature,
         class IntoSignature, 
         class F, class... Args>
auto callcc(continuation<IntoSignature> c, F f, Args&&... args) as
    (details::interrupt_continuation<NewIntoSignature>
     (std::move(c), gpd::bind(std::move(f), placeholder<0>(), std::forward<Args>(args)...)));

template<class IntoSignature, 
         class F, 
         class ... Args,
         class NewIntoSignature = typename details::deduce_signature<F>::type>
auto callcc(continuation<IntoSignature> c, F f, Args&&... args) as
    (details::interrupt_continuation<NewIntoSignature>
     (std::move(c), gpd::bind(std::move(f), gpd::placeholder<0>(), 
                              std::forward<Args>(args)...)));

// the following four oerloads are not strictly necessary but may
// speedup compilation
template<class F, 
         class Sig = typename details::deduce_signature<F>::type>
continuation<Sig> callcc(F f) {
    return details::create_continuation<Sig> (f);
}

template<class Sig, 
         class F>
continuation<Sig> callcc(F f) {
    return details::create_continuation<Sig>(std::move(f));
}

template<class NewIntoSignature,
         class IntoSignature, 
         class F>
auto callcc(continuation<IntoSignature> c, F f) as
    (details::interrupt_continuation<NewIntoSignature>
     (std::move(c), std::move(f)));

template<class IntoSignature, 
         class F,
         class NewIntoSignature = typename details::deduce_signature<F>::type>
auto callcc(continuation<IntoSignature> c, F f) as
    (details::interrupt_continuation<NewIntoSignature>
     (std::move(c), std::move(f)));

template<class F, class Continuation>
auto with_escape_continuation(F &&f, Continuation c) -> decltype(f()) {
    try {
        return f();
    } catch(...) {
        assert(c);
        throw abnormal_exit_exception(c.pilfer());
    }
}

namespace details {
template<class F>
struct escape_protected {
    F f;
    template<class C>
    auto operator()(C c) as (with_escape_continuation(f, std::move(c)));
};
}
template<class F>
details::escape_protected<F> with_escape_continuation(F f) {
    return details::escape_protected<F>{std::forward<F>(std::move(f))};
}

template<class Signature>
void exit_to(continuation<Signature> c) {
    throw exit_exception(c.pilfer());
}
 
template<class Signature>
void signal_exit(continuation<Signature> c) {
    callcc(std::move(c), [] (continuation<void()> c) { 
            exit_to(std::move(c));
        });
}

template<class Continuation>
struct output_iterator_adaptor  {
    typedef std::output_iterator_tag iterator_category;
    typedef void value_type;
    typedef size_t difference_type;
    typedef void* pointer;
    typedef void reference;
    Continuation& c;
    template<class T>
    output_iterator_adaptor& operator=(T&&x) {
        c(std::forward<T>(x));
        return *this;
    }
    output_iterator_adaptor& operator++() {return *this; }
    output_iterator_adaptor& operator*() { return *this; }
};


template<class Continuation>
struct input_iterator_adaptor  {
    Continuation* c;

    typedef std::input_iterator_tag iterator_category;
    typedef typename std::remove_reference<decltype(c->get())>::type value_type;
    typedef size_t difference_type;
    typedef value_type* pointer;
    typedef decltype(c->get()) reference;

    input_iterator_adaptor& operator=(input_iterator_adaptor const&x) {
        c = x.c;
    }
    input_iterator_adaptor& operator++() {
        (*c)();
        return *this;
    }
    reference operator*() { 
        return c->get();
    }

    bool operator==(input_iterator_adaptor const&) const {
        return !*c;
    }
    bool operator!=(input_iterator_adaptor const&) const {
        return bool(*c);
    }

};

template<class T>
output_iterator_adaptor<continuation<void(T)>>
begin(continuation<void(T)>& c) { return {c}; }

template<class T>
input_iterator_adaptor<continuation<T(void)>>
begin(continuation<T()>& c) { return {&c}; }

template<class T>
input_iterator_adaptor<continuation<T(void)>>
end(continuation<T()>& c) { return {&c}; }

}
#include "macros.hpp"
#endif
