#ifndef GPD_DYNAIMIC_DETAILS_HPP
#define GPD_DYNAIMIC_DETAILS_HPP
namespace gpd {
template<class> struct dynamic;

template<class Interface, class S>
struct wrap { using type = typename Interface::template wrap<S>; };

template<class Interface, class S>
using wrap_t = typename wrap<Interface, S>::type;

namespace details {

template<class T> 
struct outer
{
    using self_t = dynamic<T>;
    const self_t& self() const
    { return *static_cast<const self_t*>(this); }
    self_t& self()
    { return *static_cast<self_t*>(this); }

    
    enum { buffer_size = 8 };
    decltype(auto) target() const { return static_cast<const dynamic<T>*>(this)->dynamic_get(); }
    decltype(auto)  target() { return static_cast<dynamic<T>*>(this)->dynamic_get(); }
};


template<class Interface>
struct dynamic_ops : Interface
{
    virtual void dynamic_clone(void *) const = 0;
    virtual void dynamic_move(void *) = 0;
    virtual ~dynamic_ops() {};
};

template<class T, class Interface>
struct holder;

template<class T, class Interface>
struct inner : dynamic_ops<Interface> {
    using self_t = details::holder<T, Interface>;

    const self_t& self() const
    { return *static_cast<const self_t*>(this); }
    self_t& self()
    { return *static_cast<self_t*>(this); }

    const auto& target() const
    { return self().x; }
    auto& target()
    { return self().x; }


};



template<class T, class Interface>
struct holder : wrap_t<Interface, inner<T, Interface> > {

    holder(const T& x) : x(x) {  }
    holder(T&& x) : x(std::move(x)) { }
            
    void dynamic_clone(void* buffer) const {  new (buffer) holder(x); }
    void dynamic_move(void * buffer) { new (buffer) holder(std::move(x)); }
    T x;
};

template<class T, class Interface>
struct move_holder : wrap_t<Interface, inner<T, Interface> > {
    move_holder(T&& x) : x(std::move(x)) { }
            
    void dynamic_clone(void*) const {  assert("unreachable" && false); }
    void dynamic_move(void * buffer) { new (buffer) move_holder(std::move(x)); }
    T x;
};

template<class T, class Interface>
struct holder_ptr;
template<class T, class Interface>
struct inner_ptr : dynamic_ops<Interface> {

    using self_t = details::holder_ptr<T, Interface>;

    const self_t& self() const
    { return *static_cast<const self_t*>(this); }
    self_t& self()
    { return *static_cast<self_t*>(this); }

    const auto& target() const
    { return *self().ptr; }
    auto& target()
    { return *self().ptr; }
};

template<class T, class Interface>
struct holder_ptr : wrap_t<Interface, inner_ptr<T, Interface> > {
    holder_ptr(const T& x) : ptr(new T(x)) {}
    holder_ptr(T&& x) : ptr(new T(std::move(x))) {}
    holder_ptr(std::unique_ptr<T> ptr) : ptr(std::move(ptr)) {}
    void dynamic_clone(void* buffer) const { new (buffer) holder_ptr(*ptr); }
    void dynamic_move(void * buffer) { new (buffer) holder_ptr(std::move(ptr)); }
    std::unique_ptr<T> ptr;
};

template<class T, class Interface>
struct move_holder_ptr : wrap_t<Interface, inner_ptr<T, Interface> > {
    move_holder_ptr(T&& x) : ptr(new T(std::move(x))) {}
    move_holder_ptr(std::unique_ptr<T> ptr) : ptr(std::move(ptr)) {}
    void dynamic_clone(void* ) const { assert("unreachable" && false); }
    void dynamic_move(void * buffer) { new (buffer) move_holder_ptr(std::move(ptr)); }
    std::unique_ptr<T> ptr;
};


}}
#endif
