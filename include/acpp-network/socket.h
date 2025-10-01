#pragma once

#include <string>

class Socket {
public:
    Socket(const std::string& hostname, const std::string& port);

    ~Socket();

    // Prevent copies to maintain single ownership of the file descriptor.
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    bool connect();
    size_t send(const char* data, size_t len);
    size_t receive(char* buffer, size_t len);
    void close();

    bool is_valid() const;
    int get_fd() const { return fd_; }

private:
    int fd_ = -1; 

    bool resolve_address(); 
};