#include <iostream>
#include <thread>
#include <format>

#include <gtest/gtest.h> // googletest header file  

#include <acpp-network/socket.h>
#include <acpp-network/socket.inl>

#include <acpp-network/address.h>
#include <acpp-network/stream.h>
#include <acpp-network/ssl/ssl.h>
#include <acpp-network/ssl/ssl.inl>

#include <acpp-network/detail/common.h>
#include "utils.h"

int port = 8080;

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

template <typename Stream>
void client_server_socket_stream_test() {
    using namespace acpp::network::async;
    using namespace acpp::network;
    using stream_t = Stream;
    
    io_context io; //
    ip_socketaddress adr = ip4_sockaddress("127.0.0.1", port++);
    ::acpp::network::ssl::ssl_stream_context c(io, acpp::network::side_t::client, "");

    stream_t client(c);
    //stream_t server(io, acpp::network::side_t::server);


    std::vector<std::unique_ptr<stream_t>> server_sessons;
    //stream_t session(io, acpp::network::side_t::server);

    size_t total = 0, total_sent = 0;
    async::async_socket_base server_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
        async::socket_callbacks {
            .on_accepted = [&](async::async_socket_base& server, async::async_socket_base&& accepted_socket) {
                ::acpp::network::ssl::ssl_stream_context c(io, acpp::network::side_t::server, "");
                server_sessons.emplace_back(std::make_unique<stream_t>(c));
                auto& sess = *server_sessons.back();
                
                //auto& sess = session;
                
                LOG_DEBUG("sess: {}", (void*)&sess);
                sess.last().socket(std::move(accepted_socket));

                sess.on_received_cb_ = [&](const char* buf, size_t len) {
                    //echo... 
                    sess.write(buf, len);
                };

                LOG_DEBUG("SERVER ACCEPTED");
            }
        }
    );

    std::string msg("hello");

    client.on_connected_cb_ = [&]() {
        LOG_DEBUG("client.on_connected_cb_ sending msg: '{}'", msg);
        client.write(msg.c_str(), msg.size());
    };
    std::string msg2;
    client.on_received_cb_ = [&](const char* buf, size_t size) {
        msg2 = std::string(buf, size);
        LOG_DEBUG("client.on_received_cb_ sending msg2: '{}'", msg2);
        io.stop();
    };

    server_socket.bind(to_sockaddr(adr));
    server_socket.listen(5);

    client.last().connect(adr);

    //client.disconnect();

    io.wait_for_input();
    EXPECT_EQ(msg, msg2);
    LOG_DEBUG("test end");
}


// ./build.sh && ./build/tests/acpp-network-tests --gtest_filter=StreamTests.socket_stream_test1
// build.bat && build\tests\RelWithDebInfo\acpp-network-tests.exe  --gtest_filter=StreamTests.socket_stream_test1
TEST(StreamTests, socket_stream_test1)
{
    //using ssl_stream_t   = ::acpp::network::ssl::stream;
    using socket_stream_t = ::acpp::network::async::socket_stream;
    using stream_t = ::acpp::network::async::stream<socket_stream_t>;

    client_server_socket_stream_test<stream_t>();
}

// ./build.sh && ./build/tests/acpp-network-tests --gtest_filter=StreamTests.socket_stream_test2
// build.bat && build\tests\RelWithDebInfo\acpp-network-tests.exe  --gtest_filter=StreamTests.socket_stream_test2
TEST(StreamTests, socket_stream_test2)
{
    //using ssl_stream_t   = ::acpp::network::ssl::stream;
    using socket_stream_t = ::acpp::network::async::socket_stream;
    using ssl_stream_t = ::acpp::network::ssl::stream<socket_stream_t>;
    using stream_t = ::acpp::network::async::stream<ssl_stream_t>;

    client_server_socket_stream_test<stream_t>();
}



 TEST(StreamTests, socket_stream_example_org_text)
 {
    using namespace acpp::network;
    //using tcp_socket = sync::stream_socket<ip_socketaddress>;

    async::io_context io; //

    using socket_stream_t = ::acpp::network::async::socket_stream;
    using ssl_stream_t = ::acpp::network::ssl::stream<socket_stream_t>;
    using stream_t = ::acpp::network::async::stream<ssl_stream_t>;
//    using stream_t = ::acpp::network::async::stream<socket_stream_t>;
    using socket_t = stream_t::last_type;
    std::string hostname = "badssl.com";

    sync::resolve_host<socket_t, socket_t::address_type>(
        hostname, //"www.elpais.com", 
        /*"80"*/ 
        "443", [&](const ip_socketaddress& addr, bool& success){

        LOG_DEBUG("Resolved: {}", to_string(addr));
        
        ::acpp::network::ssl::ssl_stream_context c(io, acpp::network::side_t::client, hostname);
        stream_t client(c);

        client.on_connected_cb_ = [&]() {

            std::string msg = std::format(
                "GET / HTTP/1.1\r\n" 
                "Host: {}\r\n" 
                "User-Agent: Mozilla/5.0\r\n" 
                "Accept: */*\r\n" 
                "Connection: close\r\n\r\n", hostname);

            LOG_DEBUG("client.on_connected_cb_ sending msg: '{}'", msg);
            client.write(msg.c_str(), msg.size());
        };

        std::string msg2;
        std::string received;
        client.on_received_cb_ = [&](const char* buf, size_t size) {
            received += std::string(buf, size);
            LOG_DEBUG("client.on_received_cb_ sending received: '{}'", received);

            if (!received.empty()) {
                EXPECT_NE(received.find("HTTP/1.1 200 OK"), std::string::npos);
            }

            io.stop();
        };

        client.last().connect(addr);
        io.wait_for_input();

        if (success) {
        }
    });

}




// ./build.sh && ./build/tests/acpp-network-tests --gtest_filter=StreamTests.socket_stream_server

void socket_stream_server(int port)
{
    using namespace acpp::network::async;
    using namespace acpp::network;
    using socket_stream_t = ::acpp::network::async::socket_stream;
    using ssl_stream_t = ::acpp::network::ssl::stream<socket_stream_t>;

    using stream_t = ::acpp::network::async::stream<ssl_stream_t>;
    
    io_context io; //
    ip_socketaddress adr = ip4_sockaddress("127.0.0.1", port);
    ::acpp::network::ssl::ssl_stream_context c(io, acpp::network::side_t::client, "");

    std::vector<std::unique_ptr<stream_t>> server_sessons;

    size_t total = 0, total_sent = 0;
    ::timer t;
    ::acpp::network::ssl::ssl_stream_context context(io, acpp::network::side_t::server, "");
    async::async_socket_base server_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, io, 
        async::socket_callbacks {
            .on_accepted = [&](async::async_socket_base& server, async::async_socket_base&& accepted_socket) {
                t.start("create");
                server_sessons.emplace_back(std::make_unique<stream_t>(context));
                auto& sess = *server_sessons.back();
                               
                sess.last().socket(std::move(accepted_socket));

                sess.on_received_cb_ = [&](const char* buf, size_t len) {
                    std::cout << "on_received_cb_ " << std::string(buf, len) << std::endl;
                    //echo... 
                    t.stop();
                    sess.write(buf, len);
                };
                sess.on_disconnected_cb_ = [&]() {
                    std::cout << "on_disconnected_cb_ " << std::endl;
                    io.stop();
                };
                t.stop();
                t.start("received");

                LOG_DEBUG("SERVER ACCEPTED");
            }
        }
    );


    server_socket.bind(to_sockaddr(adr));
    server_socket.listen(5);

    io.wait_for_input();
    LOG_DEBUG("test end");

}


// ./build.sh && ./build/tests/acpp-network-tests --gtest_filter=StreamTests.socket_stream_client

void socket_stream_client(int port)
{
    using namespace acpp::network::async;
    using namespace acpp::network;
    using socket_stream_t = ::acpp::network::async::socket_stream;
    using ssl_stream_t = ::acpp::network::ssl::stream<socket_stream_t>;
    using stream_t = ::acpp::network::async::stream<ssl_stream_t>;
    
    io_context io; //
    ip_socketaddress adr = ip4_sockaddress("127.0.0.1", port);

    size_t total = 0, total_sent = 0;

    //std::string msg("hello");
    std::string msg(random_string(1024*10));

    ::acpp::network::ssl::ssl_stream_context c(io, acpp::network::side_t::client, "");
    stream_t client(c);
    ::timer t;

    client.on_connected_cb_ = [&]() {
    
        t.stop();

        LOG_DEBUG("client.on_connected_cb_ sending msg: '{}'", msg);
        t.start("send msg");
        client.write(msg.c_str(), msg.size());
    };
    std::string msg2;
    client.on_received_cb_ = [&](const char* buf, size_t size) {
        t.stop();
        msg2 += std::string(buf, size);
        LOG_DEBUG("client.on_received_cb_ sending msg2: '{}'", msg2);
        if (msg.size() == msg2.size()) {
            client.disconnect();
        }
    };
    client.on_disconnected_cb_ = [&] () {
        io.stop();
    };


    t.start("connect");
    client.last().connect(adr);

    io.wait_for_input();
    EXPECT_EQ(msg, msg2);
    LOG_DEBUG("test end");

}

TEST(StreamTests, socket_stream_server_client)
{
    std::thread server([&](){
        socket_stream_server(port);
    });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::thread client([&](){
        socket_stream_client(port);
    });

    client.join();
    server.join();
}
