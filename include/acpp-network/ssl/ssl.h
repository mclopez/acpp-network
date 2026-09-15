#pragma once


#include <string>
#include <exception>

#include <acpp-network/stream.h>

//#define ACPP_BIO

typedef struct ssl_ctx_st SSL_CTX;

typedef struct bio_st BIO;
typedef struct bio_method_st BIO_METHOD;

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

enum class error {
    invalid_state = 0,
    unknown = 3
};

class error_category : public std::error_category {
public:
    const char* name() const noexcept override {
        return "acpp::network::ssl::error_category";
    }

    // Translates the numeric value into an explanatory string
    std::string message(int ev) const override {
        switch (static_cast<error>(ev)) {
            case error::invalid_state: return "invalid_state";
            case error::unknown: return "unknown";
            default:                       
                return "Unknown acpp network error";
        }
    }
};

const std::error_category& error_category() noexcept;

std::error_code make_error_code(error e);

#define PARAM(name) std::string name##_; \
                    Name& name(const std::string& v) { name##_ = v; return *this;} \
                    const std::string& name() const {return name##_;}

class pkey {
public:
    pkey();
    pkey(EVP_PKEY *);
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
    x509(X509*);
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


#ifdef ACPP_BIO

template<typename Stream>
class acpp_bio {

public:    
    template<typename Chain>
    acpp_bio(Chain& chain, Stream& st);

    template<typename Chain>
    static int write(BIO *b, const char *in, int inl);

    // Called when OpenSSL needs ENCRYPTED data from the wire to decrypt it
    static int read(BIO *b, char *out, int outl);

    // Handles state changes and configuration inquiries from OpenSSL
    static long ctrl(BIO *b, int cmd, long num, void *ptr);


    // Lifecycle allocation
    static int create(BIO *b);

    // Lifecycle cleanup
    static int destroy(BIO *b);



    char* in_buf_;
    size_t in_size_;
    BIO* bio_;

private:
    template<typename Chain>
    struct resources{
        resources();
        ~resources();
        BIO_METHOD *create_method(int my_unique_type_id);
        const int type_id;
        BIO_METHOD* bio_method;

    };
    Stream& stream_;
};

#endif 




template<typename Next = acpp::network::async::null_layer>
class stream: public async::layer_base<stream<Next>, Next >  {
public:

    using base_type = async::layer_base<stream<Next>, Next >;

    enum class status  { closed, connecting, connected, peer_closing, closing};

    //[[deprecated]]
    template<typename Chain> 
    stream(Chain& chain, side_t side);

    template<typename Chain, typename Context> 
    stream(Chain& chain, Context& c);

    virtual ~stream();

    template<typename Chain>
    void connect(Chain& chain);

    template<typename Chain>
    void on_received(Chain& chain, const char* buf, size_t len);

    template<typename Chain>
    std::error_code write(Chain& chain, const char* buf, size_t len);

    template <typename Chain>
    void on_connected(Chain& chain);

    template <typename Chain>
    void on_disconnected(Chain& chain);

    template <typename Chain>
    void disconnect(Chain& chain);
 
    void set_cert(x509& x509);
    void set_pkey(pkey& pk);

    x509 cert();
    x509 peer_cert();
    void set_cert();
    void set_hostname(const std::string& hostname) { hostname_ = hostname;}


    template <typename Chain>
    auto last(Chain& chain) {
        std::get<base_type::it>(chain) = this;
        return this->next_.last(chain);
    }

    Next& next() {return this->next_;}
    SSL* handle() {return ssl_;}

private:
    template<typename Chain>
    void do_connect(Chain& chain, const char* buf, size_t len);

    template<typename Chain>
    void do_shutdown(Chain& chain, const char* buf, size_t len);

    template <typename Chain>
    std::error_code ssl_to_next(Chain& chain);

    template <typename Chain>
    int next_to_ssl(Chain& chain, const char* buf, size_t len);

    side_t side_;
    //TODO: revise to make it const context instead
    std::shared_ptr<context> ctx_;
    status status_;

    SSL *ssl_;
    std::string hostname_;
    //Next next_;
    //Next* next2_;
#ifdef ACPP_BIO
    acpp_bio<stream> custom_bio_;
#else
    BIO *int_bio;
    BIO *ext_bio;
#endif
};



} //namespace acpp::network::ssl

