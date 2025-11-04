
#include <iostream>
#include <thread>

#include <gtest/gtest.h> // googletest header file  

#include <acpp-network/socket.h>
#include <acpp-network/socket.inl>








TEST(AsyncSocketTests, first)
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
        //std::vector<async_socket_base> sockets;
        async_socket_base socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io);

        std::cout << "async_server th 2" << std::endl;

        async_socket_base server_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
            socket_callbacks {
                .on_accepted = [&](async_socket_base& server, async_socket_base&& accepted_socket) {
                    std::cout << "ACCEPTED" << std::endl;
                    accepted_socket.callbacks(socket_callbacks {
                        .on_connected = [](async_socket_base& s) {
                            std::cout << "SERVER Async server socket connected" << std::endl;
                        },
                        .on_received = [&](async_socket_base& s, const char* buf, size_t len){
                            std::string msg(buf, len);
                            std::cout << "SERVER  Async server socket  received " << msg << "  from AsyncSocketTests.first" << std::endl;
                            //io.stop();
                            //std::string msg2("hello back!");
                            s.write(msg.c_str(), msg.size());
                        },
                        .on_sent = [&](async_socket_base& s){
                            std::cout << "SERVER Async server socket sent from AsyncSocketTests.first" << std::endl;
                            io.stop();
                        }
                    });
                    socket = std::move(accepted_socket);
                    std::cout << "SERVER socket.callbacks().on_read " << (socket.callbacks().on_received? "read assigned":"read not assigned") << std::endl;


                    socket.read(); //start reading
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
            socket_callbacks{
                .on_connected = [](async_socket_base& s) {
                    std::cout << "CLIENT Socket connected from AsyncSocketTests.first" << std::endl;
                    std::string msg("hola");
                    s.read();
                    s.write(msg.c_str(), msg.size());
                },
                .on_received = [&](async_socket_base& s, const char* buf, size_t len){
                    std::string msg(buf, len);
                    std::cout << "CLIENT Socket received " << msg << "  from AsyncSocketTests.first" << std::endl;
                    EXPECT_EQ(msg, "hola");
                    io.stop();
                },
                .on_sent = [](async_socket_base& s){
                    std::cout << "CLIENT Socket sent from AsyncSocketTests.first" << std::endl;
                }
            });

        auto c_res = socket.connect(to_sockaddr(ip4_sockaddress("127.0.0.1", port)));
        std::cout << "CLIENT Socket connected c_res: " << c_res << std::endl;
        std::string msg("hola!");
        io.wait_for_input();

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
    auto async_server = [&](){
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

    if (server_th.joinable())
        server_th.join();
}

