#include "forwarding.hpp"
#include <cassert>
#include <iostream>
using namespace gpd;

placeholder<0> $1;
placeholder<1> $2;
placeholder<2> $3;

template<class F, class T>
void foo(F f, T&& t)
{
    auto x = details::transform0(f, details::indices(t), std::forward<T>(t));

}

int main() {
    auto ret = forward([](int a, int b)
                       { 
                           assert(a == 42);
                           assert(b == 77);
                           return 99;
                       },
                       pack(42, 77));
    assert(ret == 99);

    assert((details::replacer(pack(88))(42) == 42));
    assert((details::replacer(pack(88))($1) == 88));
    assert((details::replacer(pack(88, 92))($1) == 88));
    assert((details::replacer(pack(88, 92))($2) == 92));



    auto args = std::make_tuple($2, $1);

    [](std::tuple<int&&, int&&> tupret) {
        assert(std::get<0>(tupret) == 92);
        assert(std::get<1>(tupret) == 88);
    }( transform(details::replacer(pack(88, 92)), args));

    [](std::tuple<int&&, int&&, int&&> tupret) {
        assert(std::get<0>(tupret) == 92);
        assert(std::get<1>(tupret) == 88);
        assert(std::get<2>(tupret) == 44);
    }(replace_placeholders(std::make_tuple($2, $1, 44), 88, 92));

    ret = forward([](int a)
                  { 
                      assert(a == 666);
                      return 100;
                  },
                  replace_placeholders(std::make_tuple(666), 
                                        0));


    ret = forward([](int a, int b, int c)
                  { 
                      assert(a == 42);
                      assert(b == 77);
                      assert(c == 666);
                      return 100;
                  },
                  replace_placeholders(std::make_tuple($2, $1, 666), 
                                        77, 42));
    assert(ret == 100);
 
    ret = forward([](int&& a, int&& b, int&& c)
                  { 
                      assert(a == 42);
                      assert(b == 77);
                      assert(c == 666);
                      return 100;
                  },
                  replace_placeholders(pack($2, $1, 666), 
                                        77, 42));
    assert(ret == 100);

    auto bound = bind([](int x, int y){
            assert(x == 1000);
            assert(y == 100);
            return x / y; }, 1000, $1);

    assert(bound(100) == 10);
            
}
