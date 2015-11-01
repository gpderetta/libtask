static __inline__ unsigned long long rdtsc(void)
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}


#define GPD_CLOBBER_LIST                                               \
     "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"              \
        , "xmm0", "xmm1", "xmm2" , "xmm3" , "xmm4" , "xmm5" , "xmm6" , "xmm7" \
        , "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" \
        ,"st",  "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)" \
    , "memory"                                                          \
    /**/


inline switch_pair
stack_switch_impl(cont sp /*rdi + rsi*/, parm_t parm /*rdx*/) {
    return switch_pair{sp, parm};
}

typedef switch_pair trampoline_t(parm_t /* rdi */ parm, cont calling_continuation /*rsi + rdx*/); 


struct cont_impl {
    void ** ip;
    void ** sp;
};


// callee-save: RBP, RBX, R12, R13, R14, R15
// param:       RDI, RSI, RDX, RCX, R8,  R9,
// float param: XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7
// return:      RAX, RDX
// float return: XMM0
// caller save: R10, R11

// fp = rdi, from = rsi+rdx,
using = void trampline_t(void * fp, cont_impl from);

template<class FromR, class FromT, class ToR, class ToT, class F, class Parm>
cont_impl trampoline(void * fp, cont_impl from)
{
    F f = std::move(*(F*)fp);
    continuation<ToR, ToT> c2 = f(continuation<FromR, FromT> c { from });
    return c2.pilfer();
}

extern "C"
void fixup();

/// 16(rsp) points to dest ip, 8(rsp) to functor state, (rsp) to
/// functor ip on entry rax points to source ip, rdx points to source
/// sp. rcx is param and is passed through.
asm volatile (                            
    ".text                         \n\t"                              
    ".fixup                        \n\t"                   
    ".type fixup, @function        \n\t"            
    ".align 16                     \n\t"                           
    "fixup:                        \n\t"
    ".cfi_startproc                \n\t"
    ".cfi_def_cfa_offset 24        \n\t"
    "movq %%rax, %%rsi             \n\t"
    "movq (%%rsp),  %%rdi          \n\t"
    "movq 8(%%rsp), %%rcx          \n\t"
    "call *%%rcx                   \n\t"
    // at this point rax+rdx are the result of the call and also the
    // result of switch
    // pop dest ip
    "popq  %%rdi                   \n\t"
    "jmp  *%%rdi                   \n\t"
    );



void  setup(cont_impl& cont, trampline_t f, void * _this)
{
    auto orig_ip = std::exchange(cont.ip, (void*)fixup);
    *(--cont.sp) = orig_ip;
    *(--cont.sp) = _this;
    *(--cont.sp) = (void*)f;
}

template<class ToR, class ToT>
template<class R, class T>
struct continuation
{
    R * operator()(T* param)
    {
        void * p =  param;
        asm volatile (
            "movq  $1f,   %%rax \n\t"
            "movq  %%rsp, %%rdx \n\t"
            "movq  %%rsi, %%rsp \n\t"
            "jmp   *%%rbx        \n\t"
            "1:                 \n\t"
            : "=a"(data.ip), "=d"(data.sp), "+c"(p)
            : "b"(data.ip), "S"(data.sp) 
            : "rdi", "rbp"
              GPD_CLOBBER_LIST
            );
        return (R*)p;
    }
    cont_impl data;

    cont_impl pilfer() { return std::exchange(data, cont_impl{}); }
};

cont_impl make()
{
    const std::size_t len = 1024*10;
    void ** stack = new char[len];

    cont_impl result = { 0, sp + len };
    setup(result, )

    
}


int main()
{
}
