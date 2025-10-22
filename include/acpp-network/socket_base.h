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

#include <memory>

namespace acpp::network {


class socket_base {
public:    
    socket_base();
    socket_base(int fd);
    ~socket_base();

    socket_base(const socket_base&) = delete;
    socket_base& operator=(const socket_base&) = delete;

    socket_base(socket_base&& other) noexcept;
    socket_base& operator=(socket_base&& other) noexcept;

    void create_impl(int domain, int type, int protocol);

    bool connect(const sockaddr& ad);


    bool valid() const {return fd_ != invalid_fd;}

    int64_t fd() const { return fd_; }
    void close();

protected:
    static const int invalid_fd;
    int64_t fd_ = invalid_fd; 
};

class io_context;
struct socket_base_pimpl;

class async_socket_base: public socket_base {
public:
    async_socket_base(io_context& io);
    ~async_socket_base();
    void create_impl(int domain, int type, int protocol);

    bool connect(const sockaddr& adr);
private:
    io_context* io_;
    std::unique_ptr<socket_base_pimpl> pimpl_;
};

class io_context {
public:
    io_context();

    void wait_for_input();
    void remove_socket(socket_base& as);

    void add_socket(socket_base& as);

    void stop();

private:
    std::atomic_bool run;
    HANDLE hIOCP_ = INVALID_HANDLE_VALUE;

};



void log_error(const std::string& func);

} //namespace acpp::network 