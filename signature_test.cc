#include <type_traits>
#include <cstdlib>
#include "signature.hpp"

using namespace gpd;


void dummy() {}
void dummy1(int) { abort(); }
int dummy2(void) { abort(); }
int dummy3(int)  { abort(); }


struct unknown;
template<class T, class Enabled = void>
struct sig_or_unknown { typedef unknown type; };

template<class T>
struct sig_or_unknown<T, 
                      typename sfinae<typename signature<T>::type>::type> 
: signature<T> {};


template<class Expected, class T, class Sig = typename sig_or_unknown<T>::type>
void check(T) {
   static_assert(std::is_same<Sig , Expected>::value,
                 "signature mismatch");
}

template<class Sig> struct monomorphic;

template<class Ret, class... Args>
struct monomorphic<Ret(Args...)> {
    Ret operator()(Args...) { abort(); }
};

template<class Sig> struct cmonomorphic;

template<class Ret, class... Args>
struct cmonomorphic<Ret(Args...)> {
    Ret operator()(Args...) const { abort(); }
};


template<class... Sig>
struct overloaded : monomorphic<Sig>... {};

struct uncallable {};

struct polymorphic { 
    template<class... Args> void operator()(Args...) { abort(); } 
};
int main() {
    check<void()>(dummy);
    check<void(int)>(dummy1);
    check<int()>(dummy2);
    check<int(int)>(dummy3);
    check<void()>([]() { abort(); });
    check<void(int)>([](int) { abort(); });
    check<int()>([]() -> int { abort(); });
    check<int(int)>([](int) -> int { abort(); });
    check<int(int, int)>([](int, int) -> int { abort(); });
    check<void()>(monomorphic<void()>{});
    check<int(double, double)>(monomorphic<int(double, double)>{});
    check<void()>(cmonomorphic<void()>{});
    check<int(double, double)>(cmonomorphic<int(double, double)>{});
    check<unknown>(overloaded<void(), int(int)>{});
    check<unknown>(int{});
    check<unknown>(uncallable{});
    check<unknown>(polymorphic{});
}
