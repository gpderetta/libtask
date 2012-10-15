#ifndef GPD_FUTEX_HPP
#define GPD_FUTEX_HPP
#include <linux/futex.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include "atomic.hpp"

namespace gpd {
namespace details {
inline long sys_futex(void *addr1, int op, int val1, 
                      struct timespec *timeout, void *addr2, int val3) {
	return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}
}

/**
 * A futex + atomic<int> in a user friendlier package.
 */
class futex : atomic<int> {
    enum wait_result { woken = 0 , timeout = ETIMEOUT, 
                       wouldblock = EWOULDBLOCK, interrupted = EINTR };
    
    /**
     * Block the current thread unless the associated atomic int is
     * different from 'value'.  
     * 
     * NOTE: there is no guarantee that after wait returns the atomic
     * int will be equal to 'value'
     */
    wait_result
    wait(int value) {
        int ret = details::sys_futex(this, FUTEX_WAIT_PRIVATE, value, 0, 0, 0);
        assert(ret != -1);
        return wait_result(ret);
    }

    /**
     * Block untill the associated atomic int is different from
     * 'value' or the time delay 't' has expired.
     *
     * NOTE: see the note for unary wait.
     */
    wait_result 
    wait(int value, timespec t) {
        int ret = details::sys_futex(this, FUTEX_WAIT_PRIVATE, value, &t, 0, 0);
        assert(ret != -1);
        return wait_result(ret);
    }

    /**
     * Wake up to 'n' waiters of this futex.
     *
     * For n == INT_MAX (the default), wake all waiters.
     */ 
    int signal(int n = INT_MAX) {
        int ret = details::sys_futex(this, FUTEX_WAKE_PRIVATE, value, 0, 0, 0);
        assert(ret != -1);
        return ret;
    }

    

private:
    atomic<int> location;
};


}
#endif
