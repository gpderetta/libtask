#ifndef GPD_CONTINUATION_META_HPP
#define GPD_CONTINUATION_META_HPP
#include "signature.hpp"
#include <tuple>
/**
 * A few metafunctions used to compute argument and result type slots
 * and continuation->reverse continuation signatures.
 * 
 * Currently none of these are documented as they are supposed to be
 * implementation details
 */
namespace gpd { namespace details {
/// Utility Metafunctions

template<class> class tag {};

template<class Ret>
struct result_traits {
    typedef Ret              result_type;
    typedef std::tuple<Ret>&   xresult_type;
};

template<class... Ret>
struct result_traits<std::tuple<Ret...> > {
    typedef std::tuple<Ret...>&& result_type;
    typedef std::tuple<Ret...>&  xresult_type;
};

template<>
struct result_traits<void> {
    typedef void result_type;
    typedef void xresult_type;
};

template<class...Ret>
struct result_pack { typedef std::tuple<Ret...> type; };

template<class Ret>
struct result_pack<Ret> { typedef Ret type; };

template<>
struct result_pack<> { typedef void type; };

template<class Ret, class... Parm>
struct compose_signature { typedef Ret type(Parm...); };

template<class Ret, class... Parms>
struct compose_signature<Ret, std::tuple<Parms...> > 
    : compose_signature<Ret, Parms...> { };

template<class Ret>
struct compose_signature<Ret, void> 
    : compose_signature<Ret> { };

template<class> struct reverse_signature;
template<class Ret, class... Args>
struct reverse_signature<Ret(Args...)> 
    : compose_signature<typename result_pack<Args...>::type, Ret>   {};

template<class Signature> struct make_signature; 
template<class Ret, class... Args>
struct make_signature<Ret(Args...)> 
    : result_traits<Ret> {
    typedef typename reverse_signature<Ret(Args...)>::type rsignature;
};

template<class F, 
         class FSig = typename signature<F>::type,
         class Parm = typename parm0<FSig>::type,
         class RSig = typename Parm::signature,
         class Sig  = typename make_signature<RSig>::rsignature
         > 
struct deduce_signature { typedef Sig type; };

}}
#endif
