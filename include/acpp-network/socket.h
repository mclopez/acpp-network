//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#pragma once

#include <sys/socket.h> 
#include <netinet/in.h> // For sockaddr_in structure
#include <sys/un.h>    // For sockaddr_un structure
#include <variant>
#include <stdexcept>
#include <string>

void log_error(const std::string& msg);

namespace acpp::network {

template<int Family> class address;

template<> struct address<PF_INET>  { using type = struct in_addr; };
template<> struct address<PF_INET6> { using type = struct in6_addr; };
template<> struct address<PF_UNIX>  { using type = struct iun_addr; };

class socket_base {
public:    
    socket_base() = default;
    socket_base(int fd):fd_(fd){}
    ~socket_base() {
        close();
    }

    socket_base(const socket_base&) = delete;
    socket_base& operator=(const socket_base&) = delete;

    socket_base(socket_base&& other) noexcept;
    socket_base& operator=(socket_base&& other) noexcept;

    void create_impl(int domain, int type, int protocol) {
        if (valid())
            close();
        fd_ = ::socket(domain, type, protocol);
    }

    bool valid() const {return fd_ != invalid_fd;}

    int fd() const { return fd_; }
    void close() {
        if (fd_ != invalid_fd) {
            ::close(fd_);
            fd_ = invalid_fd;
        }
    }

private:
    constexpr static int invalid_fd = -1;
    int fd_ = invalid_fd; 
};

//using IpAddress = std::variant<struct in_addr, struct in6_addr>;
using IpAddress = std::variant<sockaddr_in, sockaddr_in6>;

// class ip_address {
// public:
//     sockaddr_in& as_in() { 
//         storage_.ss_family = AF_INET; 
//         return reinterpret_cast<sockaddr_in&>(&storage_);
//     }
//     sockaddr_in6& as_in6() { 
//         storage_.ss_family = AF_INET6; 
//         sockaddr_in6* ipv6_addr = reinterpret_cast<sockaddr_in6*>(&storage_);
//         return *ipv6_addr; 
//     }
//     int get_family() const { return storage_.ss_family; }
// private:
//     sockaddr_storage storage_;
// };

using UnixAddress = std::variant<sockaddr_un>;

int get_family(const IpAddress& addr);
int get_family(const UnixAddress& addr);

const sockaddr& to_sockaddr(const IpAddress& addr);

template<typename Address, int Protocol = 0>
class stream_socket {
public:
    using address_type = Address;
    constexpr static auto protocol = Protocol;

    stream_socket() = default;
    stream_socket(int fd);

    ~stream_socket()=default;

    bool connect(const address_type& ad);
    size_t send(const char* data, size_t len);
    size_t receive(char* buffer, size_t len);

    int bind(const address_type& ad);
    int listen(int backlog = 5);
    stream_socket accept();  

// int bind(int socket, const struct sockaddr *address, socklen_t address_len);
// int accept(int socket, struct sockaddr *restrict address, socklen_t *restrict address_len);

private:
    socket_base socket_;
    bool resolve_address(); 
};



} // namespace acpp::network 
