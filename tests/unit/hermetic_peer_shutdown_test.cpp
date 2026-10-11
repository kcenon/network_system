// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#include "hermetic_transport_fixture.h"
#include "mock_h2_server_peer.h"
#include "mock_quic_peer_loop.h"
#include "mock_tls_socket.h"
#include "internal/protocols/quic/packet.h"

#include <asio/read.hpp>
#include <asio/write.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

using namespace kcenon::network::tests::support;
using namespace std::chrono_literals;

namespace
{
using stage = mock_h2_server_peer::stage;
constexpr std::array<std::uint8_t, 9> settings{0, 0, 0, 4, 0, 0, 0, 0, 0};
constexpr std::array<std::uint8_t, 9> ack{0, 0, 0, 4, 1, 0, 0, 0, 0};
constexpr char preface[] = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";

template <typename Peer>
void expect_bounded_destruction(std::unique_ptr<Peer>& peer)
{
    const auto start = std::chrono::steady_clock::now();
    peer.reset();
    EXPECT_LT(std::chrono::steady_clock::now() - start,
              2s * NETWORK_COVERAGE_TIMEOUT_MULTIPLIER);
}

// Synchronous client operations run only on the test thread. CTest's 90-second
// process watchdog also bounds setup failures; no async future/thread can hang
// while a timeout assertion itself is unwinding. The client outlives the peer.
struct raw_tls_client
{
    asio::io_context io;
    asio::ssl::context ctx = make_permissive_client_context();
    asio::ssl::stream<asio::ip::tcp::socket> stream{io, ctx};

    void connect(const mock_h2_server_peer& peer)
    {
        stream.next_layer().connect(peer.endpoint());
        stream.handshake(asio::ssl::stream_base::client);
    }

    void send(asio::const_buffer bytes) { asio::write(stream, bytes); }
    void send_preface() { send(asio::buffer(preface, sizeof(preface) - 1)); }

    void receive_settings(bool expect_ack = false)
    {
        std::array<std::uint8_t, 9> bytes{};
        asio::read(stream, asio::buffer(bytes));
        EXPECT_EQ(bytes, expect_ack ? ack : settings);
    }

    void exchange_settings()
    {
        send_preface();
        receive_settings();
        send(asio::buffer(settings));
        receive_settings(true);
        send(asio::buffer(ack));
    }
};
} // namespace

class HermeticPeerShutdownTest : public hermetic_transport_fixture {};

TEST_F(HermeticPeerShutdownTest, H2StopsWithSilentTlsClientStillConnected)
{
    auto ctx = make_permissive_client_context();
    asio::ssl::stream<asio::ip::tcp::socket> client(io(), ctx);
    auto peer = std::make_unique<mock_h2_server_peer>(io());
    client.next_layer().connect(peer->endpoint());
    client.handshake(asio::ssl::stream_base::client);
    ASSERT_TRUE(wait_for([&] { return peer->current_stage() == stage::preface; }));

    // The handshake has completed, but the client keeps the connection open
    // without sending the HTTP/2 preface. Only peer destruction can stop it.
    expect_bounded_destruction(peer);
    EXPECT_TRUE(client.next_layer().is_open());
}

TEST_F(HermeticPeerShutdownTest, H2StopsWithoutClientOrRunningCallerExecutor)
{
    asio::io_context stopped;
    stopped.stop();
    auto peer = std::make_unique<mock_h2_server_peer>(stopped);
    expect_bounded_destruction(peer);
}

TEST_F(HermeticPeerShutdownTest, H2ReadDeadlineExpiresWithClientStillConnected)
{
    raw_tls_client client;
    auto peer = std::make_unique<mock_h2_server_peer>(io());
    client.connect(*peer);
    ASSERT_TRUE(wait_for([&] { return peer->current_stage() == stage::preface; }));
    ASSERT_TRUE(wait_for([&] { return peer->current_stage() == stage::stopped; },
                        12s * NETWORK_COVERAGE_TIMEOUT_MULTIPLIER));
    EXPECT_TRUE(peer->io_failed());
    expect_bounded_destruction(peer);
    EXPECT_TRUE(client.stream.next_layer().is_open());
}

TEST_F(HermeticPeerShutdownTest, H2StopsDuringTlsHandshake)
{
    asio::ip::tcp::socket client(io());
    auto peer = std::make_unique<mock_h2_server_peer>(io());
    client.connect(peer->endpoint());
    ASSERT_TRUE(wait_for([&] { return peer->current_stage() == stage::handshaking; }));
    // TCP is accepted, but this client never sends a TLS ClientHello.
    io().stop();
    expect_bounded_destruction(peer);
    EXPECT_TRUE(client.is_open());
}

TEST_F(HermeticPeerShutdownTest, H2StopsAsTlsHandshakeCompletes)
{
    raw_tls_client client;
    auto peer = std::make_unique<mock_h2_server_peer>(io());
    client.connect(*peer);
    // Intentionally do not wait for stream transfer to the protocol worker:
    // stop may race the listener's handshake completion/ownership handoff.
    expect_bounded_destruction(peer);
    EXPECT_TRUE(client.stream.next_layer().is_open());
}

enum class stalled_read
{
    partial_preface, settings_header, settings_payload, missing_ack, partial_ack,
    idle_connection, drain_header, drain_payload, unfinished_request, request_payload
};

class H2StalledReadShutdownTest : public HermeticPeerShutdownTest,
                                public ::testing::WithParamInterface<stalled_read> {};

TEST_P(H2StalledReadShutdownTest, StopsBeforeClientDisconnects)
{
    raw_tls_client client;
    const auto scenario = GetParam();
    const bool echo = scenario == stalled_read::unfinished_request ||
                      scenario == stalled_read::request_payload;
    auto peer = std::make_unique<mock_h2_server_peer>(
        io(), echo ? reply_mode::echo_one : reply_mode::drain_only);
    client.connect(*peer);
    stage expected = stage::preface;

    if (scenario == stalled_read::partial_preface)
    {
        client.send(asio::buffer(preface, 12));
    }
    else
    {
        client.send_preface();
        client.receive_settings();
        expected = stage::settings_header;
        if (scenario == stalled_read::settings_header)
        {
            client.send(asio::buffer(settings.data(), 3));
        }
        else if (scenario == stalled_read::settings_payload)
        {
            // Advertise a six-byte SETTINGS payload but send only one byte.
            const std::array<std::uint8_t, 10> partial{0, 0, 6, 4, 0, 0, 0, 0, 0, 0};
            client.send(asio::buffer(partial));
            expected = stage::settings_payload;
        }
        else
        {
            client.send(asio::buffer(settings));
            client.receive_settings(true);
            expected = stage::settings_ack;
            if (scenario == stalled_read::partial_ack)
                client.send(asio::buffer(ack.data(), 3));
            else if (scenario != stalled_read::missing_ack)
            {
                client.send(asio::buffer(ack));
                ASSERT_TRUE(wait_for([&] { return peer->settings_exchanged(); }));
                expected = stage::drain_header;
                if (scenario == stalled_read::drain_header)
                    client.send(asio::buffer(settings.data(), 3));
                else if (scenario == stalled_read::unfinished_request)
                {
                    // Empty HEADERS without END_STREAM: echo_one must wait
                    // for more DATA even though it has identified stream 1.
                    const std::array<std::uint8_t, 9> headers{0, 0, 0, 1, 4, 0, 0, 0, 1};
                    client.send(asio::buffer(headers));
                    ASSERT_TRUE(wait_for([&] { return peer->last_request_stream_id() == 1; }));
                    expected = stage::request_header;
                }
                else if (scenario == stalled_read::drain_payload ||
                         scenario == stalled_read::request_payload)
                {
                    const std::array<std::uint8_t, 10> partial{0, 0, 6, 0, 0, 0, 0, 0, 1, 0};
                    client.send(asio::buffer(partial));
                    expected = echo ? stage::request_payload : stage::drain_payload;
                }
            }
        }
    }
    ASSERT_TRUE(wait_for([&] { return peer->current_stage() == expected; }));
    io().stop(); // Cancellation must not depend on the fixture executor.
    expect_bounded_destruction(peer);
    EXPECT_TRUE(client.stream.next_layer().is_open());
}

INSTANTIATE_TEST_SUITE_P(ProtocolStages, H2StalledReadShutdownTest,
    ::testing::Values(stalled_read::partial_preface, stalled_read::settings_header,
        stalled_read::settings_payload, stalled_read::missing_ack, stalled_read::partial_ack,
        stalled_read::idle_connection, stalled_read::drain_header, stalled_read::drain_payload,
        stalled_read::unfinished_request, stalled_read::request_payload));

TEST_F(HermeticPeerShutdownTest, H2StopsDuringSlowInjectedWrite)
{
    raw_tls_client client;
    injection_spec inject;
    inject.mode = injection_mode::slow_write;
    inject.slow_step = 30s;
    auto peer = std::make_unique<mock_h2_server_peer>(io(), reply_mode::drain_only, inject);
    client.connect(*peer);
    client.send_preface();
    std::uint8_t first_byte{};
    asio::read(client.stream, asio::buffer(&first_byte, 1));
    // Receiving the first byte proves injection began. Do not wait for the
    // 30-second pacing delay or send any subsequent SETTINGS/ACK frames.
    expect_bounded_destruction(peer);
}

class H2InjectedShutdownTest : public HermeticPeerShutdownTest,
                             public ::testing::WithParamInterface<injection_mode> {};

TEST_P(H2InjectedShutdownTest, StopsWaitingForAckAfterFaultySettings)
{
    raw_tls_client client;
    injection_spec inject;
    inject.mode = GetParam();
    inject.truncate_at = 3;
    auto peer = std::make_unique<mock_h2_server_peer>(io(), reply_mode::drain_only, inject);
    client.connect(*peer);
    client.send_preface();
    client.send(asio::buffer(settings));
    ASSERT_TRUE(wait_for([&] { return peer->current_stage() == stage::settings_ack; }));
    EXPECT_FALSE(peer->settings_exchanged());
    expect_bounded_destruction(peer);
}

INSTANTIATE_TEST_SUITE_P(Faults, H2InjectedShutdownTest,
    ::testing::Values(injection_mode::drop, injection_mode::truncate, injection_mode::malform));

TEST_F(HermeticPeerShutdownTest, H2StopsWritingToClientThatDoesNotDrainOutput)
{
    raw_tls_client client;
    std::vector<std::vector<std::uint8_t>> output(1, std::vector<std::uint8_t>(32 * 1024 * 1024));
    auto peer = std::make_unique<mock_h2_server_peer>(
        io(), reply_mode::drain_only, injection_spec{}, std::move(output));
    client.connect(*peer);
    client.stream.next_layer().set_option(asio::socket_base::receive_buffer_size(4096));
    client.exchange_settings();
    std::uint8_t first_byte{};
    asio::read(client.stream, asio::buffer(&first_byte, 1));
    ASSERT_EQ(peer->current_stage(), stage::post_handshake_write);
    expect_bounded_destruction(peer);
    EXPECT_TRUE(client.stream.next_layer().is_open());
}

TEST_F(HermeticPeerShutdownTest, QuicStopsBeforeFirstDatagram)
{
    auto peer = std::make_unique<mock_quic_peer_loop>(io());
    expect_bounded_destruction(peer);
}

TEST_F(HermeticPeerShutdownTest, QuicStopsWhileDrainingWithClientStillOpen)
{
    namespace quic = kcenon::network::protocols::quic;
    asio::ip::udp::socket client(io(), asio::ip::udp::v4());
    auto peer = std::make_unique<mock_quic_peer_loop>(io());
    auto initial = quic::packet_builder::build_initial(
        quic::connection_id::generate(8), quic::connection_id::generate(8), {}, 0);
    // This mock only parses the client's long header to derive reply keys.
    initial.resize(1200, 0);
    client.send_to(asio::buffer(initial), peer->endpoint());
    ASSERT_TRUE(wait_for([&] { return peer->initial_sent(); }));
    EXPECT_FALSE(peer->io_failed());
    io().stop();
    expect_bounded_destruction(peer);
    EXPECT_TRUE(client.is_open());
}
