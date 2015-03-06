#include "future.hpp"
#include "task_waiter.hpp"
#include "cv_waiter.hpp"
#include "fd_waiter.hpp"
#include "futex_waiter.hpp"
#include "sem_waiter.hpp"
#include "continuation.hpp"
#include "task.hpp"
#include <cassert>
#include <functional>
#include <unistd.h>
#include <algorithm>

int main() {
    using namespace gpd;
    {
        promise<int> callback;

        auto c = callcc([&](task_t task) {
                using gpd::wait;
                future<int> f = callback.get_future();
                assert(!f.ready());
                wait(task, f);
                assert(f.ready());
                assert(f.get() == 10);
                return task;
            });

        assert(!c);
        callback.set_value(10);
    }
    {   
        promise<int> callback1,  callback2;

        auto c = callcc([&](task_t task) {
                using gpd::wait;
                future<int> f1 = callback1.get_future();
                future<int> f2 = callback2.get_future();
                assert(!f1.ready());
                assert(!f2.ready());
                wait_any(task, f1, f2);
                assert(f2.ready());
                assert(f2.get() == 10);
                wait_any(task, f1, f2);
                assert(f1.ready());
                assert(f1.get() == 42);
                return task;
            });

        assert(!c);
        callback2.set_value(10);
        callback1.set_value(42);
    }
    {   
        promise<int> callback1;
        promise<int> callback2;

        auto c = callcc([&](task_t task) {
                using gpd::wait;
                future<int> f1 = callback1.get_future();
                future<int> f2 = callback2.get_future();
                assert(!f1.ready());
                assert(!f2.ready());
                task();
                wait_any(task, f1, f2);
                assert(f2.ready());
                assert(f2.get() == 10);
                wait_any(task, f1, f2);
                assert(f1.ready());
                assert(f1.get() == 42);
                return task;
            });

        assert(c);
        callback2.set_value(10);
        callback1.set_value(42);
        c();
        assert(!c);
    }
    {   
        promise<int> callback1;
        promise<int> callback2;

        auto c = callcc([&](task_t task) {
                using gpd::wait;
                future<int> f1 = callback1.get_future();
                future<int> f2 = callback2.get_future();
                assert(!f1.ready());
                assert(!f2.ready());
                task();
                wait_all(task, f1, f2);
                assert(f2.ready());
                assert(f2.get() == 10);
                assert(f1.ready());
                assert(f1.get() == 42);
                return task;
            });

        assert(c);
        callback2.set_value(10);
        callback1.set_value(42);
        c();
        assert(!c);
    }

    {   
        promise<int> callback1;
        promise<int> callback2;

        auto c = callcc([&](task_t task) {
                using gpd::wait;
                future<int> f1 = callback1.get_future();
                future<int> f2 = callback2.get_future();
                assert(!f1.ready());
                assert(!f2.ready());
                wait_all(task, f1, f2);
                assert(f2.ready() && f1.ready());
                assert(f2.get() == 10);
                assert(f1.get() == 11);
                return task;
            });

        assert(!c);
        callback2.set_value(10);
        callback1.set_value(11);
    }
    {
        promise<std::tuple<int, double> > callback;

        auto c = callcc([&](task_t task) {
                using gpd::wait;
                future<std::tuple<int, double> > f = callback.get_future();
                assert(!f.ready());
                wait(task, f);
                assert(f.ready());
                auto x = f.get();
                assert(std::get<0>(x) == 10);
                assert(std::get<1>(x) == 11);
                return task;
            });

        assert(!c);
        callback.set_value(std::make_tuple(10,11));
    }

    auto launch = [] {
        gpd::promise<int> promise;
        auto future = promise.get_future();
        std::thread th([promise=std::move(promise)] () mutable {
                //::sleep(1);
                promise.set_value(0);                    
            });
        th.detach();
        return future;
    };

    auto test = [&](auto& strategy) {


        gpd::future<int> f[] = { launch(), launch(), launch(), launch() };
        while (std::any_of(std::begin(f), std::end(f),
                           [](auto&& f) { return !f.ready(); })) {
            wait_any(strategy, f[0], f[1], f[2], f[3]);
        }
    };

    for (int i = 0; i < 10000; ++i)
    {
        cv_waiter cv;
        test(cv);
    }
    for (int i = 0; i < 1000; ++i)
    {
        fd_waiter cv;
        test(cv);
    }
    for (int i = 0; i < 1000; ++i)
    {
        futex_waiter cv;
        test(cv);
    }
    for (int i = 0; i < 1000; ++i)
    {
        sem_waiter cv;
        test(cv);
    {
        sem_waiter waiter;
        auto& sched = *start_background_scheduler().get();
        auto v1 = async(sched, []{ yield(); return 42; });
        auto v2 = async(sched, []{ yield(); return 47; });
        auto v3 = async(
            sched,
            []{
                auto c1 = async(pool, [] { yield(); return 20; });
                auto c2 = async(pool, [] { yield(); return 20; });
                auto c3 = async(pool, [] { yield(); return 12; });
                auto c4 = c3.next(
                    [&](int x)
                    {
                        yield(); 
                        wait_all(pool, c1, c2);
                        return x + c1.get(pool) + c2.get(pool);
                    });
                gpd::wait(pool, c4);
                return c4.get(pool);
            });
        
        wait_all(waiter, v1, v2, v3);
        assert(v1.get() == 42);
        assert(v2.get() == 47);
        assert(v3.get() == 52);
    }

}
