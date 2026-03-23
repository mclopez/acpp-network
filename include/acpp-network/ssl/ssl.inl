#pragma once

#include <acpp-network/ssl/ssl.h>

#include <openssl/ssl.h>

#include <algorithm>

//#include <openssl/ssl.h>
#include <array>

namespace acpp::network::ssl {


using buffer = std::array<char, 1024*20>; //ojo!!


template<typename Next>
stream<Next>::stream(side_t side)
:side_(side), next_(side), ctx_(std::make_shared<context>(side)), status_(status::closed)
{
    LOG_DEBUG("ssl::stream<Next>::stream side: {} status: {}", (int)side_, (int)status_); 
    next_.prev_ = this;

    if (side == side_t::server) {
        auto c = x509::create_self_signed_cert(x509::Name().cn("xxx").l("l").o("o").st("st"));
        LOG_DEBUG("ssl::stream<Next>::stream: cert: {}", c.first.to_string());
        ctx_->set_cert(c.first);
        ctx_->set_pkey(c.second);
        //ssl_ = std::make_unique<ssl::Stream>(context_);

    }



    ssl_ = SSL_new(ctx_->handle());
    BIO_new_bio_pair(&int_bio, 0 /*1024 * 50*/, &ext_bio, 0 /* 1024 * 50*/);
    SSL_set_bio(ssl_, int_bio, int_bio);
    int size = 0;
    //BIO_set_write_buf_size(ext_bio, 1024 * 64); // Set to 64KB

    BIO_get_write_buf_size(ext_bio, size);
    LOG_DEBUG("BIO_get_write_buf_size(ext_bio) {}", size); 

}

 
template<typename Next>
template<typename Context> 
stream<Next>::stream(Context& c)
:side_(c.side()), next_(c), ctx_(c.ctx()), status_(status::closed)
{
    LOG_DEBUG("ssl::stream<Next>::stream side: {} status: {}", (int)side_, (int)status_); 
    next_.prev_ = this;

    // if (side_ == side_t::server) {
    //     auto start_time = std::chrono::high_resolution_clock::now();

    //     auto c = x509::create_self_signed_cert(x509::Name().cn("xxx").l("l").o("o").st("st"));

    //     auto end_time = std::chrono::high_resolution_clock::now();

    //     auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
    //     std::cout << "⏱️  Latency create cert: " << duration.count() << "ms" << std::endl;



    //     LOG_DEBUG("ssl::stream<Next>::stream: cert: {}", c.first.to_string());
    //     ctx_->set_cert(c.first);
    //     ctx_->set_pkey(c.second);
    //     //ssl_ = std::make_unique<ssl::Stream>(context_);
    // }


    ssl_ = SSL_new(ctx_->handle());
    BIO_new_bio_pair(&int_bio, /*0*/ 1024 * 5, &ext_bio, /*0*/ 1024 * 5);
    SSL_set_bio(ssl_, int_bio, int_bio);
    int size = 0;
    BIO_get_write_buf_size(ext_bio, size);
    LOG_DEBUG("BIO_get_write_buf_size(ext_bio) {}", size); 

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
void stream<Next>::connect() {  
    next_.template connect<Chain>();
}


//on prior connected
template<typename Next>
template <typename Chain>
void stream<Next>::on_connected() { 
    LOG_DEBUG("ssl::stream::on_connected status:{} ctx_.type():{}", (int)status_, (int)ctx_.side());
    if (side_ == side_t::client) {
        do_connect<Chain>(nullptr, 0);
    }
}

template<typename Next>
template <typename Chain>
void stream<Next>::do_connect(const char* buf, size_t len) {
    LOG_DEBUG("ssl::stream::do_connect side: {} status: {}", (int)side_, (int)status_);
    int e;
    if (len) {
        e = BIO_write(ext_bio, buf, len);
        LOG_DEBUG("ssl::stream::do_connect  side: {} BIO_write e:{} len: {}", (int)side_, e, len); 
    }
    if (status_ == status::closed || status_ == status::connecting) {
        if (side_ == side_t::server) {
            LOG_DEBUG("ssl::stream::do_connect  side: {} SSL_accept", (int)side_); 
            e =  SSL_accept(ssl_);
        }
        else {
            if (!hostname_.empty()) {
                LOG_DEBUG("ssl::stream::do_connect set SNI: {}", hostname_);
                SSL_set_tlsext_host_name(ssl_, hostname_.c_str());
            }
            LOG_DEBUG("ssl::stream::do_connect  side: {} SSL_connect", (int)side_); 
            e = SSL_connect(ssl_);    
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
    buffer b;
    int n;
    
    auto prior = acpp::network::async::get_prev<Chain, it>(prev_);


    while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
        next_.template write<Chain>(b.data(), n); //*** 
    }
    if(status_ == status::connected && prior) 
        prior->template on_connected<Chain>();

    while(n= SSL_read(ssl_, b.data(), b.size()), n > 0) {
        prior->template on_received<Chain>(b.data(), n);
    }
}
// write_input -> to app
// write_output -> to socket

//on prior connected
template<typename Next>
template <typename Chain>
void stream<Next>::on_disconnected() { 
    LOG_DEBUG("ssl::stream::on_disconnected status:{} ctx_.type():{}", (int)status_, (int)ctx_.side());
    //do_shutdown<Chain>(nullptr, 0);
}


template<typename Next>
template<typename Chain>
void stream<Next>::do_shutdown(const char* buf, size_t len) {
    LOG_DEBUG("ssl::stream::do_shutdown status_: {}", (int)status_); 
    auto prior = acpp::network::async::get_prev<Chain, it>(prev_);

    int e;
    if (len) {
        e = BIO_write(ext_bio, buf, len);
        LOG_DEBUG("ssl::stream::do_shutdown BIO_write e: {} len: {}", e, len); 

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

    while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
        next_.template write<Chain>(b.data(), n);
    }
    if(status_ == status::closed && prior) 
        prior->template on_disconnected<Chain>();

    while(n= SSL_read(ssl_, b.data(), b.size()), n > 0) {
        if (prior)
            prior->template on_received<Chain>(b.data(), n);
    }
    int ssls = SSL_get_shutdown(ssl_);
    LOG_DEBUG("ssl::stream::do_shutdown ssls(2): {}", ssls);

}

template<typename Next>
template<typename Chain>
void stream<Next>::disconnect() {
    LOG_DEBUG("ssl::stream::disconnect begin");
    if (status_ == status::connected) {
        status_ = status::closing;
        do_shutdown<Chain>(nullptr, 0);
    } else {
        LOG_ERROR("ssl::stream::disconnect: invalid state");
        //throw Exception(message);
    }    
}

template<typename Next>
template<typename Chain>
void stream<Next>::on_received(const char* buf, size_t len)  {
    LOG_DEBUG("ssl::stream::on_received side: {} len: {} status_: {} ", (int)side_, len, (int)status_);
    if (status_ == status::closed || status_ == status::connecting ) {
        do_connect<Chain>(buf, len);
        return; 
    }
    if (status_ == status::closing) {
        do_shutdown<Chain>(buf, len);
        return; 
    }
    auto prior = acpp::network::async::get_prev<Chain, it>(prev_);

    if (status_ == status::connected) 
    {
        //const char* hostname = SSL_get_servername(ssl_, TLSEXT_NAMETYPE_host_name);

        int n = 0;
        int tot_n =0;
        buffer b;
        while (tot_n < len) {
            n = BIO_write(ext_bio, buf + tot_n, len - tot_n); 
            tot_n += n;
            LOG_DEBUG("*ssl::stream::on_received BIO_write: n: {} len: {} tot_n: {}", n, len, tot_n);

            while(n = ::SSL_read(ssl_, b.data(), b.size()), n > 0) {
                LOG_DEBUG("ssl::stream::on_received SSL_read: {}", n);
                    prior->template on_received<Chain>(b.data(), n);
            } 
            if (n <= 0) {
                //SSL_get_error(ssl_, e) == SSL_ERROR_WANT_WRITE);
                LOG_DEBUG("ssl::stream::on_received SSL_read: {} error: {}", n, SSL_get_error(ssl_, n));
            }
            //shutdown_st = SSL_get_shutdown(ssl_);
            while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
                LOG_DEBUG("ssl::stream::on_received BIO_read: {}", n);
                    next_.template write<Chain>(b.data(), n);
            }        
        }

        int shutdown_st = SSL_get_shutdown(ssl_);
        if (shutdown_st ==  SSL_RECEIVED_SHUTDOWN)    {
            status_ = status::peer_closing;
            shutdown_st = SSL_shutdown(ssl_);
            LOG_DEBUG("ssl::stream::on_received SSL_shutdown");;
        }

        while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
            LOG_DEBUG("ssl::stream::on_received BIO_read(2): {}", n);
            next_.template write<Chain>(b.data(), n);
        }

//SSL_SENT_SHUTDOWN
//SSL_RECEIVED_SHUTDOWN


    }

}

template<typename Next>
template<typename Chain>
size_t stream<Next>::write(const char* buf, size_t len)  {
    LOG_DEBUG("ssl::stream::write_output len: {}", len);
    auto prior = acpp::network::async::get_prev<Chain, it>(prev_);
    if (status_ == status::connected) {
        int total_len = 0;
        while (total_len < len)  {
            size_t partial_len = std::min(len - total_len, (size_t)1024*4);
            int e = SSL_write(ssl_, buf + total_len, partial_len);
            //SSL_MODE_ENABLE_PARTIAL_WRITE option of SSL_CTX_set_mode(3). 
            LOG_DEBUG("ssl::stream::write_output len: {} written {}", len, e);
            if (e > 0)  {
                total_len += e;
            }
            else {
                break;
            }
            buffer b;
            int n;
            while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
                LOG_DEBUG("ssl::stream::write_output BIO_read: n {}", n);
                next_.template write<Chain>(b.data(), n);       
                //break;    //todo: why???
            }
        }
        // int n;
        // buffer b;
        // while (n = ::BIO_read(ext_bio, b.data(), b.size()), n > 0) {
        //     LOG_DEBUG("ssl::stream::write_output BIO_read: n {}", n);
        //     next_.template write<Chain>(b.data(), n);       
        //     //break;    //todo: why???
        // }

    } else {
        LOG_DEBUG("ssl::stream::write_output invalid state");
        //throw Exception("ssl::stream::write_output: invalid state");
    }

    return len;
}



} // namespace acpp::network::ssl 