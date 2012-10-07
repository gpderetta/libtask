#include "forwarding.hpp"
#include <cassert>
#include <iostream>
#include <functional>
using namespace gpd;

placeholder<0> $1;
placeholder<1> $2;
placeholder<2> $3;

template<class F, class T>
void foo(F f, T&& t)
{
    auto x = details::transform0(f, details::indices(t), std::forward<T>(t));

}

struct noncopyable {
    noncopyable(const noncopyable&) = delete;
    noncopyable(noncopyable&&rhs) : live(rhs.live) { rhs.live = false; }
    noncopyable() : live(true) {}
    bool live;
};

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
    {
        auto bound = bind([](int x, int y){
                assert(x == 1000);
                assert(y == 100);
                return x / y; }, 1000, $1);
        
        assert(bound(100) == 10);
    }
    {
        int o_x = 99, o_y = 100;
        auto bound = bind([&](int x, int& y){
                assert(o_x == x);
                assert(&o_y == &y);
            }, o_x, $1);
        
        bound(o_y);
    }
    {
        int o_x = 99, o_y = 100;
        auto bound = gpd::bind([&](int&& x, int& y){
                assert(&o_x != &x);
                assert(&o_y == &y);
            }, o_x, $1);
        
        bound(o_y);
    }            
    {
        int o_x = 99, o_y = 100;
        auto bound = gpd::bind([&](int& x, int& y){
                assert(&o_x == &x);
                assert(&o_y == &y);
            }, std::ref(o_x), $1);
        
        bound(o_y);
    }            
    {
        noncopyable y;
        auto bound = gpd::bind([](int, noncopyable y){  
                assert(y.live);
            }, 0, $1);
        
        bound(std::move(y));
    }            
    {
        // FIXME, this should also compile with a simple reference,
        // not only rvalue reference
        noncopyable x, y, z;
        auto bound = gpd::bind([](noncopyable&& x, noncopyable y){  
                assert(x.live);
                assert(y.live);
            }, std::move(x), $1);
        
        bound(std::move(y));
        bound(std::move(z));
    }            
    {
        noncopyable x, y, z;
        auto bound = gpd::bind([](noncopyable, noncopyable y){  
                //assert(x.live);
                assert(y.live);
            }, std::move(x), $1);
        
        bound(std::move(y));
        // FIXME: this test should not compile: on the second call
        // x will have already been moved
        bound(std::move(z));
    }            

}
