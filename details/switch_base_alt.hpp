#ifndef GPD_SWITCH_BASE_ALT_HPP
#define GPD_SWITCH_BASE_ALT_HPP
#include <cassert>
#include <stdint.h>
#include <stddef.h>

namespace gpd {

typedef void* parm_t;

struct cont { 
    void * rsp;
    void * ip;
    explicit operator bool() const { return rsp;}
};

void * stack_bottom(void * vp, size_t size) {
    char * p = (char*)vp;
    p += size ;
    p -= 7*sizeof(void*);  
    return p;
}

struct switch_pair {
    cont   sp;
    parm_t parm;
};

typedef switch_pair trampoline_t(parm_t parm, cont calling_continuation); 

#define GPD_CLOBBER_LIST                                               \
     "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"              \
        , "xmm0", "xmm1", "xmm2" , "xmm3" , "xmm4" , "xmm5" , "xmm6" , "xmm7" \
        , "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" \
        ,"st",  "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)" \
    , "memory"                                                          \
    /**/


inline switch_pair
stack_switch_impl(cont sp /*rdi + rsi*/, parm_t parm /*rdx*/) {
    asm volatile (
        "movq  %%rdx,  %%rax \n\t"
        "movq  $1f,   %%rdx \n\t"
        "xchg  %%rcx, %%rsp \n\t"
        "jmp   *%%rax        \n\t"
        "1:                 \n\t"
        : "+b"(parm), "+c"(sp.rsp), "+d"(sp.ip) 
        :  
        : "rax", "rdi", "rsi", "rbp",
          GPD_CLOBBER_LIST
        );
    return switch_pair{sp, parm};
}

typedef switch_pair trampoline_t(parm_t /* rdi */ parm, cont calling_continuation /*rsi + rdx*/); 
   
inline
switch_pair 
execute_into(parm_t parm, cont sp /* rsi + rdx */, trampoline_t * ex /*rcx*/) {
    asm volatile (
        "movq  $1f,   %%rdx \n\t"
        "xchg  %%rsi, %%rsp \n\t"
        "jmp   *%%rax        \n\t"
        "1:                \n\t"
        : "=b"(parm), "=c"(sp.rsp), "+d"(sp.ip)
        : "D"(0), "S"(parm), "d"(sp.rsp), ""(ex) 
        : "rbp",
          GPD_CLOBBER_LIST
        );
    return switch_pair{sp, parm};
}



}
#endif
