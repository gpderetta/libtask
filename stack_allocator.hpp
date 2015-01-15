#ifndef GPD_STACK_ALLOCATOR_HPP
#define GPD_STACK_ALLOCATOR_HPP
#include <stdlib.h>
#include <errno.h>
#include <cassert>
#include <memory>

namespace gpd {
struct static_stack_allocator {
    enum { stack_size = 1024*1024*1024 };
    static const size_t alignment = 16;

    static void * allocate(size_t size = stack_size) {
        void * result = 0;
        int ret = ::posix_memalign(&result, alignment, size);
        assert(ret != EINVAL);
        if(ret == ENOMEM) 
            throw  std::bad_alloc();
        return result;
    }

    static void deallocate(void * ptr) throw() {
        free(ptr);
    }

};

struct debug_stack_allocator {
    static const size_t alignment = 16;
    enum { stack_size = static_stack_allocator::stack_size };
    void * allocate(size_t size) {
        void * ret = static_stack_allocator::allocate(size);
        count++;
        return ret;
    }

    void deallocate(void * ptr) throw() {
        count--;
        return static_stack_allocator::deallocate(ptr);
    }

    debug_stack_allocator(debug_stack_allocator&) = delete;
    
    int count;
    debug_stack_allocator() : count(0) {}
    debug_stack_allocator(debug_stack_allocator&& rhs) : count(rhs.count) {
        rhs.count = 0;
    }
    ~debug_stack_allocator() { assert(count == 0); }
};

#ifdef NDEBUG
typedef static_stack_allocator default_stack_allocator;
#else
typedef debug_stack_allocator default_stack_allocator;
#endif

}
#endif
