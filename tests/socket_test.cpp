#include <iostream>
#include <thread>

#include <gtest/gtest.h> // googletest header file  
#include <acpp-network/socket.h>
#include <acpp-network/socket.inl>


TEST(SocketTests, echo)
{
    using namespace acpp::network;
    std::cout << "Socket tests" << std::endl;
    std::thread server_th([](){
        std::cout << "Socket tests th" << std::endl;

        stream_socket<ip_socketaddress> server_socket;

        ip_socketaddress adr = ip4_sockaddress("127.0.0.1", 6666);


        server_socket.bind(adr);
        server_socket.listen(5);
        auto client_socket = server_socket.accept();
        char buffer[1024];
        ssize_t bytes_received = client_socket.receive(buffer, sizeof(buffer));
        if (bytes_received > 0) {
            std::cout << "Server received: " << std::string(buffer, bytes_received) << std::endl;
            client_socket.send(buffer, bytes_received); // Echo back
        }
    });

    std::thread client_th([](){
        std::cout << "Socket tests client" << std::endl;
        stream_socket<ip_socketaddress> socket;
        ip_socketaddress adr = ip4_sockaddress("127.0.0.1", 6666);

        socket.connect(adr);
        const char* msg = "Hello, Echo Server!";
        socket.send(msg, strlen(msg));
        char buffer[1024];
        ssize_t bytes_received = socket.receive(buffer, sizeof(buffer));
        if (bytes_received > 0) {
            std::string msg(buffer, bytes_received);
            std::cout << "Client received: " << msg << std::endl;
            EXPECT_EQ(msg, "Hello, Echo Server!");
        }
    });

    if (server_th.joinable())
        server_th.join();
    if (client_th.joinable())
        client_th.join();
}


TEST(SocketTests, ip)
{
    using namespace acpp::network;
    {
        ip_socketaddress adr = ip4_sockaddress("127.0.0.1", 666);
        auto a = std::get<ip4_sockaddress>(adr);
        EXPECT_EQ(a.family(), AF_INET);
        EXPECT_EQ(a.ip(), "127.0.0.1");
        EXPECT_EQ(a.port(), 666); 
    }
    {
        ip_socketaddress adr = ip6_sockaddress("::1", 666);
        auto a = std::get<ip6_sockaddress>(adr);
        EXPECT_EQ(a.family(), AF_INET6);
        EXPECT_EQ(a.ip(), "::1");
        EXPECT_EQ(a.port(), 666); 
    }

}


TEST(SocketTests, resolve_host)
{
    using namespace acpp::network;
    resolve_host<ip_socketaddress>("www.example.com", "80", [](const ip_socketaddress& addr, bool& success){
//       std::cout << "Resolved: " << addr.ip() << ":" << addr.port() << std::endl;
       std::cout << "Resolved: "  << ":" << to_string(addr) << std::endl;
       success = true;
    });
}
