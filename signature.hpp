#ifndef GPD_SIGNATURE_HPP
#define GPD_SIGNATURE_HPP

namespace gpd {

namespace details {
template<class Sig> struct extract_signature;
template<class T, class Ret, class... Args>
struct extract_signature<Ret(T::*)(Args...)> { typedef Ret type (Args...); };
template<class T, class Ret, class... Args>
struct extract_signature<Ret(T::*)(Args...) const> { typedef Ret type (Args...); };
}

/**
 * Metafunction that returns the second parameter (defaulted to void)
 * and ignores the first.
 *
 * Useful to just provide a contest to put a SFINAE context in result types.
 */
template<class X, class Ret = void>
struct sfinae { typedef Ret type; };

/**
 * Metafunction to compute the the signature of monomorphic callable object F.
 *
 * Will detect the signature correctly with function references,
 * function pointers and function object with a non-ambiguous
 * operator() (i.e. lambda expressions).
 *
 * SFINAE if F is polymorphic or not callable.
 */
template<class F, class Enabled = void>
struct signature;

template<class F>
struct signature<F, typename sfinae<decltype(&F::operator())>::type>  : details::extract_signature<decltype(&F::operator())> {};

template<class Ret, class... Args>
struct signature<Ret(*)(Args...)> { typedef Ret type(Args...); };

template<class Ret, class... Args>
struct signature<Ret(Args...)> { typedef Ret type(Args...); };


/**
 * Metafunction to compute the first parameter of function signature
 * 'Sig' if any. 
 *
 * SFINAE if nullary function
 *
 * TODO: generalize to n-th parameter.
 */
template<class Sig>
struct parm0 {};

template<class Ret, class Arg0, class... Args>
struct parm0<Ret(Arg0, Args...)> { typedef Arg0 type; };


}
#endif
