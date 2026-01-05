#pragma once

//#include <cstdio>

#include <acpp-network/socket_base.h>
#include <detail/common.h>

namespace acpp::network {

enum class side_t {client, server};


namespace async {

template<typename Tuple, typename T>
struct AppendToTuple;

template<typename... Ts, typename T>
struct AppendToTuple<std::tuple<Ts...>, T> {
    using type = std::tuple<Ts..., T>;
};

// Helper alias
template<typename Tuple, typename T>
using AppendToTuple_t = typename AppendToTuple<Tuple, T>::type;

class null_layer;

template<typename Chain, int Int>
auto get_prev(void* p) {
    //log_debug("get_prev "  + std::to_string(Int) + " " + std::to_string(std::tuple_size<Chain>()));
    if constexpr (Int + 1 < std::tuple_size<Chain>()) {
        //using Prev = decltype(std::get<Int + 1>(Chain));
        using prev_type = std::tuple_element_t<Int + 1, Chain>;
        return static_cast<prev_type>(p);
        //return std::get<it + 1>(chain);
    } else {
        log_debug("NULLLLLL");
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
        log_debug("null_layer.connect");
    }


    template<typename Chain> 
    size_t write(const char* buf, size_t s) { 
        log_debug("null_layer.write");
        return 0;
    }

    template<typename Chain> 
    void on_received(const char* buf, size_t s) { 
        log_debug("null_layer.on_received");
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


template<typename Next>
class stream {
public:
    enum {it = Next::it+1,};  
    using next_type = Next;
    using chain_type = AppendToTuple_t<typename Next::chain_type, stream* >;
    using last_type = next_type::last_type;

    stream(side_t side):side_(side), next_(side) {
        next_.prev_ = this;
    }

    void connect() {
        log_debug("stream::connect");
        next_.template connect<chain_type>();
    }

    template<typename Chain> 
    void on_connected() { 
        log_debug(std::format("stream.on_connected side_ {}", (int)side_));
        if (on_connected_cb_) {
            on_connected_cb_();
        }
    }

    void disconnect() {
        log_debug("stream::disconnect");
        next_.template disconnect<chain_type>();
    }

    template<typename Chain> 
    void on_disconnected() { 
        log_debug(std::format("stream.on_disconnected side_ {}", (int)side_));
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


    std::function<void()> on_connected_cb_;
    std::function<void()> on_disconnected_cb_;
    std::function<void(const char*, size_t)> on_received_cb_;


    // template <typename Chain>
    // auto last() {
    //     return next_.template last<Chain>();
    // } 

    auto last() {
        return next_.template last<chain_type >();
    } 


    void* prev_;
    Next& next() { return next_;}
private:
    Next next_;
    side_t side_;
};

template<typename Next= null_layer>
class layer {
public:
    enum {it = Next::it+1,};  
    using next_type = Next;
    using chain_type = AppendToTuple_t<typename next_type::chain_type, layer* >;
    using last_type = next_type::last_type;

    layer(side_t side): side_(side), next_(side) {
        next_.prev_ = this;
    }

    template<typename Chain> 
    void connect() { 
        log_debug("null_layer.connect");
    }

    template<typename Chain> 
    size_t write(const char* buf, size_t s) { 
        log_debug("layer.write");
        return next_.template write<Chain>(buf, s);
    }

    template<typename Chain> 
    void on_received(const char* buf, size_t s) { 
        log_debug("layer.on_received");
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

/*
class socket {
public:
    enum {it = 0,};  
    
    void set_socket(async_socket_base&& c);

    size_t write(const char* buf, size_t s) { 
        return socket_.write(buf, s);
    }
    void on_received(const char* buf, size_t s) { 
        //return socket_.write(buf, s);
    }


private:
    async_socket_base socket_;    
    void* prev_;
};
*/
}

} //namespace acpp::network     