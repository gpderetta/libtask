#include <cassert>
#ifdef NDEBUG
#define XASSERT(x)                             \
    if(!x) __builtin_unreachable();            \
/**/
#else
#define XASSERT(x) assert(x)
#endif
