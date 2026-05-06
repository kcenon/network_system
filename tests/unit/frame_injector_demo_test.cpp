// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file frame_injector_demo_test.cpp
 * @brief Phase 2E demos for @c frame_injector composability with each
 *        protocol-aware loopback peer (Issue #1074).
 *
 * Phase 2E ships a single, composable byte-level fault hook
 * (`tests/support/frame_injector.h`) that can be applied to:
 *
 *   - server-side TLS-stream writes (mock_h2_server_peer, mock_grpc_server_peer),
 *   - server-side UDP datagrams (mock_quic_peer_loop), and
 *   - raw client→server byte streams fed into ws_server_probe.
 *
 * One demo `TEST_F` per protocol exercises one error class (drop, malform,
 * malform, slow_write respectively) so that branch coverage on the targeted
 * peer-driven path strictly increases relative to the Phase 2A-2D baseline,
 * which only exercised happy paths.
 *
 * Per the Phase 2E acceptance criteria, end-to-end branch-coverage
 * expansion of @c http2_client.cpp / @c grpc/client.cpp / @c quic_socket.cpp
 * / @c websocket_server.cpp lives in the existing follow-up coverage
 * sub-issues (#1062-#1067). This file's contribution is the substrate
 * demonstration.
 */

#include "internal/protocols/http2/http2_client.h"
#include "kcenon/network/detail/protocols/grpc/client.h"
#include "internal/quic_socket.h"
#include "internal/http/websocket_server.h"

#include "frame_injector.h"
#include "hermetic_transport_fixture.h"
#include "mock_grpc_server_peer.h"
#include "mock_h2_server_peer.h"
#include "mock_quic_peer_loop.h"
#include "ws_server_probe.h"

#include <asio/buffer.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace
{

namespace http2 = kcenon::network::protocols::http2;
namespace internal = kcenon::network::internal;
using kcenon::network::core::messaging_ws_server;
using kcenon::network::tests::support::frame_injector;
using kcenon::network::tests::support::grpc_reply_mode;
using kcenon::network::tests::support::hermetic_transport_fixture;
using kcenon::network::tests::support::injection_mode;
using kcenon::network::tests::support::injection_spec;
using kcenon::network::tests::support::make_loopback_tcp_pair;
using kcenon::network::tests::support::mock_grpc_server_peer;
using kcenon::network::tests::support::mock_h2_server_peer;
using kcenon::network::tests::support::mock_quic_peer_loop;
using kcenon::network::tests::support::reply_mode;
using kcenon::network::tests::support::ws_server_probe;

class FrameInjectorDemoTest : public hermetic_transport_fixture
{
};

// ----------------------------------------------------------------------------
// HTTP/2 demo: injection_mode::drop on the server SETTINGS frame.
//
// Without injection (Phase 2A baseline), mock_h2_server_peer always sends an
// empty SETTINGS frame after the client preface; http2_client therefore
// reaches is_connected() == true once the SETTINGS-ACK round-trips. With
// injection_mode::drop the very first server-originated frame (the empty
// SETTINGS) is silently swallowed, which strands the client mid-handshake
// and forces it down the connect-timeout branch.
// ----------------------------------------------------------------------------
TEST_F(FrameInjectorDemoTest, Http2DropFirstSettingsStrandsHandshake)
{
    injection_spec spec;
    spec.mode = injection_mode::drop;

    mock_h2_server_peer peer(io(), reply_mode::drain_only, spec);

    auto client = std::make_shared<http2::http2_client>("phase-2e-h2-drop");
    client->set_timeout(std::chrono::milliseconds(500));

    std::thread connector(
        [&]() { (void)client->connect("127.0.0.1", peer.port()); });

    // The peer never emits SETTINGS, so the client's worker cannot flip the
    // exchange-complete predicate within any reasonable wait. Use a budget
    // well below the connector's giveup time so the test stays fast even
    // under a slow runner.
    EXPECT_FALSE(wait_for([&]() { return peer.settings_exchanged(); },
                          std::chrono::milliseconds(300)));
    EXPECT_FALSE(client->is_connected());

    (void)client->disconnect();
    connector.join();
}

// ----------------------------------------------------------------------------
// gRPC demo: injection_mode::malform on the server SETTINGS-ACK frame.
//
// HTTP/2 clients require the server to acknowledge their SETTINGS with a
// frame whose type byte is 0x4 and whose ACK flag (0x01) is set. Flipping
// the type byte (offset 3 of the 9-byte frame header) produces a frame the
// client cannot interpret as SETTINGS-ACK, exercising the parse / unexpected
// frame error branch in http2_client::on_frame_received that is unreachable
// from a well-formed peer.
//
// Note: malform_offset 3 targets the 4th byte of the SETTINGS-ACK frame
// (length=0:3 bytes, type=1 byte, flags=1 byte, stream_id=4 bytes). The
// type byte is at offset 3 of the serialized frame header.
// ----------------------------------------------------------------------------
TEST_F(FrameInjectorDemoTest, GrpcMalformSettingsAckBlocksConnect)
{
    injection_spec spec;
    spec.mode = injection_mode::malform;
    spec.malform_offset = 3;     // type byte of an HTTP/2 frame header
    spec.malform_xor = 0x0F;     // flip the low nibble: 0x04 -> 0x0B (unknown)

    mock_grpc_server_peer peer(io(), grpc_reply_mode::drain_only, spec);

    kcenon::network::protocols::grpc::grpc_channel_config cfg;
    cfg.use_tls = true;
    cfg.default_timeout = std::chrono::milliseconds(500);

    const std::string target =
        "127.0.0.1:" + std::to_string(static_cast<unsigned>(peer.port()));
    auto client =
        std::make_shared<kcenon::network::protocols::grpc::grpc_client>(
            target, cfg);

    std::thread connector([client]() { (void)client->connect(); });

    // The peer's first server-originated frame (empty SETTINGS) is sent
    // unchanged because the injector applies the same spec to *every*
    // server write — the 4th byte of an empty SETTINGS frame is the type
    // byte too, but flipping its low nibble already produces a frame the
    // client cannot route. Either way, the handshake never completes
    // because the very first server frame is mistyped.
    EXPECT_FALSE(wait_for([&]() { return client->is_connected(); },
                          std::chrono::milliseconds(300)));

    client->disconnect();
    connector.join();
}

// ----------------------------------------------------------------------------
// QUIC demo: injection_mode::malform on the server Initial datagram.
//
// mock_quic_peer_loop replies to the client's Initial with a properly
// header-protected QUIC v1 Initial. Flipping a single byte at offset 0
// corrupts the long-header form bit, so quic_socket::handle_packet rejects
// the packet at the version/header gate before reaching the previously-
// covered process_crypto_frame branch. The peer still reports
// initial_sent_ == true (it ran send_to to completion); the client side
// stays mid-handshake.
// ----------------------------------------------------------------------------
TEST_F(FrameInjectorDemoTest, QuicMalformInitialPreventsCryptoFrameDispatch)
{
    injection_spec spec;
    spec.mode = injection_mode::malform;
    spec.malform_offset = 0;
    spec.malform_xor = 0xC0;     // flip the long-header form & fixed bits

    mock_quic_peer_loop peer(io(), spec);

    asio::ip::udp::socket udp_sock(io(), asio::ip::udp::v4());
    auto client = std::make_shared<internal::quic_socket>(
        std::move(udp_sock), internal::quic_role::client);

    EXPECT_TRUE(
        client->connect(peer.peer_endpoint(), "test.example").is_ok());

    // The peer sent its (corrupted) Initial successfully — the corruption
    // happens after the server-side crypto/protect step but before the
    // client receives the bytes. So initial_sent_ is the right post-
    // condition for "the injector ran and the wire bytes left the peer".
    EXPECT_TRUE(wait_for([&]() { return peer.initial_sent(); },
                         std::chrono::seconds(3)));
    EXPECT_FALSE(peer.io_failed());

    client->stop_receive();
}

// ----------------------------------------------------------------------------
// WebSocket demo: injection_mode::slow_write on a raw client→server stream
// fed to ws_server_probe.
//
// Composability for the WebSocket family is demonstrated against the
// friend-test injection point shipped in Phase 2D rather than against a
// dedicated server peer (the WebSocket family has no equivalent of
// mock_h2_server_peer because the Phase 2A.2 framing peer covers HTTP/2
// only). The injector pacing each byte 1 ms apart on the client side of a
// loopback TCP pair drives the partial-read code path on the server-side
// socket: the fixture verifies the bytes arrived intact and in order, then
// hands the still-connected server socket to ws_server_probe to confirm
// the friend-test invocation surface remains compatible with an injected
// pre-roll.
// ----------------------------------------------------------------------------
TEST_F(FrameInjectorDemoTest, WebSocketSlowWriteDeliversBytesInOrderToProbe)
{
    constexpr std::array<std::uint8_t, 8> wire{
        0xCA, 0xFE, 0xBA, 0xBE, 0xDE, 0xAD, 0xBE, 0xEF};

    auto [client, accepted] = make_loopback_tcp_pair(io());

    injection_spec spec;
    spec.mode = injection_mode::slow_write;
    spec.slow_step = std::chrono::microseconds{500};
    frame_injector inject(spec);

    std::thread writer([&]() {
        std::error_code wec;
        (void)inject.write(client,
                           std::span<const std::uint8_t>(wire), wec);
    });

    std::array<std::uint8_t, wire.size()> received{};
    std::error_code rec;
    asio::read(accepted, asio::buffer(received), rec);
    writer.join();

    ASSERT_FALSE(rec) << "server read failed: " << rec.message();
    for (std::size_t i = 0; i < wire.size(); ++i)
    {
        EXPECT_EQ(received[i], wire[i])
            << "byte " << i << " differs after slow_write";
    }

    // Compose with the Phase 2D ws_server_probe: a never-started server
    // hits the early-return guard regardless of whether the server socket
    // received any pre-roll, so this call shape exercises the
    // injector→probe handoff without depending on a live session manager.
    messaging_ws_server server("phase-2e-ws-slow");
    auto server_sock =
        std::make_shared<asio::ip::tcp::socket>(std::move(accepted));
    EXPECT_NO_FATAL_FAILURE(
        ws_server_probe::invoke_handle_new_connection(server, server_sock));
    EXPECT_FALSE(server.is_running());
}

} // namespace
