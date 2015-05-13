#include "variant.hpp"

// #include <utility>
#include <string>
#include <iostream>

template<int i>
struct x{ 
    x() { std::cerr << i << ":x()\n"; }
    ~x() { std::cerr << i << ":~x()\n"; }
    x(const x&) { std::cerr << i <<":x(const x&)\n"; }
    x(x&&) { std::cerr << i <<":x(x&&)\n"; }
    void operator=(x&&) { std::cerr << i << ":operator=(x&&)\n"; }
    void operator=(const x&) { std::cerr <<i << ":operator=(const x&)\n"; }
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
        //S s;
//    x<0> x0;
        //x<1> x1;
//    s = x0
        { S s = S(x<1>());}
        { S s = x<1>();}
        { S s {x<1>()};}
        { S s ; s = x<1>();}
        { S s ; s = S{x<0>()};}
        { S s ; s = x<0>();}
        { 
            using S = variant<int, double, float, std::string>;
            for (auto x: { S(0), S(1.), S(2.f), S("hello")})
                x.visit(
                    [](int x)    { std::cerr << "int " << x << "\n";},
                    [](double x) { std::cerr << "double " << x <<"\n";},
                    [](auto x)   { std::cerr << "other " << x << "\n";}
                    );
        }
        {
            using S = variant<int, double, float, std::string>;
            S x;
            variant<int*, double*, float*, std::string*> y = 
                x.map([](auto& x) { return &x; });
        }
        {
            using S = variant<int, double, float, std::string>;
            S x;
            // bool y = 
            //     x.visit([](auto& x) { return x == x; });
            (void)y;
        }

    }        
}

using V = gpd::variant<double, int, std::string, short, float>;
int foo_test(V x) {
    int y = x.visit(
        [](int x)  { return x; },
        [](auto&&)    { return 0; }
        );
    return y;
}
