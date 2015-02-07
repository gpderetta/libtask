#ifndef GPD_CONTINUATION_HPP
#define GPD_CONTINUATION_HPP
#include "continuation_exception.hpp"
#include "details/continuation_meta.hpp"
#include "details/continuation_details.hpp"
#include "details/switch_base.hpp"
#include "forwarding.hpp"
#include "tuple.hpp"
#include <exception>
#include <cstring>
#include <memory>
#include <type_traits>
#include <iterator>
#include "macros.hpp"

namespace gpd {
    
/**    
 * A continuation object represent a reified (typed) step of a
 * computation, or execution context.
 *
 * The Signature parameter is the type of the halted computation,
 * represented as a function type.
 *
 * A continuation can be understood as a typed, two-way, synchronous,
 * unbuffered pipe which communicates with another execution
 * context. operator() is used for rendez-vous and to send
 * messages. Receieved messages can be retrived via operator get().
 *
 * Like the two ends of a pipe, continuations always come in pairs;
 * for every continuation A of signature R(T), there is a dual
 * continuation B of signature T(R). Each end, or execution context,
 * will own the end of the pipe that represent the other context. 
 *
 * Resuming the context associated with a continuation halts the
 * current context and 'consumes' the continuation putting it in the
 * empty() state as no halted context is associated with it any more.
 * When the current context is resumed again the continuation may or
 * not be non empty again depending on the action of the other
 * context.
 *
 * From the above paragraph follows that for every continuation pair,
 * at most only one end will be non empty() at any time.
 *
 * Continuation pairs are usually created via the callcc family of
 * functions.
 *
 * Continuations are first class movable objects and can be put in
 * containers and even passed to other continuations, to create
 * complex control flows.
 *
 * Continuation pairs can also be disassociated and associated with
 * other endpoints.
 *
 * Continuations of the kind void(T) model output ranges, while
 * continuations of kind T() model input ranges. 
 *
 * Continuations signatures can also be n-ary, with signature R(T1,
 * T2, T3...); in this case the dual continuation will have signature
 * std::tuple<T1, T2, T3...>(R).
 *
 * A continuation signature supports all C++ argument passing styles,
 * including, pass by value, refrence, const reference, rvalue
 * reference. In particular Movable only arguments are handled correctly.
 *
 * NOTE: continuations are one shot, and should be called exactly once
 * before are destroied or replaced with a new continuaiton. Currently
 * the destructor will try to exit the associated context if the
 * continuation is not empty().
 **/
template<class Signature = void()>
class continuation;

template<class Result, class... Args>
struct continuation<Result(Args...)> {
    friend class details::switch_pair_accessor;
    // Same as the Signature template parameter.
    typedef Result signature (Args...);
    // Signature of the dual continuation.
    typedef typename details::make_signature<signature>::rsignature rsignature;
    // Type of the dual continuaiton.
    typedef continuation<rsignature> rcontinuation;

    // Result type of this continuation as a reference or rvalue reference.
    typedef typename details::make_signature<signature>::result_type  result_type;

    // A continuation is non copyable
    continuation(const continuation&) = delete;

    /** 
     * Default constructor.
     *
     * The created continuation is in the empty() state.
     **/
    continuation() : pair{{0},0} {}

    // Move constructor
    continuation(continuation&& rhs) : pair(rhs.pilfer()) {}

    // Create a continuation from the internal untyped switch_pair object.
    //
    // TODO: this should be private
    explicit continuation(switch_pair pair) : pair(pair) {}

    // Move assignment operator
    continuation& operator=(continuation rhs) {
        assert(empty());
        pair = rhs.pilfer();
        return *this;
    }

    /**
     * Perform randez-vous with the other end of the pipe, blocking
     * this execution context and resuming the other. Also send
     * parameters 'args...' to the other end;
     * 
     * The halted control flow A, represented by this continuation, is
     * resumed and the current control flow B, is halted. When B is
     * resumend, 'this' will be replaced by a new continuation,
     * usually, but not necessarily, representing the new halted
     * control flow A.
     *
     * Preconditions: this is not empty()
     *
     * Postcondition: if this is not empty(), it represent the new
     * halted control flow B; if this is empty(), the control flow of B has
     * exited or has been captured in another continuation.
     *
     * Throws: Any exception thrown from the exited control flow, or
     * any exception that has been explicitly sent to this control
     * flow.
     */
    continuation& operator() (Args... args) {
        assert(!empty());
        switch_pair cpair = pilfer();
        std::tuple<Args...> p(std::forward<Args>(args)...);
        auto new_pair = stack_switch_impl(cpair.sp, &p);
        assert(empty());
        pair = new_pair;
                
        return *this; 
    }
    
    /**
     * Precondition has_data() == true and result_type != void
     * 
     * Returns the data that has been sent to this continuation at the
     * last randez-vous.
     */
    result_type get() const {
        assert(has_data());
        return get(details::tag<result_type>(), pair.parm);
    }
    
    /** 
     * Returns true if !empty() && has_data.
     *
     * NOTE that has_data is always true if result_type is void
     */  
    explicit operator bool() const {
        return !empty() && has_data();
    }

    /**
     * Returns true if there is data retrivable via get(). If
     * result_type is void, always returns true.
     */ 
    bool has_data() const {
        return has_data(details::tag<result_type>(), pair.parm);
    }

    /**
     * Returns true if this continuation is not associated with an
     * halted execution context.
     */
    bool empty() const {
        return !pair.sp;
    }

    /**
     * Same as operator(), except that no data is sent to the other end.
     */
    continuation& yield() {
        assert(!empty());
        switch_pair cpair = pilfer(); 
        pair = stack_switch
            (cpair.sp, 0); 
        return *this; 
    }

    /**
     * Destroy current continuation. If continuation is not empty(),
     * an exit is forced. 
     *
     * Precondition: empty() or empty() after as signal_exit;
     */
    ~continuation() {
        if(!empty()) signal_exit(*this);
        assert(empty());
    }

    friend std::size_t hash(const continuation& x)
    {
        return (reinterpret_cast<std::size_t>(x.sp) >> 10);
    }

private:
    /** 
     * Returns the internal switch_pair object, leaving the current
     * continuation empty.
     *
     * Postcondition: empty() == true;
     * NOTE: this function should be private
     */
    switch_pair pilfer() {
        switch_pair result = pair;
        pair = {{0},0};
        return result;
    }

    template<class T> 
    static bool has_data(details::tag<T>, void * parm)  {
        return parm;
    }

    static bool has_data(details::tag<void>, void*)  {
        return true;
    }

    template<class T> 
    static result_type get(details::tag<T>, void * parm)  {
        typedef std::tuple<T> tuple;
        return std::move(std::get<0>(static_cast<tuple&>(*static_cast<tuple*>(parm))));
    }


    template<class T> 
    static result_type get(details::tag<T&>, void * parm)  {
        typedef std::tuple<T&> tuple;
        return std::get<0>(static_cast<tuple&>(*static_cast<tuple*>(parm)));
    }
        
    template<class... T> 
    static result_type get(details::tag<std::tuple<T...>&&>, void*parm) {
        typedef std::tuple<T... > tuple;
        return std::move(*static_cast<tuple*>(parm));
    }        

    static void get(details::tag<void>, void*)  { }        

    switch_pair pair;
};

template<class F, 
         class... Args,
         class Sig = typename details::deduce_signature<F>::type>
continuation<Sig> callcc(F f, Args&&... args) {
    return details::create_continuation<Sig> 
        (gpd::bind(std::move(f), placeholder<0>(), 
                   std::forward<Args>(args)...)); 
}


template<class Sig, 
         class F, 
         class... Args>
continuation<Sig> callcc(F f,  Args&&... args) {
    return details::create_continuation<Sig>
        (gpd::bind(std::move(f), placeholder<0>(), 
                   std::forward<Args>(args)...));
}


// execute f on existing continuation c, passing to f the current
// continuation. When f returns, c is resumed; F must return a continuation to c.
//
// returns the new continuation of c.
//
// Note that a new signature can be specified for the new continuation
// of c and consequently for the continuation passed to f
template<class NewIntoSignature,
         class IntoSignature, 
         class F, class... Args>
auto callcc(continuation<IntoSignature> c, F f, Args&&... args) as
    (details::interrupt_continuation<NewIntoSignature>
     (std::move(c), gpd::bind(std::move(f), placeholder<0>(), std::forward<Args>(args)...)));

template<class IntoSignature, 
         class F, 
         class ... Args,
         class NewIntoSignature = typename details::deduce_signature<F>::type>
auto callcc(continuation<IntoSignature> c, F f, Args&&... args) as
    (details::interrupt_continuation<NewIntoSignature>
     (std::move(c), gpd::bind(std::move(f), gpd::placeholder<0>(), 
                              std::forward<Args>(args)...)));

/**
 * The following four oerloads are not strictly necessary but may
 * speedup compilation by skipping the argument packing via bind.
 */
template<class F, 
         class Sig = typename details::deduce_signature<F>::type>
continuation<Sig> callcc(F f) {
    return details::create_continuation<Sig> (f);
}

template<class Sig, 
         class F>
continuation<Sig> callcc(F f) {
    return details::create_continuation<Sig>(std::move(f));
}

template<class NewIntoSignature,
         class IntoSignature, 
         class F>
auto callcc(continuation<IntoSignature> c, F f) as
    (details::interrupt_continuation<NewIntoSignature>
     (std::move(c), std::move(f)));

template<class IntoSignature, 
         class F,
         class NewIntoSignature = typename details::deduce_signature<F>::type>
auto callcc(continuation<IntoSignature> c, F f) as
    (details::interrupt_continuation<NewIntoSignature>
     (std::move(c), std::move(f)));

template<class F, class Continuation>
auto with_escape_continuation(F &&f, Continuation& c) -> decltype(f()) {
    try {
        return f();
    } catch(...) {
        assert(!c.empty());
        throw abnormal_exit_exception(std::move(c));
    }
}

namespace details {
template<class F>
struct escape_protected {
    F f;
    template<class C>
    C operator()(C c) {
        with_escape_continuation(f, c);
        return c;
    }
};
}
template<class F>
details::escape_protected<F> with_escape_continuation(F f) {
    return details::escape_protected<F>{std::forward<F>(std::move(f))};
}

template<class Signature>
void exit_to(continuation<Signature> c) {
    throw exit_exception(std::move(c));
}
 
template<class Signature>
void signal_exit(continuation<Signature>& c) {
    callcc(std::move(c), [] (continuation<void()> c) { 
            exit_to(std::move(c));
        });
}

template<class Continuation>
struct output_iterator_adaptor  {
    typedef std::output_iterator_tag iterator_category;
    typedef void value_type;
    typedef size_t difference_type;
    typedef void* pointer;
    typedef void reference;
    Continuation& c;
    template<class T>
    output_iterator_adaptor& operator=(T&&x) {
        c(std::forward<T>(x));
        return *this;
    }
    output_iterator_adaptor& operator++() {return *this; }
    output_iterator_adaptor& operator*() { return *this; }
};


template<class Continuation>
struct input_iterator_adaptor  {
    Continuation* c;

    typedef std::input_iterator_tag iterator_category;
    typedef typename std::remove_reference<decltype(c->get())>::type value_type;
    typedef size_t difference_type;
    typedef value_type* pointer;
    typedef decltype(c->get()) reference;

    input_iterator_adaptor& operator=(input_iterator_adaptor const&x) {
        c = x.c;
    }
    input_iterator_adaptor& operator++() {
        (*c)();
        return *this;
    }
    reference operator*() { 
        return c->get();
    }

    bool operator==(input_iterator_adaptor const&) const {
        return !*c;
    }
    bool operator!=(input_iterator_adaptor const&) const {
        return bool(*c);
    }

};

template<class T>
output_iterator_adaptor<continuation<void(T)>>
begin(continuation<void(T)>& c) { return {c}; }

template<class T>
input_iterator_adaptor<continuation<T(void)>>
begin(continuation<T()>& c) { 
    while(!c.empty() && !c.has_data()) c();
    return {&c}; 
}

template<class T>
input_iterator_adaptor<continuation<T(void)>>
end(continuation<T()>& c) { return {&c}; }

}
#include "macros.hpp"
#endif
