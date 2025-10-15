
#include <acpp-network/address.h>

namespace acpp::network {


int get_family(const ip_socketaddress& addr) {
    return std::visit([](auto&& arg) -> int {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ip4_sockaddress>) {
            return AF_INET;
        } else if constexpr (std::is_same_v<T, ip6_sockaddress>) {
            return AF_INET6;
        } else {
            return AF_UNSPEC;
            //throw std::invalid_argument("Unknown address type");
        }
    }, addr);
}
#ifndef _WIN32
int get_family(const un_socketaddress& addr) {
    return AF_UNIX;
}
#endif

std::string to_string(const ip_socketaddress& addr) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ip4_sockaddress>) {
            return std::string(arg.ip() + ":" + std::to_string(arg.port()));
        } else if constexpr (std::is_same_v<T, ip6_sockaddress>) {
            return std::string(arg.ip() + ":" + std::to_string(arg.port()));
        } else {
            throw std::invalid_argument("Unknown address type");
        }
    }, addr);
}
/*
const sockaddr& to_sockaddr(const ip_socketaddress& addr) {
    // return std::visit([](auto&& arg) -> sockaddr {
    //     using T = std::decay_t<decltype(arg)>;
    //     if constexpr (std::is_same_v<T, struct in_addr>) {
    //         return *reinterpret_cast<sockaddr*>(&sa);
    //     } else if constexpr (std::is_same_v<T, struct in6_addr>) {
    //         return *reinterpret_cast<sockaddr*>(&sa);
    //     } else {
    //         throw std::invalid_argument("Unknown address type");
    //     }
    // }, addr);
    //TODO: better way??
    return reinterpret_cast<const sockaddr&>(addr);
}
*/


const sockaddr& to_sockaddr(const ip4_sockaddress& addr) {
    return (const sockaddr &)addr.addr;
}
const sockaddr& to_sockaddr(const ip6_sockaddress& addr) {
    return (const sockaddr&)addr.addr;
}
/*
const sockaddr& to_sockaddr(const ip_socketaddress& addr)  {
    return std::visit([](auto&& arg) -> const sockaddr& {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, ip4_sockaddress>) {
            return to_sockaddr(std::get<ip4_sockaddress>(arg));
        }
        else if constexpr (std::is_same_v<T, ip6_sockaddress>) {
            return to_sockaddr(std::get<ip6_sockaddress>(arg));
        }
        else {
            //return AF_UNSPEC;
            throw std::invalid_argument("to_sockaddr: Unknown address type");
        }
        }, addr);
}
*/

const sockaddr& to_sockaddr(const ip_socketaddress& addr)  {
    if(const ip4_sockaddress* v = std::get_if<ip4_sockaddress>(&addr)) {
        return to_sockaddr(*v);
    } else if(const ip6_sockaddress* v = std::get_if<ip6_sockaddress>(&addr)) {
        return to_sockaddr(*v);
    } else {
        throw std::invalid_argument("to_sockaddr: Unknown address type");
    }
}
void from_sockaddr(const sockaddr& sa, ip4_sockaddress& out_addr) {
    if (sa.sa_family != AF_INET) {
        throw std::invalid_argument("Expected AF_INET sockaddr");
    }
    memcpy(&out_addr.addr, &sa, sizeof(sockaddr_in));
}
void from_sockaddr(const sockaddr& sa, ip6_sockaddress& out_addr) {
    if (sa.sa_family != AF_INET6) {
        throw std::invalid_argument("Expected AF_INET6 sockaddr");
    }
    memcpy(&out_addr.addr, &sa, sizeof(sockaddr_in6));
}


void from_sockaddr(const sockaddr& sa, ip_socketaddress& out_addr) {
    if (sa.sa_family == AF_INET) {
        ip4_sockaddress addr;
        //std::memcpy(&addr.addr, &sa, sizeof(sockaddr_in));
        from_sockaddr(sa, addr);
        out_addr = addr;
    } else if (sa.sa_family == AF_INET6) {
        ip6_sockaddress addr;
        //std::memcpy(&addr.addr, &sa, sizeof(sockaddr_in6));
        from_sockaddr(sa, addr);
        out_addr = addr;
    } else {
        throw std::invalid_argument("Unsupported sockaddr family");
    }
}


}// namespace acpp::network