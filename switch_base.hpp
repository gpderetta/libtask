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
    p -= 10*sizeof(void*);  
    return p;
}

struct switch_pair {
    cont   sp;
    parm_t parm;
};




extern "C"
switch_pair /*rax, rdx*/
stack_switch(cont sp /*rdi*/, parm_t parm /*rsi*/);
asm volatile (                            
".text                         \n\t"                              
".weak stack_switch            \n\t"                   
".type stack_switch, @function \n\t"            
".align 16                     \n\t"                           
"stack_switch:                 \n\t"         
        "pushq %rbp            \n\t"
        "pushq %rbx            \n\t"
        "pushq %r12            \n\t"
        "pushq %r13            \n\t"
        "pushq %r14            \n\t"
        "pushq %r15            \n\t"
        "movq %rsi, %rdx       \n\t"  // parm-> switch_pair::parm
        "movq %rsp, %rax       \n\t"  // rsp -> switch_pair::sp
        "movq %rdi, %rsp       \n\t"  // sp  -> rsp
        "popq %r15             \n\t"
        "popq %r14             \n\t"
        "popq %r13             \n\t"
        "popq %r12             \n\t"
        "popq %rbx             \n\t"
        "popq %rbp             \n\t"
        "popq %rdi             \n\t"
        "jmp  *%rdi            \n\t"  // jump to ret address
    );

typedef switch_pair trampoline_t(parm_t parm, cont calling_continuation);

extern "C"
switch_pair /*rax, rdx*/
execute_into(parm_t parm /*rdi*/, cont sp /*rsi*/, trampoline_t * ex /*rdx*/);
asm volatile (                            
".text                         \n\t"                              
".weak execute_into            \n\t"                   
".type execute_into, @function \n\t"            
".align 16                     \n\t"                           
"execute_into:                 \n\t"                
        "pushq %rbp            \n\t"
        "pushq %rbx            \n\t"
        "pushq %r12            \n\t"
        "pushq %r13            \n\t"
        "pushq %r14            \n\t"
        "pushq %r15            \n\t"
        "movq %rsp, %rbx       \n\t"
        "movq %rsi, %rsp       \n\t"
        "movq %rbx, %rsi       \n\t"
        "popq %r15             \n\t"
        "popq %r14             \n\t"
        "popq %r13             \n\t"
        "popq %r12             \n\t"
        "popq %rbx             \n\t"
        "popq %rbp             \n\t"
        "jmp  *%rdx            \n\t"  //tail call (rdi is passed through)
    );


}
#endif
