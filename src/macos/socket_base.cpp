//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#include <fcntl.h>
#include <sys/event.h>

#include <iostream>
#include <sstream>
#include <unordered_map>


#include <acpp-network/socket_base.h>


namespace acpp::network {


socket_exception::socket_exception(int error_code, const std::string_view hint) {
    error_code_ = error_code;
    std::string error;
    error = strerror(error_code_);
    std::stringstream ss;
    ss << "[POSIX] Error in " << hint << ": " << error;
    msg_ = ss.str();
}
socket_exception::socket_exception(const std::string_view hint) {
    error_code_ = errno;
    std::string error;
    error = strerror(error_code_);
    std::stringstream ss;
    ss << "[POSIX] Error in " << hint << ": " << error;
    msg_ = ss.str();
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


void log_error(const std::string& func) {
    std::string error;
    error = strerror(errno);
    std::cerr << "[POSIX] Error in " << func << ": " << error << std::endl;
}

struct socket_base_pimpl {
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


    void set_events(decltype(EVFILT_WRITE) events) {
        std::cout << "socket_base_pimpl::set_events fd: " << fd_ << " events: " << events <<std::endl;
        struct kevent ev_set = {0};
        events |= EVFILT_WRITE;
        EV_SET(&ev_set, fd_, events, EV_ADD|EV_ENABLE|EV_CLEAR, 0, 0, (void*)this);
        kevent(io_->fd(), &ev_set, 1, NULL, 0, NULL);        
    }

    void set_events_once(decltype(EVFILT_WRITE) events) {
        struct kevent ev_set = {0};
        //auto flag = enable ? EV_ENABLE : EV_DISABLE;
        events |= EVFILT_WRITE;
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

    size_t write(const char* buffer, size_t len) {
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
                    set_events(EVFILT_WRITE);
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


class timer_impl {
public:
    
    timer_impl(timer& parent, io_context& io, int milliseconds, timer::on_timeout_callback&& cb);      

    ~timer_impl() {
        cancel();
    }   

    uintptr_t next_timer_id() {
        static std::atomic<uintptr_t> timer_id_counter = 0;
        return ++timer_id_counter;
    }

    void cancel();

//    private:
    io_context* io_;
    int milliseconds_;
    timer::on_timeout_callback cb_;
    timer* parent_;
};


void timer::cancel() {
    pimpl_->cancel();
}

struct io_context_pimpl {
    std::atomic_bool run_;
    int kq_;
    std::mutex exec_mutex_;
    std::mutex timers_mutex_;
    std::queue<std::function<void()>> pending_callbacks_; 
    constexpr static size_t callback_id = 1;
    
    io_context_pimpl() : run_(false), kq_(-1) {
        kq_ = kqueue();
        //TODO: error check
        if (kq_ == -1) {
            log_error("kqueue");
            throw socket_exception(errno, "kqueue");
        }    

        struct kevent ev_set = {0};
        EV_SET(&ev_set, callback_id, EVFILT_USER, EV_ADD|EV_CLEAR, 0, 0, nullptr);
        kevent(kq_, &ev_set, callback_id, NULL, 0, NULL);        
    }


    void exec(std::function<void()>&& f) {
        {
            std::lock_guard<std::mutex> lock(exec_mutex_);
            pending_callbacks_.push(std::move(f));  
        }
        struct kevent ev_set = {0};
        EV_SET(&ev_set, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
        kevent(kq_, &ev_set, 1, NULL, 0, NULL);          
    }

    
    uintptr_t next_timer_id() {
        static std::atomic<uintptr_t> timer_id_counter = 0;
        return ++timer_id_counter;
    }

    void wait_for_input() {
        run_ = true;
        while (run_) {
            struct kevent events[5];
            int nev = kevent(kq_, NULL, 0, events, sizeof(events)/sizeof(struct kevent), NULL);
            if (nev < 0) {
                log_error("kevent"); //TODO: proper error handling
                throw socket_exception(errno, "kevent");
                //continue;
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
                            if (data->callbacks_.on_error) {
                                data->callbacks_.on_error(*(data->parent_), errno, strerror(errno), "accept");
                            }
                        } else {
                            std::cout << "New connection accepted, fd: " << new_fd << std::endl;
                            if (data->callbacks_.on_accepted) {
                                async_socket_base new_socket(data->domain_, data->type_, data->protocol_, new_fd, *data->io_, socket_callbacks{});
                                new_socket.pimpl_->set_events(EVFILT_READ|EVFILT_WRITE);
                                new_socket.pimpl_->connected_ = true;
                                data->callbacks_.on_accepted(*data->parent_, std::move(new_socket));
                            }
                        }
                    } else if (data->callbacks_.on_received || data->callbacks_.on_disconnected) {  
                        char buffer[1024]; //TODO: make this dynamic or configurable
                        std::cout << "io_context::wait_for_input EVFILT_READ data: " << events[i].data << std::endl;
                        ssize_t n;
                        while(true) {
                            n = ::recv(data->fd_, buffer, sizeof(buffer), 0); 
                            if (n > 0) {        
                                std::cout << "io_context::wait_for_input EVFILT_READ n: " << n << std::endl;
                                if (data->callbacks_.on_received){
                                    data->callbacks_.on_received(*(data->parent_), buffer, n); 
                                }
                            } else if (n == 0)    {
                                data->callbacks_.on_disconnected(*(data->parent_)); 
                                break;
                            } else if (n == -1) {
                                if ((errno == EAGAIN || errno == EWOULDBLOCK)) {
                                    // No more data to read
                                    break;
                                }
                                log_error("recv");
                                if (data->callbacks_.on_error) {
                                    data->callbacks_.on_error(*(data->parent_), errno, strerror(errno), "recv");
                                }
                                break;
                            }

                            // } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                            //     // No more data to read
                            //     break;
                            // } else {
                            //     break;
                            // }
                            //break;
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
                            data->set_events(EVFILT_READ|EVFILT_WRITE);
                        } else {
                            std::cerr << "❌ Connect failed: " << strerror(err) << "\n";
                            if (data->callbacks_.on_error) {
                                data->callbacks_.on_error(*(data->parent_), errno, strerror(errno), "getsockopt");
                            }
                        }
                    } else {
                        if (data->callbacks_.on_sent) {
                            data->callbacks_.on_sent(*(data->parent_), 0);
                        }
                    }
                } else if (events[i].filter == EVFILT_USER) {
                    std::queue<std::function<void()>> tasks;
                    {
                        std::lock_guard<std::mutex> lock(exec_mutex_);
                        std::swap(tasks, pending_callbacks_);
                    }
                    while (!tasks.empty()) {
                        tasks.front()();
                        tasks.pop();
                    }
                } else if (events[i].filter == EVFILT_TIMER) {
                    std::cout << "io_context::wait_for_input EVFILT_TIMER" << std::endl;
                    timer_impl* timer = (timer_impl*)events[i].ident;
                    if (timer) {
                        if (timer->cb_) {
                            timer->cb_(*(timer->parent_));
                        }
                    }   
                }
            }    
        }
    }
};


timer_impl::timer_impl(timer& parent, io_context& io, int milliseconds, timer::on_timeout_callback&& cb)      
: parent_(&parent), io_(&io), milliseconds_(milliseconds), cb_(std::move(cb)) {
    struct kevent ev_set = {0};
    //using this as timer id
    EV_SET(&ev_set, (uintptr_t)this, EVFILT_TIMER, EV_ADD|EV_ONESHOT, 0, milliseconds, NULL);
    kevent(io_->pimpl_->kq_, &ev_set, 1, NULL, 0, NULL);    
}

void timer_impl::cancel() {
    struct kevent ev_set = {0};
    EV_SET(&ev_set, (uintptr_t)this, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
    kevent(io_->pimpl_->kq_, &ev_set, 1, NULL, 0, NULL);  
    //::close        
}


timer::timer(io_context& io, int milliseconds, timer::on_timeout_callback&& cb)
:pimpl_(std::make_unique<timer_impl>(*this, io, milliseconds, std::move(cb))) {

}
timer::timer(timer &&other) {
    pimpl_ = std::move(other.pimpl_);
}



timer::~timer(){}


io_context::io_context() {
    pimpl_ = std::make_unique<io_context_pimpl>();
}

io_context::~io_context() {
    ::close(pimpl_->kq_);    
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
    pimpl_->run_ = false;
}



int64_t io_context::fd() const  {
    return pimpl_->kq_;
}



} //namespace acpp::network