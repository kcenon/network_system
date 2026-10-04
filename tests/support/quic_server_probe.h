// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file quic_server_probe.h
 * @brief Friend-access probe for private methods of messaging_quic_server
 *        (Issue #1074 Phase 2D + Issue #1123 expansion).
 *
 * The probe is a static-method forwarder: production code declares
 * @c quic_server_probe a friend of @c messaging_quic_server under the
 * @c NETWORK_ENABLE_TEST_INJECTION gate so tests can drive previously
 * private entry points without standing up a live UDP peer or completing
 * a TLS-1.3 handshake.
 *
 * Issue #1123 expansion adds direct access to private dispatch helpers
 * (@c generate_session_id, @c on_session_close, @c cleanup_dead_sessions,
 * @c start_receive, @c start_cleanup_timer, @c find_or_create_session, and
 * the @c invoke_* callback dispatchers) so the previously unreachable
 * branches in @c src/experimental/quic_server.cpp can be exercised under
 * coverage instrumentation. Mirrors the strategy used by
 * @c quic_socket_test_access (Issue #1122) and
 * @c http2_server_test_access (Round 3).
 */

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_server.h"

#include <asio/ip/udp.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace kcenon::network::session
{
class quic_session;
} // namespace kcenon::network::session

namespace kcenon::network::protocols::quic
{
class connection_id;
} // namespace kcenon::network::protocols::quic

namespace kcenon::network::tests::support
{

class quic_server_probe
{
public:
    using messaging_quic_server =
        ::kcenon::network::core::messaging_quic_server;
    using quic_session =
        ::kcenon::network::session::quic_session;
    using connection_id =
        ::kcenon::network::protocols::quic::connection_id;

    /**
     * @brief Forward to @c messaging_quic_server::handle_packet.
     */
    static auto invoke_handle_packet(
        messaging_quic_server& srv,
        std::span<const std::uint8_t> bytes,
        const asio::ip::udp::endpoint& from) -> void;

    /**
     * @brief Forward to @c messaging_quic_server::generate_session_id.
     *
     * Generates a session ID from the internal counter and the server_id
     * prefix. Used to assert monotonic counter increment without depending
     * on a peer-driven session creation.
     */
    static auto invoke_generate_session_id(
        messaging_quic_server& srv) -> std::string;

    /**
     * @brief Forward to @c messaging_quic_server::on_session_close.
     *
     * Drives the session-close branch reachable only on peer-driven close
     * in the production path. With an empty session map this triggers the
     * @c sessions_.find()==end branch (silent no-op); with an unknown id
     * it exercises the same branch under a populated lifecycle.
     */
    static auto invoke_on_session_close(
        messaging_quic_server& srv,
        const std::string& session_id) -> void;

    /**
     * @brief Forward to @c messaging_quic_server::cleanup_dead_sessions.
     *
     * Drives the periodic-cleanup helper. With an empty session map the
     * inner for-loop never runs, and the trailing log branch is skipped.
     */
    static auto invoke_cleanup_dead_sessions(
        messaging_quic_server& srv) -> void;

    /**
     * @brief Forward to @c messaging_quic_server::start_receive.
     *
     * The early-return branch fires when @c is_running()==false or
     * @c udp_socket_==nullptr. Used to cover the not-running guard
     * without binding a UDP socket.
     */
    static auto invoke_start_receive(
        messaging_quic_server& srv) -> void;

    /**
     * @brief Forward to @c messaging_quic_server::start_cleanup_timer.
     *
     * Same early-return branch as @c start_receive — fires when the timer
     * is null or the lifecycle has not started.
     */
    static auto invoke_start_cleanup_timer(
        messaging_quic_server& srv) -> void;

    /**
     * @brief Forward to @c messaging_quic_server::invoke_connection_callback.
     *
     * Drives the legacy connection callback dispatcher with a null session
     * (exercises the empty-function branch in the callback_manager) and
     * with a populated session pointer (exercises the populated branch).
     */
    static auto invoke_connection_callback_dispatcher(
        messaging_quic_server& srv,
        std::shared_ptr<quic_session> session) -> void;

    /**
     * @brief Forward to
     *        @c messaging_quic_server::invoke_disconnection_callback.
     */
    static auto invoke_disconnection_callback_dispatcher(
        messaging_quic_server& srv,
        std::shared_ptr<quic_session> session) -> void;

    /**
     * @brief Forward to
     *        @c messaging_quic_server::invoke_receive_callback.
     */
    static auto invoke_receive_callback_dispatcher(
        messaging_quic_server& srv,
        std::shared_ptr<quic_session> session,
        const std::vector<std::uint8_t>& data) -> void;

    /**
     * @brief Forward to
     *        @c messaging_quic_server::invoke_stream_receive_callback.
     */
    static auto invoke_stream_receive_callback_dispatcher(
        messaging_quic_server& srv,
        std::shared_ptr<quic_session> session,
        std::uint64_t stream_id,
        const std::vector<std::uint8_t>& data,
        bool fin) -> void;

    /**
     * @brief Forward to
     *        @c messaging_quic_server::invoke_error_callback.
     */
    static auto invoke_error_callback_dispatcher(
        messaging_quic_server& srv,
        std::error_code ec) -> void;
};

} // namespace kcenon::network::tests::support
