#ifdef _WIN32
#define NOMINMAX
#endif

#include <openssl/ssl.h>

#include <acpp-network/ssl/ssl.h>

#include <openssl/x509v3.h>

#include <detail/common.h>

namespace acpp::network::ssl {


using BigNum = std::unique_ptr<::BIGNUM, decltype(&::BN_free)>;

BigNum create_big_numm() {
    return BigNum(::BN_new(), ::BN_free);
}


using Rsa = std::unique_ptr<::RSA, decltype(&::RSA_free)>;

Rsa create_rsa() {
    return Rsa(::RSA_new(), ::RSA_free);
}

using Bio = std::unique_ptr<::BIO, decltype(&::BIO_free)>;

Bio create_bio() {
    //return Rsa(::BIO_new_fp(), ::BIO_free);
    return Bio(::BIO_new(BIO_s_mem()), ::BIO_free);    
}


pkey::pkey()
:handle_(::EVP_PKEY_new())
{

}

pkey::pkey(pkey&& x)
:handle_(x.handle_) 
{
    x.handle_ = nullptr;
}


pkey::~pkey() {

}

x509::~x509() {

}


void set_name(X509_NAME* name, const x509::Name& n) {
    X509_NAME_add_entry_by_txt(name, "C",  MBSTRING_ASC, (unsigned char*)n.c().c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "ST",  MBSTRING_ASC, (unsigned char*)n.st().c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "L",  MBSTRING_ASC, (unsigned char*)n.l().c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O",  MBSTRING_ASC, (unsigned char*)n.o().c_str(), -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN",  MBSTRING_ASC, (unsigned char*)n.cn().c_str(), -1, -1, 0);
}

int add_ext(::X509 *cert, int nid, char *value) {
    X509_EXTENSION *ex;
    X509V3_CTX ctx;

    /* Create a context and set the issuer and subject names */
    X509V3_set_ctx_nodb(&ctx);
    X509V3_set_ctx(&ctx, cert, cert, NULL, NULL, 0);
    ex = X509V3_EXT_conf_nid(NULL, &ctx, nid, value);

    if (!ex)
        return 0;

    X509_add_ext(cert, ex, -1);
    X509_EXTENSION_free(ex);

    return 1;
}

x509::x509()
:handle_(X509_new())
{

}

x509::x509(x509&& x)
:handle_(x.handle_) 
{
    x.handle_ = nullptr;
}



std::string x509::to_string() {
    auto bio = create_bio();

    X509_print(bio.get(), handle());

    const char *buffer;
    long n = BIO_get_mem_data(bio.get(), &buffer);
    LOG_DEBUG("cert: {}", buffer);
    return std::string(buffer);

}


std::pair<x509, pkey>  x509::create_cert(const x509::Name& n) {
    // Step 1: Generate RSA key
    std::pair<x509, pkey> result;
    pkey& pkey = result.second;
    auto pk = pkey.handle();
    Rsa rsa = create_rsa(); 
    BigNum bn = create_big_numm();
    BN_set_word(bn.get(), RSA_F4);
    RSA_generate_key_ex(rsa.get(), 2048, bn.get(), nullptr);
    EVP_PKEY_assign_RSA(pk, rsa.get());
    //TODO: why is this needed???
    rsa.release();

    auto x509 = result.first.handle();

    X509_set_version(x509, 2); // Version 3
    //TODO: lock ... atomic inc
    static int cont=0;
    ASN1_INTEGER_set(X509_get_serialNumber(x509), ++cont); // Serial number
    // Set the certificate's validity period
    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 31536000L); // Valid for one year


    // Set the public key for the certificate
    X509_set_pubkey(x509, pk);

    // Set certificate subject and issuer
    X509_NAME* name = X509_get_subject_name(x509);
    
    set_name(name, n);

    std::string cn = n.cn_;
    std::string nam("DNS:");
    //nam = nam + cn + ",DNS:" + cn;
    nam = nam + cn;
    if (!add_ext(x509, NID_subject_alt_name,  (char*)nam.c_str())) {
        exception("Failed to add SAN extension");
    }



    return result;
}





std::pair<x509, pkey>  x509::create_self_signed_cert(const x509::Name& n) {

    auto result = x509::create_cert(n);

    auto x509 = result.first.handle();

    // Self-signed: issuer is the same as subject
    X509_NAME* name = X509_get_subject_name(x509);
    X509_set_issuer_name(x509, name);

    X509_PUBKEY * pkey = X509_get_X509_PUBKEY(x509);
    // Sign the certificate with the private key
    X509_sign(x509, result.second.handle(), EVP_sha256());

    return result;
}

using X509Req = std::unique_ptr<::X509_REQ, decltype(&::X509_REQ_free)>;

X509Req create_req() {
    return X509Req(::X509_REQ_new(), ::X509_REQ_free);    
}




std::pair<x509, pkey> x509::create_signed_cert(x509& ca_cert, pkey& ca_pkey, const x509::Name& n) {
    auto x509_key = create_cert(n);
    auto x509 = x509_key.first.handle();
    // Step 2: Create new X.509 certificate request (CSR)
    //X509_REQ* req = X509_REQ_new();
    X509Req req = create_req();
    auto pkey = x509_key.second.handle();
    X509_REQ_set_pubkey(req.get(), pkey);

    X509_NAME* name = X509_REQ_get_subject_name(req.get());
    set_name(name, n);
    // Sign the CSR with the new key
    X509_REQ_sign(req.get(), pkey, EVP_sha256());

    // Set certificate issuer (from CA certificate)
    X509_set_issuer_name(x509, X509_get_subject_name((::X509*)ca_cert.handle()));

    // Sign the certificate with the CA's private key
    X509_sign(x509, ca_pkey.handle(), EVP_sha256());


    return x509_key;

}


void x509::sign(x509& ca_cert, pkey& pk) {
    //auto x509 = (::X509*)impl();
    auto x509 =handle();
    X509_set_issuer_name(x509, X509_get_subject_name(ca_cert.handle()));

    X509_sign(x509, pk.handle(), EVP_sha256());
}



context::context(side_t s)
: side_(s) 
{   const SSL_METHOD *method;

    method = side_ == side_t::server? TLS_server_method(): TLS_client_method();

    handle_ = SSL_CTX_new(method);
    if (!handle_) {
        throw exception("Unable to create SSL context");
    }

    // // Set the key and cert 
    // std::string cert;
    // cert = "/home/marcos/Documentos/projects/proxy/cert/v2/proxy_marcos2.crt";
    //  if (SSL_CTX_use_certificate_file(handle(*this), cert.c_str(), SSL_FILETYPE_PEM) <= 0) {
    //      throw Exception("Fail to set cert");
    //  }

    // std::string pkey;
    // pkey = "/home/marcos/Documentos/projects/proxy/cert/v2/proxy_marcos2.key.insecure";
    // if (SSL_CTX_use_PrivateKey_file(handle(*this), pkey.c_str(), SSL_FILETYPE_PEM) <= 0 ) {
    //      throw Exception("Fail to set key");
    // }

    //SSL_CTX_set_options(ctx_, SSL_OP_IGNORE_UNEXPECTED_EOF);
    SSL_CTX_set_mode(handle_,  SSL_MODE_ENABLE_PARTIAL_WRITE);
}






context::~context() {
    if (handle_) {
        SSL_CTX_free(handle_);
    }   
}




// #include <openssl/ssl.h>
// #include <openssl/err.h>
// #include <openssl/types.h>

// #include <iostream>


// #include <log.h>

// #include "stream.h"
// #include "impl.h"



// struct stream::Impl {
//     BIO *int_bio;
//     BIO *ext_bio;
//     SSL *ssl_;
//     Impl():int_bio(nullptr), ext_bio(nullptr), ssl_(nullptr){}
// };


void context::set_cert(x509& cert) { 
    LOG_DEBUG("ssl::context::set_cert");
    auto e = SSL_CTX_use_certificate(handle(), cert.handle());
    if (e <= 0)
        throw exception("Fail to set cert 2");
}

void context::set_pkey(pkey& pkey) {
    SSL_CTX_use_PrivateKey(handle(), pkey.handle());
}

ssl_stream_context::ssl_stream_context(acpp::network::async::io_context& io, side_t side, const std::string& hostname)
:io_(io), side_(side), hostname_(hostname), context_(std::make_shared<context>(side))
{
    if (side_ == side_t::server) {
        //auto start_time = std::chrono::high_resolution_clock::now();

        auto c = x509::create_self_signed_cert(x509::Name().cn("xxx").l("l").o("o").st("st"));

        // auto end_time = std::chrono::high_resolution_clock::now();
        // auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);        
        // std::cout << "⏱️  Latency create cert: " << duration.count() << "ms" << std::endl;

        //SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
        LOG_DEBUG("ssl::stream<Next>::stream: cert: {}", c.first.to_string());
        context_->set_cert(c.first);
        context_->set_pkey(c.second);
    }
    //SSL_CTX_set_mode(context_->handle(), SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

}


} // namespace acpp::network::ssl
