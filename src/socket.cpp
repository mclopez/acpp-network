//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include <acpp-network/socket.h>



void log_error(const std::string& func) {
    std::cerr << "[POSIX] Error in " << func << ": " << strerror(errno) << std::endl;
}

namespace acpp::network {

// socket::socket(const std::string& hostname, const std::string& port) {
//     fd_ = socket(AF_INET, SOCK_STREAM, 0);
//     if (fd_ < 0) {
//         log_error("socket");
//     } else {
//         std::cout << "Socket created with FD: " << fd_ << std::endl;
//     }
// }


socket_base::socket_base(socket_base&& other) noexcept 
    : fd_(other.fd_) {
    other.fd_ = -1; // Steal the resource
}

socket_base& socket_base::operator=(socket_base&& other) noexcept {
    if (this != &other) {
        ::close(fd_); // Clean up current resource
        fd_ = other.fd_;
        other.fd_ = -1; // Steal the resource
    }
    return *this;
}

int get_family(const ip_socketaddress& addr) {
    return std::visit([](auto&& arg) -> int {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ip4_sockaddress>) {
            return AF_INET;
        } else if constexpr (std::is_same_v<T, ip6_sockaddress>) {
            return AF_INET6;
        } else {
            throw std::invalid_argument("Unknown address type");
        }
    }, addr);
}

int get_family(const un_socketaddress& addr) {
    return AF_UNIX;
}

std::string to_string(const ip_socketaddress& addr) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ip4_sockaddress>) {
            return std::string(arg.ip() + ":" + std::to_string(arg.port()));
        } else if constexpr (std::is_same_v<T, ip6_sockaddress>) {
            return std::string(arg.ip() + ":" + std::to_string(arg.port()));
        } else {
            throw std::invalid_argument("Unknown address type");
        }
    }, addr);
}

const sockaddr& to_sockaddr(const ip_socketaddress& addr) {
    // return std::visit([](auto&& arg) -> sockaddr {
    //     using T = std::decay_t<decltype(arg)>;
    //     if constexpr (std::is_same_v<T, struct in_addr>) {
    //         return *reinterpret_cast<sockaddr*>(&sa);
    //     } else if constexpr (std::is_same_v<T, struct in6_addr>) {
    //         return *reinterpret_cast<sockaddr*>(&sa);
    //     } else {
    //         throw std::invalid_argument("Unknown address type");
    //     }
    // }, addr);
    //TODO: better way??
    return reinterpret_cast<const sockaddr&>(addr);
}

} //namespace acpp::network 