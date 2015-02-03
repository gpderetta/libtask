#ifndef GPD_FUTURE_HPP
#define GPD_FUTURE_HPP
#include "continuation.hpp"
#include <numeric>
#include <limits>
#include <iterator>
#include <atomic>
#include <cstdlib>
namespace gpd {
typedef continuation<void()> task_t;

struct latch_base;

enum class state_t  { unbound, armed, critical, done, fired };
enum class states_t { unknown, all_done, one_fired, one_armed };
enum class order_t { relaxed, strict };


template<class T>
T exchange(T& x, T&& y = T())
{
    T tmp = std::move(x);
    x = std::move(y);
    return tmp;
}

struct latches_t {
    latch_base ** b;
    latch_base ** e;
    latch_base ** begin() const { return b; }
    latch_base ** end() const { return e; }
   latches_t rest() const { return latches_t { b+1, e}; }
} latches;


namespace details {
struct waiter_t {
    const order_t order;
    std::atomic<int> count;
    task_t task;
    latches_t latches;
};

}


const int count_single = std::numeric_limits<int>::max();

std::memory_order load_order(order_t o) {
    return o == order_t::strict? 
        std::memory_order_acquire :
        std::memory_order_relaxed;
}

std::memory_order store_order(order_t o) {
    return o == order_t::strict? 
        std::memory_order_release :
        std::memory_order_relaxed;
}

template<class T, order_t order = order_t::strict>
class promise;

struct latch_base
{
    template<class T, order_t order>
    friend class promise;

    
    latch_base() : m_waiter(0), m_state(state_t::unbound) {}

    state_t 
    state(order_t o = order_t::strict) const {
        return m_state.load(load_order(o));
    }

    void set_waiter(details::waiter_t * waiter, 
                    order_t o = order_t::strict) { 
        m_waiter.store(waiter, store_order(o));
    }
    
    void set_state(state_t state, 
                    order_t o = order_t::strict) { 
        m_state.store(state, store_order(o));
    }

    details::waiter_t * get_waiter(order_t o = order_t::strict) {
        return m_waiter.load(load_order(o));
    }

    bool armed() const { return this->state() == state_t::armed; }
    bool unbound() const { return this->state() == state_t::unbound; }
    bool done() const { return state() >= state_t::done; }

    void reset() {
        m_waiter.store(0, std::memory_order_relaxed);
        m_state.store(state_t::unbound, std::memory_order_relaxed); 
    }

private:
    
    latch_base(latch_base&) = delete;
    void operator=(latch_base&) = delete;

    std::atomic<details::waiter_t *> m_waiter; 
    std::atomic<state_t> m_state;
};


template<class T>
struct latch : latch_base
{
    typedef T element_type;

    T& operator*()  {
        assert(!!*this);
        return m_value;
    }

    const T& operator*() const {
        assert(!!*this);
        return m_value;
    }

    T* operator->()  {
        assert(!!*this);
        return &m_value;
    }

    const T* operator->() const {
        assert(!!*this);
        return &m_value;
    }


    explicit operator bool () const { 
        return done();
    }

    latch() {  }

    ~latch() {
        assert(!armed());
        reset();
    }

    gpd::promise<T> promise();

    friend latch<T>* get_latch(latch<T>& self) { return &self; }

    template<class... Args>
    void set(Args&&... args) { 
        new(&m_value) T{std::forward<Args>(args)...};
    }

    void reset() { 
        assert(!!*this || unbound());
        if(!unbound()) {
            m_value.~T();
        }
        latch_base::reset();
    }

private:

    latch(latch&) = delete;
    void operator=(latch&) = delete;
            
    union { T m_value; }; 
};


template<class T>
struct future 
{
    typedef T element_type;

    future(const future&) = delete;
    future(future&& rhs) : m_latchp(rhs) {}
    future& operator=(future rhs) { swap(*this, rhs); return *this; }

    T& operator*()  {
        return **m_latchp;
    }

    const T& operator*() const {
        return **m_latchp;
    }

    explicit operator bool () const { 
        return !!*m_latchp;
    }


    T* operator->()  {
        assert(!!*this);
        return &**this;
    }

    const T* operator->() const {
        assert(!!*this);
        return &*this;
    }

    friend void swap(future& lhs, future& rhs) { std::swap(lhs.m_latchp, rhs.m_latchp);}

    future() : m_latchp(new latch<T>()) {  }

    /// \pre !armed()
    ~future() {}

    bool armed() const { return m_latchp->armed(); }

    void reset() {
        m_latchp->reset();
    }

    gpd::promise<T> promise();

    friend latch<T>* get_latch(future<T>& self) 
    {
        return self.m_latchp.get(); 
    }

private:    
    std::unique_ptr<latch<T> > m_latchp;
};



template<class... Waitables>
void wait_any(order_t order, task_t& to, Waitables&... waitables) {
    // GCC will barf in lambda capture if we omit the size here
    latch_base self;
    latch_base* latches[sizeof...(waitables)+1] = { 
        &self, get_latch(waitables)... 
    }; 

    self.set_state(state_t::critical, order_t::relaxed);
    // for (auto x: latches)
    //     assert(!x->unbound());

    details::waiter_t w { 
        order, 
        {sizeof...(waitables) == 1? count_single : 1}, 
        {},
        {std::begin(latches), std::end(latches)} ,
    };

    to = callcc
        (std::move(to),
         [&](task_t c){  
            {
                w.task = std::move(c);
                
                bool any_armed = false;
                for (auto x: w.latches.rest())
                {
                    x->set_waiter(&w, order);
                    auto state = x->state(order);
                    any_armed |= (state == state_t::armed);
                }
                
                if(any_armed && 
                   order == order_t::strict)
                    std::atomic_thread_fence(std::memory_order_seq_cst);
                
                bool all_done = true;
                for (auto x: w.latches.rest())
                {
                    while(x->state(order) == state_t::critical)
                        ;
                    auto state = x->state(order);
                    all_done  &= (state == state_t::done);
                    if (state == state_t::fired)
                        break;
                }
                
                if (all_done)
                    c = std::move(w.task);
                self.set_state(state_t::done);
            }

            if (c)
                c();
            return c;
        });

    assert([&] { 
            int count = 0;
            for (auto x: w.latches)
                count += x->done();
            return count > 0;
        }());
}
        
template<class Waitable>
void wait(task_t& to,
          Waitable& waitable, 
          order_t order = order_t::strict) {
    wait_any(order, to, waitable);
}
        

template<class... Waitables>
void wait_any(task_t& to, Waitables&... waitables) {
    wait_any(order_t::strict, to, waitables...);
}

template<class... Waitables>
void wait_all(order_t order, task_t& to, Waitables&... waitables) {
    bool _[] = { ( get_latch(waitables)->unbound() || (wait(to, waitables, order), true))... };
    (void)_;
}

template<class... Waitables>
void wait_all(task_t& to, Waitables&... waitables) {
    wait_all(order_t::strict, to, waitables...);
}


template<class Waitable>
using element_type = typename Waitable::element_type;

template<class T, class Enable = typename std::enable_if<false, T>::type>
void get_latch(T);

template<class Waitable>
using latch_type = typename std::decay<decltype(get_latch(std::declval<Waitable&>()))>::type;

template<class T, 
         class Waitable>
using if_compatible = typename  std::enable_if<
    std::is_same<element_type<Waitable >,
                 T>::value>::type;


int dec(std::atomic<int>& x, order_t order)
{
    if(order == order_t::strict)
    return x.fetch_add(-1, std::memory_order_seq_cst);
        auto v = x.load(std::memory_order_relaxed);
    x.store(v-1, std::memory_order_relaxed);
    return v-1;
}


template<class T, order_t order>
struct promise {
    
    promise(promise const& rhs) 
        : m_target(rhs.m_target) {}
    promise& operator=(promise const& rhs){
        m_target = rhs.m_target;
        return *this;
    }

    template<class Waitable, class Enable = if_compatible<T, Waitable> >
    explicit promise(Waitable& w) 
        : m_target(get_latch(w)) {
        assert(m_target->unbound());
        assert(m_target->m_waiter.load(std::memory_order_relaxed) == 0);
        
        m_target->set_state(state_t::armed, order);
    }

    // signal
    template<class... Args>
    void operator()(Args&&... args) const {
        m_target->set(std::forward<Args>(args)...);
        m_target->set_state(state_t::critical, order);


        assert(m_target);
        auto w = m_target->get_waiter(order);


        if(w == 0 && order == order_t::strict) {
            std::atomic_thread_fence(std::memory_order_seq_cst);
            w = m_target->get_waiter(order);
        }

        if( w == 0)
        {
            m_target->set_state(state_t::done, order);
            return;
        }


        auto count = w->count.load(load_order(order));

        if (count == count_single)
        {
            m_target->set_state(state_t::fired, order);
            m_target->set_waiter(0, order_t::relaxed);
            task_t x = std::move(w->task);
            x();
        } else
        {
            // disengage targets. Note, we do it here instead of the
            // tail of wait_{any|all} because we need a #storeload
            // barrier, so we piggyback on the barrier in dec. Note
            // that multple candidate threads may execute this code at
            // the same time. This is safe.
            for (auto x : w->latches)
                x->set_waiter(0, order);

            // barrier
            if (dec(w->count, order) == 1)
            {
                m_target->set_state(state_t::fired, order);
                for (auto x: w->latches)
                {
                    if (x == m_target) continue;
                    while (x->state(order) == state_t::critical)
                        ;
                }
                task_t x = std::move(w->task);
                x();
            } else
                m_target->set_state(state_t::fired, order);
            m_target->set_waiter(0, order_t::relaxed);
        }
    }

    promise() : m_target() {}

private:
    latch<T> * m_target;

};

template<class T>
inline
gpd::promise<T> future<T>::promise() { return gpd::promise<T>(*this); }

template<class T>
inline
gpd::promise<T> latch<T>::promise() { return gpd::promise<T>(*this); }





}
#endif
