#include "switch.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
struct noncopyable {
    noncopyable(const noncopyable&) = delete;
    noncopyable(noncopyable&&rhs) : live(rhs.live) { rhs.live = false; }
    noncopyable() : live(true) {}
    bool live;
};

int main() {
    using namespace gpd;
    {
        auto c = callcc([](continuation<void(int)> c) { 
                c(42);
                return c;
            });
        assert(c.get() == 42);
        c();
    }
    {
        auto c = callcc([](continuation<void()> c) { 
                c();
                return c;
            });
        assert(c);
        c();
    }

    {
        auto c = callcc([](continuation<void(noncopyable)> c) { 
                c(noncopyable());
                return c;
            });
        noncopyable nc = c.get();
        (void)nc;
        c(); 
    }
    {
        auto c = callcc([](continuation<int()> c) {
                assert(c().get() == 42);
                return c;
            });
        c(42);
    }
    {
        auto c = callcc([](continuation<noncopyable()> c) {
                noncopyable nc = c().get();
                (void)nc;
                return c;
            });
        c(noncopyable());
    }
    {
        int i = 42;
        auto c = callcc([=](continuation<void()> x) {
                assert(i == 42);
                x();
                assert(i == 42);
                return x;
            });
        i = 99;
        c();
    }
    {
        int i = 42;
        auto c = callcc([&](continuation<void()> x) {
                assert(i == 42);
                x();
                assert(i == 99);
                return x;
            });
        i = 99;
        c();
    }
    {
        int i = 42;
        auto c = callcc([&](continuation<int()> x) {
                assert(i == 42);
                //*x;
                x();
                x.get();
                assert(i == 99);
                return x;
            });
        i = 99;
        //int x;
        c(i);
    }
    {
        int i = 0;
        auto c = callcc([&](continuation<int&()> x) {
                assert(&x().get() == &i);
                return x;
            });
        c(i);
    }
    {
        int i = 0;
        auto c = callcc([&](continuation<void(int&)> x) {
                x(i);
                assert(i == 42);
                return x;
            });
        assert(&c.get() == &i);
        c.get() = 42;
        c();
    }
    {
        int * ip = 0;
        auto c = callcc([&](continuation<void(int&)> x) {
                int i = 0;
                ip = &i;
                x(i);
                assert(i == 42);
                return x;
            });
        assert(&c.get() == ip);
        c.get() = 42;
        c();
    }

    {
        auto c = callcc([&](continuation<void(int, int)> x) {
                x(0, 1);
                return x;
            });
        int a, b;
        std::tie(a, b) = c.get();
        assert(a==0 && b==1);
        c();
    }
    {
        auto f = [](continuation<int(int, std::string,
                                     std::string&&, noncopyable&&)> c) { 

            for(int i = 0; i<100 ; ++i) {
                noncopyable nc;
                int j = c(i,"hello", "world", std::move(nc)).get();
                assert(nc.live == false);
                assert(i == j); 
            }
            return c;
        };
        int j = -1;
        for(auto c = callcc(f) ; c ; c(j)) {
            int i = std::get<0>(c.get());
            {
                // XXX  std::get won't move by default because
                // it does not preserve rvalue references. GCC bug?
                std::string s  = xget<1>(c.get());
                std::string s2 = xget<2>(c.get());               
                noncopyable n  = xget<3>(c.get());

                assert(n.live == true);
                assert(std::get<3>(c.get()).live == false);
                assert(s == "hello");
                assert(std::get<2>(c.get()).empty());           
            }
            j++;
            assert(i == j);
        }
        assert(j == 99);
    }
    {
        std::vector<int> v = { 0,1,2,3,4,5,6,7,8,9,10 };
        auto f = [=](continuation<void(int)> c) { 
            return std::for_each(v.begin(), v.end(), std::move(c));
        };
    
        for(auto i = callcc(f) ; i ; i()) {
            int x  = i.get();
            assert(v[x] == x);
        }
    }
    {
        auto set_difference = [](std::vector<int> & a,
                                 std::vector<int> & b) {
            return callcc([&](continuation<void(int&)> c) {
                std::set_difference(a.begin(), a.end(),
                                    b.begin(), b.end(),
                                    begin(c));
                return c;
                });
        };

        std::vector<int> a = { 0,1,2,3,4,5 };
        std::vector<int> b = { 3,4,5,6,7,8 };
        std::vector<int> expected_result = { 0,1,2 };

        auto range = set_difference(a,b);
        std::vector<int> result(begin(range), end(range));
        assert(result == expected_result);
    }
    {
        auto merge = [](std::vector<int> & a,
                        std::vector<int> & b) {
            return callcc([&](continuation<void(int&)> c) {
                    std::merge(a.begin(), a.end(),
                               b.begin(), b.end(),
                               begin(c));
                    return c;
                });
        };

        std::vector<int> a = { 0,2,4,6,8,10 };
        std::vector<int> b = { 1,3,5,7,9,11 };

        int i = 0;
        for(auto& x: merge(a,b)){
            assert(x == i++);
        }
    }
    {

        std::vector<int> a = { 0,1,2,3,4,5,6,7,8,9,10 };

        auto filtered = callcc([&](continuation<void(int&)> c){
                    std::remove_copy_if(a.begin(), a.end(), 
                                        begin(c),
                                        [](int x) { return x%2 == 1; });
                    return c;
            });
        std::vector<int> result(begin(filtered), end(filtered));
        std::vector<int> expected_result = { 0,2,4,6,8,10 };
        assert(result == expected_result);
    }

    {
        typedef continuation<> task;
        std::vector<task> cvector;
        static const int  max = 10;

        for(int i = 2; i < max; ++i)
            cvector.push_back
                (callcc
                 ([=](task x) mutable 
                  { 
                      while(i > 0) {
                          x(); 
                          i--;
                      }
                      return x;
                  }));

        while(!cvector.empty()) {
            auto new_end = std::remove_if
                (cvector.begin(), cvector.end(), 
                 [](task& x) { return x.terminated(); }); 
            cvector.erase(new_end, cvector.end());

#pragma omp parallel for
            for(int i = 0; i < (int)cvector.size(); ++i) {
                assert(cvector[i]);
                cvector[i]();
            }
        }
    }
    {
        int caught = 0;
        {
            auto f = callcc([&](continuation<void()> c){
                    try {
                        c();  
                        assert(false && "unreachable");
                    } catch(...) {
                        caught++;
                        throw;
                    }
                    assert(false && "unreachable");
                    return c;
                });
            try {
                splice(std::move(f), [] { throw 42; } );
            } catch(int i) {
                assert(i == 42);
                caught++;
            }
        }

        assert(caught == 2);
    }
    {
        std::tuple<int, int> ret{42, 42};
        
        auto f = callcc([&](continuation<std::tuple<int,int>()> c){
                auto i = c().get();  
                assert(i == ret);
                return c;
            });
            
        splice(std::move(f), [&] () -> decltype(ret)&  { return ret; } );
    }
    {
        std::tuple<int> ret{42};
        
        auto f = callcc([&](continuation<int()> c){
                auto i = c().get();  
                assert(i == std::get<0>(ret));
                return c;
            });
            
        splice(std::move(f), [&] () -> decltype(ret)&  { return ret; } );
    }
    {
        auto f = callcc([&](continuation<void()> c){
                c();  
                return c;
            });
            
        splicecc(std::move(f), [&] (continuation<void()> x)   { return x; } );
    }
    {
        auto f = callcc([&](continuation<int()> c){
                c();  
                return c;
            });
            
        splicecc(std::move(f), [&] (continuation<int()> x)   { return x; } );
    }
    {
        auto f = callcc([&](continuation<void()> c){
                c();  
                return c;
            });
        signal_exit(std::move(f));
    }
    {
        // an experiment in continuation signature mutation
        continuation<float()> c2;
        auto f1 = callcc([&](continuation<int()> c1){
                int i = c1().get(); 
                assert(i == 42);
                c1(); // c1 no longer valid after this call
                assert(!c1);
                float f = c2().get();
                assert(f == 42);
                return std::move(c2);
            });
        
        f1(42);
        assert(f1);
        auto f2 = splicecc_ex<void(float)>
            (std::move(f1), [&] (continuation<float()> x)   
             { c2 = std::move(x);
               return continuation<int()>(); });
        f2(42.);
    }
    {
        // an experiment in continuation signature mutation, take 2
        auto f1 = callcc([&](continuation<int()> c1){
                int i = c1().get(); 
                assert(i == 42);
                try {
                    c1();
                } catch(continuation<float()>&& c2) {
                    assert(!c1);
                    float f = c2().get();
                    assert(f == 42);
                    return std::move(c2);
                }
                abort();
            });
        
        f1(42);
        assert(f1);
        auto f2 = splicecc_ex<void(float)>
            (std::move(f1), [&] (continuation<float()> x) -> continuation<int()>
             { throw std::move(x); });
        f2(42.);
    }



}
