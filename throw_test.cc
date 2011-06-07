#include "switch_base.hpp"
#include "stdio.h"
#include <exception>
extern "C"
void thrower() {
    throw int(0);
}

void catcher1() {
    try {
        thrower();
    } catch (int) {
    }
}

int i = 0;
volatile int * xp = &i;
void catcher4() {
    try {
        *xp;
    } catch(...){ 
    }
}

void __attribute__((noinline, returns_twice)) catcher2a() {
    asm ("push $1f \n\t"
         "jmp thrower \n\t"
         "1:\n\t"
         ::
         : "eax", "ebx", "ecx", "edx", "esi", "edi"   );
}

void catcher2() {
    try {
        *xp;
        thrower();
    } catch (int) {
    }    
}

void catcher3() {
    try {
        *xp;
        asm volatile (
            "call thrower"
             ::
             : "eax", "ebx", "ecx", "edx", "esi", "edi");
    } catch (...) {
    }
}


int main() {
    catcher1();
    catcher2();
    catcher3();
}
