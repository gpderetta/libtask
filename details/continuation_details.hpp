#ifndef GPD_CONTINUATION_DETAILS_HPP
#define GPD_CONTINUATION_DETAILS_HPP
#include "switch_pair_accessor.hpp"
#include "continuation_exception.hpp"
#include "stack_allocator.hpp"
#include "guard.hpp"
#include <utility>
namespace gpd {

template<class Signature>
struct continuation;

namespace details {

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
    return details::switch_pair_accessor::pilfer(f(std::move(c)));
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
        sp = details::switch_pair_accessor::pilfer(e).sp;
        cleanup_args.excp = e.nested_ptr();
    } catch(exit_exception& e) {
        sp = details::switch_pair_accessor::pilfer(e).sp;
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
        (execute_into(&f, details::switch_pair_accessor::pilfer(c).sp, 
                      &interrupt_trampoline<new_from_signature, F>));
}
}}
#endif
