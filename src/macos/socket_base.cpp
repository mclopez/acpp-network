//  Copyright Marcos Cambón-López 2025.

// Distributed under the Mozilla Public License Version 2.0.
//    (See accompanying file LICENSE or copy at
//          https://www.mozilla.org/en-US/MPL/2.0/)

#include <fcntl.h>
#include <sys/event.h>

#include <iostream>
#include <sstream>
#include <unordered_map>

//#include <acpp-network/log.h>

#include <acpp-network/socket_base.h>
#include <detail/common.h>

namespace acpp::network {

void log_error_func(const std::string& func) {
    std::string error;
    error = strerror(errno);    
    log_error(std::format("[POSIX] Error in {}: {}", func, error));
}


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

namespace sync {

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

} //namespace sync

namespace async {

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
    
    size_t get_send_buffer_size() {
        int size;
        socklen_t size_len = sizeof(size);
        if (getsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &size, &size_len) < 0) {
            log_error_func("getsockopt SO_SNDBUF");
        }
        return size;
    }

    void set_send_buffer_size(int size) {
        socklen_t size_len = sizeof(size);
        if (setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &size, size_len) < 0) {
            log_error_func("setsockopt SO_SNDBUF");
        }
    }

    void ask_read_event() {
        LOG_DEBUG("socket_base_pimpl::ask_read_event fd: {}", fd_);
        struct kevent ev_set = {0};
        EV_SET(&ev_set, fd_, EVFILT_READ, EV_ADD|EV_ENABLE|EV_CLEAR, 0, 0, (void*)this);
        auto r = kevent(io_->fd(), &ev_set, 1, NULL, 0, NULL);  
        if (r < 0) {
            log_error_func("ask_read_event"); 
            throw socket_exception(errno, "ask_read_event");
        }
    }

    void ask_write_event() {
        LOG_DEBUG("socket_base_pimpl::ask_write_event fd: {}", fd_);
        struct kevent ev_set = {0};
        EV_SET(&ev_set, fd_, EVFILT_WRITE, EV_ADD|EV_ONESHOT, 0, 0, (void*)this);
        auto r = kevent(io_->fd(), &ev_set, 1, NULL, 0, NULL);        
        if (r < 0) {
            log_error_func("ask_write_event"); 
            throw socket_exception(errno, "ask_write_event");
        }
    }

    bool bind(const sockaddr& addr) {
        int yes = 1;
        setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));

        return ::bind(fd_, &addr, sizeof(sockaddr)) == 0;
    }

    int listen(int backlog) {
        ask_read_event();
        auto res = ::listen(fd_, backlog);
        if (res == -1) {
            log_error_func("listen");
        }else {
            listening_ = true;
            LOG_DEBUG("Socket listening on fd: {}", fd_);
        }
        return res;
    }

    bool connect(const sockaddr& adr) {
        ask_write_event();
        int res = ::connect(fd_, &adr, sizeof(sockaddr));

        return  (res == 0 || errno == EINPROGRESS); 
    }


size_t write(const char* buffer, size_t len) {
    //return write_buffer_.write(buffer, len);
    return so_write(buffer, len);
}

size_t so_write(const char* buffer, size_t len) {
    size_t result = 0;
    while(true) {
        //TODO: remove this limit 
        ssize_t len_aux = std::min(len, (size_t)1024 * 4); 
        auto n = so_write_internal(buffer, len_aux);
        if (n == 0) {
            break;
        }
        result = result + n;
        buffer = buffer + n;
        len = len - n;
    }
    return result;
}

size_t so_write_internal(const char* buffer, size_t len) {
    auto n = ::send(fd_, buffer, len, 0);
    LOG_DEBUG("so_write_internal(1) fd_: {} n: {} len: {}", fd_, n, len);
    if ( n > 0) {         
        return n;
    } else if (n == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            LOG_DEBUG("so_write_internal(3) fd_: {}  ask EPOLLOUT:  len: {}", fd_, len);
            //write_enabled_ = false;
            //set_events(EPOLLIN | EPOLLOUT, "so_write_internal");
            ask_write_event();
            return 0; // nothing send, kernel buffer full
        }
        log_error_func("send");
        if (callbacks_.on_error) {
            callbacks_.on_error(*parent_, errno, strerror(errno), "send");
        }
        //error nothing send
        return 0;
    }
    return 0;
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
    LOG_DEBUG("async_socket_base constructor without fd {}", (void*) this);
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
            log_error_func("kqueue");
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
                log_error_func("kevent"); //TODO: proper error handling
                throw socket_exception(errno, "kevent");
                //continue;
            }
            for (int i = 0; i < nev; i++) {
                auto data = (socket_base_pimpl *)events[i].udata;
                if (events[i].filter == EVFILT_READ) {
                    LOG_DEBUG("io_context::wait_for_input EVFILT_READ");
                    if (data->listening_) {
                        // New connection on listening socket
                        auto new_fd = ::accept(data->fd_, NULL, NULL);
                        if (new_fd == -1) {
                            log_error_func("accept");
                            if (data->callbacks_.on_error) {
                                data->callbacks_.on_error(*(data->parent_), errno, strerror(errno), "accept");
                            }
                        } else {
                            LOG_DEBUG("New connection accepted, fd: {}", new_fd);
                            if (data->callbacks_.on_accepted) {
                                async_socket_base new_socket(data->domain_, data->type_, data->protocol_, new_fd, *data->io_, socket_callbacks{});
                                new_socket.pimpl_->ask_read_event();
                                new_socket.pimpl_->connected_ = true;
                                data->callbacks_.on_accepted(*data->parent_, std::move(new_socket));
                            }
                        }
                    } else if (data->callbacks_.on_received || data->callbacks_.on_disconnected) {  
                        char buffer[4 * 1024]; //TODO: make this dynamic or configurable
                        LOG_DEBUG("io_context::wait_for_input EVFILT_READ data: {}", events[i].data);
                        ssize_t n;
                        while(true) {
                            n = ::recv(data->fd_, buffer, sizeof(buffer), 0); 
                            if (n > 0) {        
                                LOG_DEBUG("io_context::wait_for_input EVFILT_READ n: {}", n);
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
                                log_error_func("recv");
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
                    LOG_DEBUG("io_context::wait_for_input EVFILT_WRITE connected: {}", data->connected_);
                    if (!data->connected_) {
                        data->connected_ = true;

                        int err = 0;
                        socklen_t len = sizeof(err);
                        getsockopt(data->fd_, SOL_SOCKET, SO_ERROR, &err, &len);

                        if (err == 0) {
                            LOG_DEBUG("✅ Connected!");
                            if (data->callbacks_.on_connected) {
                                LOG_DEBUG("on_connected called");
                                data->callbacks_.on_connected(*(data->parent_));
                            }
                            data->ask_read_event();
                        } else {
                            LOG_ERROR("❌ Connect failed: {}", strerror(err));
                            if (data->callbacks_.on_error) {
                                data->callbacks_.on_error(*(data->parent_), errno, strerror(errno), "getsockopt");
                            }
                        }
                    } else {
                        //data->send_pending_data();
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
                    LOG_DEBUG("io_context::wait_for_input EVFILT_TIMER");
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
    as.pimpl_->ask_read_event();

}

void io_context::stop() {
    pimpl_->run_ = false;
}



int64_t io_context::fd() const  {
    return pimpl_->kq_;
}

} // namespace async


} //namespace acpp::network