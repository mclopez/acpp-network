
#include <iostream>
#include <thread>
#include <random>
#include <format>

#include <gtest/gtest.h> // googletest header file  

#include <acpp-network/socket.h>
#include <acpp-network/socket.inl>
#include <detail/common.h>


TEST(AsyncSocketTests, simple_client_server)
{

    using namespace acpp::network;
    std::cout << "*** Socket tests" << std::endl;
    int port = 6665;
    auto sync_server = [port](){
        std::cout << "sync_server th" << std::endl;

        sync::stream_socket<ip_socketaddress> server_socket;

        ip_socketaddress adr = ip4_sockaddress("127.0.0.1", port);

        server_socket.bind(adr);
        server_socket.listen(5);
        auto client_socket = server_socket.accept();
         char buffer[1024];
         auto bytes_received = client_socket.receive(buffer, sizeof(buffer));
         if (bytes_received > 0) {
             std::cout << "Server received: " << std::string(buffer, bytes_received) << std::endl;
             client_socket.send(buffer, bytes_received); // Echo back
         }
    };

    auto async_server = [port](){
        std::cout << "async_server th" << std::endl;

        acpp::network::async::io_context io;
        std::vector<async::async_socket_base> sockets;
        //async_socket_base socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io);

        std::cout << "async_server th 2" << std::endl;

        async::async_socket_base server_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
            async::socket_callbacks {
                .on_accepted = [&](async::async_socket_base& server, async::async_socket_base&& accepted_socket) {
                    std::cout << "ACCEPTED" << std::endl;
                    accepted_socket.callbacks(async::socket_callbacks {
                        .on_connected = [](async::async_socket_base& s) {
                            std::cout << "SERVER Async server socket connected" << std::endl;
                        },
                        .on_disconnected = [&](async::async_socket_base& s) {
                            std::cout << "SERVER Async server socket disconnected" << std::endl;
                            io.stop();
                            std::erase_if(sockets, [&](auto& i){
                                //return i.fd() == s.fd();
                                return &i == &s;
                            });
                        },
                        .on_received = [&](async::async_socket_base& s, const char* buf, size_t len){
                            std::string msg(buf, len);
                            std::cout << "SERVER  Async server socket  received " << msg << "  from AsyncSocketTests.first" << std::endl;
                            //io.stop();
                            //std::string msg2("hello back!");
                            s.write(msg.c_str(), msg.size());
                            //s.read();
                        },
                        .on_sent = [&](async::async_socket_base& s, size_t length) {
                            std::cout << "SERVER Async server socket sent from AsyncSocketTests.first: length:" << length << std::endl;
                        }
                    });
                    sockets.push_back(std::move(accepted_socket));
                    std::cout << "SERVER socket.callbacks().on_read " << (sockets.back().callbacks().on_received? "read assigned":"read not assigned") << std::endl;

                    //sockets.back().read(); //start reading
                    //sockets.emplace_back(std::move(accepted_socket));
                }
            }
        );
        std::cout << "async_server th 3" << std::endl;
        //server_socket.create_impl(AF_INET, SOCK_STREAM, IPPROTO_TCP);//TODO: this dont belong here....

        std::cout << "async_server th 4" << std::endl;

        std::cout << "*** server socket " << (void*) &server_socket << std::endl;
        
        ip_socketaddress addr = ip4_sockaddress("127.0.0.1", port);

        server_socket.bind(to_sockaddr(addr));
        server_socket.listen(5);

        //auto res = server_socket.accept();
        io.wait_for_input();

        //  char buffer[1024];
        //  auto bytes_received = client_socket.receive(buffer, sizeof(buffer));
        //  if (bytes_received > 0) {
        //      std::cout << "Server received: " << std::string(buffer, bytes_received) << std::endl;
        //      client_socket.send(buffer, bytes_received); // Echo back
        //  }
        EXPECT_EQ(sockets.size(), 0);
    };


    auto sync_client = ([port](){
       std::cout << "Socket tests client" << std::endl;
       sync::stream_socket<ip_socketaddress> socket;
       ip_socketaddress adr = ip4_sockaddress("127.0.0.1", port);

       socket.connect(adr);
       //std::this_thread::sleep_for(std::chrono::microseconds(1000));
       const char* msg = "Hello, Echo Server!";
       std::cout << "client send: " << msg << std::endl;
       socket.send(msg, strlen(msg));
       char buffer[1024];
       auto bytes_received = socket.receive(buffer, sizeof(buffer));
       if (bytes_received > 0) {
           std::string msg(buffer, bytes_received);
           std::cout << "Client received: " << msg << std::endl;
           EXPECT_EQ(msg, "Hello, Echo Server!");
       }
    });

    auto async_client = [port](){
        std::cout << "Socket tests client" << std::endl;
        acpp::network::async::io_context io;
        async::async_socket_base socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
            async::socket_callbacks {
                .on_connected = [](async::async_socket_base& s) {
                    std::cout << "CLIENT Socket connected from AsyncSocketTests.first" << std::endl;
                    std::string msg("hola");
                    //s.read();
                    s.write(msg.c_str(), msg.size());
                },
                .on_received = [&](async::async_socket_base& s, const char* buf, size_t len){
                    std::string msg(buf, len);
                    std::cout << "CLIENT Socket received " << msg << "  from AsyncSocketTests.first" << std::endl;
                    EXPECT_EQ(msg, "hola");
                    io.stop();
                },
                .on_sent = [](async::async_socket_base& s, size_t length) {
                    std::cout << "CLIENT Socket sent from AsyncSocketTests.fir st" << std::endl;
                },
                .on_error = [](async::async_socket_base& s, int error_code, const std::string& msg, const std::string& hint) {
                    std::cout << "CLIENT Socket error from AsyncSocketTests.first error_code: " 
                        << error_code << " msg: " << msg << " hint: " << hint 
                        << std::endl;
                }
            });

        auto c_res = socket.connect(to_sockaddr(ip4_sockaddress("127.0.0.1", port)));
        std::cout << "CLIENT Socket connected c_res: " << c_res << std::endl;
        std::string msg("hola!");
        io.wait_for_input();
        socket.close();

        std::cout << "CLIENT Socket tests client end" << std::endl;
    };
    
    std::thread server_th(async_server);
    std::thread client_th(async_client);
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    //io.stop();
    if (server_th.joinable())
        server_th.join();
    if (client_th.joinable())
       client_th.join();


}


std::string random_string(size_t length,
                          const std::string& charset =
                              "0123456789"
                              "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz")
{
    static std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<> dist(0, charset.size() - 1);

    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        result.push_back(charset[dist(rng)]);
    }
    return result;
}

struct large_write_client_server_sesson {
public:
    large_write_client_server_sesson(acpp::network::async::async_socket_base&& s):socket(std::move(s)){}
    ~large_write_client_server_sesson() = default;
    acpp::network::async::async_socket_base socket;
    std::string received_data;
    std::string pending_data;

private:    
};  

TEST(AsyncSocketTests, large_write_client_server)
{

    using namespace acpp::network;
    log_debug("*** Socket tests");
    int port = 6666;
    std::string large_message = random_string(10 * 1000 * 1000); // 5 KB message
    //std::cout << "Large message: " << large_message << std::endl;
    std::atomic_bool listen_ok = false;
    auto async_server = [&](){

        acpp::network::async::io_context io;
        using session = large_write_client_server_sesson;
        std::vector<std::unique_ptr<session>> sessions;
        //async_socket_base socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io);

        size_t total = 0;
        size_t total_sent = 0;
        async::async_socket_base server_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
            async::socket_callbacks {
                .on_accepted = [&](async::async_socket_base& server, async::async_socket_base&& accepted_socket) {
                    sessions.emplace_back(std::make_unique<session>(std::move(accepted_socket)));
                    auto& sess = *sessions.back();

                    std::cout << "SERVER ACCEPTED" << std::endl;
                    sess.socket.callbacks(async::socket_callbacks {
                        .on_connected = [&](async::async_socket_base& s) {
                            //std::cout << "SERVER connected fd:" << s.fd() << std::endl;
                            log_debug("SERVER connected fd:" + std::to_string(s.fd()));
                        },
                        .on_disconnected = [&](async::async_socket_base& s) {
                            log_debug("SERVER disconnected fd:" + std::to_string(s.fd()));
                            io.stop();
                            std::erase_if(sessions, [&](auto& i){
                                //return i.fd() == s.fd();
                                return &(*i) == &sess;
                            });
                        },
                        .on_received = [&](async::async_socket_base& s, const char* buf, size_t len){
                            total = total + len;
                            log_debug(std::format("SERVER received fd: {} total: {}", s.fd(), total));

                            //sess.received_data.insert(sess.received_data.end(), buf, buf+len);
                            sess.received_data.append(buf, len);
                            if (sess.pending_data.empty()) {
                                auto n = s.write(buf, len);
                                total_sent += n;
                                log_debug(std::format("SERVER received <write> fd: {} n: {} len: {} total_sent: {}", s.fd(), n, len, total_sent));
                                if (n < len) {

                                    log_debug(std::format("SERVER received ************* insert sess.pending_data.size(): {}", sess.pending_data.size() ));
                                    sess.pending_data.insert(sess.pending_data.end(), buf + n, buf + len );
                                    log_debug(std::format("SERVER received ************* insert done"));
                                }
                            } else {
                                sess.pending_data.insert(sess.pending_data.end(), buf, buf + len);
                            }

                        },
                        .on_sent = [&](async::async_socket_base& s, size_t length) {
                            //log_debug("SERVER on_sent fd:" + std::to_string(s.fd()) + " "  + std::to_string(length));
                            if (!sess.pending_data.empty()) {
                                auto n = s.write(sess.pending_data.c_str(), sess.pending_data.size());
                                total_sent += n;
                                if (n > 0) {
                                    log_debug(std::format("SERVER on_sent fd: {} sent n: {} total_sent: {}", s.fd(), n, total_sent));
                                    sess.pending_data.erase(sess.pending_data.begin(), sess.pending_data.begin()+ n);
                                }
                            }
                        }
                    });
                }
            }
        );
        
        ip_socketaddress addr = ip4_sockaddress("127.0.0.1", port);

        server_socket.bind(to_sockaddr(addr));
        server_socket.listen(5);
        listen_ok = true;
        //auto res = server_socket.accept();
        io.wait_for_input();

        EXPECT_EQ(sessions.size(), 0);
    };

    auto async_client = [&](){
        log_debug("Socket tests client");
        while(!listen_ok) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        acpp::network::async::io_context io;
        std::string received_msg;
        size_t total_sent = 0;
        async::async_socket_base socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
            async::socket_callbacks {
                .on_connected = [&](async::async_socket_base& s) {
                    log_debug("CLIENT Socket connected from AsyncSocketTests.first");
                    total_sent = s.write(large_message.c_str(), large_message.size());
                    log_debug("CLIENT Socket sent initial : " +  std::to_string(total_sent));
                },
                .on_received = [&](async::async_socket_base& s, const char* buf, size_t len){
                    std::string msg(buf, len);
                    //std::cout << "CLIENT Socket received msg: " << msg << std::endl;
                    received_msg.insert(received_msg.end(), buf, buf + len);
                    log_debug("CLIENT Socket received received_msg.size(): " + std::to_string(received_msg.size()));
                    //std::cout << "CLIENT Socket received " << msg << "  from AsyncSocketTests.first" << std::endl;
                    if (received_msg.size() >= large_message.size()) {
                        log_debug("CLIENT Socket received complete message");
                        bool equal = received_msg == large_message;
                        //EXPECT_EQ(received_msg, large_message);
                        log_debug(std::format("CLIENT Socket equal: {}", equal));
                        EXPECT_TRUE(equal);
                        io.stop();
                    }
                },
                /* 3307136 -3308160*/ 
                .on_sent = [&](async::async_socket_base& s, size_t length) {
                    log_debug("CLIENT sent length: " + std::to_string(length));
                    auto n = s.write(large_message.c_str() + total_sent, large_message.size() - total_sent);
                    total_sent = total_sent + n;
                },
                .on_error = [](async::async_socket_base& s, int error_code, const std::string& msg, const std::string& hint) {
                    log_debug("CLIENT Socket error from AsyncSocketTests.first error_code: " 
                        + std::to_string(error_code) + " msg: " + msg + " hint: " +  hint );
                }
            });

        auto c_res = socket.connect(to_sockaddr(ip4_sockaddress("127.0.0.1", port)));
        //std::cout << "CLIENT Socket connected c_res: " << c_res << std::endl;
        std::string msg("hola!");
        io.wait_for_input();
        socket.close();

        std::cout << "CLIENT Socket tests client end" << std::endl;
    };
    
    std::thread server_th(async_server);
    std::thread client_th(async_client);
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    //io.stop();
    if (server_th.joinable())
        server_th.join();
    if (client_th.joinable())
       client_th.join();


}



TEST(AsyncSocketTests, exec)
{
    using namespace acpp::network;
    std::cout << "*** Socket tests" << std::endl;
    int port = 6664;
    acpp::network::async::io_context io;
    auto async_server = [&]() {
        std::cout << "async_server th" << std::endl;
        //std::vector<async_socket_base> sockets;
        io.wait_for_input();
    };
    
    std::thread server_th(async_server);

    io.exec([&]{
        std::cout << "exec " <<std::endl;
        io.stop();
    });

    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (server_th.joinable()) {
        server_th.join();
    }
}



// TEST(AsyncSocketTests, timer)
// {
//     using namespace acpp::network;
//     std::cout << "*** Socket tests" << std::endl;
//     int port = 6664;
//     acpp::network::io_context io;
//     auto async_server = [&]() {
//         std::cout << "async_server th" << std::endl;
//         //std::vector<async_socket_base> sockets;
//         io.wait_for_input();
//     };
    
//     std::thread server_th(async_server);

//     auto timer1 = io.exec_in([&](timer& t){
//         std::cout << "timer exec 1" <<std::endl;
//     }, 1000);

//     auto timer2 = io.exec_in([&](timer& t){
//         std::cout << "timer exec 2" <<std::endl;
//     }, 2000);

//     auto timer3 = io.exec_in([&](timer& t){
//         std::cout << "timer exec 3" <<std::endl;
//         // we stop the service here
//         io.stop();
//     }, 3000);

//     timer2.cancel();

//     std::this_thread::sleep_for(std::chrono::seconds(5));

//     if (server_th.joinable()) {
//         server_th.join();
//     }
// }


TEST(AsyncSocketTests, timer2)
{
    using namespace acpp::network;
    std::cout << "*** Socket tests" << std::endl;
    int port = 6664;
    acpp::network::async::io_context io;
    std::atomic_bool t1_called = false;
    std::atomic_bool t2_called = false;
    std::atomic_bool t3_called = false;

    auto async_server = [&]() {
        std::cout << "async_server th" << std::endl;

        async::timer t1(io, 1, [&](async::timer& t) {
            std::cout << "timer t1 expired" <<std::endl;
            t1_called = true;
        });

        async::timer t2(io, 100, [&](async::timer& t) {
            std::cout << "timer t2 expired" <<std::endl;
            t2_called = true;
        });

        async::timer t3(io, 200, [&](async::timer& t) {
            std::cout << "timer t3 expired" <<std::endl;
            t3_called = true;
            io.stop();
        });

        t2.cancel();
        io.wait_for_input();
    };
    
    std::thread server_th(async_server);

    //std::this_thread::sleep_for(std::chrono::seconds(5));

    if (server_th.joinable()) {
        server_th.join();
    }
    EXPECT_TRUE(t1_called);
    EXPECT_FALSE(t2_called);
    EXPECT_TRUE(t3_called);
}



// ssize_t test_write(int fd, const char* data, size_t len)
// {
//     // Simulate a write operation
//     auto n_write = std::min(len, static_cast<size_t>(test_write_max_write));
//     std::cout << "Writing data: " << std::string(data, n_write) << std::endl;
//     test_write_data += std::string(data, n_write);
//     return n_write;
// }

struct test_writer {
    test_writer():max_write_(8){}

    size_t so_write(const char* buffer, size_t len) {
        // Simulate a write operation
        auto n_write = std::min(len, static_cast<size_t>(max_write_));
        std::string msg(buffer, n_write);
        std::cout << "Writing data: " << msg << std::endl;
        result_ += msg;
        return n_write;
    }
    bool write_enabled() {return true;}

    size_t max_write_;
    std::string result_;

};

TEST(AsyncSocketTests, buffered_writer_test)

{

    const std::string data1{random_string(32)};
    //test1
    if(false)
    {
        test_writer tw;
        acpp::network::buffered_writer<test_writer> bw(tw);

        bw.write(data1.c_str(), data1.size());
        EXPECT_EQ(bw.data_size(), data1.size() - tw.max_write_);
        bw.write_buffered();
        EXPECT_EQ(bw.data_size(), data1.size() - 2 * tw.max_write_);
        bw.write_buffered();
        EXPECT_EQ(bw.data_size(), data1.size() - 3 * tw.max_write_);
        bw.write_buffered();
        EXPECT_EQ(bw.data_size(), 0); // all data written
    }
    //test2
     {
        auto msg = data1;
        int fd = 0;
        size_t buffer_size = 16;
        test_writer tw;
        acpp::network::buffered_writer<test_writer> bw(tw, buffer_size);
        std::cout << "bw.data_size(): " << bw.data_size() << std::endl;

        auto w = bw.write(msg.c_str(), msg.size());// write 8 to output and 16 to buffer

        std::cout << "bw.data_size(): " << bw.data_size() << " w: "<< w << std::endl;
        EXPECT_EQ(w, tw.max_write_+ bw.buffer_size()) << " test2.1";
        EXPECT_EQ(bw.data_size(), buffer_size) << " test2.2";
        //consume written data
        msg.erase(0, w);

        std::cout << "bw.data_size(): " << bw.data_size()  << std::endl;
        w = bw.write_buffered(); // write 8 bytes to output from buffer
        std::cout << "bw.data_size(): " << bw.data_size() << " w: "<< w << std::endl;
        EXPECT_EQ(w, tw.max_write_) << " test2.3"; //now there are test_write_max_write bytes free in buffer
        std::cout << "bw.data_size(): " << bw.data_size() << " w: "<< w << std::endl;
        EXPECT_EQ(bw.data_size(), bw.buffer_size() - tw.max_write_   ) << " test2.4";

        
        std::cout << "msg.size(): ***** " << msg.size() << " bw.data_size(): " << bw.data_size() << std::endl;
        w = bw.write(msg.c_str(), msg.size());// write 0 to output and add 8 to buffer, as buffer is not empty. Buffer is full
        EXPECT_EQ(w, tw.max_write_) << " test2.5"; //now there are test_write_max_write bytes free in buffer
        EXPECT_EQ(bw.data_size(), bw.buffer_size()) << " test2.6";  

        msg.erase(0, w);

        std::cout << "msg.size(): ***** " << msg.size() << " bw.data_size(): " << bw.data_size() << std::endl;
        w = bw.write(msg.c_str(), msg.size());// write 0 to ouput and 0 to buffer as it is full
        EXPECT_EQ(w, 0) << " test2.7"; //full buffer
        EXPECT_EQ(bw.data_size(), bw.buffer_size()) << " test2.8";
        msg.erase(0, w);

        w = bw.write_buffered(); // write 8 bytes to output from buffer
        EXPECT_EQ(w, tw.max_write_) << " test2.9"; 
        w = bw.write_buffered(); // write 8 bytes to output from buffer
        EXPECT_EQ(w, tw.max_write_) << " test2.10"; 

        EXPECT_EQ(msg.size(), 0) << " test2.11 all data should be written";
        EXPECT_EQ(tw.result_, data1) << " test2.12 all data should be written";


    }


    //bw.flush(0);
    //EXPECT_EQ(bw.buffered_size(), 0);
}
