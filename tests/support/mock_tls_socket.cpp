// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file mock_tls_socket.cpp
 * @brief Implementation of in-process TLS peer helpers (Issue #1060)
 */

#include "mock_tls_socket.h"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <asio/buffer.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace kcenon::network::tests::support
{

namespace
{

// Custom deleters for OpenSSL handles so we can manage lifetimes via unique_ptr.
struct evp_pkey_deleter
{
    void operator()(EVP_PKEY* p) const noexcept { EVP_PKEY_free(p); }
};
struct x509_deleter
{
    void operator()(X509* p) const noexcept { X509_free(p); }
};
struct bio_deleter
{
    void operator()(BIO* p) const noexcept { BIO_free(p); }
};

using evp_pkey_ptr = std::unique_ptr<EVP_PKEY, evp_pkey_deleter>;
using x509_ptr = std::unique_ptr<X509, x509_deleter>;
using bio_ptr = std::unique_ptr<BIO, bio_deleter>;

[[nodiscard]] std::string openssl_error_string()
{
    bio_ptr bio(BIO_new(BIO_s_mem()));
    if (!bio)
    {
        return "OpenSSL error (BIO allocation failed)";
    }
    ERR_print_errors(bio.get());
    char* data = nullptr;
    const auto len = BIO_get_mem_data(bio.get(), &data);
    if (len <= 0 || data == nullptr)
    {
        return "unknown OpenSSL error";
    }
    return std::string(data, static_cast<size_t>(len));
}

[[nodiscard]] evp_pkey_ptr generate_rsa_key(int bits)
{
    evp_pkey_ptr pkey(EVP_RSA_gen(static_cast<unsigned int>(bits)));
    if (!pkey)
    {
        throw std::runtime_error("EVP_RSA_gen failed: " + openssl_error_string());
    }
    return pkey;
}

[[nodiscard]] x509_ptr build_self_signed_cert(EVP_PKEY& pkey)
{
    x509_ptr cert(X509_new());
    if (!cert)
    {
        throw std::runtime_error("X509_new failed");
    }

    if (X509_set_version(cert.get(), 2) != 1)
    {
        throw std::runtime_error("X509_set_version failed: " + openssl_error_string());
    }
    ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);

    // Validity: now to now + 100 years (drift-insensitive).
    constexpr long kHundredYearsSeconds = 60L * 60 * 24 * 365 * 100;
    if (X509_gmtime_adj(X509_get_notBefore(cert.get()), 0) == nullptr ||
        X509_gmtime_adj(X509_get_notAfter(cert.get()), kHundredYearsSeconds) == nullptr)
    {
        throw std::runtime_error("X509 validity setup failed: " + openssl_error_string());
    }

    if (X509_set_pubkey(cert.get(), &pkey) != 1)
    {
        throw std::runtime_error("X509_set_pubkey failed: " + openssl_error_string());
    }

    X509_NAME* name = X509_get_subject_name(cert.get());
    const auto add_entry = [name](const char* field, const char* value) {
        if (X509_NAME_add_entry_by_txt(
                name, field, MBSTRING_ASC,
                reinterpret_cast<const unsigned char*>(value),
                -1, -1, 0) != 1)
        {
            throw std::runtime_error("X509_NAME_add_entry_by_txt failed: " +
                                     openssl_error_string());
        }
    };
    add_entry("CN", "localhost");
    add_entry("O", "kcenon-network-tests");

    if (X509_set_issuer_name(cert.get(), name) != 1)
    {
        throw std::runtime_error("X509_set_issuer_name failed: " + openssl_error_string());
    }

    if (X509_sign(cert.get(), &pkey, EVP_sha256()) == 0)
    {
        throw std::runtime_error("X509_sign failed: " + openssl_error_string());
    }

    return cert;
}

[[nodiscard]] std::string pem_from_cert(X509& cert)
{
    bio_ptr bio(BIO_new(BIO_s_mem()));
    if (!bio || PEM_write_bio_X509(bio.get(), &cert) != 1)
    {
        throw std::runtime_error("PEM_write_bio_X509 failed: " + openssl_error_string());
    }
    char* data = nullptr;
    const auto len = BIO_get_mem_data(bio.get(), &data);
    if (len <= 0 || data == nullptr)
    {
        throw std::runtime_error("PEM_write_bio_X509 produced no data");
    }
    return std::string(data, static_cast<size_t>(len));
}

[[nodiscard]] std::string pem_from_key(EVP_PKEY& pkey)
{
    bio_ptr bio(BIO_new(BIO_s_mem()));
    if (!bio ||
        PEM_write_bio_PrivateKey(bio.get(), &pkey, nullptr, nullptr, 0, nullptr, nullptr) != 1)
    {
        throw std::runtime_error("PEM_write_bio_PrivateKey failed: " + openssl_error_string());
    }
    char* data = nullptr;
    const auto len = BIO_get_mem_data(bio.get(), &data);
    if (len <= 0 || data == nullptr)
    {
        throw std::runtime_error("PEM_write_bio_PrivateKey produced no data");
    }
    return std::string(data, static_cast<size_t>(len));
}

} // namespace

self_signed_pem generate_self_signed_pem()
{
    auto pkey = generate_rsa_key(2048);
    auto cert = build_self_signed_cert(*pkey);
    return self_signed_pem{
        pem_from_cert(*cert),
        pem_from_key(*pkey),
    };
}

namespace
{

// Server-side ALPN selector. Prefers HTTP/2 ("h2"); falls back to HTTP/1.1
// for non-h2 clients. http2_client strictly verifies that the negotiated
// ALPN protocol is "h2" after handshake (see http2_client.cpp), so the
// listener context must complete the negotiation server-side. The header
// documented this; the implementation was missing.
int alpn_select_h2_then_http11(SSL* /*ssl*/,
                               const unsigned char** out,
                               unsigned char* outlen,
                               const unsigned char* in,
                               unsigned int inlen,
                               void* /*arg*/) noexcept
{
    static const unsigned char kPrefs[] = {
        2, 'h', '2',
        8, 'h', 't', 't', 'p', '/', '1', '.', '1'
    };
    if (SSL_select_next_proto(const_cast<unsigned char**>(out), outlen,
                              kPrefs, sizeof(kPrefs),
                              in, inlen) == OPENSSL_NPN_NEGOTIATED)
    {
        return SSL_TLSEXT_ERR_OK;
    }
    return SSL_TLSEXT_ERR_ALERT_FATAL;
}

} // namespace

asio::ssl::context make_self_signed_ssl_context(asio::ssl::context::method method)
{
    asio::ssl::context ctx(method);
    ctx.set_options(asio::ssl::context::default_workarounds |
                    asio::ssl::context::no_sslv2 |
                    asio::ssl::context::no_sslv3 |
                    asio::ssl::context::single_dh_use);

    const auto pem = generate_self_signed_pem();
    ctx.use_certificate_chain(asio::buffer(pem.cert_pem));
    ctx.use_private_key(asio::buffer(pem.key_pem), asio::ssl::context::pem);

    // Wire the server-side ALPN selection callback. Without this, the server
    // ignores the client's ALPN extension and the client (e.g. http2_client)
    // observes a missing or unexpected protocol after handshake.
    SSL_CTX_set_alpn_select_cb(ctx.native_handle(),
                               &alpn_select_h2_then_http11,
                               nullptr);

    return ctx;
}

asio::ssl::context make_permissive_client_context()
{
    asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
    ctx.set_verify_mode(asio::ssl::verify_none);
    return ctx;
}

tls_loopback_listener::tls_loopback_listener(asio::io_context& io)
    : server_ctx_(make_self_signed_ssl_context())
    , acceptor_(io)
{
    using asio::ip::tcp;

    tcp::endpoint bind_ep(asio::ip::address_v4::loopback(), 0);
    acceptor_.open(bind_ep.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(bind_ep);
    acceptor_.listen();
    endpoint_ = acceptor_.local_endpoint();

    // Begin accepting one connection. The handshake is also chained here so
    // accepted_socket() returns only after a successful handshake.
    auto stream = std::make_unique<asio::ssl::stream<tcp::socket>>(io, server_ctx_);
    auto* raw = stream.get();
    accepted_stream_ = std::move(stream);

    acceptor_.async_accept(
        raw->lowest_layer(),
        [this, raw](const std::error_code& accept_ec) {
            if (accept_ec)
            {
                return;
            }
            accepted_.store(true);
            raw->async_handshake(
                asio::ssl::stream_base::server,
                [this](const std::error_code& hs_ec) {
                    if (!hs_ec)
                    {
                        handshake_done_.store(true);
                    }
                });
        });
}

tls_loopback_listener::~tls_loopback_listener()
{
    std::error_code ec;
    acceptor_.close(ec);
}

std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>>
tls_loopback_listener::accepted_socket(std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (handshake_done_.load())
        {
            return std::move(accepted_stream_);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return nullptr;
}

} // namespace kcenon::network::tests::support
