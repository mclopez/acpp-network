#pragma once

//#include <cstdio>

#include <acpp-network/address.h>
#include <acpp-network/socket_base.h>
#include <acpp-network/detail/common.h>

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


template <int Index>
class ERROR_IndexOutOfRange;

template<typename Chain, int It>
auto& get_prev(Chain& chain) {
    constexpr size_t index = It + 1;
    if constexpr (0 <= index && index < std::tuple_size<Chain>()) {
        using prev_type = std::tuple_element_t<It + 1, Chain>;
        LOG_INFO("std::get<index>(chain) {}", (void*)std::get<index>(chain));
        return *std::get<index>(chain);
    } else {
        //static_assert(false, "Index out of range");
        ERROR_IndexOutOfRange<index> error;
        int r = 0;
        return r;
    }        
}



template <typename Layer, typename Next = null_layer>
class layer_base {
public:
    enum {it = Next::it + 1,};

    using next_type = Next;
    using last_type = next_type::last_type;
    using current_type = Layer;
    //the chain contains the objects in the reverse order.
    using chain_type = append_to_tuple_t<typename next_type::chain_type, current_type* >;


    template<typename Chain, typename Context>
    layer_base(Chain& chain, Context& context)
    :next_(chain, context) {
        std::get<it>(chain) = (current_type*)this;
    }

    template<typename Chain>
    auto& prev(Chain& c) {
        return get_prev<Chain, it>(c);
    }
protected:
    next_type next_;
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

//first in the chain
template<typename Next>
class stream: public layer_base<stream<Next>, Next > {
public:
    using base_type = layer_base<stream<Next>, Next >;

    template<typename Context > 
    stream(Context& ctx)
    :base_type(chain_, ctx), side_(ctx.side())/*, next_(chain_, c)*/ {
        LOG_DEBUG("stream this: {}", (void*) this);
        LOG_INFO("stream::stream pos 1: {}", (void*)std::get<1>(chain_));
        LOG_INFO("stream::stream last: {}", (void*)std::get<0>(chain_));
        //std::get<0>(chain_) = &this->next_;
        //std::get<1>(chain_) = this;
    }
    //TODO: allow move
    stream(stream&& s) = delete;


    void connect() {
        LOG_DEBUG("stream::connect");
        this->next_.connect(chain_);
    }

    template<typename Chain> 
    void on_connected(Chain& chain) { 
        LOG_DEBUG("stream.on_connected side_ {}", (int)side_);
        if (on_connected_cb_) {
            on_connected_cb_();
        }
    }

    void disconnect() {
        LOG_DEBUG("stream::disconnect");
        this->next_.disconnect(chain_);
    }

    template<typename Chain> 
    void on_disconnected(Chain& chain) { 
        LOG_DEBUG("stream.on_disconnected side_ {}", (int)side_);
        if (on_disconnected_cb_) {
            on_disconnected_cb_();
        }
    }

    size_t write(const char* buf, size_t s) { 
        LOG_DEBUG("stream::write side: {} msg: {}", (int)side_, std::string(buf, s));
        return this->next_.write(chain_, buf, s);
    }

    template<typename Chain> 
    void on_received(Chain& chain, const char* buf, size_t s) { 
        LOG_DEBUG("stream.on_received side: {} msg: {}", (int)side_, std::string(buf, s));
        if (on_received_cb_)
            on_received_cb_(buf, s);
    }



    auto last() {
        std::get<base_type::it>(chain_) = this;
        return this->next_.last(chain_);
    } 


    Next& next() { return this->next_;}

    std::function<void()> on_connected_cb_;
    std::function<void()> on_disconnected_cb_;
    std::function<void(const char*, size_t)> on_received_cb_;

private:
    side_t side_;
    base_type::chain_type chain_;
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
        wrapper(T& fe, Chain& chain):fe_(fe),chain_(chain){}
        T& fe_;
        Chain& chain_;

        template <typename Address>
        void connect(const Address& adr) { 
            fe_.connect(chain_, adr);
        }

        void on_connected() { 
            fe_.on_connected(chain_);
        }

        void disconnect_() { 
            fe_.connect(chain_);
        }

        void on_disconnected() { 
            fe_.on_disconnected(chain_);
        }

        void on_received(const char* msg, size_t size) { 
            fe_.on_received(chain_, msg, size);
        }
        async_socket_base& socket() { return fe_.socket_;}

        void socket(async_socket_base&& s) {
            fe_.socket(chain_, std::move(s));
        } 

        //std::function<void()>& on_connect_cb_() { return fe_.on_connect_cb_; } 
        //std::function<void()>& on_disconnect_cb_() { return fe_.on_disconnect_cb_; } 
        //std::function<void(const char*, size_t)>& on_write_cb() { return fe_.on_write_cb_; } 

    };

    template<typename Chain, typename Context>
    socket_stream(Chain& chain, Context& c)
    :socket_(AF_INET, SOCK_STREAM, IPPROTO_TCP, c.io()), side_(c.side())
    {
    }

    template <typename Chain>
    void callback_init(Chain& chain) {
        socket_.callbacks().on_connected = [&](async::async_socket_base& s) {
            //LOG_DEBUG("socket_stream connected fd:" + std::to_string(s.fd()));
            on_connected(chain);
        };
        socket_.callbacks().on_disconnected = [&](async::async_socket_base& s) {
            LOG_DEBUG("socket_stream disconnected fd: {} side: {}", s.fd(), (int)side_);
            
            on_disconnected(chain);
        };
        socket_.callbacks().on_received = [&](async::async_socket_base& s, const char* buf, size_t len){
            //LOG_DEBUG(std::format("socket_stream received fd: {} ", s.fd()));
            on_received(chain, buf, len);
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

    acpp::network::side_t side_;
    std::vector<char> pending_data_;

    template<typename Chain, typename Address > 
    void connect(Chain& chain, const Address& adr) { 
        LOG_DEBUG("socket_stream.connect side: {}", (int)side_);
        socket_.connect(to_sockaddr(adr));
    }

    template<typename Chain> 
    void on_connected(Chain& chain) { 
        LOG_DEBUG("socket_stream.on_connected side: {} ***", (int)side_);
        acpp::network::async::get_prev<Chain, it>(chain).on_connected(chain);
    }

    template<typename Chain> 
    void disconnect() { 
        LOG_DEBUG("socket_stream.disconnect side: {}", (int)side_);
        socket_.close();
    }

    template<typename Chain> 
    void on_disconnected(Chain& chain) { 
        LOG_DEBUG("socket_stream.on_disconnected side: {}", (int)side_);
        acpp::network::async::get_prev<Chain, it>(chain).on_disconnected(chain);
    }


    template<typename Chain> 
    auto last(Chain& chain) {
        std::get<it>(chain) = this;
        using wrapper_type = wrapper<socket_stream, Chain>;
        //TODO: NYAPA ALERT!!!! do this in constructor!!!!
        if (!callback_init_) {
            callback_init_ = true;
            callback_init(chain);
        } 
            callback_init(chain);
        return wrapper_type(*this, chain);
    }
    
    template<typename Chain> 
    size_t write(Chain& chain, const char* buf, size_t size) {
        LOG_DEBUG("socket_stream.write side: {} size: {}", (int)side_, size);
        auto n = socket_.write(buf, size);
        if (n < size) {
            //TODO: find better way
            pending_data_.insert(pending_data_.end(), buf, buf + size);
        }
        return size;
    }

    template<typename Chain> 
    void on_received(Chain& chain, const char* buf, size_t size) {
        LOG_DEBUG("socket_stream.on_received side: {} size: {}", (int)side_, size);
        acpp::network::async::get_prev<Chain, it>(chain).on_received(chain, buf, size); 
    }

    async_socket_base& socket() { return socket_;}

    template<typename Chain> 
    void socket(Chain& chain, async_socket_base&& s) {
        socket_ = std::move(s);
        //callback_init_ = false;
        callback_init(chain);
    }
private:    
    async_socket_base socket_;
    bool callback_init_ = false;

};



} //namespace async

} //namespace acpp::network     