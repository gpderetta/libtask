#include <utility>

#define nargs(...)  nargs_(__VA_ARGS__,rseq_n())
#define nargs_(...) arg_n(__VA_ARGS__)
#define arg_n( _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, N,...) N
#define rseq_n() 10, 9,8,7,6,5,4,3,2,1,0
#define xcat(a, b) a##b
#define cat(a, b) xcat(a,b)

template<class I>
struct wrap {
  constexpr wrap() = default;
  template<class... Args>
    constexpr auto operator()(Args&&... args) {
    using F = std::result_of_t<I(Args&&...)>;
    return F()(std::forward<Args>(args)...);
  }
};

template<class F>
constexpr auto make(F*) {
  return wrap<F>{};
}

template<class T>
  constexpr T* ptr(T) { return nullptr; }


#define body(...) noexcept(noexcept(__VA_ARGS__))\
	{ return (__VA_ARGS__); }} _result; return _result; }))
#define head(A, B) make(true ? nullptr : ptr([] A \
	{ struct { constexpr auto operator() B body

#define $1(arg)  head((auto&& arg),(decltype(arg) arg))
#define $2(x1, x2) head((auto&& x1, auto&& x2),(decltype(x1) x1, decltype(x2) x2))
#define $3(x1, x2, x3) head((auto&& x1, auto&& x2, auto&& x3),\
							(decltype(x1) x1, decltype(x2) x2, decltype(x3) x3))

#define $(...) cat($, nargs(__VA_ARGS__))(__VA_ARGS__)

template<int> struct integer {};
template<int i>
void foo(integer<i>);
int main()
{
  constexpr auto f = $(x)(x + 10);
     
  constexpr int i  = $(x)(x + 1)(42);
  constexpr int j  = $(x, y)(x + y)(42,43);
  
  constexpr int k  = $(x, y, z)(x + y * z)(42,43,22);
  integer<i> x;
  foo(x);
}



  
  
