#include "variant.hpp"
#include <cassert>
#include <vector>
// #include <utility>
#include <string>
#include <iostream>
std::ostream& operator<<(std::ostream& o, gpd::empty_t) { return o << "<empty>"; }
template<int i>
struct x{ 
    x() { /*std::cerr << i << ":x()\n";*/ }
    ~x() { /*std::cerr << i << ":~x()\n";*/ }
    x(const x&) { /*std::cerr << i <<":x(const x&)\n";*/ }
    x(x&&) { /*std::cerr << i <<":x(x&&)\n";*/ }
    void operator=(x&&) { /*std::cerr << i << ":operator=(x&&)\n";*/ }
    void operator=(const x&) { /*std::cerr <<i << ":operator=(const x&)\n";*/ }
};

int main()
{
    using gpd::variant;
    using S = variant<int, double, float, std::string>;
    S xx;
    xx.assign(10);
    xx = 1.0;
    xx = S("hello");
    S y (2.0f);
    xx = y;


    using S2 = variant<int, std::string>;
    S2 x2;
    xx = x2;
    S2 x3(xx);

    {
        using S = gpd::variant< x<0>, x<1>, x<2> >;
        S s;
        x<0> x0;
        x<1> x1;
        //s = x0;
        { S s = S(x<1>());}
        { S s = x<1>();}
        { S s {x<1>()};}
        { S s ; s = x<1>();}
        { S s ; s = S{x<0>()};}
        { S s ; s = x<0>();}
        {
            std::vector<int> res;
            using S = variant<int, double, float, std::string>;
            for (auto x: { S(0), S(1.), S(2.f), S("hello")})
                x.visit(
                    [&](int )    { res.push_back(0); },
                    [&](double ) { res.push_back(1); },
                    [&](auto )   { res.push_back(2); }
                    );
            assert((res == std::vector<int>{ 0, 1, 2, 2}));
        }
        {
            using S = variant<int, double, float, std::string>;
            S x;
            variant<gpd::empty_t*, int*, double*, float*, std::string*> y = 
                x.map([](auto& x) { return &x; });
            
        }
        {
            using S = variant<int, double, float, std::string>;
            S x;
            variant<int*, double*, float*, std::string*> y =
                x.map([](auto& x) { return &x; },
                      [](gpd::empty_t) { return gpd::empty_t{}; });
            
        }
        {
            using S = variant<int, double, float, std::string>;
            S x;
            bool y = 
                x.visit([](auto& x) { return x == x; });
            (void)y;
        }

   }        
}

using V = gpd::variant<double, int, std::string, short, float>;
int foo_test(V x) {
    int y = x.visit(
        [](auto&& )  { return 0; }
        ,[](int) { return 0; }
        );
    return y;
}
