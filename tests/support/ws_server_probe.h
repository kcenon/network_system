// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file ws_server_probe.h
 * @brief Friend-access probe for messaging_ws_server::handle_new_connection
 *        (Issue #1074 Phase 2D).
 *
 * The probe is a static-method forwarder: production code declares
 * @c ws_server_probe a friend of @c messaging_ws_server under the
 * @c NETWORK_ENABLE_TEST_INJECTION gate so tests can drive the previously
 * private @c handle_new_connection entry point without standing up a live
 * TCP client and WebSocket upgrade handshake.
 */

#include "internal/http/websocket_server.h"

#include <asio/ip/tcp.hpp>

#include <memory>

namespace kcenon::network::tests::support
{

class ws_server_probe
{
public:
    /**
     * @brief Forward to @c messaging_ws_server::handle_new_connection.
     */
    static auto invoke_handle_new_connection(
        kcenon::network::core::messaging_ws_server& srv,
        std::shared_ptr<asio::ip::tcp::socket> socket) -> void;
};

} // namespace kcenon::network::tests::support
