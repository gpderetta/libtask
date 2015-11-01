#include "quote.hpp"
#include <cassert>

struct X {
    int foo;
};

struct Y {
    int foo() {}
    int foo(int);
};

struct Z {
    int foo() {}
};

struct J {};

namespace nXX {
struct XX {};
int foo(XX) {}
};

int foo() {}
int foo(J) {}
int foo(Z) {}


quotable(foo);

int main() {

    X x{};

    Y y{};
    Z z{};
    J j{};

    constexpr X cx { 10 };
    constexpr int cint = $(foo)(cx);

    $(foo)(x) = 10;
    assert(x.foo == 10);
    
    const_cast<int&>($(foo)(const_cast<const X&>(x))) = 11;

    const_cast<int&>($(foo)(const_cast<X&&>(x))) = 11;

    assert(x.foo == 11);
    $(foo)(y);
    $(foo)(z);
    nXX::XX xx;

    $(foo)(xx);

}



int main() {
}
