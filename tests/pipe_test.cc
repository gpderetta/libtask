#include "pipe.hpp"
#include <cassert>
#include <vector>
#include "continuation.hpp"
#include "macros.hpp"
#include <boost/regex.hpp>
#include <boost/range.hpp>
#include <boost/algorithm/string/case_conv.hpp>
int square(int x) { return x*x; };

int main() {
    using gpd::stage; using gpd::plumb;
    using gpd::ipipe; using gpd::opipe;

    {
        auto ret = 10 | stage(square) ;
        assert(ret == 100);
    }
    {
        auto ret = 10 | stage(square) | stage(square);
        assert(ret == 100*100);
    }
    {
        auto ret 
            = 10      
            | stage([](int i) { return i+1; }) 
            | stage([](int i) { return i*3; }) 
            | stage([](int i) { return i-1; }) 
            ;
                
            assert(ret == 32);
    }
    {
        std::vector<int> x {1,2,3};
        auto ret 
            = x                                       
            | stage([](std::vector<int> x) { return x[0]; }) 
            | stage([](int i) { return i*3; })               
            | stage([](int i) { return i-1; })
            ;
        
            assert(ret == 2);
    }
    {
        std::vector<int> x {1,2,3};

        auto ret 
            = x       
            | plumb([](opipe<int> output,
                      std::vector<int> input) { 
                       for(auto& x : input) output(x*2);
                       return output;
                   })
            ;
        
        std::vector<int> exp_y {2,4,6};
        std::vector<int> y(begin(ret), end(ret));
        assert(y == exp_y);
    }
    {
        std::vector<int> x {1,2,3};
        auto ret 
            = x       
            | plumb([](opipe<int> output,
                      std::vector<int> input) { 
                       for(auto& x : input) output(x*2);
                       return output;
                   }) 
            | plumb([](opipe<int> output,
                      ipipe<int> input) { 
                       for(auto x : input) output(x*3);
                       return output;
                   })
            ;

        
        std::vector<int> exp_y {6,12,18};
        std::vector<int> y(begin(ret), end(ret));
        assert(y == exp_y);
    }
    {
        std::vector<int> x {1,2,3};
        auto ret 
            = x       
            | plumb([](gpd::continuation<void(int)> output,
                      std::vector<int> input) { 
                       for(auto& x : input) output(x*2);
                       return output;
                   }) 
            | plumb([](gpd::continuation<void(int)> output,
                      gpd::continuation<int()> input) { 
                       for(auto x : input) output(x*3);
                       return output;
                   })
            ;

        
        std::vector<int> exp_y {6,12,18};
        std::vector<int> y(begin(ret), end(ret));
        assert(y == exp_y);
    }
    {
        std::string x = "Hello tHis is a Long string with Some capitalized woRds and sOme iN cAmmel Case";
        auto ret 
            = x       
            | plumb([](opipe<std::string&&> output,
                      std::string&& input) { 
                       boost::regex r("([A-Za-z]+)");
                       boost::sregex_token_iterator b(input.begin(),
                                                      input.end(), r), e;
                       std::move(b, e, begin(output));
                       return output;
                   }) 
            | plumb([](opipe<std::string&&> output,
                      ipipe<std::string&&> input) { 
                       boost::regex r("^[A-Z].*");
                       for(auto&& x: input) {
                           if(!boost::regex_match(x, r))
                               output(std::move(x));
                       }
                       return output;
                   }) 
            | plumb([](opipe<std::string&&> output,
                      ipipe<std::string&&> input) { 
                       for(auto x : input) {
                           output("\""+boost::algorithm::to_upper_copy(x)+"\" ");
                           output("\""+boost::algorithm::to_lower_copy(x)+"\" ");
                       }
                       return output;
                   })
            ;

        for(auto x: ret) {
            std::cout<<x<<"\n";
        }
    }

}
