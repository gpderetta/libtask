#ifndef GPD_GUARD_HPP
#define GPD_GUARD_HPP
namespace gpd {
namespace detail {
template<class F>
struct guard_t {
    guard_t (F f) : dismissed(false), f(f) {}
    bool dismissed;
    F f;
    void dismiss() { dismissed = true; }
    ~guard_t() { if(!dismissed) f(); }
};

}
template<class F>
guard_t<F> guard(F f) { return guard_t<F>(f); }

}
#endif
