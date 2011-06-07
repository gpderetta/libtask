#ifndef GPD_SWITCH_BASE_ALT_HPP
#define GPD_SWITCH_BASE_ALT_HPP
#include <cassert>
#include <stdint.h>
#include <stddef.h>

namespace gpd {

typedef void* parm_t;

struct cont { 
    void * rsp; 
    explicit operator bool() const { return sp;}
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
    , "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"              \
        , "xmm0", "xmm1", "xmm2" , "xmm3" , "xmm4" , "xmm5" , "xmm6" , "xmm7" \
        , "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" \
        ,"st",  "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)" \
    , "memory"                                                          \
    /**/

inline switch_pair
stack_switch(cont sp, parm_t parm) {
    asm (
        "pushq $1f          \n\t"
        "pushq %%rbp        \n\t"
        "movq  %%rsp, %%rbx \n\t"
        "movq  %%rax, %%rsp \n\t"
        "popq  %%rbp        \n\t"
        "popq  %%rsi        \n\t"
        "jmp  *%%rsi        \n\t"
        "1:                 \n\t"
        :  "=b"(sp.rsp), "+d"(parm)
        :  "b" (sp.rsp)
        : "rsi", "rdi",, "rcx"
          GPD_CLOBBER_LIST
        );
    return switch_pair{sp, parm};
}

typedef switch_pair trampoline_t(parm_t parm, cont calling_continuation); 
   
inline
switch_pair 
execute_into(parm_t parm, cont sp, trampoline_t * ex) {
    switch_pair ret;
    asm ( 
        "pushq $1f          \n\t"
        "pushq %%rbp        \n\t"
        "xchg  %%rsi, %%rsp \n\t"
        "popq  %%rbp        \n\t"
        "jmp  *%%rbx        \n\t"
        "1:                \n\t"
        : "=d"(ret.parm), "=a"(ret.sp)
        : "D"(parm), "S"(sp), "b"(ex) 
        : "rcx"
          GPD_CLOBBER_LIST
        );
    return ret;
}

}
#endif
