#ifndef GPD_MACROS
#define GPD_MACROS
#include <type_traits>

#define as(...)                               \
    -> decltype(__VA_ARGS__) { return __VA_ARGS__ ; }  \
/**/

#define when(cond, ...)                                                 \
    -> typename std::enable_if<cond, decltype(__VA_ARGS__)>::type { return __VA_ARGS__; } \
/**/

#define id(a, ...) -> a { typedef a result; return __VA_ARGS__; }

#else
#undef GPD_MACROS
#undef as
#undef when
#undef id
#endif
