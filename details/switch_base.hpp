#ifndef GPD_SWITCH_BASE_HPP
#define GPD_SWITCH_BASE_HPP
#include <cassert>
#include <stdint.h>
#include <stddef.h>

namespace gpd {
typedef void* parm_t;

struct cont { 
    void * sp; 
    explicit operator bool() const { return sp;}
};

inline void * stack_bottom(void * vp, size_t size) {
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


#define GPD_SAVE_REGISTERS                      \
    "pushq %rbx            \n\t"                \
    "pushq %r12            \n\t"                \
    "pushq %r13            \n\t"                \
    "pushq %r14            \n\t"                \
    "pushq %r15            \n\t"                \
    "pushq %rbp            \n\t"                \
/**/
#define GPD_RESTORE_REGISTERS                   \
    "popq %rbp             \n\t"                \
    "popq %r15             \n\t"                \
    "popq %r14             \n\t"                \
    "popq %r13             \n\t"                \
    "popq %r12             \n\t"                \
    "popq %rbx             \n\t"                \

/**/

extern "C"
switch_pair /*rax, rdx*/
stack_switch_impl(cont sp /*rdi*/, parm_t parm /*rsi*/);
asm volatile (                            
    ".text                         \n\t"                              
    ".weak stack_switch_impl       \n\t"                   
    ".type stack_switch_impl, @function \n\t"            
    ".align 16                     \n\t"                           
    "stack_switch_impl:            \n\t"         
    GPD_SAVE_REGISTERS
    "movq %rsi, %rdx       \n\t"  // parm-> switch_pair::parm
    "movq %rsp, %rax       \n\t"  // rsp -> switch_pair::sp
    "movq %rdi, %rsp       \n\t"  // sp  -> rsp
    GPD_RESTORE_REGISTERS
    "popq %rdi             \n\t"
    "jmp *%rdi             \n\t"  // jump to ret address
    );

extern "C"
switch_pair /*rax, rdx*/
execute_into_impl(parm_t parm /*rdi*/, cont sp /*rsi*/, trampoline_t * ex /*rdx*/);
asm volatile (                            
    ".text                         \n\t"                              
    ".weak execute_into_impl       \n\t"                   
    ".type execute_into_impl, @function \n\t"            
    ".align 16                     \n\t"                           
    "execute_into_impl:            \n\t"                
    GPD_SAVE_REGISTERS
    "movq %rsp, %rbx       \n\t"
    "movq %rsi, %rsp       \n\t" 
    "movq %rbx, %rsi       \n\t"
    GPD_RESTORE_REGISTERS
    "jmp *%rdx             \n\t"  //tail call (rdi is passed through)
    );  

inline switch_pair
stack_switch(cont sp, parm_t parm) {
    return stack_switch_impl(sp, parm);
}
   
inline
switch_pair 
execute_into(parm_t parm, cont sp, trampoline_t * ex) {
    return execute_into_impl(parm, sp, ex);
}


}
#endif

