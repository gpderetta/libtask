#include "overloads.hpp"
#include <cassert>
using namespace gpd;


int main() {
    auto f = match(
         []              { return 1; }
        ,[](int)         { return 2; }
        ,[](float)       { return 3; }
        ,[](float, int ) { return 4; }
        );
    assert(f() == 1);
    assert(f(1) == 2);
    assert(f(1.f) == 3);
    assert(f(1.f, 0) == 4);
}
