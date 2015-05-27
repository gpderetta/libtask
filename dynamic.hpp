#ifndef GPD_DYNAIMIC_HPP
#define GPD_DYNAIMIC_HPP
#include <cstddef>
#include <utility>
#include <memory>
#include <cassert>
#include <type_traits>
#include "details/dynamic_details.hpp"


namespace gpd
{

template<class Interface, class S>
struct wrap;

template<class>
struct buffer_size { enum { value = 24 }; };

template<class Interface, class base>
struct dynamic_base : base {
    using interface = details::dynamic_ops<Interface>;

    interface &dynamic_get() {
        return *reinterpret_cast<interface*>(dynamic_buffer);
    }
    
    const interface& dynamic_get() const {
        return *reinterpret_cast<const interface*>(dynamic_buffer);
    }

    void dynamic_reset() noexcept {
        if (vtable)
            dynamic_get().~interface();
        vtable = 0;
    }

protected:
    dynamic_base() : vtable (0) {}
    ~dynamic_base() { dynamic_reset(); }
    
    template<bool move, class T>
    void dynamic_unsafe_allocate(T&&x)
    {
        using T2 = std::decay_t<T>;

        using holder_inline = std::conditional_t<
            move,
            details::move_holder<T2, Interface>,
            details::holder<T2,Interface> >;

        using holder_ptr = std::conditional_t<
            move,
            details::move_holder_ptr<T2, Interface>,
            details::holder_ptr<T2,Interface> >;
        
        using holder = std::conditional_t<
            ( sizeof(holder_inline) <= this->buffer_size ),
            holder_inline,      
            holder_ptr>;
        
        try {
            new(this->dynamic_buffer) holder(std::forward<T>(x));
        } catch(...) { this->dynamic_reset(); throw; }
        assert(this->vtable);
    }    

    template<bool move, class T>
    void dynamic_unsafe_allocate(std::unique_ptr<T> x) noexcept
    {
        using holder = std::conditional_t<
            move,
            details::move_holder_ptr<T, Interface>,
            details::holder_ptr<T,Interface> >;
                
        new(this->dynamic_buffer) holder(std::move(x));
        assert(this->vtable);
    }    

    enum { buffer_size = gpd::buffer_size<Interface>::value };
    union {
        std::uintptr_t vtable;
        alignas(std::max_align_t) char dynamic_buffer [buffer_size];        
    };
   
};

template<class Interface, class base, bool = !std::is_copy_constructible<base>::value >
struct dynamic_ops : dynamic_base<Interface, base> {
    dynamic_ops()  {}

    template<class Rhs>
    dynamic_ops(Rhs&& rhs) {
        this->template dynamic_unsafe_allocate<true>(std::forward<Rhs>(rhs));
    }

    template<class Rhs>
    dynamic_ops(std::unique_ptr<Rhs> rhs) {
        this->template dynamic_unsafe_allocate<true>(std::move(rhs));
    }

    dynamic_ops(const dynamic_ops& rhs) = delete;
    
    dynamic_ops(dynamic_ops&& rhs) {
        try {
            rhs.dynamic_get().dynamic_move(this->dynamic_buffer);
        } catch(...) { this->dynamic_reset(); throw; }
    }
    
    template<class T>
    dynamic_ops& operator=(T&& x) {
        this->dynamic_reset();
        this->template dynamic_unsafe_allocate<true>(std::forward<T>(x));
        return *this;
    }

    template<class T>
    dynamic_ops& operator=(std::unique_ptr<T> x) {
        this->dynamic_reset();
        this->template dynamic_unsafe_allocate<true>(std::move(x));
        return *this;
    }

    dynamic_ops& operator=(dynamic_ops const& rhs) = delete;
    
    dynamic_ops& operator=(dynamic_ops&& rhs) {
        this->dynamic_reset();
        try {
            rhs.dynamic_get().dynamic_move(this->dynamic_buffer);
        } catch(...) { this->dynamic_reset(); throw; }
        return *this;
    }
};

template<class Interface, class base>
struct dynamic_ops<Interface, base, false> : dynamic_base<Interface, base> {

    dynamic_ops()  {}

    template<class Rhs>
    dynamic_ops(Rhs&& rhs) {
        this->template dynamic_unsafe_allocate<false>(std::forward<Rhs>(rhs));
    }

    template<class Rhs>
    dynamic_ops(std::unique_ptr<Rhs> rhs) {
        this->template dynamic_unsafe_allocate<false>(std::move(rhs));
    }

    dynamic_ops(const dynamic_ops& rhs) {
        try {
            rhs.dynamic_get().dynamic_clone(this->dynamic_buffer);
        } catch(...) { this->dynamic_reset(); throw; }
    }

    dynamic_ops(dynamic_ops&& rhs) {
        try {
            rhs.dynamic_get().dynamic_move(this->dynamic_buffer);
        } catch(...) { this->dynamic_reset(); throw; }
    }
    
    template<class T>
    dynamic_ops& operator=(T&& x) {
        this->dynamic_reset();
        this->template dynamic_unsafe_allocate<false>(std::forward<T>(x));
        return *this;
    }

    template<class T>
    dynamic_ops& operator=(std::unique_ptr<T> x) {
        this->dynamic_reset();
        this->template dynamic_unsafe_allocate<false>(std::move(x));
        return *this;
    }

    dynamic_ops& operator=(dynamic_ops const& rhs) {
        this->dynamic_reset();
        try {
            rhs.dynamic_get().dynamic_clone(this->dynamic_buffer);
        } catch(...) { this->dynamic_reset(); throw; }
        return *this;
    }

    dynamic_ops& operator=(dynamic_ops&& rhs) {
        this->dynamic_reset();
        try {
            rhs.dynamic_get().dynamic_move(this->dynamic_buffer);
        } catch(...) { this->dynamic_reset(); throw; }
        return *this;
    }
};

template<class Interface>
struct dynamic : dynamic_ops<Interface, wrap_t<Interface, details::outer<Interface> > > {
    using ops = dynamic_ops<Interface, wrap_t<Interface, details::outer<Interface> > >;
    using ops::ops;
    using ops::operator=;
};

template<class Sig>
struct callable;    

template<class R, class... Args>
struct callable<R(Args...)> {
    virtual R operator()(Args...) const = 0;
    virtual R operator()(Args...) = 0;
     
    template<class S> struct wrap : S {
        R operator()(Args... x) {
            return this->target()(x...);
        }

        R operator()(Args... x) const {
            return this->target()(x...);
        }
    };
};

template<class R, class... Args>
struct callable<R(Args...)&&> {
    virtual R operator()(Args...) = 0;
    virtual R operator()(Args...) const = 0;
 
    template<class S> struct wrap : S {
        wrap() = default;
        wrap(const wrap&) = delete;
        R operator()(Args... x) {
            return this->target()(x...);
        }

        R operator()(Args... x) const {
            return this->target()(x...);
        }
    };
};


}
#endif
