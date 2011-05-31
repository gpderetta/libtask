#include "queue.hpp"
#include <thread>
#include <vector>
#include <iostream>      

struct val : gpd::node_base {
    val(int id, int from) : id(id), from(from) {}
    const int id;
    int from;
};
    
typedef gpd::queue<val> queue;


int main() {
    static const int count = 10; 
    std::vector<queue> queues(count);
    std::vector<std::thread> threads;
    for(int id = 0; id < count; ++id) 
        threads.push_back(std::thread([&queues, id]() {
                    const int jmp = rand() % count;
                    auto next = [=](int k) { return (k+jmp)%count; };
                    auto p = new val(id, id);
                    while(p) {
                        int from = p->from;
                        p->from = id;
                        queues[next(from)].push(p);
                        p = queues[id].pop();
                        if(p == 0) return;

                    }
                }));
    for(auto& t: threads)
        t.join();
}
