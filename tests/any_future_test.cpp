#include "any_future.hpp"
#include "sem_waiter.hpp"
using namespace gpd;

int main()
{
    {
        future<int> x;
        any_future<int> y = std::move(x);
    }

    {
        promise<int> p;
        any_future<int> f = p.get_future();
    }

    {
        promise<int> p;
        any_future<int> f = p.get_future();
        p.set_value(10);

        auto x = f.get();
        assert(x == 10);
    }

    {
        promise<int> p;
        any_future<int> f = p.get_future();
        p.set_value(10);

        any_future<float> f2 = f.then([](auto f) { return float(f.get()); });
        auto x = f2.get();
        assert(x == 10);
    }
    {
        promise<int> p;
        any_future<int> f = p.get_future();
        promise<int> p2;
        any_future<int> f2 = p2.get_future();
        sem_waiter waiter;
        p.set_value(1);
        p2.set_value(2);
        wait_all(waiter, f, f2);
    }
}
