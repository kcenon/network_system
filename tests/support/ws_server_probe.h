// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file ws_server_probe.h
 * @brief Friend-access probe for private methods of messaging_ws_server
 *        (Issue #1074 Phase 2D + Issue #1124 expansion).
 *
 * The probe is a static-method forwarder: production code declares
 * @c ws_server_probe a friend of @c messaging_ws_server under the
 * @c NETWORK_ENABLE_TEST_INJECTION gate so tests can drive the previously
 * private entry points without standing up a live TCP client and WebSocket
 * upgrade handshake.
 *
 * Issue #1124 expansion adds direct access to the per-message dispatch
 * helpers (@c on_message, @c on_close, @c on_error) and the callback
 * dispatchers (@c invoke_connection_callback,
 * @c invoke_disconnection_callback, @c invoke_message_callback,
 * @c invoke_error_callback), plus @c do_accept so the early-return
 * guards in @c src/http/websocket_server.cpp can be exercised under
 * coverage instrumentation. Mirrors the strategy used by
 * @c quic_server_probe (Issue #1123) and @c http2_server_test_access
 * (Round 3).
 */

#include "internal/http/websocket_server.h"
#include "internal/websocket/websocket_protocol.h"

#include <asio/ip/tcp.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace kcenon::network::tests::support
{

class ws_server_probe
{
public:
    using messaging_ws_server =
        ::kcenon::network::core::messaging_ws_server;
    using ws_connection =
        ::kcenon::network::core::ws_connection;
    using ws_message =
        ::kcenon::network::internal::ws_message;
    using ws_close_code =
        ::kcenon::network::internal::ws_close_code;
    using ws_message_type =
        ::kcenon::network::internal::ws_message_type;

    /**
     * @brief Forward to @c messaging_ws_server::handle_new_connection.
     */
    static auto invoke_handle_new_connection(
        messaging_ws_server& srv,
        std::shared_ptr<asio::ip::tcp::socket> socket) -> void;

    /**
     * @brief Forward to @c messaging_ws_server::on_message.
     *
     * Drives the message dispatcher reachable only on a frame-driven path
     * in the production handler. With a null connection and a locally
     * constructed @c ws_message this exercises the text vs binary
     * dispatch branches in @c invoke_message_callback without standing up
     * a live WebSocket peer.
     */
    static auto invoke_on_message(
        messaging_ws_server& srv,
        std::shared_ptr<ws_connection> conn,
        const ws_message& msg) -> void;

    /**
     * @brief Forward to @c messaging_ws_server::on_close.
     *
     * Drives the close-handler branch reachable only on peer-initiated
     * close in the production path. With an empty connection-id the
     * @c session_mgr_->get_connection() returns nullptr (early-return
     * branch); with a known connection-id it exercises the
     * remove_connection() path.
     */
    static auto invoke_on_close(
        messaging_ws_server& srv,
        const std::string& conn_id,
        ws_close_code code,
        const std::string& reason) -> void;

    /**
     * @brief Forward to @c messaging_ws_server::on_error.
     *
     * Drives the error-handler branch reachable only on transport-level
     * errors during read/write in the production path. Safe to call on
     * a never-started server: the handler logs and invokes the error
     * callback without touching the socket.
     */
    static auto invoke_on_error(
        messaging_ws_server& srv,
        const std::string& conn_id,
        std::error_code ec) -> void;

    /**
     * @brief Forward to @c messaging_ws_server::invoke_connection_callback.
     *
     * Drives the connection callback dispatcher with a null session
     * (exercises the empty-function branch in the callback_manager) and
     * with a populated session pointer (exercises the populated branch).
     */
    static auto invoke_connection_callback_dispatcher(
        messaging_ws_server& srv,
        std::shared_ptr<ws_connection> conn) -> void;

    /**
     * @brief Forward to
     *        @c messaging_ws_server::invoke_disconnection_callback.
     */
    static auto invoke_disconnection_callback_dispatcher(
        messaging_ws_server& srv,
        const std::string& conn_id,
        ws_close_code code,
        const std::string& reason) -> void;

    /**
     * @brief Forward to
     *        @c messaging_ws_server::invoke_message_callback.
     *
     * Drives the message dispatcher to exercise the text vs binary
     * branches in the dispatcher (the legacy @c message_callback always
     * fires; the type-specific branches are guarded by msg.type).
     */
    static auto invoke_message_callback_dispatcher(
        messaging_ws_server& srv,
        std::shared_ptr<ws_connection> conn,
        const ws_message& msg) -> void;

    /**
     * @brief Forward to
     *        @c messaging_ws_server::invoke_error_callback.
     */
    static auto invoke_error_callback_dispatcher(
        messaging_ws_server& srv,
        const std::string& conn_id,
        std::error_code ec) -> void;

    /**
     * @brief Forward to @c messaging_ws_server::do_accept.
     *
     * The early-return branch fires when @c is_running()==false or
     * @c acceptor_==nullptr. Used to cover the not-running guard
     * without binding a TCP acceptor.
     */
    static auto invoke_do_accept(messaging_ws_server& srv) -> void;
};

} // namespace kcenon::network::tests::support
