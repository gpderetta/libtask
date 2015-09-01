#include <cassert>
#ifdef NDEBUG
#define XASSERT(x)                                      \
    do { if(!(x)) __builtin_unreachable(); } while(0)   \
/**/
#else
#define XASSERT(x) assert(x)
#endif
