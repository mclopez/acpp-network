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

        IpAddress adr = sockaddr_in{};
        std::get<sockaddr_in>(adr).sin_family = AF_INET;                 // IPv4
        std::get<sockaddr_in>(adr).sin_addr.s_addr = INADDR_ANY;         // Bind to all local interfaces
        std::get<sockaddr_in>(adr).sin_port = htons(6666);               // Port to listen on (converted to network byte order)

        // ip_address adr;
        // adr.as_in().sin_family = AF_INET;                 // IPv4
        // adr.as_in().sin_addr.s_addr = INADDR_ANY;         // Bind to all local interfaces
        // adr.as_in().sin_port = htons(6666);               // Port to listen on (converted to network byte order)

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
        IpAddress adr = sockaddr_in{};
        std::get<struct sockaddr_in>(adr).sin_family = AF_INET;                 // IPv4
        std::get<struct sockaddr_in>(adr).sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Connect to localhost
        std::get<struct sockaddr_in>(adr).sin_port = htons(6666);               // Port to connect to (converted to network byte order)
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
 #include <arpa/inet.h>

struct ip4_sockaddress {
    ip4_sockaddress(const std::string& ip, in_port_t port) {  
        addr.sin_family = AF_INET;                 // IPv4
        //addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Connect to localhost
        int r = inet_pton(AF_INET, ip.c_str(), &addr.sin_addr.s_addr);
        if (r <= 0) {
            throw std::invalid_argument("Invalid IP address format");
        }
        addr.sin_port = htons(port);               // Port to connect to (converted to network byte order)
    }
    int family() const { return addr.sin_family; }
    std::string ip(){
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
        return std::string(ip_str); 
    }
    in_port_t port() const {            
        return ntohs(addr.sin_port);
    }   
    sockaddr_in addr;
};

struct ip6_sockaddress {
    ip6_sockaddress(const std::string& ip, in_port_t port) {  
        std::memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;                 // IPv4
        //addr.sin6_addr.s_addr = htonl(INADDR_LOOPBACK); // Connect to localhost

        addr.sin6_flowinfo = 0;
        addr.sin6_scope_id = 0;

        int result = inet_pton(AF_INET6, ip.c_str(), &addr.sin6_addr);
        if (result <= 0) {
            throw std::invalid_argument("Invalid IPv6 address format"); 
        }
        addr.sin6_port = htons(port); 
    }
    int family() const { return addr.sin6_family; }
    std::string ip(){
        char ip_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str));
        return std::string(ip_str); 
    }
    in_port_t port() const {            
        return ntohs(addr.sin6_port);
    }
    sockaddr_in6 addr;
};

using IpAddress_ = std::variant<ip4_sockaddress, ip6_sockaddress>;


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
        IpAddress_ adr = ip4_sockaddress("127.0.0.1", 666);
        auto a = std::get<ip4_sockaddress>(adr);
        EXPECT_EQ(a.family(), AF_INET);
        EXPECT_EQ(a.ip(), "127.0.0.1");
        EXPECT_EQ(a.port(), 666); 
    }
    {
        IpAddress_ adr = ip6_sockaddress("::1", 666);
        auto a = std::get<ip6_sockaddress>(adr);
        EXPECT_EQ(a.family(), AF_INET6);
        EXPECT_EQ(a.ip(), "::1");
        EXPECT_EQ(a.port(), 666); 
    }

}
