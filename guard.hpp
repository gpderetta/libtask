#ifndef GPD_GUARD_HPP
#define GPD_GUARD_HPP
namespace gpd {
namespace details {

template<class F>
struct guard_t {
    guard_t (F f) : dismissed(false), f(f) {}
    bool dismissed;
    F f;
    void dismiss() { dismissed = true; }
    ~guard_t() { if(!dismissed) f(); }
};

}

/**
 * Returns a guard object that will invoke 'f' on destruction, unless
 * dismiss() is called on it.
 *
 * Note, for correctness, the result must be bound to an lvalue.
 */
template<class F>
details::guard_t<F> guard(F f) { return {f}; }

}
#endif
