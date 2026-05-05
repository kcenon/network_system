// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file ws_server_probe_test.cpp
 * @brief Demo test for friend-test injection on messaging_ws_server
 *        (Issue #1074 Phase 2D).
 *
 * Verifies the previously-private @c handle_new_connection entry point is
 * reachable from tests via @c ws_server_probe under the
 * NETWORK_ENABLE_TEST_INJECTION gate. On a never-started server the
 * @c session_mgr_ is nullptr, so the handler hits the early-return guard
 * without touching the inbound socket — safe to drive without an
 * io_context loop or a real WebSocket upgrade handshake.
 */

#include "internal/http/websocket_server.h"

#include "hermetic_transport_fixture.h"
#include "ws_server_probe.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <utility>

using kcenon::network::core::messaging_ws_server;
using kcenon::network::tests::support::hermetic_transport_fixture;
using kcenon::network::tests::support::make_loopback_tcp_pair;
using kcenon::network::tests::support::ws_server_probe;

class WsServerProbeTest : public hermetic_transport_fixture
{
};

TEST_F(WsServerProbeTest, HandleNewConnectionEarlyReturnNoSessionManager)
{
    messaging_ws_server server("ws-probe-no-session");

    // A never-started server has session_mgr_ == nullptr, so handle_new_connection
    // hits the early-return guard before touching the socket. We still pass a real
    // connected loopback socket to keep the call shape realistic; the fixture
    // owns the io_context worker thread that drives async_accept inside
    // make_loopback_tcp_pair.
    auto [client, accepted] = make_loopback_tcp_pair(io());

    auto sock = std::make_shared<asio::ip::tcp::socket>(std::move(accepted));

    EXPECT_NO_FATAL_FAILURE(
        ws_server_probe::invoke_handle_new_connection(server, sock));

    EXPECT_FALSE(server.is_running());
}

TEST_F(WsServerProbeTest, HandleNewConnectionEmptySocketEarlyReturn)
{
    messaging_ws_server server("ws-probe-empty-socket");

    auto sock = std::make_shared<asio::ip::tcp::socket>(io());

    // session_mgr_ is nullptr on a never-started server: the early-return
    // guard fires before the socket is touched, so an unconnected socket is
    // also safe here.
    EXPECT_NO_FATAL_FAILURE(
        ws_server_probe::invoke_handle_new_connection(server, sock));

    EXPECT_FALSE(server.is_running());
}
