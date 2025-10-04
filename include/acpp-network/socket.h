#pragma once

#include <sys/socket.h> 

#include <string>

namespace acpp::network {

template<int Family> class address;

template<> struct address<PF_INET>  { using type = struct in_addr; };
template<> struct address<PF_INET6> { using type = struct in6_addr; };
template<> struct address<PF_UNIX>  { using type = struct iun_addr; };

class socket_base {
public:    
    socket_base(int domain, int type, int protocol):fd_(::socket(domain, type, protocol)){}
    ~socket_base() {
        if (fd_ != -1) {
            ::close(fd_);
        }
    }

    socket_base(const socket_base&) = delete;
    socket_base& operator=(const socket_base&) = delete;

    socket_base(socket_base&& other) noexcept;
    socket_base& operator=(socket_base&& other) noexcept;


    bool is_valid() const {return fd_ != -1;}
    int fd() const { return fd_; }
    void close() {
        if (fd_ != -1) {
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_ = -1; 
};

template<int Fam, int Type, int Protocol = 0>
class socket: socket_base {
public:
    constexpr static auto family = Fam;
    constexpr static auto type = Type;
    constexpr static auto protocol = Protocol;
    
    using addres_type = address<type>::type;

    socket():socket_base(family, type, protocol) {}

    ~socket()=default;

    bool connect(const addres_type& ad);
    size_t send(const char* data, size_t len);
    size_t receive(char* buffer, size_t len);

private:
 
    bool resolve_address(); 
};

using tcp_socket = socket<PF_INET, SOCK_STREAM>;
using udp_socket = socket<PF_INET, SOCK_DGRAM>;

} // namespace acpp::network 
