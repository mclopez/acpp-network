#include <iostream>
#include <thread>

#include <gtest/gtest.h> // googletest header file  

#include <acpp-network/socket.h>
#include <acpp-network/socket.inl>


TEST(SocketTests, tcp_echo)
{
    using namespace acpp::network;
    std::cout << "*** Socket tests" << std::endl;
    int port = 6664;
    std::thread server_th([port](){
        std::cout << "Socket tests th" << std::endl;

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
    });

    std::thread client_th([port](){
       std::cout << "Socket tests client" << std::endl;
       stream_socket<ip_socketaddress> socket;
       ip_socketaddress adr = ip4_sockaddress("127.0.0.1", port);

       socket.connect(adr);
       const char* msg = "Hello, Echo Server!";
       socket.send(msg, strlen(msg));
       char buffer[1024];
       auto bytes_received = socket.receive(buffer, sizeof(buffer));
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
        using tcp_socket = stream_socket<ip_socketaddress>;
        resolve_host<tcp_socket, tcp_socket::address_type>("www.example.com", "80", [](const ip_socketaddress& addr, bool& success){
        std::cout << "Resolved: "  << ":" << to_string(addr) << std::endl;
        tcp_socket socket;   
        success = socket.connect(addr);
        if (success) {
            std::string msg = 
                "GET / HTTP/1.1\r\n" 
                "Host: www.example.com\r\n" 
                "User-Agent: Mozilla/5.0\r\n" 
                "Accept: */*\r\n" 
                "Connection: close\r\n\r\n";
            socket.send(msg.data(), msg.size());
            char buffer[4096];
            auto bytes_received = socket.receive(buffer, sizeof(buffer)-1);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0'; // Null-terminate the received data
                std::cout << "Received " << bytes_received << " bytes:\n" << buffer;
                EXPECT_NE(std::string(buffer).find("HTTP/1.1 200 OK"), std::string::npos);
            }
        }
     });
 }



TEST(SocketTests, udp_echo)
{
    using namespace acpp::network;
    std::cout << "Socket tests" << std::endl;
    std::thread server_th([](){
        std::cout << "UDP socket tests th" << std::endl;

        udp_socket socket;
        udp_socket::address_type adr = ip4_sockaddress("127.0.0.1", 2000);
        socket.bind(adr);
        udp_socket::address_type client_adr;
        char buffer[1024];
        auto bytes_received = socket.recv_from(client_adr, buffer, sizeof(buffer)); 

        if (bytes_received > 0) {
            std::cout << "Server received: " << std::string(buffer, bytes_received) << std::endl;
            udp_socket client_socket;
            client_socket.send_to(client_adr, buffer, bytes_received);
        }
    });

    std::thread client_th([](){
        std::cout << "UDP socket test client th" << std::endl;
        udp_socket socket;
        udp_socket::address_type adr = ip4_sockaddress("127.0.0.1", 2000);

        const char* msg = "Hello, Echo Server!";
        socket.send_to(adr, msg, strlen(msg));
        char buffer[1024];
        udp_socket::address_type adr2;
        auto bytes_received = socket.recv_from(adr2, buffer, sizeof(buffer));
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


