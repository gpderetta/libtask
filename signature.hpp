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

template<class F, class Enabled = void>
struct signature;

template<class X, class Ret = void>
struct sfinae { typedef Ret type; };

template<class F>
struct signature<F, typename sfinae<decltype(&F::operator())>::type>  : details::extract_signature<decltype(&F::operator())> {};

template<class Ret, class... Args>
struct signature<Ret(*)(Args...)> { typedef Ret type(Args...); };

template<class Ret, class... Args>
struct signature<Ret(Args...)> { typedef Ret type(Args...); };

}
#endif
