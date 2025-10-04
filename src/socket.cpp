
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


} //namespace acpp::network 