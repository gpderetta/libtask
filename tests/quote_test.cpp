#include "quote.hpp"
#include <cassert>
#include <vector>
#include <algorithm>
#include <numeric>
#include <utility>
#include <array>
#include <functional>
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

#define cat(a) foo_##a

quotable(foo);
quotable(first);
quotable(second);
quotable(transform)
int main() {
    {
        X x{ 0 };
        $foo(x) = 10;
        assert(x.foo == 10);

        const_cast<int&>($foo(const_cast<const X&>(x))) = 11;
        assert(x.foo == 11);
        const_cast<int&>($foo(const_cast<X&&>(x))) = 12;
        assert(x.foo == 12);
        $foo(x);
    }
    {
        Y y{};
        $foo(y);
        $foo(y, 10);    
    }
    {
        Z z{};
        $foo(z);
    }
    {
        J j{};
        $foo(j);
    }
    
    {
        constexpr X cx { 10 };
        constexpr int cint = $foo(cx);
        (void)cint;
    }
    {
        nXX::XX xx;
        $foo(xx);
    }

    {
        std::vector<X> in { {0}, {1}, {2}, {3} };
        std::vector<int> out;
        std::vector<int> expected = { 0, 1, 2, 3 };
        std::transform(in.begin(), 
                       in.end(), 
                       std::back_inserter(out),
                       $foo);
        assert(expected == out);
    }
    {
        std::pair<int, int> p = { 0, 1};
        assert($first(p) == 0);
    }
    {
        std::vector<std::pair<int, int> > in { {0, 0}, {1,-1 }, {2, -2}, {3,-3} };
        std::vector<int> out;
        std::vector<int> expected = { 0, 1, 2, 3 };
        std::transform(in.begin(), 
                       in.end(), 
                       std::back_inserter(out),
                       $first);
        assert(expected == out);

    }
    {
        std::vector<std::pair<int, int> > in { {0, 0}, {1,-1 }, {2, -2}, {3,-3} };
        std::vector<int> out;
        std::vector<int> expected = { 0, -1, -2, -3 };
        std::transform(in.begin(), 
                       in.end(), 
                       std::back_inserter(out),
                       $second);
        assert(expected == out);
    }
    {
        std::vector<int> in { 0, 0, 0, 0, 0 };
        std::vector<int> expected { 3, 3, 3, 3, 3 };

        using namespace std::placeholders;
        std::for_each(in.begin(), 
                      in.end(), 
                      std::bind($(=), _1, 3));
        assert(expected == in);
    }

    {
        int i = 42;
        assert($(+)(5, 3) == 8);
        assert($(*)(5, 3) == 15);
        assert($(*)(&i) == 42);
        assert($(<)(0,1) == true);
        assert($(!)(false) == true);
        assert($(++)(i) == 43);
    }
    {
        std::vector<int> v = { 0, 1, 2, 3 };
        int i = std::accumulate(v.begin(), v.end(), 0, $(+));
        assert(i == 6);
    }
    {
        std::vector<std::array<int, 4> > in {
            {{1, 2, 3, 4 }},
            {{2, 4, 6, 8 }}, 
            {{3, 6, 9, 12 }},
            {{4, 8, 16, 32}} };
        std::vector<int> indices = { 0, 1, 2, 3 };
        std::vector<int> out;
        std::vector<int> expected = { 1, 4, 9, 32 };
        std::transform(in.begin(), 
                       in.end(), 
                       indices.begin(), 
                       std::back_inserter(out),
                       $([]));
        assert(expected == out);
        X x { 42 };
        assert($(())($foo, x) ==  42);
    }
    {
        std::vector<std::pair<int, int> > in { {0, 0}, {1,-1 }, {2, -2}, {3,-3} };
        std::vector<int> out;
        std::vector<int> expected = { 0, -1, -2, -3 };
        $transform(in.begin(), 
                   in.end(), 
                   std::back_inserter(out),
                   $second);
        assert(expected == out);
    }

}



