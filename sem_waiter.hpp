#ifndef GPD_SEM_WAITER_HPP
#define GPD_SEM_WAITER_HPP
#include <semaphore.h>
#include "event.hpp"
namespace gpd {
// Posix semaphore based waiter
struct sem_waiter : waiter {
    sem_t sem;
    std::atomic<std::int32_t> signal_counter = { 0 };

    sem_waiter() { auto ret = ::sem_init(&sem, 0, 0); assert(ret==0);  }

    void reset() {
        signal_counter.store(0, std::memory_order_relaxed);
    }

    void signal(event_ptr p) override {
        p.release();
        auto v = --signal_counter;
        if (v == 0) {
            auto ret = ::sem_post(&sem);
            assert(ret == 0);
        }
    }
    void wait(std::size_t count = 1) {
        auto v = signal_counter += count;
        if (v)
            while(auto ret = ::sem_wait(&sem))  {
                if (ret == -1 && errno == EINTR) continue;
                assert(ret == 0);
            }
    }
    ~sem_waiter() { auto ret = ::sem_destroy(&sem); assert(ret == 0); }
};


}
#endif
