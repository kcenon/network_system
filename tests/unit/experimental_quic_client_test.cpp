/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file experimental_quic_client_test.cpp
 * @brief Unit tests for messaging_quic_client (experimental)
 *
 * Tests validate:
 * - Construction with client ID
 * - Initial state (not running, not connected)
 * - Configuration (quic_client_config defaults)
 * - Error paths (operations before start)
 * - Callback registration
 * - Statistics structure defaults
 * - Interface compliance (i_quic_client)
 */

#ifndef NETWORK_USE_EXPERIMENTAL
#define NETWORK_USE_EXPERIMENTAL
#endif
#include "internal/experimental/quic_client.h"
#include "internal/quic_socket.h"
#include "internal/protocols/quic/varint.h"
#include "quic_client_test_access.h"
#include "quic_socket_test_access.h"
#include "mock_tls_socket.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <array>

using namespace kcenon::network::core;

namespace quic = kcenon::network::protocols::quic;
namespace support = kcenon::network::tests::support;
using SocketAccess = support::quic_socket_test_access;
using ClientAccess = support::quic_client_test_access;
using namespace std::chrono_literals;

// ============================================================================
// Config Structure Tests
// ============================================================================

class QuicClientConfigTest : public ::testing::Test
{
};

TEST_F(QuicClientConfigTest, DefaultConfigValues)
{
	quic_client_config config;

	EXPECT_FALSE(config.ca_cert_file.has_value());
	EXPECT_FALSE(config.client_cert_file.has_value());
	EXPECT_FALSE(config.client_key_file.has_value());
	EXPECT_TRUE(config.verify_server);
	EXPECT_TRUE(config.alpn_protocols.empty());
	EXPECT_EQ(config.max_idle_timeout_ms, 30000u);
	EXPECT_EQ(config.initial_max_data, 1048576u);
	EXPECT_EQ(config.initial_max_stream_data, 65536u);
	EXPECT_EQ(config.initial_max_streams_bidi, 100u);
	EXPECT_EQ(config.initial_max_streams_uni, 100u);
	EXPECT_FALSE(config.enable_early_data);
	EXPECT_FALSE(config.session_ticket.has_value());
	EXPECT_EQ(config.max_early_data_size, 16384u);
}

TEST_F(QuicClientConfigTest, StatsStructDefaultValues)
{
	quic_connection_stats stats;

	EXPECT_EQ(stats.bytes_sent, 0u);
	EXPECT_EQ(stats.bytes_received, 0u);
	EXPECT_EQ(stats.packets_sent, 0u);
}

// ============================================================================
// Construction Tests
// ============================================================================

class ExperimentalQuicClientTest : public ::testing::Test
{
};

TEST_F(ExperimentalQuicClientTest, ConstructWithClientId)
{
	auto client = std::make_shared<messaging_quic_client>("quic-client-1");

	EXPECT_EQ(client->client_id(), "quic-client-1");
	EXPECT_FALSE(client->is_running());
	EXPECT_FALSE(client->is_connected());
}

TEST_F(ExperimentalQuicClientTest, EmptyClientId)
{
	auto client = std::make_shared<messaging_quic_client>("");

	EXPECT_TRUE(client->client_id().empty());
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST_F(ExperimentalQuicClientTest, InitialStateNotRunning)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	EXPECT_FALSE(client->is_running());
	EXPECT_FALSE(client->is_connected());
	EXPECT_FALSE(client->is_handshake_complete());
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST_F(ExperimentalQuicClientTest, SendBeforeStartFails)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto result = client->send_packet(std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_err());
}

TEST_F(ExperimentalQuicClientTest, SendEmptyDataFails)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto result = client->send_packet(std::vector<uint8_t>{});
	EXPECT_TRUE(result.is_err());
}

TEST_F(ExperimentalQuicClientTest, SendStringBeforeStartFails)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto result = client->send_packet(std::string_view{});
	EXPECT_TRUE(result.is_err());
}

TEST_F(ExperimentalQuicClientTest, StartWithEmptyHostFails)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto result = client->start_client("", 443);
	EXPECT_TRUE(result.is_err());
}

TEST_F(ExperimentalQuicClientTest, StopWhenNotRunningSucceeds)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto result = client->stop_client();
	EXPECT_TRUE(result.is_ok());
}

TEST_F(ExperimentalQuicClientTest, CreateStreamBeforeStartFails)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto result = client->create_stream();
	EXPECT_TRUE(result.is_err());
}

TEST_F(ExperimentalQuicClientTest, CreateUnidirectionalStreamBeforeStartFails)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto result = client->create_unidirectional_stream();
	EXPECT_TRUE(result.is_err());
}

TEST_F(ExperimentalQuicClientTest, SendOnStreamBeforeStartFails)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto result = client->send_on_stream(0, std::vector<uint8_t>{0x01});
	EXPECT_TRUE(result.is_err());
}

TEST_F(ExperimentalQuicClientTest, CloseStreamBeforeStartFails)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto result = client->close_stream(0);
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST_F(ExperimentalQuicClientTest, SetAlpnProtocols)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	EXPECT_NO_FATAL_FAILURE(
		client->set_alpn_protocols({"h3", "hq-29"}));
}

TEST_F(ExperimentalQuicClientTest, AlpnProtocolBeforeConnectReturnsNone)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	EXPECT_FALSE(client->alpn_protocol().has_value());
}

TEST_F(ExperimentalQuicClientTest, StatsBeforeConnect)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	auto stats = client->stats();
	EXPECT_EQ(stats.bytes_sent, 0u);
	EXPECT_EQ(stats.bytes_received, 0u);
}

// stats() and alpn_protocol() delegate to the live quic_socket accessors added
// for this change. A freshly constructed socket must report zeroed counters and
// no negotiated ALPN — the same defaults the client surfaces before connecting.
TEST(QuicSocketStatsAccessorsTest, StartAtDefaults)
{
	asio::io_context io;
	asio::ip::udp::socket udp(io, asio::ip::udp::v4());
	auto sock = std::make_shared<kcenon::network::internal::quic_socket>(
		std::move(udp), kcenon::network::internal::quic_role::client);

	EXPECT_EQ(sock->packets_sent(), 0u);
	EXPECT_EQ(sock->packets_received(), 0u);
	EXPECT_EQ(sock->bytes_sent(), 0u);
	EXPECT_EQ(sock->bytes_received(), 0u);
	EXPECT_TRUE(sock->negotiated_alpn().empty());
}

// connect() emits the QUIC Initial packet, so the live send counters that
// stats() reports must advance. Driven directly on the socket with a
// single-threaded io_context for deterministic, leak-free teardown.
TEST(QuicSocketStatsAccessorsTest, ConnectIncrementsSentCounters)
{
	asio::io_context io;
	asio::ip::udp::socket udp(io, asio::ip::udp::v4());
	auto sock = std::make_shared<kcenon::network::internal::quic_socket>(
		std::move(udp), kcenon::network::internal::quic_role::client);

	asio::ip::udp::endpoint server(asio::ip::make_address("127.0.0.1"), 59998);
	auto result = sock->connect(server, "example.com");
	io.run_for(std::chrono::milliseconds(200));
	sock->stop_receive();
	io.run_for(std::chrono::milliseconds(50));

	if (result.is_err())
	{
		GTEST_SKIP() << "QUIC connect unavailable in this environment";
	}
	// The Initial packet counts as at least one sent packet / some bytes.
	EXPECT_GT(sock->packets_sent(), 0u);
	EXPECT_GT(sock->bytes_sent(), 0u);
}

TEST_F(ExperimentalQuicClientTest, EarlyDataNotAcceptedBeforeConnect)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	EXPECT_FALSE(client->is_early_data_accepted());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(ExperimentalQuicClientTest, SetCallbacksDoNotCrash)
{
	auto client = std::make_shared<messaging_quic_client>("client");

	EXPECT_NO_FATAL_FAILURE(
		client->set_stream_receive_callback(
			[](uint64_t, const std::vector<uint8_t>&, bool) {}));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(ExperimentalQuicClientTest, DestructorWhenNotRunning)
{
	{
		auto client = std::make_shared<messaging_quic_client>("client");
	}
	// No crash = success
}

// Drive protocol events at explicit timestamps, then read every result through
// the client. Test hooks never assign RTT, congestion, loss, or ALPN results.
class QuicClientRecoveryTest : public ::testing::Test
{
protected:
    asio::io_context io;
    std::shared_ptr<kcenon::network::internal::quic_socket> socket =
        std::make_shared<kcenon::network::internal::quic_socket>(
            asio::ip::udp::socket(io, asio::ip::udp::v4()),
            kcenon::network::internal::quic_role::client);
    messaging_quic_client client{"recovery"};
    const std::chrono::steady_clock::time_point epoch = std::chrono::steady_clock::now();

    void SetUp() override { ClientAccess::attach(client, socket); }
    void TearDown() override { ClientAccess::attach(client, nullptr); }

    void sent(uint64_t pn, quic::encryption_level level = quic::encryption_level::initial,
              std::chrono::microseconds offset = 0us)
    {
        quic::sent_packet packet;
        packet.packet_number = pn;
        packet.sent_time = epoch + offset;
        packet.sent_bytes = 1200;
        packet.ack_eliciting = true;
        packet.in_flight = true;
        packet.level = level;
        SocketAccess::record_sent_packet(*socket, packet);
    }

    void ack(uint64_t pn, std::chrono::microseconds offset,
             quic::encryption_level level = quic::encryption_level::initial,
             uint64_t delay = 0)
    {
        quic::ack_frame frame;
        frame.largest_acknowledged = pn;
        frame.ack_delay = delay;
        SocketAccess::process_ack(*socket, frame, level, epoch + offset);
    }
};

TEST_F(QuicClientRecoveryTest, AckUpdatesRttAndCongestionThroughClient)
{
    const auto initial = client.stats();
    EXPECT_EQ(initial.smoothed_rtt, 0us);
    EXPECT_EQ(initial.min_rtt, 0us);
    ASSERT_GT(initial.cwnd, 0u);
    sent(0);
    ack(0, 50ms, quic::encryption_level::initial, 999999);
    const auto stats = client.stats();
    EXPECT_EQ(stats.packets_sent, 1u);
    EXPECT_EQ(stats.bytes_sent, 1200u);
    EXPECT_EQ(stats.smoothed_rtt, 50ms);
    EXPECT_EQ(stats.min_rtt, 50ms);
    EXPECT_EQ(stats.cwnd, initial.cwnd + 1200);
    EXPECT_EQ(stats.packets_lost, 0u);
    ack(0, 100ms); // Duplicate ACK must not update RTT or window again.
    EXPECT_EQ(client.stats().smoothed_rtt, stats.smoothed_rtt);
    EXPECT_EQ(client.stats().cwnd, stats.cwnd);
}

TEST_F(QuicClientRecoveryTest, PacketThresholdLossIsCountedOnceAndReducesWindow)
{
    const auto initial_cwnd = client.stats().cwnd;
    for (uint64_t pn = 0; pn != 5; ++pn) sent(pn);
    ack(4, 50ms);
    auto stats = client.stats();
    EXPECT_EQ(stats.packets_lost, 2u); // PN 0 and 1 are three or more behind.
    EXPECT_LT(stats.cwnd, initial_cwnd);
    EXPECT_EQ(stats.smoothed_rtt, 50ms);
    ack(4, 50ms);
    EXPECT_EQ(client.stats().packets_lost, 2u);
    EXPECT_EQ(client.stats().cwnd, stats.cwnd);
    ack(0, 51ms); // A late ACK of a declared-lost packet cannot count it twice.
    EXPECT_EQ(client.stats().packets_lost, 2u);
}

TEST_F(QuicClientRecoveryTest, AckUsesItsPacketNumberSpaceAndDecodesApplicationDelay)
{
    sent(0, quic::encryption_level::initial);
    sent(0, quic::encryption_level::handshake);
    sent(0, quic::encryption_level::zero_rtt);
    ack(0, 10ms, quic::encryption_level::zero_rtt); // ACK forbidden in 0-RTT.
    EXPECT_EQ(client.stats().smoothed_rtt, 0us);
    ack(0, 50ms, quic::encryption_level::application);
    EXPECT_EQ(client.stats().smoothed_rtt, 50ms);
    SocketAccess::process_handshake_done_frame(*socket);
    sent(1, quic::encryption_level::application, 100ms);
    // Encoded 1250 * 2^3 = 10000us; 60ms raw RTT becomes 50ms.
    ack(1, 160ms, quic::encryption_level::application, 1250);
    EXPECT_EQ(client.stats().smoothed_rtt, 50ms);
    EXPECT_EQ(client.stats().min_rtt, 50ms);
    EXPECT_EQ(client.stats().packets_lost, 0u);
    ack(0, 20ms, quic::encryption_level::initial, 999999);
    EXPECT_EQ(client.stats().min_rtt, 20ms);
}

TEST_F(QuicClientRecoveryTest, FreshSocketAndDetachResetClientResults)
{
    sent(0);
    ack(0, 50ms);
    auto fresh = std::make_shared<kcenon::network::internal::quic_socket>(
        asio::ip::udp::socket(io, asio::ip::udp::v4()),
        kcenon::network::internal::quic_role::client);
    ClientAccess::attach(client, fresh);
    EXPECT_EQ(client.stats().packets_sent, 0u);
    EXPECT_EQ(client.stats().packets_lost, 0u);
    EXPECT_EQ(client.stats().smoothed_rtt, 0us);
    EXPECT_FALSE(client.alpn_protocol());
    ClientAccess::attach(client, nullptr);
    EXPECT_EQ(client.stats().cwnd, 0u);
}

// A bounded UDP peer drives the client's real connect/receive/crypto path.
// Its TLS endpoint uses memory BIOs, matching this stack's crypto transport.
class QuicClientLiveAccessorsTest : public ::testing::Test
{
protected:
    asio::io_context io;
    asio::ip::udp::socket peer{io, {asio::ip::make_address("127.0.0.1"), 0}};
    asio::ip::udp::endpoint remote;
    std::shared_ptr<messaging_quic_client> client =
        std::make_shared<messaging_quic_client>("live-accessors");
    std::atomic<bool> reader_stopped{false};
    std::thread reader;

    void SetUp() override { peer.non_blocking(true); }
    void TearDown() override
    {
        stop_reader();
        (void)client->stop_client();
    }

    // Some supported Apple libc++ versions lack jthread/stop_token. Keep
    // ownership in the fixture so fatal assertions still stop and join it.
    void start_reader()
    {
        reader_stopped.store(false);
        reader = std::thread([this] {
            while (!reader_stopped.load()) {
                (void)client->stats();
                (void)client->alpn_protocol();
            }
        });
    }

    void stop_reader()
    {
        reader_stopped.store(true);
        if (reader.joinable()) reader.join();
    }

    template<class Predicate> bool wait(Predicate ready)
    {
        const auto deadline = std::chrono::steady_clock::now() + 3s;
        do {
            if (ready()) return true;
            std::this_thread::sleep_for(1ms);
        } while (std::chrono::steady_clock::now() < deadline);
        return ready();
    }

    std::vector<uint8_t> receive()
    {
        std::array<uint8_t, 65536> buffer;
        size_t size = 0;
        std::error_code ec;
        const bool received = wait([&] {
            size = peer.receive_from(asio::buffer(buffer), remote, 0, ec);
            return !ec || (ec != asio::error::would_block && ec != asio::error::try_again);
        });
        if (!received || ec) return {};
        return {buffer.begin(), buffer.begin() + size};
    }

    void negotiate(bool offer_alpn)
    {
        quic_client_config config;
        config.verify_server = false;
        if (offer_alpn) config.alpn_protocols = {"unused", "h2"};
        ASSERT_TRUE(client->start_client("127.0.0.1", peer.local_endpoint().port(), config).is_ok());
        auto initial = receive();
        ASSERT_FALSE(initial.empty());
        auto parsed = quic::packet_parser::parse_header(initial);
        ASSERT_TRUE(parsed.is_ok());
        const auto header = std::get<quic::long_header>(parsed.value().first);
        const auto pn_offset = parsed.value().second;
        auto keys = quic::initial_keys::derive(header.dest_conn_id);
        ASSERT_TRUE(keys.is_ok());
        auto hp = quic::packet_protection::unprotect_header(keys.value().write,
            std::span(initial.data(), pn_offset + 4), pn_offset,
            std::span(initial.data() + pn_offset + 4, quic::hp_sample_size));
        ASSERT_TRUE(hp.is_ok());
        ASSERT_EQ(initial[pn_offset], 0);
        auto plain = quic::packet_protection::unprotect(keys.value().write,
            initial, pn_offset + hp.value().second, 0);
        ASSERT_TRUE(plain.is_ok());
        auto frames = quic::frame_parser::parse_all(plain.value().second);
        ASSERT_TRUE(frames.is_ok());
        std::vector<uint8_t> hello;
        for (const auto& frame : frames.value()) {
            if (auto crypto = std::get_if<quic::crypto_frame>(&frame)) hello = crypto->data;
        }
        ASSERT_FALSE(hello.empty());

        auto ctx = support::make_self_signed_ssl_context(); // selects h2, our second offer
        std::unique_ptr<SSL, decltype(&SSL_free)> ssl(SSL_new(ctx.native_handle()), SSL_free);
        ASSERT_NE(ssl, nullptr);
        BIO* in = BIO_new(BIO_s_mem());
        BIO* out = BIO_new(BIO_s_mem());
        SSL_set_bio(ssl.get(), in, out);
        SSL_set_accept_state(ssl.get());
        ASSERT_EQ(BIO_write(in, hello.data(), static_cast<int>(hello.size())), hello.size());
        const auto result = SSL_do_handshake(ssl.get());
        ASSERT_TRUE(result == 1 || SSL_get_error(ssl.get(), result) == SSL_ERROR_WANT_READ);
        std::vector<uint8_t> flight(BIO_ctrl_pending(out));
        ASSERT_FALSE(flight.empty());
        ASSERT_EQ(BIO_read(out, flight.data(), static_cast<int>(flight.size())), flight.size());

        quic::crypto_frame crypto{0, std::move(flight)};
        auto payload = quic::frame_builder::build(crypto);
        quic::ack_frame ack;
        auto ack_bytes = quic::frame_builder::build(ack);
        payload.insert(payload.end(), ack_bytes.begin(), ack_bytes.end());
        auto response_header = quic::packet_builder::build_initial(header.src_conn_id,
            header.dest_conn_id, {}, 0);
        auto length = quic::varint::encode(1 + payload.size() + quic::aead_tag_size);
        response_header.insert(response_header.end() - 1, length.begin(), length.end());
        auto response = quic::packet_protection::protect(keys.value().read, response_header, payload, 0);
        ASSERT_TRUE(response.is_ok());
        auto& wire = response.value();
        const auto response_pn = response_header.size() - 1;
        ASSERT_TRUE(quic::packet_protection::protect_header(keys.value().read,
            std::span(wire.data(), response_header.size()), response_pn, 1,
            std::span(wire.data() + response_pn + 4, quic::hp_sample_size)).is_ok());
        ASSERT_EQ(peer.send_to(asio::buffer(wire), remote), wire.size());
        ASSERT_TRUE(wait([&] { return client->is_handshake_complete(); }));
        ASSERT_TRUE(wait([&] { return client->stats().smoothed_rtt > 0us; }));
        EXPECT_EQ(client->alpn_protocol(), offer_alpn ? std::optional<std::string>{"h2"} : std::nullopt);
        const auto stats = client->stats();
        EXPECT_GE(stats.bytes_sent, initial.size());
        EXPECT_EQ(stats.bytes_received, wire.size());
        EXPECT_GE(stats.packets_sent, 1u);
        EXPECT_EQ(stats.packets_received, 1u);
        EXPECT_EQ(stats.packets_lost, 0u);
        EXPECT_GT(stats.min_rtt, 0us);
        EXPECT_GT(stats.cwnd, 12000u);
    }
};

TEST_F(QuicClientLiveAccessorsTest, NegotiatedAlpnAndLiveStatistics)
{
    negotiate(true);
}

TEST_F(QuicClientLiveAccessorsTest, HandshakeWithoutAlpnReturnsNone)
{
    negotiate(false);
}

TEST_F(QuicClientLiveAccessorsTest, ConcurrentReadsDuringStopAndRestart)
{
    negotiate(true);
    ASSERT_FALSE(HasFatalFailure());
    start_reader();
    ASSERT_TRUE(client->stop_client().is_ok());
    stop_reader();
    EXPECT_FALSE(client->alpn_protocol());
    EXPECT_EQ(client->stats().packets_sent, 0u);
    EXPECT_EQ(client->stats().cwnd, 0u);
    // A second handshake must use a new socket and cannot retain the old ALPN.
    negotiate(false);
}

TEST_F(QuicClientRecoveryTest, TimeThresholdLossAndPtoHaveDifferentStatistics)
{
    socket->start_receive();
    sent(0);
    sent(1);
    ack(1, 50ms); // PN 0 is not yet old enough or three packets behind.
    EXPECT_EQ(client.stats().packets_lost, 0u);
    const auto before_loss = client.stats().cwnd;
    SocketAccess::on_retransmit_timeout(*socket, epoch + 57ms);
    EXPECT_EQ(client.stats().packets_lost, 1u);
    EXPECT_LT(client.stats().cwnd, before_loss);
    SocketAccess::on_retransmit_timeout(*socket, epoch + 58ms);
    EXPECT_EQ(client.stats().packets_lost, 1u);

    sent(2, quic::encryption_level::initial, 100ms);
    SocketAccess::on_retransmit_timeout(*socket, epoch + 1s); // PTO, not loss
    EXPECT_EQ(client.stats().packets_lost, 1u);
    socket->stop_receive();
}

TEST_F(QuicClientRecoveryTest, AckRangesIncludePacketZeroAndRejectUnderflow)
{
    for (uint64_t pn = 0; pn != 5; ++pn) sent(pn);
    quic::ack_frame invalid;
    invalid.largest_acknowledged = 4;
    invalid.ranges = {{0, 5}};
    SocketAccess::process_ack(*socket, invalid, quic::encryption_level::initial, epoch + 50ms);
    EXPECT_EQ(client.stats().smoothed_rtt, 0us);
    EXPECT_EQ(client.stats().packets_lost, 0u);

    quic::ack_frame ranges;
    ranges.largest_acknowledged = 4;
    ranges.ranges = {{0, 0}, {0, 2}}; // ACK 4, gap at 3, ACK 2..0.
    SocketAccess::process_ack(*socket, ranges, quic::encryption_level::initial, epoch + 50ms);
    EXPECT_EQ(client.stats().packets_lost, 0u);
    EXPECT_EQ(client.stats().cwnd, 12000u + 4 * 1200u);
    ack(3, 50ms);
    EXPECT_EQ(client.stats().cwnd, 12000u + 5 * 1200u);
}

TEST_F(QuicClientRecoveryTest, FailedUdpSendDoesNotIncreaseSuccessfulCounters)
{
    asio::ip::udp::socket peer(io, {asio::ip::make_address("127.0.0.1"), 0});
    ASSERT_TRUE(socket->connect(peer.local_endpoint(), "localhost").is_ok());
    io.run_for(10ms);
    const auto before = client.stats();
    ASSERT_EQ(before.packets_sent, 1u);
    socket->socket().close();
    ASSERT_TRUE(SocketAccess::send_packet(*socket, quic::encryption_level::initial,
                                         {quic::ping_frame{}}).is_ok());
    io.restart();
    io.run_for(10ms);
    EXPECT_EQ(client.stats().packets_sent, before.packets_sent);
    EXPECT_EQ(client.stats().bytes_sent, before.bytes_sent);
    EXPECT_EQ(client.stats().packets_lost, 0u);
    socket->stop_receive();
}

TEST_F(QuicClientLiveAccessorsTest, StopDuringConnectCannotPublishStaleSocket)
{
    start_reader();
    for (int attempt = 0; attempt < 10; ++attempt) {
        ASSERT_TRUE(client->start_client("127.0.0.1", peer.local_endpoint().port()).is_ok());
        ASSERT_TRUE(client->stop_client().is_ok());
        EXPECT_FALSE(client->alpn_protocol());
        EXPECT_FALSE(client->is_connected());
        EXPECT_EQ(client->stats().cwnd, 0u);
        EXPECT_EQ(client->stats().bytes_sent, 0u);
    }
}
