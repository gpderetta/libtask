#include "future.hpp"
#include "task_waiter.hpp"
#include "continuation.hpp"
#include <cassert>
#include <functional>

int main() {
    using namespace gpd;
    {
        promise<int> callback;

        auto c = callcc([&](task_t task) {
                using gpd::wait;
                future<int> f = callback.get_future();
                assert(!f);
                wait(task, f);
                assert(f);
                assert(*f == 10);
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
                assert(!f1);
                assert(!f2);
                wait_any(task, f1, f2);
                assert(f2);
                assert(*f2 == 10);
                wait_any(task, f1, f2);
                assert(f1);
                assert(*f1 == 42);
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
                assert(!f1);
                assert(!f2);
                task();
                wait_any(task, f1, f2);
                assert(f2);
                assert(*f2 == 10);
                wait_any(task, f1, f2);
                assert(f1);
                assert(*f1 == 42);
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
                assert(!f1);
                assert(!f2);
                task();
                wait_all(task, f1, f2);
                assert(f2);
                assert(*f2 == 10);
                assert(f1);
                assert(*f1 == 42);
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
                assert(!f1);
                assert(!f2);
                wait_all(task, f1, f2);
                assert(f2 && f1);
                assert(*f2 == 10);
                assert(*f1 == 11);
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
                assert(!f);
                wait(task, f);
                assert(f);
                auto x = *f;
                assert(std::get<0>(x) == 10);
                assert(std::get<1>(x) == 11);
                return task;
            });

        assert(!c);
        callback.set_value(std::make_tuple(10,11));
    }

}
