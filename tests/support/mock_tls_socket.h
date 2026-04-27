// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file mock_tls_socket.h
 * @brief In-process TLS peer for HTTP/2 + gRPC client/server unit tests
 *        (Issue #1060)
 *
 * Provides:
 *  - @ref generate_self_signed_pem — synthesizes an RSA-2048 self-signed
 *    certificate + private key in memory at runtime via OpenSSL X509 APIs.
 *    No on-disk files, no embedded secrets in source.
 *  - @ref make_self_signed_ssl_context — constructs a server-side
 *    @c asio::ssl::context loaded with a freshly-generated cert/key.
 *  - @ref make_permissive_client_context — constructs a client-side
 *    @c asio::ssl::context that accepts the self-signed cert (verify_none).
 *  - @ref tls_loopback_listener — RAII helper that opens a TCP acceptor on a
 *    kernel-assigned loopback port and asynchronously accepts one TLS
 *    connection. Tests connect their @c http2_client / @c grpc_client to
 *    @c listener.endpoint() and the server-side @c asio::ssl::stream is
 *    delivered via @c accepted_socket().
 *
 * Hermetic: bound to @c 127.0.0.1:0; cert is regenerated per fixture; no DNS,
 * no external network, no on-disk secrets.
 */

#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ssl.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace kcenon::network::tests::support
{

/**
 * @brief PEM-encoded certificate + private key pair generated in memory.
 */
struct self_signed_pem
{
    std::string cert_pem;
    std::string key_pem;
};

/**
 * @brief Generate an RSA-2048 self-signed certificate for @c localhost /
 *        @c 127.0.0.1, valid for 100 years (drift-insensitive).
 *
 * Uses OpenSSL @c X509 / @c EVP_PKEY APIs which are guaranteed available
 * because @c asio::ssl already links OpenSSL transitively.
 *
 * @throws std::runtime_error if any OpenSSL call fails.
 */
[[nodiscard]] self_signed_pem generate_self_signed_pem();

/**
 * @brief Construct a server-side @c asio::ssl::context loaded with a
 *        freshly-generated self-signed cert + key.
 *
 * Configured for TLS 1.2+ with HTTP/2 ALPN ("h2") and HTTP/1.1 fallback so
 * both HTTP/2 and gRPC clients negotiate successfully.
 *
 * @param method @c asio::ssl::context::tlsv12_server (default) or any other
 *        server-side method.
 */
[[nodiscard]] asio::ssl::context make_self_signed_ssl_context(
    asio::ssl::context::method method = asio::ssl::context::tlsv12_server);

/**
 * @brief Construct a client-side @c asio::ssl::context that accepts the
 *        self-signed cert (@c verify_none).
 *
 * Suitable for tests where the client needs to handshake against the embedded
 * server cert without managing a CA bundle.
 */
[[nodiscard]] asio::ssl::context make_permissive_client_context();

/**
 * @brief RAII helper that opens a TCP acceptor on @c 127.0.0.1:0 and accepts
 *        one TLS connection in the background.
 *
 * Typical usage from a test:
 * @code
 * tls_loopback_listener listener(io());
 * client->connect("127.0.0.1", listener.port());
 * auto server_stream = listener.accepted_socket();  // waits for handshake
 * @endcode
 *
 * The listener owns its own @c asio::ssl::context (the server-side cert) and
 * runs the handshake on the supplied @c io_context. The accepted SSL stream
 * is exposed via @ref accepted_socket() once handshake completes.
 */
class tls_loopback_listener
{
public:
    /**
     * @brief Open the acceptor and start accepting one connection.
     * @param io io_context to run accept + handshake on.
     */
    explicit tls_loopback_listener(asio::io_context& io);
    ~tls_loopback_listener();

    tls_loopback_listener(const tls_loopback_listener&) = delete;
    tls_loopback_listener& operator=(const tls_loopback_listener&) = delete;

    /**
     * @brief Port the acceptor is bound to.
     */
    [[nodiscard]] auto port() const -> unsigned short { return endpoint_.port(); }

    /**
     * @brief Endpoint the acceptor is bound to.
     */
    [[nodiscard]] auto endpoint() const -> asio::ip::tcp::endpoint { return endpoint_; }

    /**
     * @brief Block up to @p timeout waiting for a peer to connect AND the TLS
     *        handshake to complete; return the accepted SSL stream on success.
     * @return @c std::unique_ptr to the accepted stream, or @c nullptr on
     *         timeout / handshake error.
     */
    [[nodiscard]] auto accepted_socket(
        std::chrono::milliseconds timeout = std::chrono::seconds(5))
        -> std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>>;

    /**
     * @brief True if a connection has been accepted (handshake state ignored).
     */
    [[nodiscard]] auto accepted() const -> bool { return accepted_.load(); }

    /**
     * @brief True if the TLS handshake completed successfully.
     */
    [[nodiscard]] auto handshake_done() const -> bool { return handshake_done_.load(); }

private:
    asio::ssl::context server_ctx_;
    asio::ip::tcp::acceptor acceptor_;
    asio::ip::tcp::endpoint endpoint_;
    std::unique_ptr<asio::ssl::stream<asio::ip::tcp::socket>> accepted_stream_;
    std::atomic<bool> accepted_{false};
    std::atomic<bool> handshake_done_{false};
};

} // namespace kcenon::network::tests::support
