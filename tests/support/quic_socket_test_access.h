// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file quic_socket_test_access.h
 * @brief Friend-accessor for hermetic dispatcher coverage of
 *        src/internal/quic_socket.cpp (Issue #1122).
 *
 * Reuses the existing Phase 2D friend gate @c NETWORK_ENABLE_TEST_INJECTION
 * (defined PUBLIC on @c network_system when @c BUILD_TESTS=ON, see
 * @c cmake/network_system_targets.cmake). Production builds with
 * @c BUILD_TESTS=OFF compile byte-identical because the macro is undefined
 * and the friend declaration / forward declaration in
 * @c src/internal/quic_socket.h are gated by
 * @c #if defined(NETWORK_ENABLE_TEST_INJECTION).
 *
 * Mirrors @c http2_server_test_access.h (Round 3 / Issue #1121). The
 * @c quic_socket dispatcher (process_frame, process_crypto_frame,
 * process_stream_frame, process_ack_frame, process_connection_close_frame,
 * process_handshake_done_frame), the encryption-level resolver
 * (determine_encryption_level), the state transitioner (transition_state),
 * the crypto-data queuer (queue_crypto_data), the pending-packet flusher
 * (send_pending_packets) and the retransmit-timeout handler
 * (on_retransmit_timeout) cannot be reached hermetically from the public
 * API: doing so requires a live UDP peer that speaks the QUIC wire format
 * end-to-end through a real TLS 1.3 handshake. Direct invocation via this
 * access class sidesteps the wire path entirely while still routing
 * through the production dispatcher logic.
 *
 * Round 1 of #1122: covers
 *  - process_frame: every alternative arm of the @c protocols::quic::frame
 *    variant (crypto, stream, ack, connection_close, handshake_done, ping,
 *    padding, plus the implicit fall-through for the 13 other unhandled
 *    variants).
 *  - process_stream_frame: callback-set + callback-empty branches.
 *  - process_connection_close_frame: callback-set + callback-empty
 *    branches, and the resulting state transition to draining.
 *  - process_handshake_done_frame: client + server role branches and the
 *    handshake-already-complete short-circuit.
 *  - process_ack_frame: placeholder no-op invocation for coverage.
 *  - send_pending_packets: closed/idle early-return branch and the
 *    populated-state path that flushes queued data.
 *  - queue_crypto_data: append path with single + multiple + max-size
 *    payloads.
 *  - determine_encryption_level: every long-header packet_type branch
 *    (initial, zero_rtt, handshake, default fall-through) and the
 *    short-header (application) branch.
 *  - transition_state: drives the state machine through every reachable
 *    transition without a peer, exercising the atomic store and ensuring
 *    state() observes the new value.
 */

#include "internal/quic_socket.h"
#include "internal/protocols/quic/frame.h"
#include "internal/protocols/quic/frame_types.h"
#include "internal/protocols/quic/keys.h"
#include "internal/protocols/quic/packet.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

namespace kcenon::network::tests::support
{

/**
 * @brief Test-only friend class exposing private dispatcher entry points
 *        of @ref kcenon::network::internal::quic_socket.
 *
 * Forward-declared in @c tests/support/network_test_friends.h alongside
 * @c http2_client_test_access and @c http2_server_test_access. The
 * production header @c src/internal/quic_socket.h forward-declares this
 * class under @c #if defined(NETWORK_ENABLE_TEST_INJECTION) and grants
 * friendship.
 *
 * Each static thunk is a pure forwarding call to a private member of
 * @c quic_socket. The class itself holds no state.
 */
class quic_socket_test_access
{
public:
    using quic_socket = ::kcenon::network::internal::quic_socket;
    using quic_connection_state =
        ::kcenon::network::internal::quic_connection_state;
    using frame = ::kcenon::network::protocols::quic::frame;
    using crypto_frame = ::kcenon::network::protocols::quic::crypto_frame;
    using stream_frame = ::kcenon::network::protocols::quic::stream_frame;
    using ack_frame = ::kcenon::network::protocols::quic::ack_frame;
    using connection_close_frame =
        ::kcenon::network::protocols::quic::connection_close_frame;
    using packet_header =
        ::kcenon::network::protocols::quic::packet_header;
    using long_header =
        ::kcenon::network::protocols::quic::long_header;
    using short_header =
        ::kcenon::network::protocols::quic::short_header;
    using encryption_level =
        ::kcenon::network::protocols::quic::encryption_level;

    // ------------------------------------------------------------------
    // Dispatcher
    // ------------------------------------------------------------------

    /**
     * @brief Direct invocation of @c quic_socket::process_frame.
     *
     * Bypasses the do_receive / handle_packet read path so frame-dispatch
     * branches can be measured under coverage instrumentation without a
     * live UDP peer. The dispatch arm and per-handler body coverage is
     * collected regardless of socket connectivity state.
     */
    static auto process_frame(quic_socket& s, const frame& f) -> void
    {
        s.process_frame(f);
    }

    /**
     * @brief Direct invocation of @c quic_socket::process_crypto_frame.
     */
    static auto process_crypto_frame(quic_socket& s, const crypto_frame& f)
        -> void
    {
        s.process_crypto_frame(f);
    }

    /**
     * @brief Direct invocation of @c quic_socket::process_stream_frame.
     */
    static auto process_stream_frame(quic_socket& s, const stream_frame& f)
        -> void
    {
        s.process_stream_frame(f);
    }

    /**
     * @brief Direct invocation of @c quic_socket::process_ack_frame.
     */
    static auto process_ack_frame(quic_socket& s, const ack_frame& f) -> void
    {
        s.process_ack_frame(f);
    }

    /**
     * @brief Direct invocation of
     *        @c quic_socket::process_connection_close_frame.
     */
    static auto process_connection_close_frame(
        quic_socket& s, const connection_close_frame& f) -> void
    {
        s.process_connection_close_frame(f);
    }

    /**
     * @brief Direct invocation of
     *        @c quic_socket::process_handshake_done_frame.
     */
    static auto process_handshake_done_frame(quic_socket& s) -> void
    {
        s.process_handshake_done_frame();
    }

    /**
     * @brief Direct invocation of @c quic_socket::send_pending_packets.
     *
     * Used to drive the early-return branch (state == closed/idle) and
     * the populated-state happy-path branch where queued crypto/stream
     * data triggers send_packet().
     */
    static auto send_pending_packets(quic_socket& s) -> void
    {
        s.send_pending_packets();
    }

    /**
     * @brief Direct invocation of @c quic_socket::queue_crypto_data.
     */
    static auto queue_crypto_data(
        quic_socket& s, std::vector<uint8_t> data) -> void
    {
        s.queue_crypto_data(std::move(data));
    }

    /**
     * @brief Direct invocation of
     *        @c quic_socket::determine_encryption_level.
     */
    static auto determine_encryption_level(
        const quic_socket& s, const packet_header& header) noexcept
        -> encryption_level
    {
        return s.determine_encryption_level(header);
    }

    /**
     * @brief Direct invocation of @c quic_socket::transition_state.
     */
    static auto transition_state(
        quic_socket& s, quic_connection_state new_state) -> void
    {
        s.transition_state(new_state);
    }

    /**
     * @brief Direct invocation of @c quic_socket::on_retransmit_timeout.
     */
    static auto on_retransmit_timeout(quic_socket& s) -> void
    {
        s.on_retransmit_timeout();
    }

    // ------------------------------------------------------------------
    // State observers (private member reads)
    // ------------------------------------------------------------------

    /**
     * @brief Read the size of the pending CRYPTO data queue at @p level.
     *
     * Used by queue_crypto_data tests to assert append behavior, and by
     * send_pending_packets tests to verify the queue is drained on the
     * happy path.
     */
    static auto pending_crypto_data_size(
        const quic_socket& s, encryption_level level) noexcept -> std::size_t
    {
        const auto idx = static_cast<std::size_t>(level);
        return s.pending_crypto_data_[idx].size();
    }

    /**
     * @brief Read the size of the pending STREAM data queue for
     *        @p stream_id, returning 0 when the stream is unknown.
     */
    static auto pending_stream_data_size(
        const quic_socket& s, std::uint64_t stream_id) -> std::size_t
    {
        std::lock_guard<std::mutex> lock(s.state_mutex_);
        auto it = s.pending_stream_data_.find(stream_id);
        if (it == s.pending_stream_data_.end()) {
            return 0;
        }
        return it->second.size();
    }

    /**
     * @brief Force the @c handshake_complete_ atomic flag.
     *
     * Used to exercise the @c process_handshake_done_frame
     * "already-complete" short-circuit branch which would otherwise
     * require a peer-driven handshake to reach.
     */
    static auto set_handshake_complete(
        quic_socket& s, bool value) noexcept -> void
    {
        s.handshake_complete_.store(value);
    }

    /**
     * @brief Read the @c handshake_complete_ flag without going through
     *        @c is_handshake_complete() (kept as a separate accessor for
     *        symmetry with @c http2_server_test_access::is_alive).
     */
    static auto handshake_complete(const quic_socket& s) noexcept -> bool
    {
        return s.handshake_complete_.load();
    }

    /**
     * @brief Read the connection state without going through @c state().
     */
    static auto state(const quic_socket& s) noexcept
        -> quic_connection_state
    {
        return s.state_.load();
    }
};

} // namespace kcenon::network::tests::support
