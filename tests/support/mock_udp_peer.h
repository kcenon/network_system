// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file mock_udp_peer.h
 * @brief In-process UDP peer for QUIC client/server unit tests (Issue #1060)
 *
 * QUIC code in this project (@c quic_socket, @c messaging_quic_server) accepts
 * an @c asio::ip::udp::socket via constructor injection and exposes
 * @c handle_packet(span<const uint8_t>) (or @c handle_packet(data, endpoint)
 * for the server) for byte-level packet ingestion. This header provides:
 *
 *  - @ref make_loopback_udp_pair (re-exported from
 *    @ref hermetic_transport_fixture.h) — a pair of UDP sockets bound on
 *    @c 127.0.0.1:0 and connected to each other.
 *  - @ref mock_udp_peer — light wrapper around one socket of the pair that
 *    sends synthesized packet bytes and receives reply bytes with timeout.
 *  - @ref make_quic_initial_packet_stub — synthesizes a minimal QUIC
 *    long-header Initial packet header byte sequence for tests that drive
 *    @c quic_socket::handle_packet directly. Not RFC-9000 compliant; just
 *    enough first-byte bits for the parser to attempt a frame walk.
 *
 * Hermetic: bound to @c 127.0.0.1:0; no DNS, no external network.
 */

#include "hermetic_transport_fixture.h"

#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

#include <chrono>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace kcenon::network::tests::support
{

/**
 * @brief Wraps one socket from a loopback UDP pair with synchronous helpers.
 *
 * The peer is "synchronous" in the test sense: it does not require the shared
 * io_context to be running, because @c asio::ip::udp::socket supports blocking
 * @c send / @c receive when the io_context is not driving it. Tests that need
 * async semantics should use @c async_send / @c async_receive directly on the
 * underlying socket.
 */
class mock_udp_peer
{
public:
    /**
     * @brief Take ownership of an already-connected UDP socket (one half of a
     *        loopback pair).
     */
    explicit mock_udp_peer(asio::ip::udp::socket socket) noexcept
        : socket_(std::move(socket))
    {
    }

    /**
     * @brief Local endpoint the peer is bound to.
     */
    [[nodiscard]] auto endpoint() const -> asio::ip::udp::endpoint
    {
        return socket_.local_endpoint();
    }

    /**
     * @brief Remote endpoint (the other half of the pair) the peer is
     *        @c connect()ed to.
     */
    [[nodiscard]] auto remote() const -> asio::ip::udp::endpoint
    {
        return socket_.remote_endpoint();
    }

    /**
     * @brief Send @p bytes to the connected peer. Returns bytes sent.
     */
    auto send(std::span<const uint8_t> bytes) -> std::size_t
    {
        return socket_.send(asio::buffer(bytes.data(), bytes.size()));
    }

    /**
     * @brief Receive a single datagram into a buffer of @p max_size bytes.
     * @param timeout maximum time to wait. Returns an empty vector on timeout.
     */
    auto receive(std::size_t max_size = 2048,
                 std::chrono::milliseconds timeout = std::chrono::milliseconds(200))
        -> std::vector<uint8_t>;

    /**
     * @brief Mutable access to the underlying socket (for advanced uses).
     */
    [[nodiscard]] auto socket() -> asio::ip::udp::socket& { return socket_; }

private:
    asio::ip::udp::socket socket_;
};

/**
 * @brief Build a minimal QUIC long-header Initial packet stub.
 *
 * The output starts with a long-header first byte (0xC0 | type bits) followed
 * by a version field, dest/source connection ID lengths, IDs, token length,
 * and a tiny payload. This is intentionally NOT a valid RFC-9000 packet —
 * its only purpose is to give @c quic_socket::handle_packet a sequence of
 * bytes whose first-byte parsing branches can be exercised.
 *
 * Tests that need real QUIC framing should build the packet themselves; this
 * helper is for branch-coverage testing of the parse/dispatch entry point.
 *
 * @param dcid destination connection ID payload (0..20 bytes).
 * @param scid source connection ID payload (0..20 bytes).
 */
[[nodiscard]] std::vector<uint8_t> make_quic_initial_packet_stub(
    std::span<const uint8_t> dcid = {},
    std::span<const uint8_t> scid = {});

} // namespace kcenon::network::tests::support
