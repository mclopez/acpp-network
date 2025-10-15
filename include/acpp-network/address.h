//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#pragma once

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ws2def.h>
#pragma comment(lib, "Ws2_32.lib")
#include <windows.h>
#include <io.h>
#include <mswsock.h>

using in_port_t = decltype(sockaddr_in::sin_port);

#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>    // For sockaddr_un structure
#include <arpa/inet.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#endif


#include <string>
#include <variant>
#include <stdexcept>


namespace acpp::network {


template<int Family> class address;

template<> struct address<PF_INET>  { using type = struct in_addr; };
template<> struct address<PF_INET6> { using type = struct in6_addr; };
template<> struct address<PF_UNIX>  { using type = struct iun_addr; };


struct ip4_sockaddress {
    ip4_sockaddress() = default;
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
    std::string ip()const {
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
    ip6_sockaddress() = default;
    ip6_sockaddress(const std::string& ip, in_port_t port) {  
        memset(&addr, 0, sizeof(addr));
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
    std::string ip() const {
        char ip_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr.sin6_addr, ip_str, sizeof(ip_str));
        return std::string(ip_str); 
    }
    in_port_t port() const {            
        return ntohs(addr.sin6_port);
    }
    sockaddr_in6 addr;
};

using ip_socketaddress = std::variant<ip4_sockaddress, ip6_sockaddress>;

#ifndef _WIN32
using un_socketaddress = std::variant<sockaddr_un>;
int get_family(const un_socketaddress& addr);
#endif

int get_family(const ip_socketaddress& addr);

void from_sockaddr(const sockaddr& sa, ip4_sockaddress& out_addr);
void from_sockaddr(const sockaddr& sa, ip6_sockaddress& out_addr);
void from_sockaddr(const sockaddr& sa, ip_socketaddress& out_addr);

const sockaddr& to_sockaddr(const ip4_sockaddress& addr);
const sockaddr& to_sockaddr(const ip6_sockaddress& addr);
const sockaddr& to_sockaddr(const ip_socketaddress& addr);

std::string to_string(const ip_socketaddress& addr);

}// namespace acpp::network 