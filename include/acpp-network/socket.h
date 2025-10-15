//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#pragma once




#include <variant>
#include <stdexcept>
#include <string>
#include <functional>
#include <cstring>


#include <acpp-network/address.h>
#include <acpp-network/socket_base.h>

void log_error(const std::string& msg);

namespace acpp::network {



//const sockaddr& to_sockaddr(const ip_socketaddress& addr);

template<typename Address, int Protocol = 0>
class stream_socket {
public:
    using address_type = Address;
    constexpr static auto socket_type = SOCK_STREAM;
    constexpr static auto protocol = Protocol;

    stream_socket() = default;
    stream_socket(int fd);

    ~stream_socket()=default;

    bool connect(const address_type& adr);
    size_t send(const char* data, size_t len);
    size_t receive(char* buffer, size_t len);

    int bind(const address_type& ad);
    int listen(int backlog = 5);
    stream_socket accept();  


private:
    socket_base socket_;
};


template<typename Address, int Protocol = 0>
class datagram_socket {
public:
    using address_type = Address;
    constexpr static auto socket_type = SOCK_DGRAM;
    constexpr static auto protocol = Protocol;

    datagram_socket() = default;
    datagram_socket(int fd);

    ~datagram_socket()=default;

    //bool connect(const address_type& ad);
    size_t send_to(const address_type& addr, const char* data, size_t len);
    size_t recv_from(address_type& addr, char* buffer, size_t len );

    int bind(const address_type& ad);

private:
    socket_base socket_;
};

using tpc_socket = stream_socket<ip_socketaddress>;
using udp_socket = datagram_socket<ip_socketaddress>;

template<typename SocketAddress>
using resolve_address_callback = std::function<void(SocketAddress& addr, bool& success)>;

//template<typename SocketAddress>
//void resolve_host(const std::string& host, const std::string& service, resolve_address_callback<SocketAddress>&& callback);
template<typename Socket, typename Address = Socket::address_type>
void resolve_host(const std::string& host, const std::string& service, resolve_address_callback<Address>&& callback);


template<typename Address, int Protocol = 0>
class async_stream_socket {
public:
    using address_type = Address;
    constexpr static auto socket_type = SOCK_STREAM;
    constexpr static auto protocol = Protocol;

    async_stream_socket() = default;
    async_stream_socket(int fd);

    ~async_stream_socket()=default;

    bool connect(const address_type& adr);
//    size_t send(const char* data, size_t len);
//    size_t receive(char* buffer, size_t len);

//    int bind(const address_type& ad);
//    int listen(int backlog = 5);
//    stream_socket accept();  


private:
    socket_base socket_;
};




} // namespace acpp::network 
