#include "continuation.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/timer/timer.hpp>

using namespace gpd;

template<class C>
void traverse(C& out, int depth)
{
    out(depth);
    if (depth > 0) 
        traverse(out, depth - 1);
}

int main(int argc, char*argv[])
{
    if (argc != 2) exit(1);
    int depth = boost::lexical_cast<int>(std::string(argv[1]));

    for (int i =0; i < 4; ++i)
    {
        auto c = callcc([depth](continuation<void(int)> c) { 
                traverse(c, depth); 
                return c;
            });
        
        {
            boost::timer::auto_cpu_timer t;
            while(c)
            {
                c();
            }
        }
    }
        


}
 
