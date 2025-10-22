
#include <iostream>
#include <thread>

#include <gtest/gtest.h> // googletest header file  

#include <acpp-network/socket.h>
#include <acpp-network/socket.inl>



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


class io_context;

struct async_socket {
    async_socket(io_context& io):io_(&io){}

    ~async_socket() {
        if (fd) {
            closesocket(fd);
        }
    }
    void start_reading() {
        auto& op = read_op;
        read_op.fd = fd;
        read_op.op_type = operation_type::read;
        
        // Set up the WSABUF
        op.buf_info.buf = op.buffer;
        op.buf_info.len = sizeof(op.buffer);

        DWORD flags = 0;
        DWORD bytes = 0;
        
        // WSARecv returns SOCKET_ERROR if the operation is pending (standard for IOCP)
        if (WSARecv(fd, 
                    &(op.buf_info), 
                    1, 
                    &bytes, 
                    &flags, 
                    &(op.overlapped), 
                    NULL) == SOCKET_ERROR) 
        {
            // Ignore WSA_IO_PENDING (997), which means the operation is running asynchronously
            if (WSAGetLastError() != WSA_IO_PENDING) {
                std::cerr << "WSARecv failed: " << WSAGetLastError() << std::endl;
                //CleanupContext(clientContext, ioContext);
            }
        }

    }

    int connect(const std::string& ip, int port) {
        sockaddr_in service;
        service.sin_family = AF_INET;
        int r = inet_pton(AF_INET, ip.c_str(), &service.sin_addr.s_addr);
        if (r <= 0) {
            throw std::invalid_argument("Invalid IP address format ****");
        }

        service.sin_port = htons(port);

//        if (fd) ...
        fd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
        std::cout << "connect fd: " << fd << std::endl;
        r = ::connect(fd, (sockaddr*)&service, sizeof(sockaddr_in));
        std::cout << "connect result: " << r << std::endl;
        return r;
    }
    int connect2(const std::string& ip, int port);



    void start_write(std::string msg){
        write_op.fd = fd;
        write_op.op_type = operation_type::write;
        
        // Copy data from the completed read buffer to the new write buffer
        memcpy(write_op.buffer, msg.c_str(), msg.size()); //check buffer size!!
        write_op.buf_info.buf = write_op.buffer;
        write_op.buf_info.len = msg.size(); 

        DWORD bytes = 0;

        // WSASend returns SOCKET_ERROR if the operation is pending
        if (WSASend(fd, 
                    &(write_op.buf_info), 
                    1, 
                    &bytes, 
                    0, // flags
                    &(write_op.overlapped), 
                    NULL) == SOCKET_ERROR) 
        {
            if (WSAGetLastError() != WSA_IO_PENDING) {
                std::cerr << "WSASend failed: " << WSAGetLastError() << std::endl;
                //CleanupContext(clientContext, writeContext);
            }
        }        
    }


    SOCKET fd;
    async_operation connect_op;
    async_operation read_op;
    async_operation write_op;
    io_context* io_;
};


class io_context {
public:
    io_context(){
        hIOCP_ = CreateIoCompletionPort(
            INVALID_HANDLE_VALUE, // FileHandle (null for creation)
            NULL,                 // ExistingCompletionPort (null for creation)
            0,                    // CompletionKey (not used yet)
            0                     // NumberOfConcurrentThreads (0 uses system default)    
        );
        if (hIOCP_ == NULL) {
            throw std::runtime_error("Error in CreateIoCompletionPort (1)");
        }
    }

    void wait_for_input() {
        DWORD bytesTransferred = 0;
        ULONG_PTR completionKey = 0;
        LPOVERLAPPED lpOverlapped = NULL;

        std::cout << "wait_for_input" << std::endl;
        run = true;
        while (run) {
            // Blocks until an I/O operation completes and is placed on the queue
            BOOL success = GetQueuedCompletionStatus(
                hIOCP_,
                &bytesTransferred,
                &completionKey,
                &lpOverlapped,
                INFINITE // Wait indefinitely
            );
            std::cout <<"***************************************** process ...." << std::endl;

            // The completionKey is our CLIENT_CONTEXT*
            async_socket* clientContext = (async_socket*)completionKey;
            std::cout <<"process ... socekt: " << (void*)clientContext << " op:" << (void*)lpOverlapped << std::endl;
            if (!clientContext) continue; // Should not happen

            // lpOverlapped points to our IO_CONTEXT::Overlapped member
            async_operation* ioContext = (async_operation*)lpOverlapped;

            // if (!success || (success && bytesTransferred == 0)) {
            //     // Connection closed or error. Handle cleanup.

            //     //CleanupContext(clientContext, ioContext);
            //     std::cout <<"continue ...." << std::endl;
            //     continue;
            // }

            // --- Process the Completed I/O Operation ---
            if (ioContext->op_type == operation_type::connect) {
                std::cout << "*** async connected! "  << std::endl;
                run = false;
            }else if (ioContext->op_type == operation_type::read) {
                std::string msg(ioContext->buf_info.buf, bytesTransferred);
                std::cout << "*** async read done! msg: " << msg << std::endl;
                run = false;

            } else if (ioContext->op_type == operation_type::write) {
                // Write completed, clean up this specific I/O context.
                //delete ioContext; 
                std::cout << "*** async write done!" << std::endl;
            }
        }
        return;

    }
    void remove_socket(async_socket& as) {}

    void add_socket(async_socket& as) {
        CreateIoCompletionPort(
            (HANDLE)as.fd, // FileHandle (null for creation)
            hIOCP_,                 // ExistingCompletionPort (null for creation)
            (ULONG_PTR)&as,                    // CompletionKey (not used yet)
            0                     // NumberOfConcurrentThreads (0 uses system default)    
        );
        std::cout << "add_socket to coml port" << std::endl; 
    }
    void stop() {run = false;}
    std::atomic_bool run;
private:
    HANDLE hIOCP_ = INVALID_HANDLE_VALUE;

};


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


TEST(AsyncSocketTests, first)
{

    using namespace acpp::network;
    std::cout << "*** Socket tests" << std::endl;
    int port = 6664;
    std::thread server_th([port](){
        std::cout << "Socket tests th" << std::endl;

        stream_socket<ip_socketaddress> server_socket;

        ip_socketaddress adr = ip4_sockaddress("127.0.0.1", port);

        server_socket.bind(adr);
        server_socket.listen(5);
        auto client_socket = server_socket.accept();
         char buffer[1024];
         auto bytes_received = client_socket.receive(buffer, sizeof(buffer));
         if (bytes_received > 0) {
             std::cout << "Server received: " << std::string(buffer, bytes_received) << std::endl;
             client_socket.send(buffer, bytes_received); // Echo back
         }
    });

    // std::thread client_th([port](){
    //    std::cout << "Socket tests client" << std::endl;
    //    stream_socket<ip_socketaddress> socket;
    //    ip_socketaddress adr = ip4_sockaddress("127.0.0.1", port);

    //    socket.connect(adr);
    //    const char* msg = "Hello, Echo Server!";
    //    socket.send(msg, strlen(msg));
    //    char buffer[1024];
    //    auto bytes_received = socket.receive(buffer, sizeof(buffer));
    //    if (bytes_received > 0) {
    //        std::string msg(buffer, bytes_received);
    //        std::cout << "Client received: " << msg << std::endl;
    //        EXPECT_EQ(msg, "Hello, Echo Server!");
    //    }
    // });

    std::thread client_th([port](){
        std::cout << "Socket tests client" << std::endl;
        io_context io;
        async_socket socket(io);
        std::cout << (void*) &socket.read_op << " " << (void*) &socket.write_op << " " << (void*) &socket.connect_op << std::endl;
        socket.connect2("127.0.0.1", port);
        //io.add_socket(socket);
        //socket.start_write();
        std::string msg("hola!");
        //::send(socket.fd, msg.c_str(), msg.size(), 0);
        //socket.start_write(msg);
        //socket.start_reading();
        io.wait_for_input();

        std::cout << "Socket tests client end" << std::endl;
    });
    std::this_thread::sleep_for(std::chrono::seconds(5));
    //io.stop();
    if (server_th.joinable())
        server_th.join();
    if (client_th.joinable())
       client_th.join();


}
