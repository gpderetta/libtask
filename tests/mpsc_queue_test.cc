#include "mpsc_queue.hpp"
#include <thread>
#include <vector>
#include <iostream>      
#include <mutex>

struct val : gpd::node {
    val(int id, int from) : id(id), from(from) {}
    const int id;
    int from;
    int count = 0;
};
    
typedef gpd::mpsc_queue<val> queue;


int main() {
    static const int count =100; 
    std::vector<queue> queues(count);
    std::vector<std::thread> threads;
    std::mutex mux;
    for(int id = 0; id < count; ++id) 
        threads.push_back(std::thread([&queues,&mux, id]() {
                    auto p = new val(id, id);
                    int i = 0;
                    int create = 60;
                    while(true) {
                        ++i;
                        p->from = id;
                        queues[(id + 1) % count].push(p);
                        while(create --> 0) {
                            queues[(id + 1) % count].push(new val(id, id));
                        }
                        
                        while(!(p = queues[id].pop())) {
                            std::this_thread::yield();
                        }
                        p->count++;
                        if (p->count == count*10  )
                            break;
                    }
                    std::unique_lock<std::mutex> l(mux);
                    std::cerr << id << "->" << ((id + 1)%count) << ':' << i <<'\n';
                }));
    for(auto& t: threads)
        t.join();
}
