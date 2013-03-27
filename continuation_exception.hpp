#ifndef GPD_CONTINUATION_EXCEPTION_HPP
#define GPD_CONTINUATION_EXCEPTION_HPP
#include "details/switch_pair_accessor.hpp"
#include <exception>
#include <utility> 
namespace gpd {

template<class Signature>
struct continuation;

/**
 * Exception class used to transport a continuation through
 * stack unwinding. Mainly needed to force a context to exit to
 * another specific continuaiton.
 */
struct exit_exception 
    : std::exception {
    friend class details::switch_pair_accessor;

    /* Construct an exit_exception from a continuation that will be
     * transported */
    template<class Signature>
    explicit exit_exception(continuation<Signature> exit_to)
        : exit_to(details::switch_pair_accessor::pilfer(exit_to)) {}

    /**
     * Precondition: the exception has been pilfered
     */
    ~exit_exception() throw() {
        assert(!exit_to.sp);
    }

    exit_exception(const exit_exception&) = delete;

    exit_exception(exit_exception&& rhs) : exit_to(rhs.pilfer()) {}    

private:
    switch_pair pilfer() { 
        switch_pair result = {{0},0};
        std::swap(result, exit_to);
        return result;
    }

    switch_pair exit_to;
};

/**
 * Like exit_exception, but also captures the current exception in an
 * std::exception_ptr.
 */
 struct abnormal_exit_exception : exit_exception {

    std::exception_ptr ptr;

     abnormal_exit_exception(gpd::abnormal_exit_exception&&) = default;

     /**
      * Construct an exit_exception from a continuation that will be
      * transported. Also capture the current pending exception by
      * calling std::current_exception().
      */     
    template<class Signature>
    explicit abnormal_exit_exception(continuation<Signature> exit_to)
        : exit_exception(std::move(exit_to))
        , ptr(std::current_exception()){}
     
     /** Returns the captured nested exception */
     std::exception_ptr nested_ptr() const { 
         return ptr;
     }
     
     ~abnormal_exit_exception() throw() {}
};
}
#endif
