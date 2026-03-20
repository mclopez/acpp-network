#pragma once

//#include <cstdio>

#include <acpp-network/address.h>
#include <acpp-network/socket_base.h>
#include <detail/common.h>

namespace acpp::network {

enum class side_t {client, server};


namespace async {

template<typename Tuple, typename T>
struct append_to_tuple;

template<typename... Ts, typename T>
struct append_to_tuple<std::tuple<Ts...>, T> {
    using type = std::tuple<Ts..., T>;
};

// Helper alias
template<typename Tuple, typename T>
using append_to_tuple_t = typename append_to_tuple<Tuple, T>::type;

class null_layer;

template<typename Chain, int Int>
auto get_prev(void* p) {
    if constexpr (Int + 1 < std::tuple_size<Chain>()) {
        //using Prev = decltype(std::get<Int + 1>(Chain));
        using prev_type = std::tuple_element_t<Int + 1, Chain>;
        return static_cast<prev_type>(p);
    } else {
        return (null_layer*) nullptr;
    }
}


class null_layer {
public:    
    enum {it = 0,};  
    using chain_type = std::tuple<null_layer*>;
    //chain_type chain = {this};
    using last_type = null_layer;

    null_layer(side_t side):side_(side){}
    void* prev_; 

    template<typename Chain> 
    void connect() { 
        LOG_DEBUG("null_layer.connect");
    }


    template<typename Chain> 
    size_t write(const char* buf, size_t s) { 
        LOG_DEBUG("null_layer.write");
        return 0;
    }

    template<typename Chain> 
    void on_received(const char* buf, size_t s) { 
        LOG_DEBUG("null_layer.on_received");
        auto p = get_prev<Chain, it>(prev_);
        if (p) p->template on_received<Chain>(buf, s);
    }


    template <typename Chain>
    last_type& last() {
        return (null_layer&)*this;
    }

    //Next& next() { return next_;}

    private:
    side_t side_;
};  

class stream_context {
public:
    stream_context(acpp::network::async::io_context& io, side_t side, const std::string& hostname)
    :io_(io), side_(side), hostname_(hostname){}
    side_t side() { return side_;}
    const std::string& hostname() { return hostname_;}
    acpp::network::async::io_context& io() { return io_;}
private:
    std::string hostname_;
    side_t side_;
    acpp::network::async::io_context& io_;
};

template<typename Next>
class stream {
public:
    enum {it = Next::it+1,};  
    using next_type = Next;
    using chain_type = append_to_tuple_t<typename Next::chain_type, stream* >;
    using last_type = next_type::last_type;

    stream(side_t side):side_(side), next_(side) {
        next_.prev_ = this;
        LOG_DEBUG("stream this: {}", (void*) this);
    }

    stream(stream_context& c)
    :side_(c.side()), next_(c) {
        next_.prev_ = this;
        LOG_DEBUG("stream this: {}", (void*) this);
    }
    
    stream(stream&& s) = delete;


    void connect() {
        LOG_DEBUG("stream::connect");
        next_.template connect<chain_type>();
    }

    template<typename Chain> 
    void on_connected() { 
        LOG_DEBUG("stream.on_connected side_ {}", (int)side_);
        if (on_connected_cb_) {
            on_connected_cb_();
        }
    }

    void disconnect() {
        LOG_DEBUG("stream::disconnect");
        next_.template disconnect<chain_type>();
    }

    template<typename Chain> 
    void on_disconnected() { 
        LOG_DEBUG("stream.on_disconnected side_ {}", (int)side_);
        if (on_disconnected_cb_) {
            on_disconnected_cb_();
        }
    }

    size_t write(const char* buf, size_t s) { 
        LOG_DEBUG("stream::write side: {} msg: {}", (int)side_, std::string(buf, s));
        return next_.template write<chain_type>(buf, s);
    }

    template<typename Chain> 
    void on_received(const char* buf, size_t s) { 
        LOG_DEBUG("stream.on_received side: {} msg: {}", (int)side_, std::string(buf, s));
        if (on_received_cb_)
            on_received_cb_(buf, s);
    }




    // template <typename Chain>
    // auto last() {
    //     return next_.template last<Chain>();
    // } 

    auto last() {
        return next_.template last<chain_type >();
    } 


    Next& next() { return next_;}

    std::function<void()> on_connected_cb_;
    std::function<void()> on_disconnected_cb_;
    std::function<void(const char*, size_t)> on_received_cb_;

private:
    void* prev_;
    Next next_;
    side_t side_;
};

template<typename Next= null_layer>
class layer {
public:
    enum {it = Next::it+1,};  
    using next_type = Next;
    using chain_type = append_to_tuple_t<typename next_type::chain_type, layer* >;
    using last_type = next_type::last_type;

    layer(side_t side): side_(side), next_(side) {
        next_.prev_ = this;
    }

    template<typename Chain> 
    void connect() { 
        LOG_DEBUG("null_layer.connect");
    }

    template<typename Chain> 
    size_t write(const char* buf, size_t s) { 
        LOG_DEBUG("layer.write");
        return next_.template write<Chain>(buf, s);
    }

    template<typename Chain> 
    void on_received(const char* buf, size_t s) { 
        LOG_DEBUG("layer.on_received");
        auto p = get_prev<Chain, it>(prev_);
        if (p) p->template on_received<Chain>(buf, s);
    }


    template <typename Chain>
    last_type& last() {
        return next_.template last<Chain>();
    }
    void* prev_;

    Next& next() { return next_;}

private:
    side_t side_;
    next_type next_;    
};

class socket_stream {
public:
    enum {it = 0,};  
    using chain_type = std::tuple<socket_stream*>;
    using last_type = socket_stream;
    using address_type = ip_socketaddress;

    static constexpr int socket_type = SOCK_DGRAM;
    template <typename T, typename Chain>
    struct wrapper {
    public:
        wrapper(T& fe):fe_(fe){}
        T& fe_;

        template <typename Address>
        void connect(const Address& adr) { 
            fe_.template connect<Chain, Address>(adr);
        }

        void on_connected() { 
            fe_.template on_connected<Chain>();
        }

        void disconnect_() { 
            fe_.template connect<Chain>();
        }

        void on_disconnected() { 
            fe_.template on_disconnected<Chain>();
        }

        void on_received(const char* msg, size_t size) { 
            fe_.template on_received<Chain>(msg, size);
        }
        async_socket_base& socket() { return fe_.socket_;}

        void socket(async_socket_base&& s) {
            fe_.template socket<Chain>(std::move(s));
        } 

        //std::function<void()>& on_connect_cb_() { return fe_.on_connect_cb_; } 
        //std::function<void()>& on_disconnect_cb_() { return fe_.on_disconnect_cb_; } 
        //std::function<void(const char*, size_t)>& on_write_cb() { return fe_.on_write_cb_; } 

    };


    socket_stream(stream_context& c)
    :socket_(AF_INET, SOCK_STREAM, IPPROTO_TCP, c.io()), side_(c.side())
    {
    }

    // ~socket_stream() {
    //     LOG_DEBUG("~socket_stream()");
    // }

    template <typename Chain>
    void callback_init() {
        socket_.callbacks().on_connected = [&](async::async_socket_base& s) {
            //LOG_DEBUG("socket_stream connected fd:" + std::to_string(s.fd()));
            on_connected<Chain>();
        };
        socket_.callbacks().on_disconnected = [&](async::async_socket_base& s) {
            //LOG_DEBUG("socket_stream disconnected fd:" + std::to_string(s.fd()));
            on_disconnected<Chain>();
        };
        socket_.callbacks().on_received = [&](async::async_socket_base& s, const char* buf, size_t len){
            //LOG_DEBUG(std::format("socket_stream received fd: {} ", s.fd()));
            on_received<Chain>(buf, len);
        };
        socket_.callbacks().on_sent = [&](async::async_socket_base& s, size_t length) {
            //LOG_DEBUG("socket_stream on_sent fd:" + std::to_string(s.fd()) + " "  + std::to_string(length));
            if (!pending_data_.empty()) {
                auto n = socket_.write(pending_data_.data(), pending_data_.size());
                if (n > 0) {
                    //TODO: find a better way
                    pending_data_.erase(pending_data_.begin(), pending_data_.begin() + n);
                }
            }
        };

    }
    void* prev_; 

    acpp::network::side_t side_;
    std::vector<char> pending_data_;

    template<typename Chain, typename Address > 
    void connect(const Address& adr) { 
        LOG_DEBUG("socket_stream.connect side: {}", (int)side_);
        socket_.connect(to_sockaddr(adr));
    }

    template<typename Chain> 
    void on_connected() { 
        LOG_DEBUG("socket_stream.on_connected side: {} ***", (int)side_);
        auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
        if (prior) {
            LOG_DEBUG("socket_stream.on_connected side: {} prior: {}", (int)side_, (void*) prior);
            prior->template on_connected<Chain>();
        }
    }

    template<typename Chain> 
    void disconnect() { 
        LOG_DEBUG("socket_stream.disconnect side: {}", (int)side_);
        socket_.close();
    }

    template<typename Chain> 
    void on_disconnected() { 
        LOG_DEBUG("socket_stream.on_disconnected side: {}", (int)side_);
        auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
        if (prior)
            prior->template on_disconnected<Chain>();
    }


    template<typename Chain> 
    auto last() {
        using wrapper_type = wrapper<socket_stream, Chain>;
        //TODO: NYAPA ALERT!!!! do this in constructor!!!!
        if (!callback_init_) {
            callback_init_ = true;
            callback_init<Chain>();
        } 
        return wrapper_type(*this);
    }
    
    template<typename Chain> 
    size_t write(const char* buf, size_t size) {
        LOG_DEBUG("socket_stream.write side: {} size: {}", (int)side_, size);
        auto n = socket_.write(buf, size);
        if (n < size) {
            //TODO: find better way
            pending_data_.insert(pending_data_.end(), buf, buf + size);
        }
        return size;
    }

    template<typename Chain> 
    void on_received(const char* buf, size_t size) {
        LOG_DEBUG("socket_stream.on_received side: {} size: {} prev: {}", (int)side_, size, (void*)prev_);
        auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
        if (prior)
            prior->template on_received<Chain>(buf, size);
 
    }

    async_socket_base& socket() { return socket_;}

    template<typename Chain> 
    void socket(async_socket_base&& s) {
        socket_ = std::move(s);
        callback_init<Chain>();
    }
private:    
    async_socket_base socket_;
    bool callback_init_ = false;

};



} //namespace async

} //namespace acpp::network     