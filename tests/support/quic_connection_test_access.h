// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file quic_connection_test_access.h
 * @brief Friend-accessor for hermetic dispatch coverage of
 *        kcenon::network::protocols::quic::connection (Issue #1145).
 *
 * Reuses the existing Phase 2D friend gate @c NETWORK_ENABLE_TEST_INJECTION
 * (defined PUBLIC on @c network_system when @c BUILD_TESTS=ON, see
 * @c cmake/network_system_targets.cmake). Production builds with
 * @c BUILD_TESTS=OFF compile byte-identical because the macro is undefined
 * and the friend declaration / forward declaration in
 * @c connection.h are gated by @c #if defined(NETWORK_ENABLE_TEST_INJECTION).
 *
 * Rationale (Issue #1145 / Round 7 of #953): the QUIC handshake exceeds
 * @c wait_for(3s) under @c -fprofile-arcs -ftest-coverage on shared CI
 * runners (PR #1111 diagnosis). Branches in the @c handle_frame visit,
 * @c process_frames, @c build_packet, @c generate_ack_frame,
 * @c handle_loss_detection_result, @c queue_frames_for_retransmission,
 * @c generate_probe_packets, and the closing/draining state guards are
 * therefore unreachable through normal public-API tests within a
 * coverage-friendly budget. Direct invocation via this access class
 * sidesteps the handshake while still routing through the production
 * dispatch logic.
 *
 * Mirrors the pattern of @c http2_client_test_access (#1115) and
 * @c quic_socket_test_access (#1122).
 */

#include "internal/protocols/quic/connection.h"
#include "internal/protocols/quic/frame_types.h"
#include "internal/protocols/quic/loss_detector.h"
#include "internal/protocols/quic/keys.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kcenon::network::tests::support
{

namespace quic_priv = ::kcenon::network::protocols::quic;

/**
 * @brief Test-only friend class exposing private dispatch entry points of
 *        @ref kcenon::network::protocols::quic::connection.
 *
 * Forward-declared in @c tests/support/network_test_friends.h. The
 * production header @c connection.h forward-declares this class under
 * @c #if defined(NETWORK_ENABLE_TEST_INJECTION) and grants friendship.
 *
 * Each static thunk is a pure forwarding call to a private member of
 * @c connection. The class itself holds no state.
 */
class quic_connection_test_access
{
public:
    // ------------------------------------------------------------------
    // Direct dispatch helpers (private methods)
    // ------------------------------------------------------------------

    /**
     * @brief Forward to @c connection::handle_frame.
     *
     * Bypasses the handshake gate so the @c std::visit dispatch arms in
     * @c handle_frame can be measured under coverage instrumentation
     * without waiting on a 15 s+ async TLS handshake.
     */
    static auto handle_frame(
        quic_priv::connection& conn,
        const quic_priv::frame& frm,
        quic_priv::encryption_level level)
    {
        return conn.handle_frame(frm, level);
    }

    /**
     * @brief Forward to @c connection::process_frames.
     *
     * Drives the parse-then-dispatch loop with a raw payload. Used to
     * exercise the parse-error branch (malformed payload) and the
     * dispatch loop's early-exit on inner frame-handler failure.
     */
    static auto process_frames(
        quic_priv::connection& conn,
        std::span<const std::uint8_t> payload,
        quic_priv::encryption_level level)
    {
        return conn.process_frames(payload, level);
    }

    /**
     * @brief Forward to @c connection::build_packet.
     *
     * Returns an empty vector when keys are not yet derived (the
     * @c keys_result.is_err() early-return arm), and a populated buffer
     * when invoked after seeding pending CRYPTO / ACK / pending_frames.
     */
    static auto build_packet(
        quic_priv::connection& conn,
        quic_priv::encryption_level level) -> std::vector<std::uint8_t>
    {
        return conn.build_packet(level);
    }

    /**
     * @brief Forward to @c connection::generate_ack_frame.
     *
     * Drives the @c std::nullopt arm (largest_received==0 && !ack_needed)
     * and the populated-frame arm via @c seed_pn_space.
     */
    static auto generate_ack_frame(
        const quic_priv::connection& conn,
        const quic_priv::packet_number_space& space)
        -> std::optional<quic_priv::ack_frame>
    {
        return const_cast<quic_priv::connection&>(conn).generate_ack_frame(space);
    }

    /**
     * @brief Forward to @c connection::handle_loss_detection_result.
     *
     * Drives the three-arm switch on @c loss_detection_event
     * (none / packet_lost / pto_expired) plus the ECN-signal post-switch
     * branches (none / congestion_signal / ecn_failure).
     */
    static auto handle_loss_detection_result(
        quic_priv::connection& conn,
        const quic_priv::loss_detection_result& result) -> void
    {
        conn.handle_loss_detection_result(result);
    }

    /**
     * @brief Forward to @c connection::generate_probe_packets.
     *
     * Drives the encryption-level selection cascade (app > handshake >
     * initial) and the pending_crypto_* short-circuits.
     */
    static auto generate_probe_packets(quic_priv::connection& conn) -> void
    {
        conn.generate_probe_packets();
    }

    /**
     * @brief Forward to @c connection::queue_frames_for_retransmission.
     *
     * Drives the per-frame-type std::visit dispatch in the retransmission
     * helper. Each frame variant lands on a distinct constexpr-if arm.
     */
    static auto queue_frames_for_retransmission(
        quic_priv::connection& conn,
        const quic_priv::sent_packet& lost) -> void
    {
        conn.queue_frames_for_retransmission(lost);
    }

    /**
     * @brief Forward to @c connection::update_state.
     *
     * Used to exercise the @c crypto_.is_handshake_complete() guard
     * without driving a real handshake.
     */
    static auto update_state(quic_priv::connection& conn) -> void
    {
        conn.update_state();
    }

    /**
     * @brief Forward to @c connection::get_pn_space (non-const).
     *
     * Used to seed @c largest_received, @c ack_needed, etc. before
     * driving @c build_packet / @c generate_ack_frame.
     */
    static auto get_pn_space(
        quic_priv::connection& conn,
        quic_priv::encryption_level level)
        -> quic_priv::packet_number_space&
    {
        return conn.get_pn_space(level);
    }

    /**
     * @brief Forward to @c connection::get_pn_space (const).
     */
    static auto get_pn_space(
        const quic_priv::connection& conn,
        quic_priv::encryption_level level)
        -> const quic_priv::packet_number_space&
    {
        return conn.get_pn_space(level);
    }

    // ------------------------------------------------------------------
    // State seeding (private members)
    // ------------------------------------------------------------------

    /**
     * @brief Force the connection state to @c connected without a real
     *        handshake.
     *
     * Combined with @c set_handshake_state(complete), this bypasses the
     * @c state_ != connected guard in @c process_short_header_packet and
     * the @c connection_state::connected guard in
     * @c generate_packets / @c build_packet. Tests must be careful not
     * to trigger code paths that re-read crypto keys; those still require
     * the production crypto state.
     */
    static auto set_state(
        quic_priv::connection& conn,
        quic_priv::connection_state s) -> void
    {
        conn.state_ = s;
    }

    /**
     * @brief Force the handshake state without a real handshake.
     */
    static auto set_handshake_state(
        quic_priv::connection& conn,
        quic_priv::handshake_state s) -> void
    {
        conn.hs_state_ = s;
    }

    /**
     * @brief Seed the close-sent flag and error code so @c build_packet
     *        exercises the CONNECTION_CLOSE branch and @c generate_packets
     *        exercises the close-packet shortcut.
     */
    static auto set_close_sent(
        quic_priv::connection& conn,
        bool sent,
        std::uint64_t error_code,
        std::string reason,
        bool application_close) -> void
    {
        conn.close_sent_ = sent;
        conn.close_error_code_ = error_code;
        conn.close_reason_ = std::move(reason);
        conn.application_close_ = application_close;
    }

    /**
     * @brief Seed the close-received flag (for testing the close-sent &&
     *        !close-received branch in @c generate_packets).
     */
    static auto set_close_received(
        quic_priv::connection& conn, bool received) -> void
    {
        conn.close_received_ = received;
    }

    /**
     * @brief Push a CRYPTO payload onto the pending-initial queue.
     *
     * Drives the @c build_packet CRYPTO-frame branch and the
     * @c queue_frames_for_retransmission CRYPTO retransmit branch.
     */
    static auto push_pending_crypto_initial(
        quic_priv::connection& conn,
        std::vector<std::uint8_t> data) -> void
    {
        conn.pending_crypto_initial_.push_back(std::move(data));
    }

    /**
     * @brief Push a CRYPTO payload onto the pending-handshake queue.
     */
    static auto push_pending_crypto_handshake(
        quic_priv::connection& conn,
        std::vector<std::uint8_t> data) -> void
    {
        conn.pending_crypto_handshake_.push_back(std::move(data));
    }

    /**
     * @brief Push a CRYPTO payload onto the pending-app queue.
     */
    static auto push_pending_crypto_app(
        quic_priv::connection& conn,
        std::vector<std::uint8_t> data) -> void
    {
        conn.pending_crypto_app_.push_back(std::move(data));
    }

    /**
     * @brief Push a frame onto the @c pending_frames_ deque.
     *
     * Drives the @c build_packet "pending frames" branch.
     */
    static auto push_pending_frame(
        quic_priv::connection& conn,
        quic_priv::frame f) -> void
    {
        conn.pending_frames_.push_back(std::move(f));
    }

    /**
     * @brief Read the @c pending_frames_ deque size for assertions.
     */
    static auto pending_frames_size(
        const quic_priv::connection& conn) -> std::size_t
    {
        return conn.pending_frames_.size();
    }

    /**
     * @brief Read the @c pending_crypto_initial_ deque size.
     */
    static auto pending_crypto_initial_size(
        const quic_priv::connection& conn) -> std::size_t
    {
        return conn.pending_crypto_initial_.size();
    }

    /**
     * @brief Read the @c pending_crypto_handshake_ deque size.
     */
    static auto pending_crypto_handshake_size(
        const quic_priv::connection& conn) -> std::size_t
    {
        return conn.pending_crypto_handshake_.size();
    }

    /**
     * @brief Read the @c pending_crypto_app_ deque size.
     */
    static auto pending_crypto_app_size(
        const quic_priv::connection& conn) -> std::size_t
    {
        return conn.pending_crypto_app_.size();
    }

    /**
     * @brief Read the application-close flag.
     */
    static auto application_close(
        const quic_priv::connection& conn) -> bool
    {
        return conn.application_close_;
    }

    /**
     * @brief Read the close-received flag (set when CONNECTION_CLOSE is
     *        received from the peer).
     */
    static auto close_received(
        const quic_priv::connection& conn) -> bool
    {
        return conn.close_received_;
    }

    /**
     * @brief Force-set the idle deadline so @c on_timeout's idle branch
     *        fires deterministically.
     */
    static auto set_idle_deadline(
        quic_priv::connection& conn,
        std::chrono::steady_clock::time_point deadline) -> void
    {
        conn.idle_deadline_ = deadline;
    }

    /**
     * @brief Force-set the drain deadline so @c on_timeout's drain branch
     *        fires deterministically.
     */
    static auto set_drain_deadline(
        quic_priv::connection& conn,
        std::chrono::steady_clock::time_point deadline) -> void
    {
        conn.drain_deadline_ = deadline;
    }

    /**
     * @brief Seed an entry in the @c sent_packets map of a packet number
     *        space so dispatcher tests can observe acknowledged-packet
     *        cleanup in the ACK frame handler.
     */
    static auto seed_sent_packet(
        quic_priv::connection& conn,
        quic_priv::encryption_level level,
        std::uint64_t packet_number) -> void
    {
        auto& space = conn.get_pn_space(level);
        quic_priv::sent_packet_info info;
        info.packet_number = packet_number;
        info.sent_time = std::chrono::steady_clock::now();
        info.level = level;
        space.sent_packets.emplace(packet_number, std::move(info));
    }

    /**
     * @brief Read the size of the @c sent_packets map of a packet number
     *        space (for ACK cleanup assertions).
     */
    static auto sent_packets_size(
        const quic_priv::connection& conn,
        quic_priv::encryption_level level) -> std::size_t
    {
        return conn.get_pn_space(level).sent_packets.size();
    }

    /**
     * @brief Forward to @c connection::to_sent_packet.
     *
     * The helper is otherwise dead code in the production translation unit
     * (no caller in @c connection.cpp). Exposing it through the friend lets
     * the field-by-field copy lines be measured under coverage.
     */
    static auto to_sent_packet(
        const quic_priv::connection& conn,
        const quic_priv::sent_packet_info& info) -> quic_priv::sent_packet
    {
        return conn.to_sent_packet(info);
    }

    /**
     * @brief Build a fully-populated @c sent_packet_info (used together with
     *        @c to_sent_packet to round-trip the copy logic).
     */
    static auto make_sent_packet_info(
        std::uint64_t pn,
        std::size_t bytes,
        bool ack_eliciting,
        bool in_flight,
        quic_priv::encryption_level level) -> quic_priv::sent_packet_info
    {
        quic_priv::sent_packet_info info;
        info.packet_number = pn;
        info.sent_time = std::chrono::steady_clock::now();
        info.sent_bytes = bytes;
        info.ack_eliciting = ack_eliciting;
        info.in_flight = in_flight;
        info.level = level;
        return info;
    }
};

} // namespace kcenon::network::tests::support
