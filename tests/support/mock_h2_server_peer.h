// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file mock_h2_server_peer.h
 * @brief Server-side HTTP/2 framing peer for http2_client unit tests
 *        (Phase 2A + Phase 2A.2 of Issue #1074, on top of Issue #1060)
 *
 * Builds on top of @ref tls_loopback_listener (which already accepts one
 * TLS-with-ALPN-h2 connection on @c 127.0.0.1:0) and adds the minimal
 * server-side HTTP/2 framing required to push @c http2_client past the
 * connection-preface / SETTINGS-exchange gate. Once the exchange completes,
 * @c http2_client::is_connected() reports true *and* the post-connect public
 * methods (e.g. @c get_settings, @c set_settings, @c disconnect) reach code
 * paths that previously timed out behind a silent peer.
 *
 * Two operating modes are supported via @ref reply_mode:
 *
 * - @ref reply_mode::drain_only (default, Phase 2A behavior): after the
 *   SETTINGS handshake, the worker loops reading any further client frames
 *   (e.g. HEADERS for a GET, DATA for a POST body, GOAWAY emitted by
 *   @c disconnect()) and discards their payloads. This drives the request
 *   timeout path on @c http2_client because the client's response future
 *   never resolves.
 *
 * - @ref reply_mode::echo_one (Phase 2A.2): after the SETTINGS handshake,
 *   the worker reads exactly one request stream from the client (HEADERS,
 *   plus any DATA frames until END_STREAM is set), then replies on the same
 *   stream with a server HEADERS frame carrying @c :status: 200 followed by
 *   a single DATA frame carrying a small body with END_STREAM set. After
 *   the response is on the wire the worker continues draining client frames
 *   until the socket closes. This drives @c http2_client's
 *   @c handle_headers_frame / @c handle_data_frame / @c on_complete success
 *   paths that @ref reply_mode::drain_only cannot reach.
 *
 * Sequence performed on a dedicated worker thread (both modes):
 *  1. Wait for the @ref tls_loopback_listener to deliver the post-handshake
 *     SSL stream (TLS 1.2+/1.3 negotiated with ALPN "h2" by the listener).
 *  2. Read the fixed 24-byte client connection preface
 *     (@c "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n").
 *  3. Send an empty server SETTINGS frame (length=0, type=0x4, flags=0,
 *     stream_id=0).
 *  4. Read the client's SETTINGS frame (9-byte header + payload). The
 *     payload is parsed for type/flags consistency but its values are not
 *     enforced — the post-connect path coverage we want does not depend on
 *     negotiated settings.
 *  5. Send a SETTINGS-ACK frame (length=0, type=0x4, flags=0x1,
 *     stream_id=0).
 *
 * After step 5, behavior diverges per @ref reply_mode (see above).
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
#include <cstdint>
#include <thread>

namespace kcenon::network::tests::support
{

/**
 * @brief Selects the post-handshake behavior of @ref mock_h2_server_peer.
 */
enum class reply_mode
{
    /// Phase 2A behavior: drain client frames after the SETTINGS exchange,
    /// never reply with HEADERS+DATA. Drives the request-timeout path on
    /// @c http2_client.
    drain_only,

    /// Phase 2A.2 behavior: read exactly one client request stream after the
    /// SETTINGS exchange, then reply on the same stream_id with a HEADERS
    /// frame (`:status: 200`) and a DATA frame (small body, END_STREAM).
    /// Drives the response-success path on @c http2_client.
    echo_one
};

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
 *
 * For tests that need a successful response (Phase 2A.2):
 * @code
 * mock_h2_server_peer peer(io(), reply_mode::echo_one);
 *
 * auto client = std::make_shared<http2::http2_client>("test");
 * std::thread connector([&]() {
 *     (void)client->connect("127.0.0.1", peer.port());
 * });
 * wait_for([&]() { return peer.settings_exchanged(); },
 *          std::chrono::seconds(3));
 *
 * auto response = client->get("/echo", {});
 * EXPECT_TRUE(response.is_ok());
 * EXPECT_TRUE(peer.response_sent());
 *
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
     * @param mode Post-handshake behavior. Defaults to
     *        @ref reply_mode::drain_only for backward compatibility with
     *        Phase 2A timeout-path tests.
     */
    explicit mock_h2_server_peer(asio::io_context& io,
                                 reply_mode mode = reply_mode::drain_only);

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

    /**
     * @brief True after the worker has read a complete client request
     *        (HEADERS + any DATA frames up to END_STREAM) on the
     *        @ref reply_mode::echo_one path.
     *
     * Always false in @ref reply_mode::drain_only because the worker never
     * makes the half-closed-remote transition.
     */
    [[nodiscard]] auto request_received() const -> bool
    {
        return request_received_.load();
    }

    /**
     * @brief True after the worker has written the server HEADERS frame
     *        (status: 200) and a single END_STREAM DATA frame on the
     *        @ref reply_mode::echo_one path.
     *
     * Always false in @ref reply_mode::drain_only.
     */
    [[nodiscard]] auto response_sent() const -> bool
    {
        return response_sent_.load();
    }

    /**
     * @brief Stream id observed in the first client HEADERS frame after
     *        SETTINGS exchange (typically 1 for a fresh client).
     *
     * Zero until @ref request_received() returns true.
     */
    [[nodiscard]] auto last_request_stream_id() const -> std::uint32_t
    {
        return last_request_stream_id_.load();
    }

private:
    /**
     * @brief Worker-thread entry point. Performs the SETTINGS exchange and
     *        then dispatches to the per-mode tail (drain or echo).
     */
    void run();

    tls_loopback_listener listener_;
    reply_mode mode_;
    std::atomic<bool> settings_exchanged_{false};
    std::atomic<bool> io_failed_{false};
    std::atomic<bool> stop_{false};
    std::atomic<bool> request_received_{false};
    std::atomic<bool> response_sent_{false};
    std::atomic<std::uint32_t> last_request_stream_id_{0};
    std::thread worker_;
};

} // namespace kcenon::network::tests::support
