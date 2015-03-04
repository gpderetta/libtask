#ifndef GPD_CV_WAITER_HPP
#define GPD_CV_WAITER_HPP
#include "event.hpp"
#include <thread>
#include <iostream>
namespace gpd {
struct cv_waiter : waiter {
    std::int32_t signal_counter = 0;
    std::mutex mux;
    std::condition_variable cvar;

    void reset() { signal_counter = 0; }
    
    void signal(event_ptr p) override {
        p.release();
        bool signal = false;
        {   std::unique_lock<std::mutex>_ (mux);

            // thechnically <= would be safe. 
            signal = (--signal_counter == 0);
        }
        if (signal)
            cvar.notify_one();
    }

    void wait(std::uint32_t count = 1)   {
        std::unique_lock<std::mutex> lock (mux);
        signal_counter += count;
        while(signal_counter > 0) {
            cvar.wait(lock);
        }
    }


};
}
#endif
