#ifndef GPD_FD_WAITER_HPP
#define GPD_FD_WAITER_HPP
#include "event.hpp"
#include <sys/eventfd.h>
#include <unistd.h> // read/write
#include <poll.h>   
namespace gpd {

// eventfd based waiter
struct fd_waiter : waiter {
    int fd;
    
    fd_waiter() : fd( ::eventfd(0, EFD_CLOEXEC|EFD_NONBLOCK )) {
        assert(fd != -1);
    }

    char padding[60];
    std::atomic<std::int32_t> signal_counter = { 0 };

    void reset() {
        signal_counter.store(0, std::memory_order_relaxed);
    }
    
    void signal(event_ptr p) override {
        p.release();
        std::uint64_t buf = 1;
        auto v = --signal_counter;
        if (v == 0)
            while(true) {
                auto ret = ::write(fd, &buf, sizeof(buf));
                if (ret == -1) {
                    assert(errno == EINTR);
                    continue;
                }
                assert(ret == 8);
                break;
            }
    }
        
    void wait(std::size_t count = 1) {
        std::uint64_t buf = 0;
        auto v = signal_counter += count;
        if (v)
            while(true)  {
                auto ret = ::read(fd, &buf, sizeof(buf));
                if (ret == -1)
                    switch(errno) {
                    case EINTR: continue;
                    case EAGAIN: { //
                        ::pollfd fds[1] = { { fd,POLLIN, 0 } };
                        ::poll(fds, 1, -1);
                        continue;
                    }
                    default: assert(false);
                    }
                assert(ret == 8);
                break;
            }
    }

    ~fd_waiter() { ::close(fd); }
};


}
#endif
