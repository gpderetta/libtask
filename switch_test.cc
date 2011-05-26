#include "switch.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
struct noncopyable {
    noncopyable(const noncopyable&) = delete;
    noncopyable(noncopyable&&) {}
    noncopyable() {}
};
int main() {
    using namespace gpd;
    {
        auto f = [](continuation<std::tuple<int>(int, std::string, noncopyable&&)> c) { 
            for(int i = 0; i<100 ; ++i) {
                int j = std::get<0>(*c(i,"hello",noncopyable()));
                assert(i == j);
            }
            return std::move(c);
        };
    
        int j = -1;
        for(auto c = callcc(f) ; c ; c(j)) {
            int i;
            std::string s;
            noncopyable n;
            std::tie(i, s, n) = *c;
            
            j++;
            assert(i == j);
            assert(s == "hello");
        }
            assert(j == 99);
    }
    {
        std::vector<int> v = { 0,1,2,3,4,5,6,7,8,9,10 };
        auto f = [=](continuation<void(int)> c) { 
            std::for_each(v.begin(), v.end(), [&](int i){ c(i); });
            return std::move(c);
        };
    
        for(auto i = callcc(f) ; i ; i++) {
            int x  = std::get<0>(*i);
            assert(v[x] == x);
        }

    }

    {
        typedef continuation<> task;
        std::vector<task> cvector;
        for(int i = 4; i < 100; ++i)
            cvector.push_back(callcc([=](task x) mutable 
                                     { while(--i > 0)
                                             x(); 
                                         return std::move(x); }));
        while(!cvector.empty()) {
            cvector.erase(std::remove_if(cvector.begin(), cvector.end(), 
                                         [](task& x) {return !bool(x); }),
                          cvector.end());
//#pragma omp parallel for
            for(int i = 0; i < (int)cvector.size(); ++i) {
                assert(cvector[i]);
                cvector[i]();
            }
        }
    }
}
