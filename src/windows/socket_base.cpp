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


socket_base::socket_base():fd_(invalid_fd) {

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

void socket_base::create_impl(int domain, int type, int protocol) {
    if (valid())
        close();
    //fd_ = ::socket(domain, type, protocol);
    DWORD flags = 0;
    fd_ = WSASocket(domain, type, protocol, NULL, 0, flags);
}

bool socket_base::connect(const sockaddr& adr) {
    //WSAConnect(fd_, )
    return WSAConnect(fd_, &adr, sizeof(sockaddr), NULL, NULL, NULL, NULL) == 0;

}

enum class operation_type {
    connect, read, write, accept, exec,
};

struct async_operation {
    WSAOVERLAPPED olOverlap = { 0 };
    operation_type type;
};

struct read_operation: public  async_operation  {
public:    
    WSABUF buf_info;
    char buffer[1024];
};

struct write_operation: public  async_operation  {
public:    
    WSABUF buf_info;
    char buffer[1024];
};

struct connect_operation: public  async_operation  {
public:    
    WSABUF buf_info;
    char buffer[1024];
};

struct accept_operation: public  async_operation  {
public:    
    char buffer[1024];
    std::unique_ptr<async_socket_base> new_socket;
};

struct execution_operation: public  async_operation  {
public:    
    execution_operation(){type = operation_type::exec;}
    std::function<void()> fun;
};


struct socket_base_pimpl {
public:
    socket_base_pimpl(async_socket_base& parent):fd_(invalid_fd),io_(nullptr), parent_(&parent){
    }

    io_context* io_;
    async_socket_base::fd_type fd_ = invalid_fd; 

    socket_callbacks callbacks_;

    //TODO: make something more compact: if accept is not connection. std::variant for ops? 
    connect_operation connect_op;
    read_operation read_op;
    write_operation write_op;
    accept_operation accept_op;
    async_socket_base* parent_;
    static const async_socket_base::fd_type invalid_fd = INVALID_SOCKET;

    int domain_;
    int type_;
    int protocol_;

    bool valid() { return fd_ != invalid_fd;}

    socket_base_pimpl(int domain, int type, int protocol, async_socket_base::fd_type fd, io_context& io, socket_callbacks&& callbacks)
    :   domain_(domain), type_(type), protocol_(protocol), 
        fd_(fd),
        io_(&io), callbacks_(std::move(callbacks)) 
    {
        if (valid()) {
            // int flags = fcntl(fd_, F_GETFL, 0);
            // fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        }
    }

    socket_base_pimpl(int domain, int type, int protocol, io_context& io, socket_callbacks&& callbacks)
    :   domain_(domain), type_(type), protocol_(protocol), 
        //fd_(::socket(domain, type, protocol)),
        fd_(WSASocket(domain, type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED)),
        io_(&io), callbacks_(std::move(callbacks)) 
    {
        if (valid()) {
            // int flags = fcntl(fd_, F_GETFL, 0);
            // fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        }
    }


    int listen(int backlog) {
        //TOOD: check vality
        auto res = ::listen(fd_, backlog);
        if (res == SOCKET_ERROR) {
            log_error("listen");
            return res;
        }
        return start_accept();
    }

    int start_accept() {
        //TOOD: check vality
        // if (!valid()) {
        //     create_impl(AF_INET, SOCK_STREAM, IPPROTO_TCP);//TODO: this dont belong here....
        // }

        GUID guidFunc =  WSAID_ACCEPTEX;
        DWORD bytesReturned = 0;
        LPFN_ACCEPTEX lpAcceptEx = NULL;
        
        WSAIoctl(fd_, SIO_GET_EXTENSION_FUNCTION_POINTER, //SIO_GET_EXTENSION_FUNCTION_POINTER
            &guidFunc, sizeof(guidFunc),
            &lpAcceptEx, sizeof(lpAcceptEx),
            &bytesReturned, NULL, NULL);

        if (lpAcceptEx)    {
            accept_op.type = operation_type::accept;
            accept_op.new_socket = std::make_unique<async_socket_base>(domain_, type_, protocol_, *io_, socket_callbacks{});

            DWORD dwLocalAddressLength = sizeof(sockaddr_in) + 16;
            DWORD dwRemoteAddressLength = sizeof(sockaddr_in) + 16;
            DWORD lpdwBytesReceived = 0;

            BOOL result = lpAcceptEx(
                fd_, 
                accept_op.new_socket->fd(), 
                &accept_op.buffer, 
                0, //sizeof(pimpl_->accept_op.buffer) - ((sizeof (sockaddr_in) + 16) * 2), 
                dwLocalAddressLength, 
                dwRemoteAddressLength, 
                &lpdwBytesReceived, 
                (LPOVERLAPPED)&(accept_op.olOverlap));



            if (!result) {
                int err = WSAGetLastError();
                if (err != ERROR_IO_PENDING) {
                    printf("lpAcceptEx failed: %d\n", err);
                    closesocket(fd_);
                    return err;
                }
            }        
            std::cout << "accept result: " << result << std::endl;
        } else {
            std::cout << "accept lpAcceptEx faild: " << std::endl;

        }
        return 0;
    }



    void close() {
        if (valid()) {
            ::shutdown(fd_, SD_SEND);
            ::closesocket(fd_);
            fd_ = invalid_fd;
        }
    }



private:
};



// async_socket_base::async_socket_base()
// :pimpl_(std::make_unique<socket_base_pimpl>(*this))
// {
// }

//    async_socket_base(int domain, int type, int protocol, io_context& io, socket_callbacks&& callbacks = socket_callbacks{});
//    async_socket_base(int domain, int type, int protocol, fd_type fd, io_context& io, socket_callbacks&& callbacks = socket_callbacks{});


async_socket_base::async_socket_base(int domain, int type, int protocol, io_context& io, socket_callbacks&& callbacks)
{
    std::cout << "async_socket_base constructor without fd " << (void*) this << std::endl;
    pimpl_ =  std::make_unique<socket_base_pimpl>(domain, type, protocol, io, std::move(callbacks));
    pimpl_->parent_ = this;
    io.add_socket(*this);
}

async_socket_base::async_socket_base(int domain, int type, int protocol, fd_type fd, io_context& io, socket_callbacks&& callbacks)
{
    std::cout << "async_socket_base constructor without fd " << (void*) this << std::endl;
    pimpl_ =  std::make_unique<socket_base_pimpl>(domain, type, protocol, fd, io, std::move(callbacks));
    pimpl_->parent_ = this;
    io.add_socket(*this);
}


async_socket_base::async_socket_base(async_socket_base&& other) noexcept 
{
    pimpl_ = std::move(other.pimpl_);
    pimpl_->parent_ = this;
    //TODO: should we create a impl?
    //other.pimpl_ = std::make_unique<socket_base_pimpl>(other);
}


async_socket_base::~async_socket_base() {
    close();
}

int64_t async_socket_base::fd() { 
    return pimpl_->fd_;
}


bool async_socket_base::valid() const {
    //return (pimpl_->fd_ != invalid_fd);
    if (pimpl_) return pimpl_->valid();
    return false;
}


async_socket_base& async_socket_base::operator=(async_socket_base&& other) noexcept {
    pimpl_ = std::move(other.pimpl_);
    pimpl_->parent_ = this;
    other.pimpl_ = std::make_unique<socket_base_pimpl>(other);
    return *this;
}


// void async_socket_base::create_impl(int domain, int type, int protocol) {
//     if (valid())
//         close();
//     pimpl_->fd_ = WSASocket(domain, type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
//     pimpl_->io_->add_socket(*this);
// }

void async_socket_base::close() {
    if (pimpl_) pimpl_->close();
}

void async_socket_base::callbacks(socket_callbacks&& callbacks) {
    pimpl_->callbacks_ = std::move(callbacks);
}

socket_callbacks& async_socket_base::callbacks() { 
    return pimpl_->callbacks_;
}


bool async_socket_base::bind(const sockaddr& addr) {
    return ::bind(pimpl_->fd_, &addr, sizeof(sockaddr));
}

int async_socket_base::listen(int backlog) {
    return pimpl_->listen(backlog);
}

/*
int async_socket_base::accept() {
    //TOOD: check vality
    // if (!valid()) {
    //     create_impl(AF_INET, SOCK_STREAM, IPPROTO_TCP);//TODO: this dont belong here....
    // }

    GUID guidFunc =  WSAID_ACCEPTEX;
    DWORD bytesReturned = 0;
    LPFN_ACCEPTEX lpAcceptEx = NULL;
    
    WSAIoctl(pimpl_->fd_, SIO_GET_EXTENSION_FUNCTION_POINTER, //SIO_GET_EXTENSION_FUNCTION_POINTER
        &guidFunc, sizeof(guidFunc),
        &lpAcceptEx, sizeof(lpAcceptEx),
        &bytesReturned, NULL, NULL);

    if (lpAcceptEx)    {
        //OVERLAPPED overlapped = { 0 };
        pimpl_->accept_op.type = operation_type::accept;
        pimpl_->accept_op.new_socket = std::make_unique<async_socket_base>(pimpl_->domain_, pimpl_->type_, pimpl_->protocol_, *pimpl_->io_, socket_callbacks{});
        //pimpl_->accept_op.new_socket->create_impl(AF_INET, SOCK_STREAM, IPPROTO_TCP); //TODO: make multy protocol

        DWORD dwLocalAddressLength = sizeof(sockaddr_in) + 16;
        DWORD dwRemoteAddressLength = sizeof(sockaddr_in) + 16;
        DWORD lpdwBytesReceived = 0;

        BOOL result = lpAcceptEx(
            pimpl_->fd_, 
            pimpl_->accept_op.new_socket->pimpl_->fd_, 
            &pimpl_->accept_op.buffer, 
            0, //sizeof(pimpl_->accept_op.buffer) - ((sizeof (sockaddr_in) + 16) * 2), 
            dwLocalAddressLength, 
            dwRemoteAddressLength, 
            &lpdwBytesReceived, 
            (LPOVERLAPPED)&(pimpl_->accept_op.olOverlap));



        if (!result) {
            int err = WSAGetLastError();
            if (err != ERROR_IO_PENDING) {
                printf("lpAcceptEx failed: %d\n", err);
                closesocket(pimpl_->fd_);
                return err;
            }
        }        
        std::cout << "accept result: " << result << std::endl;
    } else {
        std::cout << "accept lpAcceptEx faild: " << std::endl;

    }
    return 0;
}
*/

bool async_socket_base::connect(const sockaddr& adr) {
    // if (!valid()) {
    //     create_impl(AF_INET, SOCK_STREAM, IPPROTO_TCP);//TODO: this dont belong here....
    // }
    SOCKADDR_IN localAddr = { 0 };
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = 0; // let system choose

    ::bind(pimpl_->fd_, (SOCKADDR*)&localAddr, sizeof(localAddr));


    GUID guidConnectEx = WSAID_CONNECTEX;
    DWORD bytesReturned = 0;
    LPFN_CONNECTEX lpConnectEx = NULL;
    
    WSAIoctl(pimpl_->fd_, SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guidConnectEx, sizeof(guidConnectEx),
        &lpConnectEx, sizeof(lpConnectEx),
        &bytesReturned, NULL, NULL);

    if (lpConnectEx)    {
        //OVERLAPPED overlapped = { 0 };
        pimpl_->connect_op.type = operation_type::connect;

        BOOL result = lpConnectEx(pimpl_->fd_, &adr, sizeof(sockaddr), NULL, 0, NULL, (LPOVERLAPPED)&(pimpl_->connect_op.olOverlap));

        if (!result) {
            int err = WSAGetLastError();
            if (err != ERROR_IO_PENDING) {
                printf("ConnectEx failed: %d\n", err);
                closesocket(pimpl_->fd_);
                return err;
            }
        }        
        std::cout << "connect2 result: " << result << std::endl;
    } else {
        std::cout << "connect2 lpConnectEx faild: " << std::endl;

    }
    return false;
}


void async_socket_base::read() {
    auto& op = pimpl_->read_op;
    op.type = operation_type::read;
    
    // Set up the WSABUF
    op.buf_info.buf = op.buffer;
    op.buf_info.len = sizeof(op.buffer);

    DWORD flags = 0;
    DWORD bytes = 0;
    
    // WSARecv returns SOCKET_ERROR if the operation is pending (standard for IOCP)
    if (WSARecv(fd(), 
                &(op.buf_info), 
                1, 
                &bytes, 
                &flags, 
                (LPOVERLAPPED)&op.olOverlap, 
                NULL) == SOCKET_ERROR) 
    {
        // Ignore WSA_IO_PENDING (997), which means the operation is running asynchronously
        if (WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSARecv failed: " << WSAGetLastError() << std::endl;
        }
    }    
}


size_t async_socket_base::write(const char* buffer, size_t len) {
    auto& op = pimpl_->write_op;
    op.type = operation_type::write;// TODO: not need to always set the type
    
    memcpy(op.buffer, buffer, len); //check buffer size!!
    op.buf_info.buf = op.buffer;
    op.buf_info.len = len; 

    DWORD bytes = 0;
    int res;
    // WSASend returns SOCKET_ERROR if the operation is pending
    res = WSASend(fd(), 
                &(op.buf_info), 
                1, 
                &bytes, 
                0, // flags
                (LPOVERLAPPED)&op.olOverlap, 
                NULL);
                
    if (res == 0) {
        //synchronous call
    } else if (res == SOCKET_ERROR) {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSASend failed: " << WSAGetLastError() << std::endl;
            //CleanupContext(clientContext, writeContext);
        }
    }    
    std::cout << "write res: " << res << std::endl;    
    return 0;
}


struct io_context_pimpl {
    std::atomic_bool run;
    HANDLE hIOCP_ = INVALID_HANDLE_VALUE;
};

io_context::io_context():pimpl_(std::make_unique<io_context_pimpl>()){

    pimpl_->hIOCP_ = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, // FileHandle (null for creation)
        NULL,                 // ExistingCompletionPort (null for creation)
        0,                    // CompletionKey (not used yet)
        0                     // NumberOfConcurrentThreads (0 uses system default)    
    );
    if (pimpl_->hIOCP_ == NULL) {
        throw std::runtime_error("Error in CreateIoCompletionPort (1)");
    }
}

io_context::~io_context() {
    
}


void io_context::exec(std::function<void()>&& f) {
    auto op = std::make_unique<execution_operation>();
    //op->type = operation_type::exec;
    op->fun = std::move(f);
    if (PostQueuedCompletionStatus(pimpl_->hIOCP_, 0, (ULONG_PTR)nullptr, &op->olOverlap)) {
        // if posted dont release op
        op.release();
    }
}

void io_context::wait_for_input() {
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED lpOverlapped = NULL;

    std::cout << "wait_for_input" << std::endl;
    pimpl_->run = true;
    while (pimpl_->run) {
        // Blocks until an I/O operation completes and is placed on the queue
        std::cout << "wait_for_input GetQueuedCompletionStatus" << std::endl;
        BOOL success = GetQueuedCompletionStatus(
            pimpl_->hIOCP_,
            &bytesTransferred,
            &completionKey,
            &lpOverlapped,
            INFINITE // Wait indefinitely
        );

        std::cout <<"***************************************** process ...." << std::endl;

        // The completionKey is our CLIENT_CONTEXT*
        socket_base_pimpl* socket = (socket_base_pimpl*)completionKey;
        if (!socket) {
            std::cout <<"***************************************** process .... 2" << std::endl;
            if (lpOverlapped) {
                std::cout <<"***************************************** process .... 3" << std::endl;
                async_operation* op = CONTAINING_RECORD(lpOverlapped, async_operation, olOverlap);
                std::cout <<"***************************************** process .... 4 op->type: " << (int)op->type << std::endl;
                if (op->type == operation_type::exec)    {
                    auto exec_op = std::unique_ptr<execution_operation>((execution_operation*)op);
                    exec_op->fun();
                }
            }
            continue; 
        }


        // lpOverlapped points to our IO_CONTEXT::Overlapped member
        //async_operation* operation = (async_operation*)lpOverlapped;
        async_operation* operation = CONTAINING_RECORD(lpOverlapped, async_operation, olOverlap);

        // --- Process the Completed I/O Operation ---
        if (operation->type == operation_type::accept) {
            accept_operation& op = (accept_operation&)*operation;
            std::string msg(op.buffer, bytesTransferred);
            if(socket->callbacks_.on_accepted) {
                socket->callbacks_.on_accepted(*socket->parent_, std::move(*op.new_socket));
            }
        } else if (operation->type == operation_type::connect) {
            if(socket->callbacks_.on_connected) {
                socket->callbacks_.on_connected(*socket->parent_);
            }
         }else if (operation->type == operation_type::read) {
            std::cout <<"***************************************** READ  .... bytesTransferred: " << bytesTransferred << std::endl;
            read_operation& op = *(read_operation*)operation;
            std::string msg(op.buf_info.buf, bytesTransferred);
            if (bytesTransferred == 0) {
                if (socket->callbacks_.on_disconnected) 
                    socket->callbacks_.on_disconnected(*socket->parent_);
            } else if(socket->callbacks_.on_received) {
                socket->callbacks_.on_received(*socket->parent_, op.buf_info.buf, bytesTransferred);
            }
        } else if (operation->type == operation_type::write) {
            if(socket->callbacks_.on_sent) {
                socket->callbacks_.on_sent(*socket->parent_);
            }
        }
    }
    return;
}



void io_context::remove_socket(async_socket_base& as) {}

void io_context::add_socket(async_socket_base& as) {
    CreateIoCompletionPort(
        (HANDLE)as.fd(), // FileHandle (null for creation)
        pimpl_->hIOCP_,                 // ExistingCompletionPort (null for creation)
        (ULONG_PTR)as.pimpl_.get(),                    // CompletionKey (not used yet)
        0                     // NumberOfConcurrentThreads (0 uses system default)    
    );
    std::cout << "add_socket to coml port" << std::endl; 
}

void io_context::stop() {
    pimpl_->run = false;
}



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