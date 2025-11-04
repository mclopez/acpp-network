//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#include <iostream>

#include <fcntl.h>
//#include <sys/types.h>
#include <sys/event.h>
//#include <sys/time.h>

#include <acpp-network/socket_base.h>


namespace acpp::network {


const int socket_base::invalid_fd = -1;


socket_base::socket_base():fd_(invalid_fd) {

}

socket_base::socket_base(int fd):fd_(fd) {
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
        ::close(fd_);
        fd_ = invalid_fd;
    }
}

void socket_base::create_impl(int domain, int type, int protocol) {
    if (valid())
        close();
    fd_ = ::socket(domain, type, protocol);
}

bool socket_base::connect(const sockaddr& adr) {
    return ::connect(fd_, &adr, sizeof(sockaddr)) == 0;
}


void log_error(const std::string& func) {
    std::string error;
    error = strerror(errno);
    std::cerr << "[POSIX] Error in " << func << ": " << error << std::endl;
}

struct socket_base_pimpl {
public:    
    int domain_;
    int type_; 
    int protocol_;
    int64_t fd_;
    io_context* io_;
    async_socket_base* parent_;
    socket_callbacks callbacks_;
    bool connected_ = false;
    bool listening_ = false;
    static const int64_t invalid_fd = -1;


    // socket_base_pimpl(async_socket_base& parent)
    // :fd_(async_socket_base::invalid_fd),io_(nullptr), parent_(&parent){

    // }   


    socket_base_pimpl(int domain, int type, int protocol, int fd, io_context& io, socket_callbacks&& callbacks)
    :   domain_(domain), type_(type), protocol_(protocol), 
        fd_(fd),
        io_(&io), callbacks_(std::move(callbacks)) 
    {
        if (valid()) {
            int flags = fcntl(fd_, F_GETFL, 0);
            fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        }
    }

    socket_base_pimpl(int domain, int type, int protocol, io_context& io, socket_callbacks&& callbacks)
    :   domain_(domain), type_(type), protocol_(protocol), 
        fd_(::socket(domain, type, protocol)),
        io_(&io), callbacks_(std::move(callbacks)) 
    {
        if (valid()) {
            int flags = fcntl(fd_, F_GETFL, 0);
            fcntl(fd_, F_SETFL, flags | O_NONBLOCK);
        }
    }


    void set_events(decltype(EVFILT_WRITE) events) {
        struct kevent ev_set = {0};
        EV_SET(&ev_set, fd_, events, EV_ADD|EV_ENABLE|EV_CLEAR, 0, 0, (void*)this);
        kevent(io_->fd(), &ev_set, 1, NULL, 0, NULL);        
    }

    void set_events_once(decltype(EVFILT_WRITE) events) {
        struct kevent ev_set = {0};
        //auto flag = enable ? EV_ENABLE : EV_DISABLE;
        EV_SET(&ev_set, fd_, events, EV_ADD|EV_ONESHOT, 0, 0, (void*)this);
        kevent(io_->fd(), &ev_set, 1, NULL, 0, NULL);        
    }

    bool bind(const sockaddr& addr) {
        return ::bind(fd_, &addr, sizeof(sockaddr)) == 0;
    }

    int listen(int backlog) {
        set_events(EVFILT_READ);
        auto res = ::listen(fd_, backlog);
        if (res == -1) {
            log_error("listen");
        }else {
            listening_ = true;
            std::cout << "Socket listening on fd " << fd_ << std::endl;
        }
        return res;
    }

    bool connect(const sockaddr& adr) {
        set_events_once(EVFILT_WRITE);
        int res = ::connect(fd_, &adr, sizeof(sockaddr));

        return  (res == 0 || errno == EINPROGRESS); 
    }

    bool valid() const {
        return (fd_ != invalid_fd);
    }

    void close() {
        if (valid()) {
            ::close(fd_);
            fd_ = invalid_fd;
        }
    }
};

// async_socket_base::async_socket_base() {    
//     pimpl_ = std::make_unique<socket_base_pimpl>(*this);
// }

// void async_socket_base::create_impl(int domain, int type, int protocol) {
//     if (valid())
//         close();
//     pimpl_->fd_ = ::socket(domain, type, protocol);
//     int flags = fcntl(pimpl_->fd_, F_GETFL, 0);
//     fcntl(pimpl_->fd_, F_SETFL, flags | O_NONBLOCK);
//     pimpl_->io_->add_socket(*this);
// }


async_socket_base::async_socket_base(int domain, int type, int protocol, io_context& io, socket_callbacks&& callbacks) {
    std::cout << "async_socket_base constructor without fd " << (void*) this << std::endl;
    pimpl_ =  std::make_unique<socket_base_pimpl>(domain, type, protocol, io, std::move(callbacks));
    pimpl_->parent_ = this;
    io.add_socket(*this);
}

async_socket_base::async_socket_base(int domain, int type, int protocol, fd_type fd, io_context& io, socket_callbacks&& callbacks) {
    pimpl_ =  std::make_unique<socket_base_pimpl>(domain, type, protocol, fd, io, std::move(callbacks));
    pimpl_->parent_ = this;
    io.add_socket(*this);
}

async_socket_base::async_socket_base(async_socket_base&& other) noexcept {
    pimpl_ = std::move(other.pimpl_);
    pimpl_->parent_ = this;
    //TODO: should we create a impl?
    //other.pimpl_ = std::make_unique<socket_base_pimpl>(other);
}

async_socket_base::~async_socket_base() {
    close(); 
}

async_socket_base& async_socket_base::operator=(async_socket_base&& other) noexcept {
    pimpl_ = std::move(other.pimpl_);
    pimpl_->parent_ = this;
    //other.pimpl_ = std::make_unique<socket_base_pimpl>(other);
    return *this;
}


bool async_socket_base::bind(const sockaddr& adr) {
    return ::bind(pimpl_->fd_, &adr, sizeof(sockaddr)) == 0;

}

int async_socket_base::listen(int backlog) {
    return pimpl_->listen(backlog);
}

bool async_socket_base::connect(const sockaddr& adr) {
    return pimpl_->connect(adr);
}

void async_socket_base::callbacks(socket_callbacks&& calbacks) {
    pimpl_->callbacks_ = std::move(calbacks);
}

socket_callbacks& async_socket_base::callbacks() {
    return pimpl_->callbacks_;
}

void async_socket_base::read() {
    //TOOD: check vality
    if (!valid()) {
        throw(socket_exception("Socket not valid"));
    }
    //TODO: implement
    std::cout << "async_socket_base::read set EVFILT_READ" << std::endl;
    pimpl_->set_events(EVFILT_READ);
}

size_t async_socket_base::write(const char* buffer, size_t) {
    //TOOD: check vality
    if (!valid()) {
        throw(socket_exception("Socket not valid"));
    }
    ::send(pimpl_->fd_, buffer, strlen(buffer), 0);
    return 0;
}

void async_socket_base::close() {
    if (pimpl_) {
        pimpl_->close();
    }
}


bool async_socket_base::valid() const {
    if (pimpl_) 
        return pimpl_->valid();
    return false;
}

int64_t async_socket_base::fd() {
    return pimpl_->fd_;
}

//const int64_t socket_base_pimpl::invalid_fd = -1;

struct io_context_pimpl {
    std::atomic_bool run;
    int kq;
};

io_context::io_context() {
    pimpl_ = std::make_unique<io_context_pimpl>();
    pimpl_->kq = kqueue();
    //TODO: error check
    if (pimpl_->kq == -1) {
        log_error("kqueue");
    }    
}

io_context::~io_context() {
    ::close(pimpl_->kq);    
}

void io_context::wait_for_input() {
    pimpl_->run = true;
    while (pimpl_->run) {
        struct kevent events[5];
        int nev = kevent(pimpl_->kq, NULL, 0, events, sizeof(events)/sizeof(struct kevent), NULL);
        if (nev < 0) {
            log_error("kevent");
            continue;
        }
        for (int i = 0; i < nev; i++) {
            auto data = (socket_base_pimpl *)events[i].udata;
            if (events[i].filter == EVFILT_READ) {
                std::cout << "io_context::wait_for_input EVFILT_READ" << std::endl;
                if (data->listening_) {
                    // New connection on listening socket
                    auto new_fd = ::accept(data->fd_, NULL, NULL);
                    if (new_fd == -1) {
                        log_error("accept");
                    } else {
                        std::cout << "New connection accepted, fd: " << new_fd << std::endl;
                        if (data->callbacks_.on_accepted) {
                            async_socket_base new_socket(data->domain_, data->type_, data->protocol_, new_fd, *data->io_, socket_callbacks{});
                            //new_socket.pimpl_->fd_ = new_fd;
                            //new_socket.pimpl_->io_ = data->io_;
                            new_socket.pimpl_->connected_ = true;
                            data->callbacks_.on_accepted(*data->parent_, std::move(new_socket));
                        }
                    }
                } else if (data->callbacks_.on_received)   {
                    char buffer[1024];
                    auto n = ::recv(data->fd_, buffer, sizeof(buffer), 0); 
                    std::cout << "io_context::wait_for_input EVFILT_READ n: " << n << std::endl;
                    if (n>0)    {
                        data->callbacks_.on_received(*(data->parent_), buffer, n); //TODO: handle n==0 and n<0
                    } else {
                        log_error("recv");
                    }

                }
            } else if (events[i].filter == EVFILT_WRITE) {
                std::cout << "io_context::wait_for_input EVFILT_WRITE" << std::endl;
                if (!data->connected_) {
                    data->connected_ = true;

                    int err = 0;
                    socklen_t len = sizeof(err);
                    getsockopt(data->fd_, SOL_SOCKET, SO_ERROR, &err, &len);

                    if (err == 0) {
                        std::cout << "✅ Connected!\n";
                        if (data->callbacks_.on_connected) {
                            std::cout << "on_connected called\n";
                            data->callbacks_.on_connected(*(data->parent_));
                        }
                        //data->set_events(EVFILT_READ);
                    } else {
                        std::cerr << "❌ Connect failed: " << strerror(err) << "\n";
                    }


                } else {
                    if (data->callbacks_.on_sent) {
                        data->callbacks_.on_sent(*(data->parent_));
                    }
                }
            }
        }    
    }
}

void io_context::exec(std::function<void()>&&) {

}

void io_context::remove_socket(async_socket_base& as) {

}

void io_context::add_socket(async_socket_base& as) {
    /*
    struct kevent ev_set;
    EV_SET(&ev_set, as.fd(), EVFILT_WRITE
    //|EVFILT_READ
    , EV_ADD, 0, 0, (void*)as.pimpl_.get());
    kevent(pimpl_->kq, &ev_set, 1, NULL, 0, NULL);
    */
    as.pimpl_->set_events_once(EVFILT_WRITE);

}

void io_context::stop() {
    pimpl_->run = false;
}



int64_t io_context::fd() const  {
    return pimpl_->kq;
}



} //namespace acpp::network