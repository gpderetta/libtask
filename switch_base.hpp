#ifndef GPD_SWITCH_BASE_HPP
#define GPD_SWITCH_BASE_HPP
#include <cassert>
#include <stdint.h>

namespace gpd {
typedef void* parm_t;

struct cont { void * sp; explicit operator bool() const { return sp;}  };

void * stack_bottom(void * vp, size_t size) {
    char * p = (char*)vp;
    p += size ;
    return p;
}

struct switch_pair {
    cont   sp;
    parm_t parm;
};

inline 
switch_pair // rax, rdx
__attribute__((noinline, noclone, returns_twice))
stack_switch(cont sp,           // rdi
             parm_t parm        // rsi
 ) {
    asm (
        "movq %%rsi,     %%rdx \n\t"  // parm-> switch_pair::parm
        "movq %%rsp,     %%rax \n\t"  // rsp -> switch_pair::sp
        "leaq 8(%%rdi),  %%rsp \n\t"  // sp  -> rsp, skipping ret address
        "jmp  *-8(%%rsp)       \n\t"  // jump to ret address
        : "=a" (sp),
          "+S" (parm) 
        : "D" (sp) );
    return {sp, parm};
}

typedef switch_pair trampoline_t(parm_t parm, cont calling_continuation);

inline 
__attribute__((noinline, returns_twice, noclone))
switch_pair execute_into(parm_t parm, // rdi
                         cont    sp,  // rsi
                         trampoline_t * ex    // rdx
    ) {
    asm (
        "movq %%rsp,    %%rbx \n\t"
        "movq %%rsi,    %%rsp \n\t"  
        "movq %%rbx,    %%rsi \n\t"
        "jmp  *%%rdx       \n\t"  //tail call (rdi is passed through)
        : "+D" (parm),
          "+S" (sp) 
        : "d" (ex)
        );
    return {sp, parm};
}

}
#endif
