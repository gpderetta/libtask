// #include <valgrind/drd.h>
// #define _GLIBCXX_SYNCHRONIZATION_HAPPENS_BEFORE(addr) ANNOTATE_HAPPENS_BEFORE(addr)
// #define _GLIBCXX_SYNCHRONIZATION_HAPPENS_AFTER(addr) ANNOTATE_HAPPENS_AFTER(addr)
// #define _GLIBCXX_EXTERN_TEMPLATE -1 
#include <atomic>
#include <vector>
#include <functional>
#include <boost/shared_ptr.hpp>
#include <boost/optional.hpp>
#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/lexical_cast.hpp>
#include <thread>
#include <iostream>
#include "continuation.hpp" 
#include "future.hpp"
#include "task_waiter.hpp"

typedef boost::asio::ip::tcp::acceptor acceptor_type;
typedef boost::asio::ip::tcp::endpoint endpoint_type;
using std::placeholders::_1;
boost::asio::io_service demuxer;


// Do something, calculate xor of the buffer.
void frob(char * begin, size_t len) {
    char first = begin[0];
    for(size_t i=0; i<len -1; i++)
        begin[i] ^=begin[i+1];
    begin[len-1] ^= first;
}

typedef gpd::continuation<void(void)> task_t;

typedef boost::system::error_code error_type;
task_t thread(task_t scheduler,
            acceptor_type* acceptor,
            endpoint_type* endpoint,
            int counter, int token_size){
    boost::shared_ptr<char> token_(new char[token_size]);
    char * token = token_.get();
    for(int i=0; i<token_size; i++) token[i] = 0;
    boost::asio::ip::tcp::socket sink(demuxer);
    boost::asio::ip::tcp::socket source(demuxer);

    gpd::promise<error_type> accept_promise;
    gpd::promise<error_type> connect_promise;
    auto accept_error = accept_promise.get_future();
    auto connect_error = connect_promise.get_future();

    auto bind = [](auto& x) {
        return [p = &x] (auto... y) mutable {
            p->set_value({ y... });
        };
    };
    
    acceptor->async_accept(sink, bind(accept_promise));

    source.async_connect(*endpoint, bind(connect_promise));

    wait_all(scheduler, connect_error, accept_error);
   
    assert(connect_error.is_ready());
    assert(accept_error.is_ready());
 
    if (auto error = accept_error.get()) {
        std::cerr << "accept error, \"" << boost::system::system_category().message(error.value()) << "\"\n";
        return scheduler;

    }

    if (auto error = connect_error.get()) {
        std::cerr << "connect error, \"" << boost::system::system_category().message(error.value()) << "\"\n";
        return scheduler;

    }
    
    struct err { error_type error;  std::size_t len; };
    gpd::promise<err> read_promise, write_promise;
    auto read_error = read_promise.get_future();
    auto write_error = write_promise.get_future();

    boost::asio::async_read(source,
                            boost::asio::buffer(token, token_size),
                            bind(read_promise));

    boost::asio::async_write(sink,
                             boost::asio::buffer(token, token_size),
                             bind(write_promise));

    while(counter) {

        if(write_error.is_ready()) {
            auto x = write_error.get(scheduler);
            if(x.error){
               std::cerr << "write error, \"" << boost::system::system_category().message(x.error.value()) << "\", count="<<counter <<"\n";

                break;
            }
            write_promise = gpd::promise<err>();
            write_error = write_promise.get_future();

            frob(token, token_size);
            boost::asio::async_write(sink,
                                     boost::asio::buffer(token, token_size),
                                     bind(write_promise));
            counter--;

        }
 
        if(read_error.is_ready()) {
            auto x = read_error.get(scheduler);
            if(x.error) {
                std::cerr << "read error, count="<<counter <<"\n";
                break;
            }
            read_promise = gpd::promise<err>();
            read_error = read_promise.get_future();
            boost::asio::async_read(source,
                                    boost::asio::buffer(token, token_size),
                                    bind(read_promise));
        }
 
        wait_any(scheduler, read_error, write_error);
    }
   
    sink.close();

    wait_all(scheduler, read_error, write_error);
    return scheduler;
}
 
typedef std::vector<boost::shared_ptr<acceptor_type> > acceptor_vector_type;
typedef std::vector< endpoint_type > endpoint_vector_type;

int main(int argc, char** argv) {
    int count = ((argc >= 2) ? boost::lexical_cast<int>(argv[1]): 100);
    int task_count = ((argc >= 3) ? boost::lexical_cast<int>(argv[2]): 3);
    int base_port = ((argc >= 4) ? boost::lexical_cast<int>(argv[3]): 30000);
    int token_size = ((argc == 5) ? boost::lexical_cast<int>(argv[4]): 200);;
    int thread_count = ((argc == 6) ? boost::lexical_cast<int>(argv[5]): 2);;

    acceptor_vector_type acceptor_vector;
    endpoint_vector_type endpoint_vector;

    acceptor_vector.reserve(task_count);
    endpoint_vector.reserve(task_count);

    for(int i= 0; i< task_count; i++) {
        endpoint_vector.push_back
            (endpoint_type
             (boost::asio::ip::address_v4::from_string("127.0.0.1"),
              base_port + i));
        acceptor_vector.push_back
            (boost::shared_ptr<acceptor_type>
             (new acceptor_type(demuxer)));
        acceptor_vector.back()->open(endpoint_vector.back().protocol());
        acceptor_vector.back()->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_vector.back()->bind(endpoint_vector.back());
        acceptor_vector.back()->listen();
    }

    auto acceptor = &*acceptor_vector.back();
    auto endpoint = &endpoint_vector.back();

    gpd::callcc([=](task_t master) { 
            return thread(std::move(master),
                   acceptor, 
                   endpoint,
                   count, token_size);
        });
                
    for(int i=1; i< task_count; i++) {
        auto acceptor = &*acceptor_vector[i-1];
        auto endpoint = &endpoint_vector[i-1];

        gpd::callcc([&](task_t master) { 
                return thread(std::move(master),
                       acceptor, endpoint,
                       count, token_size);
            });
    }
    
    std::vector<std::thread> v;
    for(int i = 0; i < thread_count; ++i) 
        v.push_back(std::thread([] {
                    demuxer.run();
                }));

    for (auto& t: v) t.join();
}
 
