#include "q.hpp"
#include <cassert>
#include <vector>
#include <algorithm>
#include <numeric>
#include <utility>
#include <array>
#include <functional>
#include <sstream>
struct X {
    int foo;
};

struct Y {
    void foo() {  }
    void foo(int) {  }
};

struct Z {
    void foo() {  }
};

struct J {};

namespace nXX {
struct XX {};
void foo(XX) { }
};

void foo() {}
void foo(J) {}
void foo(Z) {}
void foo(Y, int){}
void foo(X, int){}


int main() {

    {
        X x{ 0 };
        $(foo)(x) = 10;
        

        assert(x.foo == 10);
    }
    {
        const X x {11};
        int j = $(foo)(x);
        assert(j == 11);
    }
    {
        X x {11};

        int&& j = $(foo)(std::move(x)) ;
        j = 12;
        assert(x.foo == 12);
        int y = $(foo)(x);
        (void)y;
    }
    {
        Z z{};
        $(foo)(z);
    }
    {
        Y y{};
        $(foo)(y);
        $(foo)(y, 10);
    }
    {
        J j{};
        $(foo)(j);
    }
    {
        nXX::XX xx;
        $(foo)(xx);
    }

    {
        std::vector<X> in { {0}, {1}, {2}, {3} };
        std::vector<int> out;
        std::vector<int> expected = { 0, 1, 2, 3 };
        std::transform(in.begin(), 
                       in.end(), 
                       std::back_inserter(out),
                       $(foo));
        assert(expected == out);
    }
    {
        std::pair<int, int> p = { 0, 1};
        assert($(first)(p) == 0);
        (void)p;
    }
    {
        std::vector<std::pair<int, int> > in { {0, 0}, {1,-1 }, {2, -2}, {3,-3} };
        std::vector<int> out;
        std::vector<int> expected = { 0, 1, 2, 3 };
        std::transform(in.begin(), 
                       in.end(), 
                       std::back_inserter(out),
                       $(first));
        assert(expected == out);

    }
    {
        std::vector<std::pair<int, int> > in { {0, 0}, {1,-1 }, {2, -2}, {3,-3} };
        std::vector<int> out;
        std::vector<int> expected = { 0, -1, -2, -3 };
        std::transform(in.begin(), 
                       in.end(), 
                       std::back_inserter(out),
                       $(second));
        assert(expected == out);
    }
    {
        std::vector<std::pair<int, int> > in { {0, 0}, {1,-1 }, {2, -2}, {3,-3} };
        std::vector<int> out;
        std::vector<int> expected = { 0, -1, -2, -3 };
        $(transform)(in.begin(), 
                   in.end(), 
                   std::back_inserter(out),
                   $(second));
        assert(expected == out);
    }
    {
        int x = 10;
        auto rx = $(foo) = x;
        rx.foo = 12;
        assert(x == 12);

        auto cx = rx.$bind();
        cx.foo = 42;
        assert(cx.foo == 42 && x == 12);

        auto cy = gpd::capture($(bar) = x);
        cy.bar = 69;
        assert(cy.bar == 69 && x == 12);

        auto xbar = cy.$meta();
        auto cz = gpd::capture(xbar = 10);
        assert(cz.bar == 10);

        assert($(bar)(cz) == 10);

    }
    using gpd::tup;
    {
        auto tuple = tup($(foo) = 10, $(bar) = 20);
        assert(tuple.foo == 10);
        assert(tuple.bar == 20);
    }
    {
        auto $foo = $(foo);
        auto $bar = $(bar);
        auto tuple = tup($foo = 10, $bar = 20);
        assert(tuple.foo == 10);
        assert(tuple.bar == 20);
    }
    {
        auto $foo = $(foo);
        static_assert($foo.name() == "foo", "failed");
    }
    {
        auto tuple = tup($(foo) = 10, $(bar) = 20., $(baz) = "hello");
        std::stringstream s;
        s << tuple << std::endl;
        assert(s.str() == "{foo: 10, bar: 20, baz: hello}\n");
    }
    {
        J j;
        auto x = tup($(foo) = $(foo));
        x.foo(j);
    }
}
