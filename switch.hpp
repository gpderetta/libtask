#ifndef SWITCH_HPP
#define SWITCH_HPP
#include "forwarding.hpp"
#include <exception>
#include <cstring>
#include <iterator>
#include <stdint.h>
#include "macros.hpp"
namespace gpd {

typedef void* parm_t;

struct cont { void * sp; };

uint64_t stack_top(cont x) {
    return *static_cast<uint64_t*>(x.sp);
}

void * return_address(cont x) {
    return *std::next(static_cast<void **>(x.sp));
}

struct switch_ret {
    cont    sp;
    parm_t parm;
};


inline 
switch_ret // rax, rdx
__attribute__((noinline, noclone, returns_twice))
stack_switch(cont sp,           // rdi
             uint64_t topstack, // rsi 
             parm_t parm        // rdx, passthru
    ) {
    asm (
        "pushq %%rsi            \n\t" // put topstack on top of stack
        "movq %%rsp,     %%rax \n\t"  // rsp -> switch_ret::sp
        "leaq 8(%%rdi),  %%rsp \n\t"  // sp  -> rsp, skipping ret address and topstack
        "jmp  *-8(%%rsp)       \n\t"  // jump to ret address
        : "=a" (sp),
          "+d" (parm) 
        : "D" (sp), "S" (topstack)   );
    return {sp, parm};
}

typedef switch_ret trampoline_t(cont calling_continuation, parm_t parm);

inline 
__attribute__((noinline, returns_twice, noclone))
switch_ret execute_into(parm_t parm, // rdi
                        cont    sp,   // rsi
                        trampoline_t * ex    // rdx
    ) {
    asm (
        "movq %%rsp,    %%rbx \n\t"
        "movq %%rsi,    %%rsp \n\t"  
        "leaq 4(%%rbx), %%rsi \n\t"
        "jmp  *%%rdx       \n\t"  //tail call (rdi is passed through)
        : "+D" (parm),
          "+S" (sp) 
        : "d" (ex)
        );
    return {sp, parm};
}


namespace detail {
template<class F, class D>
void startup_trampoline(cont sp, F*p); 
}


template<class Signature>
class continuation;


struct exit_exception;

struct from_sp_tag{} from_sp;


template<class result_type, class... Args>
struct continuation<result_type(Args...)> {

    continuation(const continuation&) = default;
    continuation(cont sp,  from_sp_tag) : sp(sp) {}

    auto operator() (Args... args) -> result_type
    {
        auto arg_pack = pack(std::forward<Args>(args)...);
        switch_ret sret = stack_switch(sp, 0, parm_t{&arg_pack}); 
        sp = sret.sp;
        return *static_cast<result_type*>(sret.parm);
    };

private:
    cont sp;
};


template<class T, class Enable = void>
struct arg2arg {
    static auto pack(T&x) as(&x);
    static auto unpack(parm_t x) as(*static_cast<T*>(x));
};

template<class A, class B>
A bitcast(B b) {
    static_assert(sizeof(A) == sizeof(b), "size mismatch");
    static_assert(std::is_pod<A>::value, "not a POD");
    static_assert(std::is_pod<B>::value, "not a POD");
    A ret;
    memcpy(&ret, &b, sizeof(b));
    return ret;
}

template<class T>
struct arg2arg<T, typename std::enable_if<
                      std::is_pod<T>::value &&
                      sizeof(T) == sizeof(parm_t)>::type>  {
    static auto pack(T x) as (bitcast<parm_t>(x));
    static auto unpack(parm_t x) as (bitcast<T>(x));
};

template<>
struct arg2arg<void>  {
    static void unpack(parm_t) {};
};

template<class... T>
struct arg2arg<std::tuple<T...>, void>  {
    static auto pack(std::tuple<T...>& x) as (static_cast<parm_t>(&x));
    static auto unpack(parm_t x) as (*static_cast<std::tuple<T...>*>(x));
};

template<class result_type, class Arg>
struct continuation<result_type(Arg)> {
    continuation(const continuation&) = default;
    continuation(cont sp,  from_sp_tag) : sp(sp) {}

    auto operator() (Arg arg) -> result_type
    {
        switch_ret sret = stack_switch(sp, 0, arg2arg<Arg>::pack(arg)); 
        sp = sret.sp;
        return arg2arg<result_type>::unpack(sret.parm);
    };

private:
    cont sp;
};

// template<class... Args, class Ret>
// struct continuation<Ret(Args...)> {
//     typedef std::tuple<Args...> arg_pack_t;
//     typedef Ret result_type;

//     auto operator() (Args... args) -> result_type
//     {
//         arg_pack_t arg_pack = pack(std::forward<Args>(args)...);
//         switch_ret sret = stack_switch(m_sp, parm_t{&arg_pack}); 
//         m_sp = sret.sp;
//         return *reinterpret_cast<Ret*>(sret.parm);
//     };

// private:
//     void * m_sp;
// };


typedef continuation<std::tuple<>()> exit_continuation;

struct exit_exception : std::exception {
    explicit exit_exception(exit_continuation exit_to)
        : exit_to(std::move(exit_to)) {}
    
    exit_continuation exit_to;
};

struct abnormal_exit_exception : exit_exception, std::nested_exception {
    explicit abnormal_exit_exception(exit_continuation exit_to)
        : exit_exception(std::move(exit_to)) {}
};

template<class F>
auto with_escape_continuation(F &&f, exit_continuation c) -> decltype(F()) {
    try {
        f();
    } catch(...) {
        throw abnormal_exit_exception(c);
    }
}


namespace detail {

template<class Deleter>
struct deleter_holder {
    Deleter deleter; // Remove if is_empty<Deleter>
    std::exception_ptr ptr;
};

template<class T, class Deleter>
struct startup_trampoline_ctx { // POD
    startup_trampoline_ctx(startup_trampoline_ctx const&) = default;
    T functor;
};

template<class Deleter>
switch_ret destroy(cont, deleter_holder<Deleter> * holderp) {
    Deleter d(std::move(holderp->deleter));
    d();
    return {0,0};
}

template<class F, class D, class Continuation>
void startup_trampoline(cont sp, startup_trampoline_ctx<F, D> * ctxp) {
    auto ctx = *ctxp;
    try {
        F f(std::forward<F>(ctx.functor)); 
        sp  = f(Continuation(sp, from_sp));
    } catch(exit_exception& e) {
        sp = e.exit_to.sp;
    }
    execute_into(sp, 0); 
}
}




}
#include "macros.hpp"
#endif
