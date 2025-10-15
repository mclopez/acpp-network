#include <iostream>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

// A. Context for the I/O operation (the OVERLAPPED structure)
// This is specific to a single pending Read or Write operation.
struct IO_CONTEXT {
    OVERLAPPED Overlapped;
    SOCKET ClientSocket;
    WSABUF DataBuf;
    char Buffer[1024];
    DWORD OperationType; // e.g., OP_READ or OP_WRITE
};

// B. Context for the entire Client Connection
// This is associated with the CompletionKey passed to IOCP.
struct CLIENT_CONTEXT {
    SOCKET ClientSocket;
    // Pointers to the IO_CONTEXT structures can be stored here for organization
};

// Global Constants/Handles
const int NUM_THREADS = std::thread::hardware_concurrency();
const int OP_READ = 1;
const int OP_WRITE = 2;
HANDLE g_hIOCP = INVALID_HANDLE_VALUE;


void InitializeServer(const char* port) {
    // 1. Winsock Initialization
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return;
    }

    // 2. Create the Completion Port
    g_hIOCP = CreateIoCompletionPort(
        INVALID_HANDLE_VALUE, // FileHandle (null for creation)
        NULL,                 // ExistingCompletionPort (null for creation)
        0,                    // CompletionKey (not used yet)
        0                     // NumberOfConcurrentThreads (0 uses system default)
    );
    if (g_hIOCP == NULL) {
        std::cerr << "CreateIoCompletionPort failed: " << GetLastError() << std::endl;
        WSACleanup();
        return;
    }

    // 3. Create Listening Socket
    SOCKET listenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (listenSocket == INVALID_SOCKET) { /* ... error handling ... */ }

    // Bind and Listen skipped for brevity (standard Winsock calls)
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = htonl(INADDR_ANY);
    service.sin_port = htons(atoi(port));
    
    bind(listenSocket, (SOCKADDR*)&service, sizeof(service));
    listen(listenSocket, SOMAXCONN);
    
    std::cout << "Server listening on port " << port << " with " << NUM_THREADS << " threads." << std::endl;

    // 4. Create Worker Threads
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(WorkerThread, i + 1);
    }

    // 5. Start the Accept Loop (main thread handles accepts)
    SOCKET acceptSocket;
    while (true) {
        acceptSocket = accept(listenSocket, NULL, NULL);
        if (acceptSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        // --- Critical Step: Associate the new socket with the IOCP ---
        CLIENT_CONTEXT* clientContext = new CLIENT_CONTEXT();
        clientContext->ClientSocket = acceptSocket;

        CreateIoCompletionPort(
            (HANDLE)acceptSocket, // The new socket handle
            g_hIOCP,              // The IOCP handle
            (ULONG_PTR)clientContext, // The Completion Key (Client Context)
            0
        );

        // Immediately start an asynchronous read operation on the new socket
        // so that the worker threads can process incoming data.
        StartAsyncRead(clientContext); 
    }

    // ... cleanup ...
}



DWORD WINAPI WorkerThread(LPVOID threadIndex) {
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED lpOverlapped = NULL;

    std::cout << "Worker Thread " << (int)(ULONG_PTR)threadIndex << " started." << std::endl;

    while (true) {
        // Blocks until an I/O operation completes and is placed on the queue
        BOOL success = GetQueuedCompletionStatus(
            g_hIOCP,
            &bytesTransferred,
            &completionKey,
            &lpOverlapped,
            INFINITE // Wait indefinitely
        );

        // The completionKey is our CLIENT_CONTEXT*
        CLIENT_CONTEXT* clientContext = (CLIENT_CONTEXT*)completionKey;
        if (!clientContext) continue; // Should not happen

        // lpOverlapped points to our IO_CONTEXT::Overlapped member
        IO_CONTEXT* ioContext = (IO_CONTEXT*)lpOverlapped;

        if (!success || (success && bytesTransferred == 0)) {
            // Connection closed or error. Handle cleanup.
            CleanupContext(clientContext, ioContext);
            continue;
        }

        // --- Process the Completed I/O Operation ---
        if (ioContext->OperationType == OP_READ) {
            // Echo the received data
            StartAsyncWrite(clientContext, ioContext, bytesTransferred);
            
            // Immediately start a new read operation
            StartAsyncRead(clientContext);

        } else if (ioContext->OperationType == OP_WRITE) {
            // Write completed, clean up this specific I/O context.
            delete ioContext; 
        }
    }
    return 0;
}



// Helper to initiate an asynchronous read
void StartAsyncRead(CLIENT_CONTEXT* clientContext) {
    IO_CONTEXT* ioContext = new IO_CONTEXT();
    ioContext->ClientSocket = clientContext->ClientSocket;
    ioContext->OperationType = OP_READ;
    
    // Set up the WSABUF
    ioContext->DataBuf.buf = ioContext->Buffer;
    ioContext->DataBuf.len = sizeof(ioContext->Buffer);

    DWORD flags = 0;
    DWORD bytes = 0;
    
    // WSARecv returns SOCKET_ERROR if the operation is pending (standard for IOCP)
    if (WSARecv(clientContext->ClientSocket, 
                &(ioContext->DataBuf), 
                1, 
                &bytes, 
                &flags, 
                &(ioContext->Overlapped), 
                NULL) == SOCKET_ERROR) 
    {
        // Ignore WSA_IO_PENDING (997), which means the operation is running asynchronously
        if (WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSARecv failed: " << WSAGetLastError() << std::endl;
            CleanupContext(clientContext, ioContext);
        }
    }
}

// Helper to initiate an asynchronous write (echo)
void StartAsyncWrite(CLIENT_CONTEXT* clientContext, IO_CONTEXT* readContext, DWORD size) {
    IO_CONTEXT* writeContext = new IO_CONTEXT();
    writeContext->ClientSocket = clientContext->ClientSocket;
    writeContext->OperationType = OP_WRITE;
    
    // Copy data from the completed read buffer to the new write buffer
    memcpy(writeContext->Buffer, readContext->Buffer, size);
    writeContext->DataBuf.buf = writeContext->Buffer;
    writeContext->DataBuf.len = size; 

    DWORD bytes = 0;

    // WSASend returns SOCKET_ERROR if the operation is pending
    if (WSASend(clientContext->ClientSocket, 
                &(writeContext->DataBuf), 
                1, 
                &bytes, 
                0, // flags
                &(writeContext->Overlapped), 
                NULL) == SOCKET_ERROR) 
    {
        if (WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSASend failed: " << WSAGetLastError() << std::endl;
            CleanupContext(clientContext, writeContext);
        }
    }
}

// Minimal cleanup function (needs more robust error checking in a real app)
void CleanupContext(CLIENT_CONTEXT* clientContext, IO_CONTEXT* ioContext) {
    if (clientContext) {
        closesocket(clientContext->ClientSocket);
        delete clientContext;
    }
    if (ioContext) {
        delete ioContext;
    }
}

