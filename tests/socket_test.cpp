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

        stream_socket<IpAddress> server_socket;

        IpAddress adr = ip4_sockaddress("127.0.0.1", 6666);


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
        stream_socket<IpAddress> socket;
        IpAddress adr = ip4_sockaddress("127.0.0.1", 6666);

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


//using IpAddress_ = std::variant<sockaddr_in, sockaddr_in6>;

// int get_family(const IpAddress_& addr) {
//     return std::visit([](auto&& arg) -> int {
//         using T = std::decay_t<decltype(arg)>;
//         if constexpr (std::is_same_v<T, in_addr>) {
//             return AF_INET;
//         } else if constexpr (std::is_same_v<T, in6_addr>) {
//             return AF_INET6;
//         } else {
//             throw std::invalid_argument("Unknown address type");
//         }
//     }, addr);
// }


// TEST(SocketTests, ip)
// {
//     using namespace acpp::network;
//     IpAddress_ adr = sockaddr_in{};
//     std::get<sockaddr_in>(adr).sin_family = AF_INET;                 // IPv4
//     std::get<sockaddr_in>(adr).sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Connect to localhost
//     std::get<sockaddr_in>(adr).sin_port = htons(6666);               // Port to connect to (converted to network byte order)

//     EXPECT_EQ(std::get<sockaddr_in>(adr).sin_family, AF_INET);
//     EXPECT_EQ(std::get<sockaddr_in>(adr).sin_addr.s_addr, htonl(INADDR_LOOPBACK));
//     EXPECT_EQ(std::get<sockaddr_in>(adr).sin_port, htons(6666)); 
//     EXPECT_EQ(get_family(adr), AF_INET);
// }

TEST(SocketTests, ip)
{
    using namespace acpp::network;
    {
        IpAddress adr = ip4_sockaddress("127.0.0.1", 666);
        auto a = std::get<ip4_sockaddress>(adr);
        EXPECT_EQ(a.family(), AF_INET);
        EXPECT_EQ(a.ip(), "127.0.0.1");
        EXPECT_EQ(a.port(), 666); 
    }
    {
        IpAddress adr = ip6_sockaddress("::1", 666);
        auto a = std::get<ip6_sockaddress>(adr);
        EXPECT_EQ(a.family(), AF_INET6);
        EXPECT_EQ(a.ip(), "::1");
        EXPECT_EQ(a.port(), 666); 
    }

}
