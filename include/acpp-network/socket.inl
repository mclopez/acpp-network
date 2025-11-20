//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#include <iostream>

#include <acpp-network/socket.h>

namespace acpp::network {



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
    //int res = ::bind(socket_.fd(), reinterpret_cast<const sockaddr*>(&ad), sizeof(sockaddr));
    int res = ::bind(socket_.fd(), &to_sockaddr(ad), sizeof(sockaddr));
    if (res < 0) {
        log_error("bind");
    }
    return res;
}

template<typename Address, int Protocol>
int stream_socket<Address, Protocol>::listen(int backlog) {
    //TODO: fix check
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
    //TODO: fix check
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

template<typename Address, int Protocol>
size_t datagram_socket<Address, Protocol>::send_to(const address_type& addr, const char* data, size_t len) {
    if (!socket_.valid()) {
        socket_.create_impl(get_family(addr), socket_type, 0);
    }
    auto res = ::sendto(socket_.fd(), data, len, 0, reinterpret_cast<const sockaddr*>(&addr), sizeof(sockaddr));
    return res;
}

template<typename Address, int Protocol>
size_t datagram_socket<Address, Protocol>::recv_from(address_type& addr, char* data, size_t len) {
    if (!socket_.valid()) {
        socket_.create_impl(get_family(addr), socket_type, 0);
    }
    sockaddr addr1;
    memset(&addr1, 0, sizeof(addr1));
    socklen_t len1 = sizeof(addr1);
    auto res = ::recvfrom(socket_.fd(), data, len, 0, &addr1, &len1);
    if (res < 0) {
        log_error("recvfrom");
        return 0;
    }
    if (res > 0) {
        std::string family = (addr1.sa_family == AF_INET) ? "AF_INET" : (addr1.sa_family == AF_INET6) ? "AF_INET6" : "Other";
        std::cout << "recv_from " <<  std::string(data, res) << " family: " << family <<  " len1: " << len1 << std::endl;
    } else {
        std::cout << "recv_from 0 bytes" << std::endl;
    }    
    try {
        from_sockaddr(addr1, addr);
    } catch (const std::exception& ex) {
        std::cerr << "recv_from exception: " << ex.what() << std::endl;
        return 0;
    }
    std::cout << "recv_from 2"  << std::endl;
    return res;
}

template<typename Address, int Protocol>
int datagram_socket<Address, Protocol>::bind(const address_type& ad) {
    if (!socket_.valid()) {
        socket_.create_impl(get_family(ad), socket_type, Protocol);
    }
    int res = ::bind(socket_.fd(), reinterpret_cast<const sockaddr*>(&ad), sizeof(sockaddr));
    if (res < 0) {
        log_error("bind");
    }
    return res;
}







template<typename Socket, typename Address>
void resolve_host(const std::string& host, const std::string& service, resolve_address_callback<Address>&& callback) {
//    int                      sfd, s;
    struct addrinfo          hints;
    struct addrinfo          *result, *rp;

    std::unique_ptr<struct addrinfo, decltype(&freeaddrinfo)> ptrResult(nullptr, freeaddrinfo);

    typename Socket::address_type addr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = get_family(addr); //AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = Socket::socket_type; //SOCK_DGRAM; //TODO: make it configurable
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    auto s = getaddrinfo(host.c_str(), service.c_str(), &hints, &result);
    ptrResult.reset(result);// Ensure resources are freed
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        bool success = false;
        from_sockaddr(*rp->ai_addr, addr);
        // if (rp->ai_family == AF_INET) {
        //     ip4_sockaddress add;
        //     memcpy(&add.addr, rp->ai_addr, rp->ai_addr->sa_len);
        //     addr = add; 
        // } else if (rp->ai_family == AF_INET6) {
        //     ip6_sockaddress add;
        //     memcpy(&add.addr, rp->ai_addr, rp->ai_addr->sa_len);
        //     addr = add; 

        // } else {
        //     // Unknown family
        //     continue;
        // }

        callback(addr, success);
        if (success) {
            // Use addr
            break;
        }
    }   
}



template<typename Address, int Protocol>
bool async_stream_socket<Address, Protocol>::connect(const address_type& adr) {
    socket_.create_impl(get_family(adr), SOCK_STREAM, protocol, true);
    return ::connect(socket_.fd(), &to_sockaddr(adr), sizeof(sockaddr)) == 0;
}



} //namespace acpp::network 
