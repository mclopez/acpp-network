//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)


#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <ws2def.h>
#pragma comment(lib, "Ws2_32.lib")
#include <windows.h>
#include <io.h>
#include <mswsock.h>

#include <iostream>

#include <acpp-network/address.h>

#include <acpp-network/socket_base.h>


namespace acpp::network {


using in_port_t = decltype(sockaddr_in::sin_port);


const int socket_base::invalid_fd = INVALID_SOCKET;

enum class operation_type {
    connect, read, write,
};

typedef struct  {
public:    
    OVERLAPPED overlapped;
    SOCKET fd;
    WSABUF buf_info;
    char buffer[1024];
    operation_type op_type; 
}async_operation;

struct socket_base_pimpl {
public:
    socket_base_pimpl()= default;
    async_operation connect_op;
private:
};

socket_base::socket_base() {

}

socket_base::socket_base(int fd)
:fd_(fd) {

}


socket_base::socket_base(socket_base&& other) noexcept 
    : fd_(other.fd_) {
    other.fd_ = -1; // Steal the resource
}

socket_base::~socket_base() {
    close();
}


socket_base& socket_base::operator=(socket_base&& other) noexcept {
    if (this != &other) {
        ::close(fd_); // Clean up current resource
        fd_ = other.fd_;
        other.fd_ = -1; // Steal the resource
    }
    return *this;
}


void socket_base::close() {
    if (fd_ != invalid_fd) {
        ::shutdown(fd_, SD_SEND);
        ::closesocket(fd_);
        fd_ = invalid_fd;
    }
}

void socket_base::create_impl(int domain, int type, int protocol, bool non_blocking) {
    if (valid())
        close();
    non_blocking_ = non_blocking;    
    //fd_ = ::socket(domain, type, protocol);
    DWORD flags = 0;
    if (non_blocking)   {
        flags |= WSA_FLAG_OVERLAPPED;
    }
    fd_ = WSASocket(domain, type, protocol, NULL, 0, flags);
    if (non_blocking)   {
        pimpl_ = std::make_unique<socket_base_pimpl>();
        //pimpl_ = new socket_base_pimpl();
    }
}

bool socket_base::connect(const sockaddr& adr) {
    if (non_blocking_) {
        //WSAConnect(fd_, )
        return WSAConnect(fd_, &adr, sizeof(sockaddr), NULL, NULL, NULL, NULL) == 0;

    } else {
        SOCKADDR_IN localAddr = { 0 };
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddr.sin_port = 0; // let system choose

        bind(fd_, (SOCKADDR*)&localAddr, sizeof(localAddr));


//        io_->add_socket(*this);

        GUID guidConnectEx = WSAID_CONNECTEX;
        DWORD bytesReturned = 0;
        LPFN_CONNECTEX lpConnectEx = NULL;
        
        WSAIoctl(fd_, SIO_GET_EXTENSION_FUNCTION_POINTER,
            &guidConnectEx, sizeof(guidConnectEx),
            &lpConnectEx, sizeof(lpConnectEx),
            &bytesReturned, NULL, NULL);

        if (lpConnectEx)    {
            //OVERLAPPED overlapped = { 0 };
            pimpl_->connect_op.op_type = operation_type::connect;

            BOOL result = lpConnectEx(fd_, &adr, sizeof(sockaddr), NULL, 0, NULL, (LPOVERLAPPED)&(pimpl_->connect_op));

            if (!result) {
                int err = WSAGetLastError();
                if (err != ERROR_IO_PENDING) {
                    printf("ConnectEx failed: %d\n", err);
                    closesocket(fd_);
                    return err;
                }
            }        
            std::cout << "connect2 result: " << result << std::endl;
        }
    }
    return false;
}

/*
    int async_socket::connect2(const std::string& ip, int port) {
        sockaddr_in service;
        service.sin_family = AF_INET;
        int r = inet_pton(AF_INET, ip.c_str(), &service.sin_addr.s_addr);
        if (r <= 0) {
            throw std::invalid_argument("Invalid IP address format ****");
        }

        service.sin_port = htons(port);

//        if (fd) ...
        fd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        std::cout << "connect2 fd: " << fd << std::endl;

        SOCKADDR_IN localAddr = { 0 };
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddr.sin_port = 0; // let system choose

        bind(fd, (SOCKADDR*)&localAddr, sizeof(localAddr));


        io_->add_socket(*this);

        GUID guidConnectEx = WSAID_CONNECTEX;
        DWORD bytesReturned = 0;
        LPFN_CONNECTEX lpConnectEx = NULL;
        
        WSAIoctl(fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
            &guidConnectEx, sizeof(guidConnectEx),
            &lpConnectEx, sizeof(lpConnectEx),
            &bytesReturned, NULL, NULL);

        if (lpConnectEx)    {
            //OVERLAPPED overlapped = { 0 };
            connect_op.op_type = operation_type::connect;

            BOOL result = lpConnectEx(fd, (sockaddr*)&service, sizeof(sockaddr_in), NULL, 0, NULL, (LPOVERLAPPED)&(connect_op));

            if (!result) {
                int err = WSAGetLastError();
                if (err != ERROR_IO_PENDING) {
                    printf("ConnectEx failed: %d\n", err);
                    closesocket(fd);
                    return err;
                }
            }        
            std::cout << "connect2 result: " << result << std::endl;
        }
        std::cout << "connect2 result: " << r << std::endl;
        return r;
    }
*/


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



void log_error(const std::string& func) {
    std::string error;
    error = winsock_error_to_string(WSAGetLastError());
    std::cerr << "[POSIX] Error in " << func << ": " << error << std::endl;
}




} // namespace acpp::network 