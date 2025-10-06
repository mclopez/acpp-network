//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

//#include <netdb.h>
//#include <unistd.h>
#include <cstring>
#include <iostream>

#include <acpp-network/socket.h>

#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h> // Required for WSAGetLastError, FormatMessage, etc.
#endif






#ifdef _WIN32

// Function to convert a Winsock error code to a string
std::string winsock_error_to_string(int errorCode) {
    // Flag to format message from the system's message table
    // and allocate memory for the string buffer.
    const DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER;
    
    // Pass the error code as the message identifier.
    // Use the language neutral ID (LANGID 0).
    LPTSTR buffer = nullptr;
    DWORD length = FormatMessage(
        flags,
        NULL,              // Source (not used when FORMAT_MESSAGE_FROM_SYSTEM)
        errorCode,         // The error code
        0,                 // Language ID (0 for language-neutral)
        (LPTSTR)&buffer,   // Output buffer pointer
        0,                 // Minimum buffer size
        NULL               // Arguments (not used)
    );

    if (length == 0) {
        // If FormatMessage failed, return a default message with the code.
        return "Winsock Error " + std::to_string(errorCode) + " (No description available)";
    }

    // The returned buffer contains the string (which may have a trailing newline).
    std::string message(buffer);

    // Free the buffer allocated by FormatMessage
    LocalFree(buffer); 

    // Windows error messages often include a trailing newline/carriage return. Remove it.
    size_t last_char = message.find_last_not_of("\r\n");
    if (std::string::npos != last_char) {
        message.resize(last_char + 1);
    }
    
    return message;
}


class socket_init{
public:    
    socket_init();
    ~socket_init();
};


socket_init::socket_init(){
    std::cout << "socket_init" << std::endl;
    WSADATA wsaData;
    // We request Winsock version 2.2 (the standard modern version)
    WORD wVersionRequested = MAKEWORD(2, 2); 

    // 2. Initialize Winsock
    int iResult = WSAStartup(wVersionRequested, &wsaData);
    
    if (iResult != 0) {
        // Initialization failed. Print the error code.
        // (WSAGetLastError() won't work reliably here, as the environment isn't set up yet)
        std::cerr << "WSAStartup failed with error: " << iResult << std::endl;
    }
    //TODO: throw exception??
}

socket_init::~socket_init(){
    std::cout << "~socket_init" << std::endl;
    //TODO: always even when WSAStartup fails??
    WSACleanup();
}

socket_init init;

#endif

void log_error(const std::string& func) {
    std::string error;
#ifndef _WIN32
    error = strerror(errno);
#else
    error = winsock_error_to_string(WSAGetLastError());
#endif
    std::cerr << "[POSIX] Error in " << func << ": " << error << std::endl;
}


namespace acpp::network {



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



} //namespace acpp::network 