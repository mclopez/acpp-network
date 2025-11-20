
//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <atomic>
#include <queue>


#include <acpp-network/socket_base.h>


namespace acpp::network {


void log_error(const std::string& func) {
    std::string error;
    error = strerror(errno);
    std::cerr << "[POSIX] Error in " << func << ": " << error << std::endl;
}


socket_exception::socket_exception(int error_code, std::string_view hint)
:error_code_(error_code)
{
    std::string error;
    error = strerror(error_code_);
    std::stringstream ss;
    ss << "[POSIX] Error in " << hint << ": " << error;
    msg_ = ss.str();
}


socket_exception::socket_exception(std::string_view hint)
:socket_exception(errno, hint)
{
}



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




class event_handler{
public:
    virtual ~event_handler() = default;
    virtual void handle_event(uint32_t events) = 0;
};





struct socket_base_pimpl: public event_handler {
    friend class io_context_pimpl;
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
    std::vector<char> write_buffer_;


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



    bool bind(const sockaddr& addr) {
        return ::bind(fd_, &addr, sizeof(sockaddr)) == 0;
    }

    int listen(int backlog);

    bool connect(const sockaddr& adr);

    size_t write(const char* buffer, size_t len);
    
    bool valid() const {
        return (fd_ != invalid_fd);
    }



    void close() {
        if (valid()) {
            ::close(fd_);
            fd_ = invalid_fd;
        }
    }

    void handle_event(uint32_t events) override;


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

// void async_socket_base::read() {
//     //TOOD: check vality
//     if (!valid()) {
//         throw(socket_exception("Socket not valid"));
//     }
//     //TODO: implement
//     std::cout << "async_socket_base::read set EVFILT_READ" << std::endl;
//     pimpl_->set_events(EVFILT_READ);
// }

size_t async_socket_base::write(const char* buffer, size_t len) {
    return pimpl_->write(buffer, len);
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



class timer_impl: public event_handler {
public:

    timer_impl(io_context& io, timer& parent, int milliseconds, timer::on_timeout_callback&& cb={});

    ~timer_impl() {
        cancel();
    }

    void cancel() {
        if (timer_fd_ != -1)
            ::close(timer_fd_);
    }

    void handle_event(uint32_t events) override {   
        if (events & EPOLLIN) {
            if (callback_) {
                callback_(*parent_);
            }
        }
    }

    // void init(size_t id, int epollfd) {  
    //     timer_id_ = id;
    //     epollfd_ = epollfd;   

    // }
private:
    int timer_fd_;
    //int epollfd_;    
    io_context* io_;
    int milliseconds_;
    timer::on_timeout_callback callback_;  
    timer* parent_;
};




void timer::cancel() {
        pimpl_->cancel();
}

class exec_event_handler : public event_handler {
public:
    exec_event_handler(io_context_pimpl& io_pimpl);

    ~exec_event_handler() override{
        close(fd_);
    }   

    void handle_event(uint32_t events) override;

    void trigger() {
        static std::atomic<uint64_t> cont;
        uint64_t counter = ++cont;
        ssize_t s = write(fd_, &counter, sizeof(uint64_t));
        if (s != sizeof(uint64_t)) {
            log_error("exec_event_handler::trigger write");
            throw socket_exception("exec_event_handler::trigger write");
        }
    }
    
private:
    io_context_pimpl* io_pimpl_;    
    int fd_;
};


struct io_context_pimpl {
    friend socket_base_pimpl;
    friend timer_impl;

public:    
    std::atomic_bool run;
    int epollfd;
    std::mutex exec_mutex_;
    std::queue<std::function<void()>> pending_callbacks_; 

    constexpr static size_t callback_id = 1;
    io_context* parent_;
    exec_event_handler exec_handler_{*this};
    

    io_context_pimpl(io_context& parent) 
    : epollfd(epoll_create1(0)), parent_(&parent), run(false) {
        ;
        if (epollfd == -1) {
            log_error("epoll_create1");
            throw socket_exception("epoll_create1");
        }    

    }


    void exec(std::function<void()>&& f) {
        {
            std::lock_guard<std::mutex> lock(exec_mutex_);
            pending_callbacks_.push(std::move(f));  
        }
        exec_handler_.trigger();
    }

    void wait_for_input() {
        run = true;
        while (run) {
            constexpr size_t MAX_EVENTS = 5;
            struct epoll_event events[MAX_EVENTS];

            int nev = epoll_wait(epollfd, events, MAX_EVENTS, -1);

            if (nev < 0) {
                log_error("epoll_wait"); //TODO: proper error handling
                throw socket_exception("epoll_wait");
            }
            for (int i = 0; i < nev; i++) {
                auto data = (event_handler*)events[i].data.ptr;
                data->handle_event(events[i].events);
            }    
        }
    }

    void set_events(decltype(EPOLLOUT) events, int fd, event_handler* handler) {
        std::cout << "io_context_pimpl::set_events fd: " << fd << " events: " << events <<std::endl;

        struct epoll_event sd;
        sd.events = events;
        sd.data.ptr = handler;

        if (epoll_ctl(epollfd, EPOLL_CTL_ADD|EPOLL_CTL_MOD, fd, &sd) == -1) {
            log_error("epoll_ctl 2");
        }

    }

    void set_events_once(decltype(EPOLLOUT) events, int fd, event_handler* handler) {

        struct epoll_event sd;
        sd.events = events|EPOLLONESHOT;
        sd.data.ptr = handler;

        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &sd) == -1) {
            log_error("epoll_ctl 1");
        }
    }
};

exec_event_handler::exec_event_handler(io_context_pimpl& io_pimpl)
: io_pimpl_(&io_pimpl) {
    // Register this handler with the io_context
    //io_pimpl_.parent_->add_event_handler(this); 
    fd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (fd_ == -1) {
        throw socket_exception("eventfd");
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.ptr = this; 

    if (epoll_ctl(io_pimpl_->epollfd, EPOLL_CTL_ADD, fd_, &event) == -1) {
        throw socket_exception("epoll_ctl failed for eventfd");
    }
}   

void exec_event_handler::handle_event(uint32_t events) {
    std::queue<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(io_pimpl_->exec_mutex_);
        std::swap(callbacks, io_pimpl_->pending_callbacks_);
    }
    while (!callbacks.empty()) {
        auto& cb = callbacks.front();
        cb();
        callbacks.pop();
    }
}


int socket_base_pimpl::listen(int backlog) {
    io_->pimpl_->set_events(EPOLLIN, fd_, this);
    auto res = ::listen(fd_, backlog);
    if (res == -1) {
        log_error("listen");
    }else {
        listening_ = true;
        std::cout << "Socket listening on fd " << fd_ << std::endl;
    }
    return res;
}

bool socket_base_pimpl::connect(const sockaddr& adr) {
    io_->pimpl_->set_events_once(EPOLLOUT, fd_, this);
    int res = ::connect(fd_, &adr, sizeof(sockaddr));

    return  (res == 0 || errno == EINPROGRESS); 
}


size_t socket_base_pimpl::write(const char* buffer, size_t len) {
    std::cout << "*********.  async_socket_base::write sent len: " << len << std::endl;
    //TOOD: check vality
    if (!valid()) {
        throw(socket_exception("Socket not valid"));
    }
    while (true) {
        auto n = ::send(fd_, buffer, len, 0);
        if (n > 0) {
            std::cout << "async_socket_base::write sent n: " << n << std::endl;
            len -= n;
            buffer += n;    
            if (len <= 0) {
                return n;
            }
        }    
        if (n == -1) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                // Would block, need to wait for writability    
                write_buffer_.insert(write_buffer_.end(), buffer, buffer + len);
                io_->pimpl_->set_events_once(EPOLLOUT, fd_, this);
                return 0;
            }
            log_error("send");
            if (callbacks_.on_error) {
                callbacks_.on_error(*parent_, errno, strerror(errno), "send");
            }
            return 0;
        }
    }

    return 0; //not used
}




void socket_base_pimpl::handle_event(uint32_t events)  {   
    if (events & EPOLLIN) {
        std::cout << "io_context::wait_for_input EPOLLIN" << std::endl;
        if (listening_) {
            // New connection on listening socket
            auto new_fd = ::accept(fd_, NULL, NULL);
            if (new_fd == -1) {
                log_error("accept");
                if (callbacks_.on_error) {
                    callbacks_.on_error(*parent_, errno, strerror(errno), "accept");
                }
            } else {
                std::cout << "New connection accepted, fd: " << new_fd << std::endl;
                if (callbacks_.on_accepted) {
                    async_socket_base new_socket(domain_, type_, protocol_, new_fd, *io_, socket_callbacks{});
                    io_->pimpl_->set_events(EPOLLIN, new_socket.fd(), new_socket.pimpl_.get());
                    new_socket.pimpl_->connected_ = true;
                    callbacks_.on_accepted(*parent_, std::move(new_socket));
                }
            }
        } else if (callbacks_.on_received || callbacks_.on_disconnected) {  
            char buffer[1024]; //TODO: make this dynamic or configurable
            auto n = ::recv(fd_, buffer, sizeof(buffer), 0); 
            std::cout << "io_context::wait_for_input EVFILT_READ n: " << n << std::endl;
            if (n == 0)    {
                callbacks_.on_disconnected(*(parent_)); 
            } else if (n > 0)    {
                callbacks_.on_received(*(parent_), buffer, n); 
            } else {
                log_error("recv");
                if (callbacks_.on_error) {
                    callbacks_.on_error(*(parent_), errno, strerror(errno), "recv");
                }
            }
        }
    } else if (events | EPOLLOUT) {
        std::cout << "io_context::wait_for_input EPOLLOUT" << std::endl;
        if (!connected_) {
            connected_ = true;

            int err = 0;
            socklen_t len = sizeof(err);
            getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &len);

            if (err == 0) {
                std::cout << "✅ Connected!\n";
                if (callbacks_.on_connected) {
                    std::cout << "on_connected called\n";
                    callbacks_.on_connected(*(parent_));
                }
                io_->pimpl_->set_events(EPOLLIN, fd_, this);
            } else {
                std::cerr << "❌ Connect failed: " << strerror(err) << "\n";
                if (callbacks_.on_error) {
                    callbacks_.on_error(*(parent_), errno, strerror(errno), "getsockopt");
                }
            }
        } else {
            if (callbacks_.on_sent) {
                callbacks_.on_sent(*(parent_), 0);
            }
        }
    }  
}

timer_impl::timer_impl(io_context& io, timer& parent, int milliseconds, timer::on_timeout_callback&& cb)
: timer_fd_(-1), io_(&io), parent_(&parent), milliseconds_(milliseconds), callback_(std::move(cb)) {


    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd_ == -1) {
        throw socket_exception("timerfd_create failed");
    }

    struct itimerspec ts;

    auto secs = milliseconds_ / 1000;
    auto millis_remainder = milliseconds_ % 1000;

    ts.it_value.tv_sec = secs;

    ts.it_value.tv_nsec = millis_remainder * 1000 * 1000;

    // Interval: Repeat every 3 seconds
    ts.it_interval.tv_sec = 0; 
    ts.it_interval.tv_nsec = 0;


    // 3. Start the timer
    // TFD_TIMER_ABSTIME (0): The time is relative to the moment timerfd_settime is called.
    if (timerfd_settime(timer_fd_, 0, &ts, NULL) == -1) {
        //log_error("timerfd_settime");
        throw socket_exception("timerfd_settime failed timer_fd_:" + std::to_string(timer_fd_));
    }

    io_->pimpl_->set_events_once(EPOLLIN, timer_fd_, this);

}



timer::timer(io_context& io, int milliseconds, on_timeout_callback&& cb)
: pimpl_(std::make_unique<timer_impl>(io, *this, milliseconds, std::move(cb))) {

}

timer::timer(timer &&other) {
    pimpl_ = std::move(other.pimpl_);
}



timer::~timer(){}


io_context::io_context() {
    pimpl_ = std::make_unique<io_context_pimpl>(*this);
}

io_context::~io_context() {
    ::close(pimpl_->epollfd);    
}

void io_context::wait_for_input() {
    pimpl_->wait_for_input();
}

void io_context::exec(std::function<void()>&& f) {
    pimpl_->exec(std::move(f));
}


void io_context::remove_socket(async_socket_base& as) {

}

void io_context::add_socket(async_socket_base& as) {

    pimpl_->set_events_once(EPOLLOUT, as.fd(), as.pimpl_.get());

}

void io_context::stop() {
    pimpl_->run = false;
}



int64_t io_context::fd() const  {
    return pimpl_->epollfd;
}



} //namespace acpp::network