#pragma once


#include <openssl/ssl.h>
#include <openssl/bio.h>

#include <algorithm>

#include <array>

#include <acpp-network/ssl/ssl.h>
#include <acpp-network/utils.h>

namespace acpp::network::ssl {

/*
./build.sh && ./build/Release/tests/acpp-network-tests --gtest_filter=-StreamTests.ssl_stream_test1:StreamTests.ssl_stream_test2:StreamTests.socket_stream_test1:StreamTests.socket_stream_test2

*/ 

#ifdef ACPP_BIO

//custom bio

// Called when OpenSSL wants to transmit ENCRYPTED data to the wire
template<typename Stream>
template<typename Chain>
int acpp_bio<Stream>::write(BIO *b, const char *in, int inl) {
    LOG_DEBUG("acpp_bio<Stream>::write len: {}", inl);
    // Clear OpenSSL retry flags before executing your I/O
    BIO_clear_retry_flags(b);
    
    acpp_bio<Stream>* ctx = (acpp_bio<Stream>*)BIO_get_data(b);
    
    // Direct zero-copy dispatch straight to your custom transport layer!
    int bytes_sent = ctx->stream_.next().template write<Chain>(in, inl); //my_network_layer_send(ctx->my_network_context, in, inl);
    
    if (bytes_sent <= 0) {
        // If your network layer is non-blocking and busy, signal OpenSSL to pause
        // if (my_network_layer_would_block(ctx->my_network_context)) {
        //     BIO_set_retry_write(b);
        // }
    }
    //return bytes_sent;
    return inl;
}

// Called when OpenSSL needs ENCRYPTED data from the wire to decrypt it
template<typename Stream>
int acpp_bio<Stream>::read(BIO *b, char *out, int outl) {
    LOG_DEBUG("acpp_bio<Stream>::read ini out: {} outl: {} ", (void*)out, outl);
    //BIO_clear_retry_flags(b);
    
    acpp_bio<Stream>*ctx = (acpp_bio<Stream>*)BIO_get_data(b);
    LOG_DEBUG("acpp_bio<Stream>::read ini ctx: {} in_buf_: {}, in_size_: {}", (void*)ctx, (void*)ctx->in_buf_, ctx->in_size_);

    // Pull wire bytes directly into OpenSSL's internal parsing buffer
    //int bytes_received = my_network_layer_recv(ctx->my_network_context, out, outl);
    int len = std::min(outl, (int)ctx->in_size_);
    memcpy(out, ctx->in_buf_, len);
        LOG_DEBUG("acpp_bio<Stream>::read memcpy");

    ctx->in_size_ -= len;
    ctx->in_buf_ = ctx->in_buf_ + len;
    if (len <= 0) {
        // if (my_network_layer_would_block(ctx->my_network_context)) {
        //     BIO_set_retry_read(b);
        // }
    }
    // Clear retry flags because we are returning successful data
    BIO_clear_retry_flags(b);
    LOG_DEBUG("acpp_bio<Stream>::read len: {}", len);
    return len; //bytes_received;
}

// Handles state changes and configuration inquiries from OpenSSL
template<typename Stream>
long acpp_bio<Stream>::ctrl(BIO *b, int cmd, long num, void *ptr) {
    LOG_DEBUG("acpp_bio<Stream>::ctrl");
    switch (cmd) {
        case BIO_CTRL_FLUSH:
            LOG_DEBUG("acpp_bio<Stream>::ctrl BIO_CTRL_FLUSH");
            // Flush your underlying custom network TX queues if necessary
            return 1;
        case BIO_CTRL_PUSH:
            LOG_DEBUG("acpp_bio<Stream>::ctrl BIO_CTRL_PUSH");
            return 0; // Not a filtering layer
        case BIO_CTRL_POP:
            LOG_DEBUG("acpp_bio<Stream>::ctrl BIO_CTRL_POP");
            return 0; // Not a filtering layer
        case BIO_CTRL_EOF:
            LOG_DEBUG("acpp_bio<Stream>::ctrl BIO_CTRL_EOF");
            //return 1; // no more data
            return 0; // more data
        default:
            LOG_DEBUG("acpp_bio<Stream>::ctrl default cmd: {}", cmd);
            return 0; // Unhandled commands must return 0
    }
}

// Lifecycle allocation
template<typename Stream>
int acpp_bio<Stream>::create(BIO *b) {
    LOG_ERROR("acpp_bio<Stream>::create");
    BIO_set_init(b, 1);
    BIO_set_data(b, NULL);
    return 1;
}

// Lifecycle cleanup
template<typename Stream>
int acpp_bio<Stream>::destroy(BIO *b) {
    LOG_ERROR("acpp_bio<Stream>::destroy");
    acpp_bio<Stream>* ctx = (acpp_bio<Stream>*)BIO_get_data(b);
    if (ctx) {
        //free(ctx);
        BIO_set_data(b, NULL);
    }
    return 1;
}


template<typename Stream>
template<typename Chain>
BIO_METHOD* acpp_bio<Stream>::resources<Chain>::create_method(int my_unique_type_id) {
    LOG_ERROR("acpp_bio<Stream>::resources::create_method");

    // Choose an unused type identifier index above BIO_TYPE_START (e.g., 0x0200 | 50)
    BIO_METHOD *method = BIO_meth_new(my_unique_type_id | BIO_TYPE_SOURCE_SINK, "MyNetworkBIO");
    
    if (method) {
        BIO_meth_set_write(method, acpp_bio<Stream>::write<Chain>);
        BIO_meth_set_read(method, acpp_bio<Stream>::read);
        BIO_meth_set_ctrl(method, acpp_bio<Stream>::ctrl);
        BIO_meth_set_create(method, acpp_bio<Stream>::create);
        BIO_meth_set_destroy(method, acpp_bio<Stream>::destroy);
    }
        LOG_ERROR("acpp_bio<Stream>::resources::create_method (2)");

    return method;
}


// template<typename Stream>
// BIO_METHOD* acpp_bio<Stream>::bio_method = acpp_bio<Stream>::create_method(type_id);


template<typename Stream>
template<typename Chain>
acpp_bio<Stream>::resources<Chain>::resources()
:type_id(50 | BIO_TYPE_SOURCE_SINK)
{
    bio_method = create_method(type_id);
}

template<typename Stream>
template<typename Chain>
acpp_bio<Stream>::resources<Chain>::~resources(){
    BIO_meth_free(bio_method);
}


template<typename Stream>
template<typename Chain>
acpp_bio<Stream>::acpp_bio(Chain& chain, Stream& st):stream_(st), in_size_(0), in_buf_(nullptr){
    LOG_ERROR("acpp_bio<Stream>::acpp_bio");
    static resources<Chain> resources;
    bio_ = BIO_new(resources.bio_method);
    //marcos
    BIO_set_data(bio_, this);
    LOG_ERROR("acpp_bio<Stream>::acpp_bio (2)");
}


//end custom bio

#endif // ACPP_BIO



using buffer = std::array<char, 1024*20>; //ojo!!


template<typename Next>
template<typename Chain> 
stream<Next>::stream(Chain& chain, side_t side)
:side_(side), /*next_(chain, side),*/ ctx_(std::make_shared<context>(side)), status_(status::closed)
#ifdef ACPP_BIO
, custom_bio_(chain, *this)
#endif
{
    LOG_DEBUG("ssl::stream<Next>::stream side: {} status: {}", (int)side_, (int)status_); 
    //next_.prev_ = this;

    if (side == side_t::server) {
        auto c = x509::create_self_signed_cert(x509::Name().cn("xxx").l("l").o("o").st("st"));
        LOG_DEBUG("ssl::stream<Next>::stream: cert: {}", c.first.to_string());
        ctx_->set_cert(c.first);
        ctx_->set_pkey(c.second);
        //ssl_ = std::make_unique<ssl::Stream>(context_);

    }


    LOG_DEBUG("ssl::stream<Next>::stream side: {} status: {} (2)", (int)side_, (int)status_); 

    ssl_ = SSL_new(ctx_->handle());

#ifdef ACPP_BIO
    SSL_set_bio(ssl_, custom_bio_.bio_, custom_bio_.bio_);
    LOG_ERROR("ACPP_BIO on");
#else
    BIO_new_bio_pair(&int_bio, 0, &ext_bio, 0 );
    SSL_set_bio(ssl_, int_bio, int_bio);
    int size = 0;
    BIO_get_write_buf_size(ext_bio, size);
    LOG_DEBUG("ACPP_BIO off");

#endif
    LOG_DEBUG("ssl::stream<Next>::stream side: {} status: {} (3)", (int)side_, (int)status_); 



}

 

template<typename Next>
template<typename Chain, typename Context> 
stream<Next>::stream(Chain& chain, Context& ctx)
:base_type(chain, ctx), side_(ctx.side()), ctx_(ctx.ctx()), status_(status::closed)
#ifdef ACPP_BIO
, custom_bio_(chain, *this)
#endif

{
    LOG_DEBUG("ssl::stream<Next>::stream side: {} status: {}", (int)side_, (int)status_); 
    //next_.prev_ = this;

    ssl_ = SSL_new(ctx_->handle());
#ifdef ACPP_BIO
    SSL_set_bio(ssl_, custom_bio_.bio_, custom_bio_.bio_);
    LOG_ERROR("ACPP_BIO on");
#else
    BIO_new_bio_pair(&int_bio, 0, &ext_bio, 0 );
    SSL_set_bio(ssl_, int_bio, int_bio);
    int size = 0;
    BIO_get_write_buf_size(ext_bio, size);
    LOG_INFO("BIO_get_write_buf_size(ext_bio) {}", size); 
    LOG_ERROR("ACPP_BIO off");

#endif



}


// template<typename Next>
// stream<Next>::stream(acpp::network::async::io_context& io, side_t side)
// :stream(side)
// {}


template<typename Next>
stream<Next>::~stream() {
    if (ssl_)
        SSL_free(ssl_);
    ssl_ = nullptr;    
    //TODO: rest of ssl componets???    
}


template<typename Next>
void stream<Next>::set_cert(x509& cert) {
    SSL_use_certificate(ssl_, cert.handle());
}

template<typename Next>
void stream<Next>::set_pkey(pkey& pk) {
    SSL_use_PrivateKey(ssl_, pk.handle());
}


template<typename Next>
x509 stream<Next>::cert() {
    if (!ssl_)
        return x509();
    return x509(SSL_get_certificate(ssl_));
}

template<typename Next>
x509 stream<Next>::peer_cert() {
    x509 result;
    if (!ssl_)
        return result;
    result = x509(SSL_get_peer_certificate(ssl_));
    LOG_DEBUG("ssl::stream::peer_cert cert: {}", result.to_string());
    return result;
}

template<typename Next>
template<typename Chain>
void stream<Next>::connect(Chain& chain) {  
    this->next_.connect(chain);
}


//on prior connected
template<typename Next>
template <typename Chain>
void stream<Next>::on_connected(Chain& chain) { 
    LOG_DEBUG("ssl::stream::on_connected status:{} ctx_.type():{}", (int)status_, (int)side_);
    if (side_ == side_t::client) {
        do_connect(chain, nullptr, 0);
    }
}

template<typename Next>
template <typename Chain>
void stream<Next>::do_connect(Chain& chain, const char* buf, size_t len) {
    LOG_DEBUG("ssl::stream::do_connect side: {} status: {} len: {}", (int)side_, (int)status_, len);
    //acpp::network::timer t("ssl do_connect");
    int e;
    if (len) {
        // //acpp::network::timer t("ssl do_connect BIO_write");
        // e = BIO_write(ext_bio, buf, len);
        // if ( e > 0 && e < len) {
        //     LOG_ERROR("ssl::stream::do_connect NOT ALL DATA WRITTEN side: {} BIO_write e:{} len: {}", (int)side_, e, len);
        // } else {
        //     LOG_DEBUG("ssl::stream::do_connect  side: {} BIO_write e:{} len: {}", (int)side_, e, len); 
        // }
        next_to_ssl(chain, buf, len);

    }
    if (status_ == status::closed || status_ == status::connecting) {
        if (side_ == side_t::server) {
            LOG_DEBUG("ssl::stream::do_connect  side: {} SSL_accept", (int)side_); 
            //acpp::network::timer t("ssl do_connect SSL_accept");

//printf("rbio pending before ssl_accept =%ld\n", BIO_ctrl_pending(int_bio));
//printf("wbio pending before ssl_accept =%ld\n", BIO_ctrl_pending(ext_bio));


            //msg_callback(0,0,0, nullptr, 0, nullptr, nullptr);
//BIO_clear_retry_flags(custom_bio_.bio_);
            e =  SSL_accept(ssl_);
//BIO_clear_retry_flags(custom_bio_.bio_);

//printf("rbio pending after ssl_accept =%ld\n", BIO_ctrl_pending(int_bio));
//printf("wbio pending after ssl_accept =%ld\n", BIO_ctrl_pending(ext_bio));


//            printf("Cipher: %s\n", SSL_get_cipher_name(ssl_));
//            printf("Version: %s\n", SSL_get_version(ssl_));
            //e = SSL_do_handshake(ssl_);
        }
        else {
            if (!hostname_.empty()) {
                LOG_DEBUG("ssl::stream::do_connect set SNI: {}", hostname_);
                SSL_set_tlsext_host_name(ssl_, hostname_.c_str());
            }
            LOG_DEBUG("ssl::stream::do_connect  side: {} SSL_connect *****", (int)side_); 
            //acpp::network::timer t("ssl do_connect SSL_connect");
            
            e = SSL_connect(ssl_);   
            LOG_DEBUG("ssl::stream::do_connect  side: {} SSL_connect e: {}", (int)side_, e); 

            //e = SSL_do_handshake(ssl_); 
            //TODO: SSL_get_verify_result   SSL_CTX_set_verify  
            // SSL_get0_peer_certificate/SSL_get1_peer_certificate
        }
        status_  = status::connecting;
    } else {
        LOG_ERROR("connecting in invalid state status_: {}", (int)status_);
        throw exception(std::format("connecting in invalid state status_: {}", (int)status_));  
    }
    // check error
    if (e < 0)  {
        if (SSL_get_error(ssl_, e) == SSL_ERROR_WANT_READ)  {
            LOG_DEBUG("ssl::stream::do_connect  side: {}  SSL_ERROR_WANT_READ", (int)side_);
        } else if (SSL_get_error(ssl_, e) == SSL_ERROR_WANT_WRITE)  {
            LOG_DEBUG("ssl::stream::do_connect  side: {}  SSL_ERROR_WANT_WRITE", (int)side_);
        } else  {
            int err = SSL_get_error(ssl_, e);
            LOG_DEBUG("ssl::stream::do_connect  side: {}  ERROR.... err: {}", (int)side_, err);
        }
    } else if (e == 0){
        int err = SSL_get_error(ssl_, e);
        LOG_ERROR("ssl::stream::do_connect  ERROR: {}", err);
        status_ = status::closed; // do we need error status?
 
    } else if (e == 1){
        LOG_DEBUG("ssl::stream::do_connect OK ");
        status_ = status::connected;
    }
    

    if(status_ == status::connected) {
        this->prev(chain).on_connected(chain);
    }

    ssl_to_next(chain);


    {
        buffer b;
        int n;
        //acpp::network::timer t("ssl do_connect BIO_read (2)");
        while(n= SSL_read(ssl_, b.data(), b.size()), n > 0) {
            this->prev(chain).on_received(chain, b.data(), n);
        }
    }
}
// write_input -> to app
// write_output -> to socket

//on prior connected
template<typename Next>
template <typename Chain>
void stream<Next>::on_disconnected(Chain& chain) { 
    LOG_DEBUG("ssl::stream::on_disconnected status:{} ctx_.type():{}", (int)status_, (int)side_);
    //already disconnected
    // auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
    // if (prior)
    //     prior->template on_disconnected<Chain>();
    this->prev(chain).on_disconnected(chain);
}


template<typename Next>
template<typename Chain>
void stream<Next>::do_shutdown(Chain& chain, const char* buf, size_t len) {
    LOG_DEBUG("ssl::stream::do_shutdown status_: {} len: {}", (int)status_, len); 
    //auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
    auto& prior = this->prev(chain);

    int e;
    if (len) {
        // e = BIO_write(ext_bio, buf, len);
        // LOG_DEBUG("ssl::stream::do_shutdown BIO_write e: {} len: {}", e, len); 
        next_to_ssl(chain, buf, len);

    }
    if (status_ == status::closing) {
        LOG_DEBUG("ssl::stream::do_shutdown  going shutdown");
        e = SSL_shutdown(ssl_);
        int ssls = SSL_get_shutdown(ssl_);
        LOG_DEBUG("ssl::stream::do_shutdown ssls(1): {}", ssls);
        //status_  = Status::closing;
    } else {
        throw exception(std::string("disconnecting in valid state status_:") // (int)status_
        );  
    }
    // check error
    if (e < 0)  {
        if (SSL_get_error(ssl_, e) == SSL_ERROR_WANT_READ)  {
//            LOG_DEBUG("ssl::stream::do_connect  SSL_ERROR_WANT_READ ");
        } else if (SSL_get_error(ssl_, e) == SSL_ERROR_WANT_WRITE)  {
//            LOG_DEBUG("ssl::stream::do_connect  SSL_ERROR_WANT_WRITE ");
        } else  {
//            LOG_DEBUG("ssl::stream::do_connect  ERROR.... ");
        }
    } else if (e == 0){
        LOG_DEBUG("ssl::stream::do_shutdown  OK pending ");
        status_ = status::closing; // do we need error status?
 
    } else if (e == 1){
        LOG_DEBUG("ssl::stream::do_shutdown OK finished");
        status_ = status::closed;
        // prior connected ...
    }
    buffer b;
    int n;
    // while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
    //     next_.template write<Chain>(b.data(), n);
    // }
    ssl_to_next(chain);

    if(status_ == status::closed) 
        this->prev(chain).on_disconnected(chain);

    while(n= SSL_read(ssl_, b.data(), b.size()), n > 0) {
        this->prev(chain).on_received(chain, b.data(), n);
    }
    int ssls = SSL_get_shutdown(ssl_);
    LOG_DEBUG("ssl::stream::do_shutdown ssls(2): {}", ssls);

}

template<typename Next>
template<typename Chain>
void stream<Next>::disconnect(Chain& chain) {
    LOG_DEBUG("ssl::stream::disconnect begin");
    if (status_ == status::connected) {
        status_ = status::closing;
        do_shutdown(chain, nullptr, 0);
    } else {
        LOG_ERROR("ssl::stream::disconnect: invalid state");
        //throw Exception(message);
    }    
}

template<typename Next>
template<typename Chain>
void stream<Next>::on_received(Chain& chain, const char* buf, size_t len)  {
    //acpp::network::timer t; t.start("ssl on_received");

    LOG_DEBUG("ssl::stream::on_received(1) side: {} len: {} status_: {} ", (int)side_, len, (int)status_);
    if (status_ == status::closed || status_ == status::connecting ) {
        do_connect(chain, buf, len);
        return; 
    }
    if (status_ == status::closing) {
        do_shutdown(chain, buf, len);
        return; 
    }

    if (status_ == status::connected) 
    {
        //auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
        //const char* hostname = SSL_get_servername(ssl_, TLSEXT_NAMETYPE_host_name);

        int n = 0;
        int tot_n =0;
        buffer b;
        while (tot_n < len) {
            //n = BIO_write(ext_bio, buf + tot_n, len - tot_n); 
            n = next_to_ssl(chain, buf + tot_n, len - tot_n);
            tot_n += n;
            LOG_DEBUG("*ssl::stream::on_received(2) BIO_write: n: {} len: {} tot_n: {}", n, len, tot_n);

            while(n = ::SSL_read(ssl_, b.data(), b.size()), n > 0) {
                LOG_DEBUG("ssl::stream::on_received(3) SSL_read: {}", n);
                    this->prev(chain).on_received(chain, b.data(), n);
            } 
            if (n <= 0) {
                //SSL_get_error(ssl_, e) == SSL_ERROR_WANT_WRITE);
                LOG_DEBUG("ssl::stream::on_received(4) SSL_read: {} error: {}", n, SSL_get_error(ssl_, n));
            }
            //shutdown_st = SSL_get_shutdown(ssl_);
            //TODO: not sure if this is needed, only if start shutdown??
            // while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
            //     LOG_DEBUG("ssl::stream::on_received(5) BIO_read: {}", n);
            //         next_.template write<Chain>(b.data(), n);
            // }        
        }

        int shutdown_st = SSL_get_shutdown(ssl_);
        if (shutdown_st ==  SSL_RECEIVED_SHUTDOWN)    {
            status_ = status::peer_closing;
            shutdown_st = SSL_shutdown(ssl_);
            LOG_DEBUG("ssl::stream::on_received(6) SSL_shutdown");;
        }

        // while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
        //     LOG_DEBUG("ssl::stream::on_received(7) BIO_read (2): {}", n);
        //     next_.template write<Chain>(b.data(), n);
        // }
        ssl_to_next(chain);

//SSL_SENT_SHUTDOWN
//SSL_RECEIVED_SHUTDOWN


    }

}

template<typename Next>
template<typename Chain>
std::error_code stream<Next>::write(Chain& chain, const char* buf, size_t len)  {
    //acpp::network::timer t; t.start("ssl write");
    LOG_DEBUG("ssl::stream::write(1) len: {}", len);
    //auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
    if (status_ == status::connected) {
        int total_len = 0;
        while (total_len < len)  {
            size_t partial_len = std::min(len - total_len, (size_t)1024*4);
            int e = SSL_write(ssl_, buf + total_len, partial_len);
            //SSL_MODE_ENABLE_PARTIAL_WRITE option of SSL_CTX_set_mode(3). 
            LOG_DEBUG("ssl::stream::write(2) len: {} written {}", len, e);
            if (e > 0)  {
                total_len += e;
            }
            else {
                break;
            }
            if (total_len == len) {
                //last write
                return ssl_to_next(chain);
            } else {
                auto err = ssl_to_next(chain);
                if (err != acpp::network::error::success && err != acpp::network::error::data_pending) {
                    return err;
                }
            }
        }

    } else {
        LOG_ERROR("ssl::stream<>::write invalid state");
//        throw Exception("ssl::stream<>::write: invalid state");
        return make_error_code(error::invalid_state);
    }
}

/*

option1: 
callback on write method
w +++++++ cb
w ++++ ++++ ++++ cb
                 ->cb(OK)
w +++++++ cb
w ++++ ++++ E ++++ cb
            -> cb(E)

option2: 
send_ready generic callback, as in on_received_ 
    to be send when pending data is 0
    what happens when several sync writes from the same operation, the first is sync.
errors reported in on_error callback
    add a flag to know if the sync write produced an error to stop writing in loop? then wait for the error in generic cb
    add a parameter to say you are interested in send_ready callback, ssl will set it in the last call

what happens if we call serveral times write form the app?  forbiden or queue?    
one solution to this: flag in the call, several write calls can set it, and when when send_ready is sent, the flag is cleared

option3:
all write operations are sychronous and return a error code/struct, layer and codes ssl error codes, abstractc error codes
and also ok (all done), data_pending or error 
so, when result = data_pending wait for send_ready callback to send data again
std::optional for result and error codes?

*/ 

template<typename Next>
template <typename Chain>
std::error_code stream<Next>::ssl_to_next(Chain& chain) {
#ifdef ACPP_BIO
    //nothing to do. the custom bio will write directly to next 
#else
    buffer b;
    int n;
    std::error_code result;
    while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
        result = this->next_.write(chain, b.data(), n); 
        if (is_error(result)) 
            return result;
    }
    return result;
#endif    
}

template<typename Next>
template <typename Chain>
int stream<Next>::next_to_ssl(Chain& chain, const char* buf, size_t len) {
#ifdef ACPP_BIO
    LOG_DEBUG("next_to_ssl len: {}", len);
    custom_bio_.in_buf_ = (char*)buf;
    custom_bio_.in_size_ = len;
    return len;
#else
    int e = BIO_write(ext_bio, buf, len);
    if ( e > 0 && e < len) {
        LOG_ERROR("ssl::stream::do_connect NOT ALL DATA WRITTEN side: {} BIO_write e:{} len: {}", (int)side_, e, len);
    } else {
        LOG_DEBUG("ssl::stream::do_connect  side: {} BIO_write e:{} len: {}", (int)side_, e, len); 
    }
    return e;
#endif    
}



} // namespace acpp::network::ssl 