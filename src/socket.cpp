
#include <sys/socket.h> 
#include <netdb.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

#include <acpp-network/socket.h>

namespace {
    void log_error(const std::string& func) {
        std::cerr << "[POSIX] Error in " << func << ": " << strerror(errno) << std::endl;
    }
}

Socket::Socket(const std::string& hostname, const std::string& port) {
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        log_error("socket");
    } else {
        std::cout << "Socket created with FD: " << fd_ << std::endl;
    }
}

Socket::~Socket() {
    close();
}

Socket::Socket(Socket&& other) noexcept 
    : fd_(other.fd_) {
    other.fd_ = -1; // Steal the resource
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        close(); // Clean up current resource
        fd_ = other.fd_;
        other.fd_ = -1; // Steal the resource
    }
    return *this;
}

bool Socket::connect() {
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

size_t Socket::send(const char* data, size_t len) {
    if (!is_valid()) return 0;
    ssize_t sent = ::send(fd_, data, len, 0);
    if (sent < 0) {
        log_error("send");
        return 0;
    }
    return static_cast<size_t>(sent);
}

size_t Socket::receive(char* buffer, size_t len) {
    if (!is_valid()) return 0;
    ssize_t received = ::recv(fd_, buffer, len, 0);
    if (received < 0) {
        log_error("receive");
        return 0;
    }
    return static_cast<size_t>(received);
}

void Socket::close() {
    if (fd_ != -1) {
        ::close(fd_); // The actual POSIX C call
        std::cout << "Socket with FD " << fd_ << " closed." << std::endl;
        fd_ = -1;
    }
}

bool Socket::is_valid() const {
    return fd_ != -1;
}

bool Socket::resolve_address() {
    // This private helper would contain the getaddrinfo logic, 
    // which is internal to the module.
    return true; 
}