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

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_client.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

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
