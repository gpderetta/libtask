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

struct K {};



template<class T>
auto does_not_understand(gpd::symbol<T>, K){ return 42; }

template<int>
struct placeholder { };
constexpr placeholder<1> $1;


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
        K x;
        auto y = $(foo)(x);
        assert(y == 42);
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
        rx.rhs.value = 12;
        assert(x == 12);

        auto cx = gpd::as_field(rx);
        cx.foo = 42;
        assert(cx.foo == 42 && x == 12);

        auto cy = gpd::as_field($(bar) = x);
        cy.bar = 69;
        assert(cy.bar == 69 && x == 12);

        auto xbar = gpd::meta<std::decay_t<decltype(cy) > >;
        auto cz = gpd::as_field(xbar = 10);
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
        int x = 10;
        int y = 20;
        auto $foo = $(foo);
        auto $bar = $(bar);
        auto tuple = ref($foo = x, $bar = y);
        assert(tuple.foo == 10);
        assert(tuple.bar == 20);
        tuple.foo = 22;
        assert(x == 22);
    }
    {
        auto $foo = $(foo);
        auto $bar = $(bar);
        using tuple_t = decltype(tup($foo = 10, $bar = 20));
        tuple_t tuple($foo = 42, $bar = 54);

        assert(tuple.foo == 42);
        assert(tuple.bar == 54);

        tuple_t tuplep ($foo = 69);
        assert(tuplep.foo == 69);
        assert(tuplep.bar == 0);
        
        tuple = ref($(foo) = 10, $(bar) = 20);
        assert(tuple.foo == 10);
        assert(tuple.bar == 20);

        struct { int foo = 1000, bar = 200; } x;


        tuple_t tuple2 (gpd::from(x));
        assert(tuple2.foo == 1000);
        assert(tuple2.bar == 200);

        tuple = x;
        assert(tuple.foo == 1000);
        assert(tuple.bar == 200);

        gpd::to(x) = gpd::tup($(bar) = 20);
        assert(x.bar == 20);

        struct { int foo, bar; } y (tuple);
        assert(y.foo == 1000);
        assert(y.bar == 200);

        int foo, bar;
        gpd::ref($foo = foo, $bar = bar) = tuple;
        assert(foo == 1000);
        assert(bar == 200);
    }

    {
        J j;
        auto x = tup($(foo) = $(foo));
        x.foo(j);
    }
    {
        static_assert(gpd::callable<Y>($(foo)), "failed");
        static_assert(gpd::callable<Y, int>($(foo)), "failed");
        static_assert(!gpd::callable<Y, int, int>($(foo)), "failed");
        static_assert(!gpd::callable<Y, int, int>($(foo)), "failed");

    }
    {
        struct X { int foo; int baz; };
        std::vector<X> v = { {0,1}, {1,3}, {2,4} };;
        // for (auto x : from(v)
        //          .map($(foo), $(bar) = $(baz) * 2)
        //          .where($(bar) > $(foo)))
        // {
        // }
    }
}
