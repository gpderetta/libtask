#ifndef GPD_CV_WAITER_HPP
#define GPD_CV_WAITER_HPP
#include "event.hpp"
#include <thread>

namespace gpd {
struct cv_waiter : waiter {
    event_ptr payload;
    std::mutex mux;
    std::condition_variable cvar;

    void signal(event_ptr p) override {
        { std::unique_lock<std::mutex>_ (mux); payload.swap(p); }
        cvar.notify_one();
    }
    
    event_ptr wait()   {
        std::unique_lock<std::mutex> lock (mux);
        while(!payload) cvar.wait(lock);
        return std::move(payload);
    }

    
};
}
#endif
