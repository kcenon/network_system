// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file mock_quic_peer_loop.h
 * @brief Server-side QUIC Initial echo peer for quic_socket unit tests
 *        (Phase 2C of Issue #1074, on top of Issue #1060)
 *
 * Provides a hermetic UDP peer that receives one client Initial datagram,
 * derives QUIC-v1 initial keys from the client's original Destination Connection
 * ID (RFC 9001 Section 5.2), and replies with a server Initial packet carrying
 * a stub crypto_frame (frame type 0x06). The reply is a properly header-protected
 * QUIC packet, which lets quic_socket::handle_packet decrypt it and dispatch to
 * quic_socket::process_crypto_frame — a branch previously unreachable from tests.
 *
 * Sequence performed on a dedicated worker thread:
 *  1. Receive the client's first UDP datagram (the client's Initial).
 *  2. Parse the long header to extract the client's SCID (becomes server's DCID,
 *     RFC 9000 Section 17.2.5) and the original DCID for key derivation.
 *  3. Derive QUIC v1 initial secrets from the client's original DCID
 *     (RFC 9001 Section 5.2) using initial_keys::derive().
 *  4. Build a server Initial long-header using packet_builder::build_initial().
 *  5. Build a plaintext payload: PADDING frame + crypto_frame (type 0x06) with
 *     a small stub TLS record (8 zero bytes). process_crypto_frame fires even
 *     when TLS parse returns an error.
 *  6. Encrypt and header-protect the packet using packet_protection::protect()
 *     and packet_protection::protect_header().
 *  7. Send the reply via the loopback UDP socket; set initial_sent_.
 *  8. Drain remaining datagrams until stop_ is set, then exit.
 *
 * Hermetic: bound to 127.0.0.1:0; no DNS, no external network, no on-disk
 * secrets. Concurrent test executions never collide because every port is
 * ephemeral.
 */

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <atomic>
#include <cstdint>
#include <thread>

namespace kcenon::network::tests::support
{

/**
 * @brief UDP loopback peer that replies to the client's first QUIC Initial
 *        with a valid server Initial carrying a stub crypto_frame.
 *
 * Typical usage from a test fixture derived from hermetic_transport_fixture:
 * @code
 * mock_quic_peer_loop peer(io());
 *
 * auto client = std::make_shared<internal::quic_socket>(
 *     make_loopback_udp_pair_to(io(), peer.peer_endpoint()),
 *     internal::quic_role::client);
 * client->connect(peer.peer_endpoint(), "test.example");
 *
 * EXPECT_TRUE(wait_for([&]{ return peer.initial_sent(); },
 *                      std::chrono::seconds(3)));
 * EXPECT_FALSE(peer.io_failed());
 * @endcode
 */
class mock_quic_peer_loop
{
public:
    /**
     * @brief Construct the peer, binding a loopback UDP socket and spawning
     *        the worker thread.
     * @param io io_context for constructing the UDP socket.
     */
    explicit mock_quic_peer_loop(asio::io_context& io);

    /**
     * @brief Destructor. Sets stop_, closes the socket, and joins the worker.
     */
    ~mock_quic_peer_loop();

    mock_quic_peer_loop(const mock_quic_peer_loop&) = delete;
    mock_quic_peer_loop& operator=(const mock_quic_peer_loop&) = delete;
    mock_quic_peer_loop(mock_quic_peer_loop&&) = delete;
    mock_quic_peer_loop& operator=(mock_quic_peer_loop&&) = delete;

    /**
     * @brief Port the peer socket is bound to.
     */
    [[nodiscard]] auto port() const noexcept -> uint16_t { return port_; }

    /**
     * @brief Local endpoint the peer socket is bound to.
     */
    [[nodiscard]] auto endpoint() const noexcept -> asio::ip::udp::endpoint
    {
        return endpoint_;
    }

    /**
     * @brief The endpoint the client should send datagrams to (same as
     *        endpoint()).
     */
    [[nodiscard]] auto peer_endpoint() const noexcept -> asio::ip::udp::endpoint
    {
        return endpoint_;
    }

    /**
     * @brief True after the worker has sent the server Initial reply.
     *
     * Tests should poll this via hermetic_transport_fixture::wait_for.
     */
    [[nodiscard]] auto initial_sent() const noexcept -> bool
    {
        return initial_sent_.load();
    }

    /**
     * @brief True if the worker thread exited via an I/O or parse failure.
     *
     * Informational; tests that want to fail fast on a setup error can
     * assert this stays false.
     */
    [[nodiscard]] auto io_failed() const noexcept -> bool
    {
        return io_failed_.load();
    }

private:
    /**
     * @brief Worker-thread entry point. Runs the receive-derive-reply loop.
     */
    void run();

    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint endpoint_;
    uint16_t port_{0};

    std::atomic<bool> stop_{false};
    std::atomic<bool> initial_sent_{false};
    std::atomic<bool> io_failed_{false};

    std::thread worker_;
};

} // namespace kcenon::network::tests::support
