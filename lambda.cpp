#include <utility>

#define nargs(...)  nargs_(__VA_ARGS__,rseq_n())
#define nargs_(...) arg_n(__VA_ARGS__)
#define arg_n( _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, N,...) N
#define rseq_n() 10, 9,8,7,6,5,4,3,2,1,0
#define xcat(a, b) a##b
#define cat(a, b) xcat(a,b)

template<class I>
struct wrap {

    template<class... Args>
    constexpr auto operator()(Args&&... args) const  {
        using F = std::result_of_t<I(Args&&...)>;
        return F()(std::forward<Args>(args)...);
    }

    template<class R, class... Args>
    using _fn = R(Args...);

    template<class R, class... Args>
    constexpr operator _fn<R, Args...>*() const { return &wrap::_call; }
    
    template<class R, class... Args>
    constexpr operator _fn<R, Args...>&() const { return &wrap::_call; }
    
private:
    template<class... Args>
    static constexpr auto _call(Args... args)  {
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

//noexcept(noexcept(__VA_ARGS__))               
#define body(...) \
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
constexpr void foo(integer<i>){}

template<class R, class F, class... T>
struct table_t {
    using fn =  R(void*, F& f);
    fn* table[sizeof...(T)];
};

template<class R, class... T, class F>
constexpr table_t<R, F, T...> table { $(cast)($(ptr, f)(f(*static_cast<std::decay_t<decltype(cast)> >(ptr))))((T*)0) ... };

template<class R, class... T, class F>
constexpr R cast(void * p, int index, F f) {
    using fn =  R(void*, F& f);
    //f(static_cast<T*>(ptr))
    // constexpr fn* table[] = {
    //     $(cast)($(ptr, f)(f(*static_cast<std::decay_t<decltype(cast)> >(ptr))))((T*)0) ...
    // };                          
    return table<R, F, T...>[index](p, f);
}

constexpr int bar(int k, int j) { return k + j;}
int main()
{
    {
        constexpr auto f = $(x)(x + 10);
        constexpr int k = f(11);
        static_assert(k == 21, "");
    }
    {
        constexpr int i  = $(x)(x + 1)(42);
        static_assert(i == 43, "");
        constexpr integer<i> x {};
        foo(x);
    }
    {
        constexpr auto f = $(x, y)(bar(y, x) + bar(2*x, 2*x));
        constexpr int i = f(1,2);
        static_assert(i == 7, "");
    }
    {
        constexpr int j  = $(x, y)(x + y)(42,43) + $(x, y, z)(x + y * z)(42,43,22);
        static_assert(j == 1073, "");

    }
    {
        constexpr int(* p)(int)   = $(x)(x + 10);
        static_assert(p(10) == 20, "");

    }
    {
        constexpr int(& p)(int)   = $(x)(x + 10);
        static_assert(p(10) == 20, "");

    }
    {
        constexpr int j = 0;
        constexpr int x = cast<int, int, float, double>(const_cast<int*>(&j), 0, $(x)(0));
    }

}



  
  
