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
#include <asio/bind_executor.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
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
    // Use X509_time_adj_ex (day-based) for notAfter: 100 years in seconds
    // (~3.15e9) overflows a 32-bit long (LLP64/Windows), and X509_gmtime_adj's
    // offset argument is a long.
    if (X509_gmtime_adj(X509_get_notBefore(cert.get()), 0) == nullptr ||
        X509_time_adj_ex(X509_get_notAfter(cert.get()), 365 * 100, 0, nullptr) == nullptr)
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

namespace
{

// CTest runs discovered cases in separate processes. Share one test identity
// within a process so simultaneous mock peers use the same trust anchor.
// The production client's verify_peer setting remains enabled.
struct trusted_test_identity
{
    self_signed_pem pem = generate_self_signed_pem();
    std::filesystem::path directory;
    std::optional<std::string> previous_ca_file;

    trusted_test_identity()
    {
        if (const char* previous = std::getenv("SSL_CERT_FILE"))
        {
            previous_ca_file = previous;
        }
        std::random_device random;
        do
        {
            directory = std::filesystem::temp_directory_path() /
                ("network-test-ca-" + std::to_string(random()) + "-" +
                 std::to_string(random()));
        } while (!std::filesystem::create_directory(directory));
        auto ca_file = directory / "ca.pem";
        std::ofstream output(ca_file);
        output << pem.cert_pem;
        output.close();
        if (!output)
        {
            throw std::runtime_error("Cannot write test CA certificate");
        }
#ifdef _WIN32
        _putenv_s("SSL_CERT_FILE", ca_file.string().c_str());
#else
        setenv("SSL_CERT_FILE", ca_file.string().c_str(), 1);
#endif
    }

    ~trusted_test_identity()
    {
#ifdef _WIN32
        _putenv_s("SSL_CERT_FILE", previous_ca_file.value_or("").c_str());
#else
        if (previous_ca_file)
            setenv("SSL_CERT_FILE", previous_ca_file->c_str(), 1);
        else
            unsetenv("SSL_CERT_FILE");
#endif
        std::error_code ec;
        std::filesystem::remove_all(directory, ec);
    }
};

asio::ssl::context make_server_context(asio::ssl::context::method method,
                                      const self_signed_pem& pem)
{
    asio::ssl::context ctx(method);
    ctx.set_options(asio::ssl::context::default_workarounds |
                    asio::ssl::context::no_sslv2 |
                    asio::ssl::context::no_sslv3 |
                    asio::ssl::context::single_dh_use);

    SSL_CTX_set_min_proto_version(ctx.native_handle(), TLS1_2_VERSION);
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

asio::ssl::context make_trusted_server_context()
{
    static const trusted_test_identity identity;
    return make_server_context(asio::ssl::context::tls_server, identity.pem);
}

} // namespace

asio::ssl::context make_self_signed_ssl_context(asio::ssl::context::method method)
{
    return make_server_context(method, generate_self_signed_pem());
}

asio::ssl::context make_permissive_client_context()
{
    asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
    ctx.set_verify_mode(asio::ssl::verify_none);
    return ctx;
}

// Handlers retain the transport independently of the stack-allocated listener.
// The strand serializes cancellation with the composed TLS handshake, while
// the mutex protects transfer of the completed stream to a test worker.
struct tls_loopback_listener::state
{
    state(asio::io_context& io, bool trusted)
        : server_ctx(trusted ? make_trusted_server_context() : make_self_signed_ssl_context())
        , strand(asio::make_strand(io))
        , acceptor(io)
        , stream(std::make_unique<asio::ssl::stream<asio::ip::tcp::socket>>(io, server_ctx))
    {
    }

    asio::ssl::context server_ctx;
    asio::strand<asio::io_context::executor_type> strand;
    asio::ip::tcp::acceptor acceptor;
    std::mutex mutex;
    std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> stream;
    std::atomic<bool> accepted{false};
    std::atomic<bool> handshake_done{false};
    bool stopped = false;
};

tls_loopback_listener::tls_loopback_listener(asio::io_context& io, bool trusted)
    : state_(std::make_shared<state>(io, trusted))
{
    using asio::ip::tcp;
    auto shared = state_;
    tcp::endpoint bind_ep(asio::ip::address_v4::loopback(), 0);
    shared->acceptor.open(bind_ep.protocol());
    shared->acceptor.set_option(tcp::acceptor::reuse_address(true));
    shared->acceptor.bind(bind_ep);
    shared->acceptor.listen();
    endpoint_ = shared->acceptor.local_endpoint();

    shared->acceptor.async_accept(shared->stream->lowest_layer(),
        asio::bind_executor(shared->strand, [shared](const std::error_code& accept_ec) {
            std::lock_guard<std::mutex> lock(shared->mutex);
            if (accept_ec || shared->stopped) return;
            shared->accepted.store(true);
            shared->stream->async_handshake(asio::ssl::stream_base::server,
                asio::bind_executor(shared->strand, [shared](const std::error_code& hs_ec) {
                    std::lock_guard<std::mutex> lock(shared->mutex);
                    if (!hs_ec && !shared->stopped) shared->handshake_done.store(true);
                }));
        }));
}

tls_loopback_listener::~tls_loopback_listener()
{
    auto shared = state_;
    asio::post(shared->strand, [shared] {
        std::lock_guard<std::mutex> lock(shared->mutex);
        shared->stopped = true;
        std::error_code ec;
        shared->acceptor.close(ec);
        if (shared->stream) shared->stream->lowest_layer().close(ec);
    });
}

auto tls_loopback_listener::accepted() const -> bool
{
    return state_->accepted.load();
}

auto tls_loopback_listener::handshake_done() const -> bool
{
    return state_->handshake_done.load();
}

std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>>
tls_loopback_listener::accepted_socket(std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (state_->handshake_done.load()) return std::move(state_->stream);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return nullptr;
}

} // namespace kcenon::network::tests::support
