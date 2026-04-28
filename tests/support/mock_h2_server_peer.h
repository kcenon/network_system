// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file mock_h2_server_peer.h
 * @brief Server-side HTTP/2 framing peer for http2_client unit tests
 *        (Phase 2A of Issue #1074, on top of Issue #1060)
 *
 * Builds on top of @ref tls_loopback_listener (which already accepts one
 * TLS-with-ALPN-h2 connection on @c 127.0.0.1:0) and adds the minimal
 * server-side HTTP/2 framing required to push @c http2_client past the
 * connection-preface / SETTINGS-exchange gate. Once the exchange completes,
 * @c http2_client::is_connected() reports true *and* the post-connect public
 * methods (e.g. @c get_settings, @c set_settings, @c disconnect) reach code
 * paths that previously timed out behind a silent peer.
 *
 * Sequence performed on a dedicated worker thread:
 *  1. Wait for the @ref tls_loopback_listener to deliver the post-handshake
 *     SSL stream (TLS 1.2+/1.3 negotiated with ALPN "h2" by the listener).
 *  2. Read the fixed 24-byte client connection preface
 *     (@c "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n").
 *  3. Send an empty server SETTINGS frame (length=0, type=0x4, flags=0,
 *     stream_id=0).
 *  4. Read the client's SETTINGS frame (9-byte header + payload). The
 *     payload is parsed for type/flags consistency but its values are not
 *     enforced — Phase 2A intentionally accepts any client SETTINGS.
 *  5. Send a SETTINGS-ACK frame (length=0, type=0x4, flags=0x1,
 *     stream_id=0).
 *
 * After the exchange, the worker loops reading any further client frames
 * (e.g. GOAWAY emitted by @c disconnect()) and discards their payloads
 * until the socket closes or the peer is destroyed. Phase 2A does NOT
 * reply with HEADERS+DATA — that is deferred to a Phase 2A.2 follow-up.
 *
 * Hermetic: bound to @c 127.0.0.1:0 via the underlying listener; cert is
 * regenerated per construction; no DNS, no external network, no on-disk
 * secrets. Concurrent test executions never collide because every port is
 * ephemeral.
 */

#include "mock_tls_socket.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <atomic>
#include <thread>

namespace kcenon::network::tests::support
{

/**
 * @brief Server-side HTTP/2 framing peer composed on top of
 *        @ref tls_loopback_listener.
 *
 * Typical usage from a test fixture derived from
 * @ref hermetic_transport_fixture:
 * @code
 * mock_h2_server_peer peer(io());
 *
 * auto client = std::make_shared<http2::http2_client>("test");
 * client->set_timeout(std::chrono::milliseconds(2000));
 *
 * std::thread connector([&]() {
 *     (void)client->connect("127.0.0.1", peer.port());
 * });
 *
 * EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
 *                      std::chrono::seconds(3)));
 * EXPECT_TRUE(client->is_connected());
 *
 * // Now reach previously-unreachable post-connect paths.
 * (void)client->disconnect();
 * connector.join();
 * @endcode
 */
class mock_h2_server_peer
{
public:
    /**
     * @brief Construct the peer, opening the TLS listener and spawning the
     *        worker thread.
     * @param io io_context used by the underlying listener for accept
     *        and TLS handshake.
     */
    explicit mock_h2_server_peer(asio::io_context& io);

    /**
     * @brief Destructor. Signals the worker to stop, closes the listener,
     *        and joins the worker thread.
     */
    ~mock_h2_server_peer();

    mock_h2_server_peer(const mock_h2_server_peer&) = delete;
    mock_h2_server_peer& operator=(const mock_h2_server_peer&) = delete;
    mock_h2_server_peer(mock_h2_server_peer&&) = delete;
    mock_h2_server_peer& operator=(mock_h2_server_peer&&) = delete;

    /**
     * @brief Port the underlying TLS listener is bound to.
     */
    [[nodiscard]] auto port() const -> unsigned short
    {
        return listener_.port();
    }

    /**
     * @brief Endpoint the underlying TLS listener is bound to.
     */
    [[nodiscard]] auto endpoint() const -> asio::ip::tcp::endpoint
    {
        return listener_.endpoint();
    }

    /**
     * @brief True after the worker has read the client preface, sent
     *        the server SETTINGS, read the client SETTINGS, and sent the
     *        SETTINGS-ACK.
     *
     * Tests should poll this via
     * @ref hermetic_transport_fixture::wait_for to synchronize with the
     * mock peer before asserting on the client.
     */
    [[nodiscard]] auto settings_exchanged() const -> bool
    {
        return settings_exchanged_.load();
    }

    /**
     * @brief True if the worker thread exited via an I/O failure (handshake
     *        timeout, preface mismatch, peer closed before SETTINGS, etc.).
     *
     * This is informational; tests that want to fail fast on a setup error
     * can assert that this stays false.
     */
    [[nodiscard]] auto io_failed() const -> bool
    {
        return io_failed_.load();
    }

private:
    /**
     * @brief Worker-thread entry point. Performs the 5-step exchange and
     *        then loops draining frames until the socket closes or @c stop_
     *        is set.
     */
    void run();

    tls_loopback_listener listener_;
    std::atomic<bool> settings_exchanged_{false};
    std::atomic<bool> io_failed_{false};
    std::atomic<bool> stop_{false};
    std::thread worker_;
};

} // namespace kcenon::network::tests::support
