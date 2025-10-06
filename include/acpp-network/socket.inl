//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#include <iostream>

#include <acpp-network/socket.h>

namespace acpp::network {

/*
bool socket::connect(const addres_type& ad) {
    if (!is_valid()) return false;
    
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result;
    
    int s = getaddrinfo("localhost", "8080", &hints, &result);
    if (s != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(s) << std::endl;
        return false;
    }

    bool success = ::connect(fd_, result->ai_addr, result->ai_addrlen) == 0;
    freeaddrinfo(result);

    if (success) {
        std::cout << "Socket connected successfully." << std::endl;
    } else {
        log_error("connect");
    }
    return success;
}

size_t socket::send(const char* data, size_t len) {
    if (!is_valid()) return 0;
    ssize_t sent = ::send(fd_, data, len, 0);
    if (sent < 0) {
        log_error("send");
        return 0;
    }
    return static_cast<size_t>(sent);
}

size_t socket::receive(char* buffer, size_t len) {
    if (!is_valid()) return 0;
    ssize_t received = ::recv(fd_, buffer, len, 0);
    if (received < 0) {
        log_error("receive");
        return 0;
    }
    return static_cast<size_t>(received);
}



bool Socket::resolve_address() {
    // This private helper would contain the getaddrinfo logic, 
    // which is internal to the module.
    return true; 
}


stream_socket::stream_socket(int fd)    
    : socket_ (fd) {
}
*/


template<typename Address, int Protocol>
stream_socket<Address, Protocol>::stream_socket(int fd):socket_(fd) {

}


template<typename Address, int Protocol>
bool stream_socket<Address, Protocol>::connect(const address_type& adr) {
    socket_.create_impl(get_family(adr), SOCK_STREAM, protocol);
    return ::connect(socket_.fd(), &to_sockaddr(adr), sizeof(sockaddr)) == 0;
}

template<typename Address, int Protocol>
size_t stream_socket<Address, Protocol>::send(const char* data, size_t len) {
    return ::send(socket_.fd(), data, len, 0);
}

template<typename Address, int Protocol>
size_t stream_socket<Address, Protocol>::receive(char* buffer, size_t len) {
    return ::recv(socket_.fd(), buffer, len, 0);
}

template<typename Address, int Protocol>
int stream_socket<Address, Protocol>::bind(const address_type& ad) {
    if (!socket_.valid()) {
        socket_.create_impl(get_family(ad), SOCK_STREAM, 0);
    }
    int res = ::bind(socket_.fd(), reinterpret_cast<const struct sockaddr*>(&ad), sizeof(sockaddr));
    if (res < 0) {
        log_error("bind");
    }
    return res;
}

template<typename Address, int Protocol>
int stream_socket<Address, Protocol>::listen(int backlog) {
    if (!socket_.valid()) {
        std::cerr << "Socket not valid for listen" << std::endl;
        return -1;
    }
    int res = ::listen(socket_.fd(), backlog);
    if (res < 0) {
        log_error("listen");
    }
    return res;
}

template<typename Address, int Protocol>
stream_socket<Address, Protocol> stream_socket<Address, Protocol>::accept() {
    if (!socket_.valid()) {
        std::cerr << "Socket not valid for accept" << std::endl;
        return stream_socket();
    }
    int client_fd = ::accept(socket_.fd(), nullptr, nullptr);
    if (client_fd < 0) {
        log_error("accept");
        return stream_socket();
    }
    return stream_socket(client_fd);
}


} //namespace acpp::network 
