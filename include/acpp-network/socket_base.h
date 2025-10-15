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
#include <functional>

namespace acpp::network {

class socket_exception  : public std::exception {
public:    
    socket_exception(int error_code, const std::string_view hint);
    socket_exception(const std::string_view hint);
    const char* what() const noexcept override { return msg_.c_str();};
    int error_code() { return error_code_;}
private:
    std::string msg_;
    int error_code_;
};

class socket_base {
public:    
    socket_base();
    explicit socket_base(int fd);
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

private:
    static const int invalid_fd;
    int64_t fd_ = invalid_fd; 
};

class io_context;
class async_socket_base;
struct socket_base_pimpl;

struct socket_callbacks {
public:

    using on_accepted_callback = std::function<void(async_socket_base&, async_socket_base&&)>;
    using on_disconnected_callback = std::function<void(async_socket_base&)>;
    using on_connected_callback = std::function<void(async_socket_base&)>;
    using on_received_callback = std::function<void(async_socket_base&, const char* buffer, size_t length) >;
    using on_sent_callback = std::function<void(async_socket_base&)>;
    using on_error_callback = std::function<void(async_socket_base&, int error, const std::string& error_message, const std::string& hint)>; 

    on_connected_callback on_connected;
    on_disconnected_callback on_disconnected;
    on_received_callback on_received;
    on_sent_callback on_sent;
    on_accepted_callback on_accepted;
    on_error_callback on_error;
};

class async_socket_base {
public:
    friend class socket_base_pimpl;
    friend class io_context;
    friend class io_context_pimpl;
    
    using fd_type = int64_t;
    //async_socket_base();
    async_socket_base(int domain, int type, int protocol, io_context& io, socket_callbacks&& callbacks = socket_callbacks{});
    async_socket_base(int domain, int type, int protocol, fd_type fd, io_context& io, socket_callbacks&& callbacks = socket_callbacks{});
    async_socket_base(const socket_base&) = delete;
    async_socket_base(async_socket_base&& other) noexcept;

    ~async_socket_base();


    async_socket_base& operator=(const async_socket_base&) = delete;

    async_socket_base& operator=(async_socket_base&& other) noexcept;


    bool bind(const sockaddr& adr);
    int listen(int backlog=0);

    //TODO: this should return nothing, as it is async, just throws in case of error.
    bool connect(const sockaddr& adr);
    void callbacks(socket_callbacks&& calbacks);
    socket_callbacks& callbacks();

    size_t write(const char* buffer, size_t);

    void close();
  
    bool valid() const;
    int64_t fd();


private:
    std::unique_ptr<socket_base_pimpl> pimpl_;
};

class timer_impl;

class timer {
public:
    friend class io_context_pimpl;
    using on_timeout_callback = std::function<void(timer&)>; 

    timer(io_context& io, int milliseconds, on_timeout_callback&& cb={});
    timer(timer&& other);
    ~timer();

    void cancel();
private:
    std::unique_ptr<timer_impl> pimpl_;
};


struct io_context_pimpl;

class io_context {
public:
    friend class async_socket_base;
    friend class socket_base_pimpl;
    friend class timer;
    friend class timer_impl;

    io_context();
    ~io_context();

    void wait_for_input();
    void exec(std::function<void()>&&);



    void remove_socket(async_socket_base& as);

    void add_socket(async_socket_base& as);

    void stop();
    int64_t fd() const;

private:
    std::unique_ptr<io_context_pimpl> pimpl_;
};



void log_error(const std::string& func);

} //namespace acpp::network 