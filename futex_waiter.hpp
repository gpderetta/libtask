#ifndef GPD_FUTEX_WAITER_HPP
#define GPD_FUTEX_WAITER_HPP
#include "event.hpp"
#include "futex.hpp"
namespace gpd {
// Futex based one shot binary semaphore. Not fast pathed.
struct futex_waiter : waiter {
    char padding[60];
    futex signal_counter = { 0 };

    void reset() {
        signal_counter.store(0, std::memory_order_relaxed);
    }
    

    void signal(event_ptr p) override {
        p.release();
        auto v = --signal_counter;
        if (v == 0)
            signal_counter.signal();
    }
    void  wait(std::size_t count = 1)   {
        auto v = signal_counter += count;
        while (v > 0) {
            signal_counter.wait(v);
            v = signal_counter.load(std::memory_order_relaxed);
        }
    }

};


}
#endif
