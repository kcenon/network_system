// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file mock_grpc_server_peer.h
 * @brief Server-side gRPC framing peer for grpc_client unit tests
 *        (Phase 2B of Issue #1074, on top of Issue #1060)
 *
 * Builds on top of @ref tls_loopback_listener and adds the minimal
 * server-side HTTP/2 + gRPC framing required to push @c grpc_client past
 * the TCP/TLS connect gate, the HTTP/2 SETTINGS-exchange gate, and
 * (optionally) all the way through a unary RPC. Once the SETTINGS
 * exchange completes, @c grpc_client::is_connected() reports true *and*
 * the post-connect public methods (e.g. @c call_raw, @c disconnect,
 * @c wait_for_connected) reach code paths that previously timed out
 * behind a silent peer.
 *
 * Two operating modes are supported via @ref grpc_reply_mode:
 *
 * - @ref grpc_reply_mode::drain_only (default): after the SETTINGS
 *   handshake, the worker loops reading any further client frames
 *   (HEADERS for the unary call, DATA for the gRPC-framed request,
 *   GOAWAY emitted by @c disconnect()) and discards their payloads.
 *   This drives the request-timeout / connection-error path on
 *   @c grpc_client because the client's response future never resolves.
 *
 * - @ref grpc_reply_mode::echo_unary: after the SETTINGS handshake,
 *   the worker reads exactly one client request stream (HEADERS, plus
 *   any DATA frames until END_STREAM is set), then replies on the same
 *   stream with:
 *     1. A server HEADERS frame carrying @c :status: 200 and
 *        @c content-type: application/grpc (END_HEADERS, no END_STREAM).
 *     2. A DATA frame carrying one length-prefixed gRPC message with a
 *        small body (END_STREAM unset because trailers follow).
 *     3. A trailing HEADERS frame carrying @c grpc-status: 0
 *        (END_HEADERS + END_STREAM).
 *   This drives @c grpc_client::call_raw's full success path, including
 *   the @c grpc_message::parse step and the trailer-based status
 *   extraction.
 *
 * Sequence performed on a dedicated worker thread (both modes):
 *  1. Wait for the @ref tls_loopback_listener to deliver the post-handshake
 *     SSL stream (TLS 1.2+/1.3 negotiated with ALPN "h2" by the listener).
 *  2. Read the fixed 24-byte client connection preface
 *     (@c "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n").
 *  3. Send an empty server SETTINGS frame.
 *  4. Read the client's SETTINGS frame.
 *  5. Send a SETTINGS-ACK frame.
 *
 * After step 5, behavior diverges per @ref grpc_reply_mode (see above).
 *
 * Hermetic: bound to @c 127.0.0.1:0 via the underlying listener; cert is
 * regenerated per construction; no DNS, no external network, no on-disk
 * secrets. Concurrent test executions never collide because every port
 * is ephemeral.
 */

#include "mock_tls_socket.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace kcenon::network::tests::support
{

/**
 * @brief Selects the post-handshake behavior of @ref mock_grpc_server_peer.
 */
enum class grpc_reply_mode
{
    /// Drain client frames after the SETTINGS exchange, never reply with
    /// HEADERS+DATA+trailers. Drives the request-timeout / unavailable
    /// path on @c grpc_client.
    drain_only,

    /// Read exactly one client request stream after the SETTINGS exchange,
    /// then reply on the same stream with response HEADERS
    /// (`:status: 200`, `content-type: application/grpc`), one
    /// length-prefixed DATA frame, and trailers
    /// (`grpc-status: 0`, END_STREAM). Drives the response-success path on
    /// @c grpc_client::call_raw.
    echo_unary
};

/**
 * @brief Server-side gRPC framing peer composed on top of
 *        @ref tls_loopback_listener.
 *
 * Typical usage from a test fixture derived from
 * @ref hermetic_transport_fixture:
 * @code
 * mock_grpc_server_peer peer(io());
 *
 * grpc::grpc_channel_config cfg;
 * cfg.use_tls = false;  // mock peer presents TLS via the listener
 * grpc::grpc_client client("127.0.0.1:" + std::to_string(peer.port()), cfg);
 *
 * std::thread connector([&]() {
 *     (void)client.connect();
 * });
 *
 * EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
 *                      std::chrono::seconds(3)));
 * EXPECT_TRUE(client.is_connected());
 *
 * client.disconnect();
 * connector.join();
 * @endcode
 *
 * For tests that need a successful unary response:
 * @code
 * mock_grpc_server_peer peer(io(), grpc_reply_mode::echo_unary);
 *
 * grpc::grpc_client client(...);
 * std::thread connector([&]() { (void)client.connect(); });
 * wait_for([&]() { return peer.settings_exchanged(); },
 *          std::chrono::seconds(3));
 *
 * auto response = client.call_raw("/svc/Method", {0x01, 0x02},
 *                                 grpc::call_options{});
 * EXPECT_TRUE(response.is_ok());
 * EXPECT_TRUE(peer.request_received());
 * EXPECT_TRUE(peer.response_sent());
 *
 * client.disconnect();
 * connector.join();
 * @endcode
 */
class mock_grpc_server_peer
{
public:
    /**
     * @brief Construct the peer, opening the TLS listener and spawning
     *        the worker thread.
     * @param io io_context used by the underlying listener for accept
     *        and TLS handshake.
     * @param mode Post-handshake behavior. Defaults to
     *        @ref grpc_reply_mode::drain_only for backward compatibility
     *        with timeout-path tests.
     */
    explicit mock_grpc_server_peer(asio::io_context& io,
                                   grpc_reply_mode mode
                                       = grpc_reply_mode::drain_only);

    /**
     * @brief Destructor. Signals the worker to stop, closes the listener,
     *        and joins the worker thread.
     */
    ~mock_grpc_server_peer();

    mock_grpc_server_peer(const mock_grpc_server_peer&) = delete;
    mock_grpc_server_peer& operator=(const mock_grpc_server_peer&) = delete;
    mock_grpc_server_peer(mock_grpc_server_peer&&) = delete;
    mock_grpc_server_peer& operator=(mock_grpc_server_peer&&) = delete;

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
     * @brief True after the worker has read the client preface, sent the
     *        server SETTINGS, read the client SETTINGS, and sent the
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
     * @brief True if the worker thread exited via an I/O failure
     *        (handshake timeout, preface mismatch, peer closed before
     *        SETTINGS, etc.).
     *
     * Informational; tests that want to fail fast on a setup error can
     * assert that this stays false.
     */
    [[nodiscard]] auto io_failed() const -> bool
    {
        return io_failed_.load();
    }

    /**
     * @brief True after the worker has read a complete client request
     *        (HEADERS + any DATA frames up to END_STREAM) on the
     *        @ref grpc_reply_mode::echo_unary path.
     *
     * Always false in @ref grpc_reply_mode::drain_only because the worker
     * never makes the half-closed-remote transition.
     */
    [[nodiscard]] auto request_received() const -> bool
    {
        return request_received_.load();
    }

    /**
     * @brief True after the worker has written the response HEADERS frame
     *        (status 200), a single DATA frame with a length-prefixed
     *        gRPC message, and a trailing HEADERS frame
     *        (`grpc-status: 0`) on the @ref grpc_reply_mode::echo_unary
     *        path.
     *
     * Always false in @ref grpc_reply_mode::drain_only.
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

    /**
     * @brief Bytes accumulated from all DATA frames the client sent on
     *        the request stream, in receive order.
     *
     * Empty until at least one client DATA frame has been read. For a
     * unary RPC this contains the gRPC-framed request body
     * (1-byte compression flag + 4-byte big-endian length + payload).
     * Tests can use this to assert that the client serialized its
     * payload through the gRPC frame layer correctly.
     */
    [[nodiscard]] auto request_body() const -> std::vector<std::uint8_t>;

private:
    /**
     * @brief Worker-thread entry point. Performs the SETTINGS exchange
     *        and dispatches to the per-mode tail (drain or echo).
     */
    void run();

    tls_loopback_listener listener_;
    grpc_reply_mode mode_;
    std::atomic<bool> settings_exchanged_{false};
    std::atomic<bool> io_failed_{false};
    std::atomic<bool> stop_{false};
    std::atomic<bool> request_received_{false};
    std::atomic<bool> response_sent_{false};
    std::atomic<std::uint32_t> last_request_stream_id_{0};
    mutable std::mutex request_body_mutex_;
    std::vector<std::uint8_t> request_body_;
    std::thread worker_;
};

} // namespace kcenon::network::tests::support
