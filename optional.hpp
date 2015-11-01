#ifndef GPD_OPTIONAL_HPP
#define GPD_OPTIONAL_HPP
#include "variant.hpp"

namespace gpd {

struct bad_optional_value : std::exception {};
template<class T>
struct optional : variant<T> {
    using 
    explicit operator bool() const { return !this->is(ptr<empty>); }
    T& value() { return !*this ? throw bad_optional_value : this->get(ptr<T>); }
    T&& value() && { return !*this ? throw bad_optional_value : this->get(ptr<T>); }
    const T& value() const {
        return !*this ? throw bad_optional_value : this->get(ptr<T>);
    }

};


}
#endif
