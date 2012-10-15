#ifndef GPD_MACROS
#define GPD_MACROS
#include <type_traits>
/**
 * Simple macros to help define single expression functions whose
 * result type is the same as the expression type itself.
 *
 * To prevent macro pollution, this header file is meant to be
 * included last in an header file, and also included again at the end
 * of file.
 *
 */


/**
 * Usage:
 * 
 * auto my_function(T1 a1, T2 a2...) as ( <expression> );
 *
 * my_function is a function that returns the result of <expression>.
 **/
#define as(...)                               \
    -> decltype(__VA_ARGS__) { return __VA_ARGS__ ; }  \
/**/

/**
 * Same as 'as', but also allows passing a 'cond' expression that will
 * be evaluated in a SFINAE context.
 *
 */
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
