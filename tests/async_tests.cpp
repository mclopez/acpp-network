
#include <iostream>
#include <thread>
#include <random>

#include <gtest/gtest.h> // googletest header file  

#include <acpp-network/socket.h>
#include <acpp-network/socket.inl>

TEST(AsyncSocketTests, simple_client_server)
{

    using namespace acpp::network;
    std::cout << "*** Socket tests" << std::endl;
    int port = 6665;
    auto sync_server = [port](){
        std::cout << "sync_server th" << std::endl;

        stream_socket<ip_socketaddress> server_socket;

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

        acpp::network::io_context io;
        std::vector<async_socket_base> sockets;
        //async_socket_base socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io);

        std::cout << "async_server th 2" << std::endl;

        async_socket_base server_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
            socket_callbacks {
                .on_accepted = [&](async_socket_base& server, async_socket_base&& accepted_socket) {
                    std::cout << "ACCEPTED" << std::endl;
                    accepted_socket.callbacks(socket_callbacks {
                        .on_connected = [](async_socket_base& s) {
                            std::cout << "SERVER Async server socket connected" << std::endl;
                        },
                        .on_disconnected = [&](async_socket_base& s) {
                            std::cout << "SERVER Async server socket disconnected" << std::endl;
                            io.stop();
                            std::erase_if(sockets, [&](auto& i){
                                //return i.fd() == s.fd();
                                return &i == &s;
                            });
                        },
                        .on_received = [&](async_socket_base& s, const char* buf, size_t len){
                            std::string msg(buf, len);
                            std::cout << "SERVER  Async server socket  received " << msg << "  from AsyncSocketTests.first" << std::endl;
                            //io.stop();
                            //std::string msg2("hello back!");
                            s.write(msg.c_str(), msg.size());
                            //s.read();
                        },
                        .on_sent = [&](async_socket_base& s, size_t length) {
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
       stream_socket<ip_socketaddress> socket;
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
        acpp::network::io_context io;
        async_socket_base socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
            socket_callbacks {
                .on_connected = [](async_socket_base& s) {
                    std::cout << "CLIENT Socket connected from AsyncSocketTests.first" << std::endl;
                    std::string msg("hola");
                    //s.read();
                    s.write(msg.c_str(), msg.size());
                },
                .on_received = [&](async_socket_base& s, const char* buf, size_t len){
                    std::string msg(buf, len);
                    std::cout << "CLIENT Socket received " << msg << "  from AsyncSocketTests.first" << std::endl;
                    EXPECT_EQ(msg, "hola");
                    io.stop();
                },
                .on_sent = [](async_socket_base& s, size_t length) {
                    std::cout << "CLIENT Socket sent from AsyncSocketTests.fir st" << std::endl;
                },
                .on_error = [](async_socket_base& s, int error_code, const std::string& msg, const std::string& hint) {
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
    large_write_client_server_sesson(acpp::network::async_socket_base&& s):socket(std::move(s)){}
    ~large_write_client_server_sesson() = default;
    acpp::network::async_socket_base socket;
    std::string received_data;
private:    
};  

TEST(AsyncSocketTests, large_write_client_server)
{

    using namespace acpp::network;
    std::cout << "*** Socket tests" << std::endl;
    int port = 6665;
    std::string large_message = random_string(50000); // 5 KB message
    std::cout << "Large message: " << large_message << std::endl;

    auto async_server = [&](){
        std::cout << "async_server th" << std::endl;

        acpp::network::io_context io;
        using session = large_write_client_server_sesson;
        std::vector<std::unique_ptr<session>> sessions;
        //async_socket_base socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io);

        std::cout << "async_server th 2" << std::endl;

        async_socket_base server_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
            socket_callbacks {
                .on_accepted = [&](async_socket_base& server, async_socket_base&& accepted_socket) {
                    sessions.emplace_back(std::make_unique<session>(std::move(accepted_socket)));
                    auto& sess = *sessions.back();

                    std::cout << "SERVER ACCEPTED" << std::endl;
                    sess.socket.callbacks(socket_callbacks {
                        .on_connected = [&](async_socket_base& s) {
                            std::cout << "SERVER connected fd:" << s.fd() << std::endl;
                        },
                        .on_disconnected = [&](async_socket_base& s) {
                            std::cout << "SERVER disconnected fd:" << s.fd() << std::endl;
                            io.stop();
                            std::erase_if(sessions, [&](auto& i){
                                //return i.fd() == s.fd();
                                return &(*i) == &sess;
                            });
                        },
                        .on_received = [&](async_socket_base& s, const char* buf, size_t len){
                            std::string msg(buf, len);
                            std::cout << "SERVER  received  fd:" << s.fd() << " " << msg << std::endl;
                            //io.stop();
                            //std::string msg2("hello back!");

                            sess.received_data += msg;

                            s.write(buf, len);

                        },
                        .on_sent = [&](async_socket_base& s, size_t length) {
                            // std::cout << "SERVER sent length:  fd:" << s.fd() << " " << length << std::endl;
                            // sess.pending_data_to_send.erase(0, length);
                            // if (!sess.pending_data_to_send.empty()) {
                            //     s.write(sess.pending_data_to_send.c_str(), sess.pending_data_to_send.size());   
                            // }
                        }
                    });
                    //std::cout << "SERVER socket.callbacks().on_read " << (sockets.back().callbacks().on_received? "read assigned":"read not assigned") << std::endl;

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

        EXPECT_EQ(sessions.size(), 0);
    };

    auto async_client = [&](){
        std::cout << "Socket tests client" << std::endl;
        acpp::network::io_context io;
        std::string received_msg;
        async_socket_base socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
            socket_callbacks {
                .on_connected = [&](async_socket_base& s) {
                    std::cout << "CLIENT Socket connected from AsyncSocketTests.first" << std::endl;
                    auto n = s.write(large_message.c_str(), large_message.size());
                    std::cout << "CLIENT Socket sent initial n: " << n << std::endl;
                },
                .on_received = [&](async_socket_base& s, const char* buf, size_t len){
                    std::string msg(buf, len);
                    std::cout << "CLIENT Socket received msg: " << msg << std::endl;
                    //received_msg = received_msg + msg;
                    received_msg.insert(received_msg.end(), buf, buf + len);
                    std::cout << "CLIENT Socket received received_msg.size(): " << received_msg.size() << std::endl;
                    //std::cout << "CLIENT Socket received " << msg << "  from AsyncSocketTests.first" << std::endl;
                    if (received_msg.size() >= large_message.size()) {
                        std::cout << "CLIENT Socket received complete message. \n" 
                            //<< received_msg 
                            << std::endl;
                        EXPECT_EQ(received_msg, large_message);
                        io.stop();
                    }
                },
                .on_sent = [&](async_socket_base& s, size_t length) {
                    std::cout << "CLIENT sent length: " << length << std::endl;
                },
                .on_error = [](async_socket_base& s, int error_code, const std::string& msg, const std::string& hint) {
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


TEST(AsyncSocketTests, exec)
{
    using namespace acpp::network;
    std::cout << "*** Socket tests" << std::endl;
    int port = 6664;
    acpp::network::io_context io;
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
    acpp::network::io_context io;
    std::atomic_bool t1_called = false;
    std::atomic_bool t2_called = false;
    std::atomic_bool t3_called = false;

    auto async_server = [&]() {
        std::cout << "async_server th" << std::endl;

        timer t1(io, 1, [&](timer& t) {
            std::cout << "timer t1 expired" <<std::endl;
            t1_called = true;
        });

        timer t2(io, 100, [&](timer& t) {
            std::cout << "timer t2 expired" <<std::endl;
            t2_called = true;
        });

        timer t3(io, 200, [&](timer& t) {
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

