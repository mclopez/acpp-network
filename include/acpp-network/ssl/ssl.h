#pragma once


#include <string>
#include <exception>

#include <acpp-network/stream.h>



typedef struct ssl_ctx_st SSL_CTX;

typedef struct bio_st BIO;
typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;

typedef struct ssl_st SSL;

namespace acpp::network::ssl {

class exception  : public std::exception {
public:    
    exception(const std::string& msg):msg_(msg){}
    const char* what() const noexcept override { return msg_.c_str();};
private:
    std::string msg_;
};


// struct ssl_context_deleter {
//     void operator()(SSL_CTX* ctx) const {
//         if (ctx) {
//             ssl_ctx_free(ctx); // The cleanup function for SSL*
//         }
//     }
// };

// // Define a unique_ptr type alias for convenience
// using ssl_context_ptr = std::unique_ptr<SSL_CTX, ssl_context_deleter>;


// struct ssl_deleter {
//     void operator()(SSL* ssl) const {
//         if (ssl) {
//             SSL_free(ssl); // The cleanup function for SSL*
//         }
//     }
// };

// // Define a unique_ptr type alias for convenience
// using SSL_ptr = std::unique_ptr<SSL, SSLDeleter>;

// // void manageSslConnection() {
// //     // ... create SSL object ...
// //     SSL* raw_ssl = SSL_new(ssl_context);

// //     // Use unique_ptr to manage it
// //     SSL_ptr ssl_manager(raw_ssl); 

// //     // When ssl_manager goes out of scope here, SSL_free(raw_ssl) is called automatically.
// // }    



#define PARAM(name) std::string name##_; \
                    Name& name(const std::string& v) { name##_ = v; return *this;} \
                    const std::string& name() const {return name##_;}

class pkey {
public:
    pkey();
    pkey(void *);
    pkey(const pkey& x);
    pkey(pkey&& x);
    ~pkey();
    static pkey load_from_file(const std::string& file);
    void operator=(const pkey& x);
    EVP_PKEY* handle() const {return handle_;}
private:
    EVP_PKEY* handle_;
};

class x509 {
public:
    struct Name {
        PARAM(c);
        PARAM(st);
        PARAM(l);
        PARAM(o);
        PARAM(cn);
    };
    x509();
    x509(void *);
    x509(const X509& x);
    x509(x509&& x);
    //TODO: needede???
    x509(Name& n){
    }

    void operator=(const x509& x);

    ~x509();
    x509 make_copy();

    static std::pair<x509, pkey> make_copy(const x509& cert);
    
    X509* handle() const {return handle_;}

    std::string to_string();
    void save_to_file(const std::string& cert_file);
    //TODO: static???
    static x509 load_from_file(const std::string& cert_file);

    void sign(x509& ca_cert, pkey& pk);

    static std::pair<x509, pkey> create_cert(const x509::Name&);
    static std::pair<x509, pkey> create_self_signed_cert(const x509::Name&);
    static std::pair<x509, pkey> create_signed_cert(x509& cert_ca, pkey& pk_ca, const x509::Name& n);

    //operator bool() {return impl_; }
private:
    X509* handle_;
};


class context {
public:
    context(side_t s);
    ~context();
    SSL_CTX* handle() { return handle_;}
    side_t side() { return side_;}

    void set_cert(x509& cert);
    void set_pkey(pkey& pkey);

private:
    side_t side_;
    SSL_CTX* handle_;
};

class ssl_stream_context {
public:
    // ssl_stream_context(acpp::network::async::io_context& io, side_t side, const std::string& hostname)
    // :io_(io), side_(side), hostname_(hostname){}
    ssl_stream_context(acpp::network::async::io_context& io, side_t side, const std::string& hostname);
    side_t side() { return side_;}
    const std::string& hostname() { return hostname_;}
    acpp::network::async::io_context& io() { return io_;}
    std::shared_ptr<context> ctx() { return context_;}
private:
    std::string hostname_;
    side_t side_;
    acpp::network::async::io_context& io_;
    std::shared_ptr<context> context_;
};


template<typename Next = acpp::network::async::null_layer>
class stream  {
public:

    enum {it = Next::it+1,};  
    using next_type = Next;
    using chain_type = async::append_to_tuple_t<typename next_type::chain_type, stream* >;
    using last_type = next_type::last_type;
    enum class status  { closed, connecting, connected, peer_closing, closing};

    [[deprecated]]
    stream(side_t side);
    //stream(acpp::network::async::io_context& io, side_t side);
    template<typename Context> 
    stream(Context& c);

    virtual ~stream();

    template<typename Chain>
    void connect();

    template<typename Chain>
    void on_received(const char* buf, size_t len);

    template<typename Chain>
    size_t write(const char* buf, size_t len);

    template <typename Chain>
    void on_connected();

    template <typename Chain>
    void on_disconnected();

    template <typename Chain>
    void disconnect();
 
    void set_cert(x509& x509);
    void set_pkey(pkey& pk);

    x509 cert();
    x509 peer_cert();
    void set_cert();
    void set_hostname(const std::string& hostname) { hostname_ = hostname;}

    void* prev_;

    template <typename Chain>
    auto last() {
        return next_.template last<Chain>();
    }

    Next& next() {return next_;}

private:
    template<typename Chain>
    void do_connect(const char* buf, size_t len);

    template<typename Chain>
    void do_shutdown(const char* buf, size_t len);

    side_t side_;
    //TODO: revise to make it const context instead
    std::shared_ptr<context> ctx_;
    status status_;

    BIO *int_bio;
    BIO *ext_bio;
    SSL *ssl_;
    std::string hostname_;
    Next next_;
    Next* next2_;

};

//std::string to_string()
    
} //namespace acpp::network::ssl