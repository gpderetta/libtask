#include "dynamic.hpp"
#include <iostream>
using namespace gpd;

int foo(int i)  { return i; }

int main()
{
    static int construct = 0;
    static int copy = 0;
    static int move = 0;
    static int destroy = 0;
    static int call = 0;

    using function = dynamic<callable<void()> >;
    {

        function x = [] { call++; };
        x();
        assert(call == 1);
        std::array<long, 4> arr { 1, 2, 3, 4 };
        x = [y = arr] {
            assert(y[0] == 1 && y[1] == 2 && y[2] == 3 && y[3] == 4);
            call++;
        };

        x();
        assert(call == 2);
    
        {
            auto y = x;
            y();
            assert(call == 3);
        }
    
        struct ff {
            ff() { construct++; }
            ff(const ff&) { copy++; }
            ff(ff&&) { move++; }
            void operator()() const { call ++; }
            ~ff() { destroy++; }
        };

        x = ff{};
        assert(construct == 1);
        assert(move == 1);
        assert(copy == 0);
        assert(destroy == 1);
        x();
        assert(move == 1);
        assert(copy == 0);
        assert(call == 4);
        assert(destroy == 1);

        {
            {
                auto z = x;
                assert(copy == 1);
                assert(move == 1);
                z();
                assert(call == 5);
            }
            assert(destroy == 2);
        }

        auto f = ff{};
        x = f;
        assert(construct == 2);
        assert(destroy == 3);
        assert(move == 1);
        assert(copy == 2);
        x.dynamic_reset();
    
        assert(destroy == 4);
    }

    call = 0;
    construct = 0;
    move = 0;
    destroy = 0;
    using rfunction = dynamic<callable<int(int)&&> >;

    {
        rfunction x = [] (int i){ call++; return i; };

        auto ret = x(9);
        assert(ret == 9);
        assert(call == 1);

        struct ff {
            ff() { construct++; }
            ff(const ff&) = delete;
            ff(ff&&) { move++; }
            int operator()(int i) const { call ++; return i;}
            ~ff() { destroy++; }
        };

        x = ff{};
        assert(construct == 1);
        assert(move == 1);
        assert(destroy == 1);
        ret = x(10);
        assert(ret == 10);

        assert(call == 2);

        x = std::unique_ptr<ff> (new ff);
        assert(move == 1);
        assert(destroy == 2);
        ret = x(11);
        assert(ret == 11);
        assert(call == 3);

        {
            auto y = std::move(x);
            assert(move == 1);
        }
        {
            x = foo;
            ret = x(15);
            assert(ret == 15);
        }
        assert(destroy == 3);

    }

}


