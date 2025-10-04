#include <socket.h>
#include <iostream>

namespace acpp::network {


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


} //namespace acpp::network 
