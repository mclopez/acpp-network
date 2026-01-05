#include <iostream>
#include <thread>
#include <random>
#include <format>

#include <gtest/gtest.h> // googletest header file  

//#include <acpp-network/log.h>
#include <acpp-network/stream.h>
#include <acpp-network/ssl/ssl.h>
#include <acpp-network/ssl/ssl.inl>

#include <detail/common.h>

TEST(StreamTests, stream_test1)
{
    using namespace acpp::network::async;
    using stream_t = stream<layer<>>;
    stream_t l(acpp::network::side_t::client);

    std::string msg("hello");
    l.write(msg.data(), msg.size());
    std::string msg2("world");
    auto last = l.last();
    last.on_received<stream_t::chain_type>(msg2.data(), msg2.size());
    //l.on_received(msg2.data(), msg2.size());

}




class fake_endpoint {
public:
    enum {it = 0,};  
    using chain_type = std::tuple<fake_endpoint*>;
    using last_type = fake_endpoint;

    template <typename T, typename Chain>
    struct wrapper {
    public:
        wrapper(T& fe):fe_(fe){}
        T& fe_;

        void connect() { 
            fe_.template connect<Chain>();
        }

        void on_connected() { 
            fe_.template on_connected<Chain>();
        }

        void disconnect() { 
            fe_.template connect<Chain>();
        }

        void on_disconnected() { 
            fe_.template on_disconnected<Chain>();
        }

        void on_received(const char* msg, size_t size) { 
            fe_.template on_received<Chain>(msg, size);
        }

        std::function<void()>& on_connect_cb_() { return fe_.on_connect_cb_; } 
        std::function<void()>& on_disconnect_cb_() { return fe_.on_disconnect_cb_; } 
        std::function<void(const char*, size_t)>& on_write_cb() { return fe_.on_write_cb_; } 

    };


    fake_endpoint(acpp::network::side_t side):side_(side){}    

    void* prev_; 

    acpp::network::side_t side_;
    //fake_endpoint *other_ = nullptr;

    template<typename Chain> 
    void connect() { 
        LOG_DEBUG("fake_layer.connect side: {}", (int)side_);
        if (on_connect_cb_) {
            on_connect_cb_(); 
            on_connected<Chain>();
        }
    }

    template<typename Chain> 
    void on_connected() { 
        LOG_DEBUG("fake_layer.on_connected side: {} ***", (int)side_);
        auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
        if (prior) {
            LOG_DEBUG("fake_layer.on_connected side: {} prior: {}", (int)side_, (void*) prior);
            prior->template on_connected<Chain>();
        }
    }

    template<typename Chain> 
    void disconnect() { 
        LOG_DEBUG("fake_layer.disconnect side: {}", (int)side_);
        if (on_disconnect_cb_) {
            on_disconnect_cb_(); 
            on_disconnected<Chain>();
        }
    }

    template<typename Chain> 
    void on_disconnected() { 
        LOG_DEBUG("fake_layer.on_disconnected side: {}", (int)side_);
        auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
        if (prior)
            prior->template on_disconnected<Chain>();
    
    }



    template<typename Chain> 
    auto last() {
        using wrapper_type = wrapper<fake_endpoint, Chain>; 
        return wrapper_type(*this);
    }

    template<typename Chain> 
    size_t write(const char* buf, size_t size) {
        // if (other_) {
        //     other_->template on_received<Chain>(buf, size);
        // }
        LOG_DEBUG("fake_layer.write side: {} size: {}", (int)side_, size);
        if (on_write_cb_) {
            on_write_cb_(buf, size);
        }
        return size;
    }

    void on_received(const char* buf, size_t size) {

    }

    template<typename Chain> 
    void on_received(const char* buf, size_t size) {
       auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
        if (prior)
            prior->template on_received<Chain>(buf, size);
 
    }



    std::function<void()> on_connect_cb_;
    std::function<void()> on_disconnect_cb_;
    std::function<void(const char*, size_t)> on_write_cb_;

};


// template <typename T>
// class ShowType; // No definition!

template <typename Stream>
void client_server_test() {
    using namespace acpp::network::async;
    using namespace acpp::network;
    using stream_t = Stream;
    
    stream_t client(acpp::network::side_t::client);
    stream_t server(acpp::network::side_t::server);

    client.last().on_connect_cb_() = [&]() {
        server.last().on_connected();
    };

    client.last().on_disconnect_cb_() = [&]() {
        server.last().on_disconnected();
    };

    client.last().on_write_cb() = [&](const char* buf, size_t size) {
        server.last().on_received(buf, size);
    };
    
    std::string msg("hello");
    client.on_connected_cb_ = [&]() {
        LOG_DEBUG("client.on_connected_cb_");
        client.write(msg.c_str(), msg.size());
    };
    std::string msg2;
    client.on_received_cb_ = [&](const char* buf, size_t size) {
        msg2 = std::string(buf, size);
    };

    server.on_received_cb_ = [&](const char* buf, size_t len) {
        //echo... 
        server.write(buf, len);
    };
/*
    auto c = ssl::X509::create_self_signed_cert(ssl::X509::Name().cn("xxx").l("l").o("o").st("st"));
    LOG_DEBUG("SslStreamEchoServerSession: cert: {}", c.first.to_string());
    context_.set_cert(c.first);
    context_.set_pkey(c.second);
    ssl_ = std::make_unique<ssl::Stream>(context_);

*/ 
    server.last().on_write_cb() = [&](const char* buf, size_t size) {
        client.last().on_received(buf, size);
    };

    client.connect();
    
    auto last = client.last();
    EXPECT_EQ(msg, msg2);

    client.disconnect();

}

// ./build.sh && ./build/tests/acpp-network-tests --gtest_filter=StreamTests.ssl_stream_test1
TEST(StreamTests, ssl_stream_test1)
{
    using namespace acpp::network::async;
    client_server_test<stream<fake_endpoint>>();
}

// ./build.sh && ./build/tests/acpp-network-tests --gtest_filter=StreamTests.ssl_stream_test1
// build.bat && build\tests\RelWithDebInfo\acpp-network-tests.exe  --gtest_filter=StreamTests.ssl_stream_test2
TEST(StreamTests, ssl_stream_test2)
{
    using namespace acpp::network::async;
    //using stream_t = stream<fake_endpoint>;
    using stream_t = stream<::acpp::network::ssl::stream<fake_endpoint>>;
    client_server_test<stream_t>();
}


// ./build.sh && ./build/tests/acpp-network-tests --gtest_filter=StreamTests.ssl_stream_test2
TEST(StreamTests, ssl_stream_test3)
{

    using namespace acpp::network::async;
    using namespace acpp::network;
    using stream_t = stream<::acpp::network::ssl::stream<fake_endpoint>>;
    
    stream_t client(acpp::network::side_t::client);
    stream_t server(acpp::network::side_t::server);

    // client.last().other_connected = [&](){
    //     server.on_connected();
    // };

    client.connect();
    std::string msg2("world");
    
    auto last = client.last();

    //last.on_received(msg2.data(), msg2.size());
    //last.on_connected();

    //last.on_received<stream_t::chain_type>(msg2.data(), msg2.size());
    //l.on_received(msg2.data(), msg2.size());

}
