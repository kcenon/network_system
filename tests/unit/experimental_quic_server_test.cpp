/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file experimental_quic_server_test.cpp
 * @brief Unit tests for messaging_quic_server (experimental)
 *
 * Tests validate:
 * - Construction with server ID
 * - Initial state (not running)
 * - Configuration (quic_server_config defaults)
 * - Error paths (operations before start)
 * - Callback registration
 * - Session management (empty when not running)
 * - Interface compliance (i_quic_server)
 */

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_server.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Config Structure Tests
// ============================================================================

class QuicServerConfigTest : public ::testing::Test
{
};

TEST_F(QuicServerConfigTest, DefaultConfigValues)
{
	quic_server_config config;

	EXPECT_TRUE(config.cert_file.empty());
	EXPECT_TRUE(config.key_file.empty());
	EXPECT_FALSE(config.ca_cert_file.has_value());
	EXPECT_FALSE(config.require_client_cert);
	EXPECT_TRUE(config.alpn_protocols.empty());
	EXPECT_EQ(config.max_idle_timeout_ms, 30000u);
	EXPECT_EQ(config.initial_max_data, 1048576u);
	EXPECT_EQ(config.initial_max_stream_data, 65536u);
	EXPECT_EQ(config.initial_max_streams_bidi, 100u);
	EXPECT_EQ(config.initial_max_streams_uni, 100u);
	EXPECT_EQ(config.max_connections, 10000u);
	EXPECT_TRUE(config.enable_retry);
}

// ============================================================================
// Construction Tests
// ============================================================================

class ExperimentalQuicServerTest : public ::testing::Test
{
};

TEST_F(ExperimentalQuicServerTest, ConstructWithServerId)
{
	auto server = std::make_shared<messaging_quic_server>("quic-server-1");

	EXPECT_EQ(server->server_id(), "quic-server-1");
	EXPECT_FALSE(server->is_running());
}

TEST_F(ExperimentalQuicServerTest, EmptyServerId)
{
	auto server = std::make_shared<messaging_quic_server>("");

	EXPECT_TRUE(server->server_id().empty());
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST_F(ExperimentalQuicServerTest, InitialStateNotRunning)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->session_count(), 0u);
	EXPECT_EQ(server->connection_count(), 0u);
}

TEST_F(ExperimentalQuicServerTest, EmptySessionListInitially)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	auto sessions = server->sessions();
	EXPECT_TRUE(sessions.empty());
}

TEST_F(ExperimentalQuicServerTest, GetNonExistentSessionReturnsNull)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	auto session = server->get_session("nonexistent");
	EXPECT_EQ(session, nullptr);
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST_F(ExperimentalQuicServerTest, StopWhenNotRunningFails)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	auto result = server->stop_server();
	EXPECT_TRUE(result.is_err());
}

TEST_F(ExperimentalQuicServerTest, DisconnectNonExistentSessionFails)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	auto result = server->disconnect_session("nonexistent", 0);
	EXPECT_TRUE(result.is_err());
}

TEST_F(ExperimentalQuicServerTest, BroadcastWhenNoSessions)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	auto result = server->broadcast(std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_ok());
}

TEST_F(ExperimentalQuicServerTest, MulticastWhenNoSessions)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	auto result = server->multicast(
		{"session-1", "session-2"},
		std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(ExperimentalQuicServerTest, SetConnectionCallbackDoesNotCrash)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	EXPECT_NO_FATAL_FAILURE(
		server->set_connection_callback(
			[](std::shared_ptr<kcenon::network::session::quic_session>) {}));
}

TEST_F(ExperimentalQuicServerTest, SetDisconnectionCallbackDoesNotCrash)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	EXPECT_NO_FATAL_FAILURE(
		server->set_disconnection_callback(
			[](std::shared_ptr<kcenon::network::session::quic_session>) {}));
}

TEST_F(ExperimentalQuicServerTest, SetErrorCallbackDoesNotCrash)
{
	auto server = std::make_shared<messaging_quic_server>("server");

	EXPECT_NO_FATAL_FAILURE(
		server->set_error_callback([](std::error_code) {}));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(ExperimentalQuicServerTest, DestructorWhenNotRunning)
{
	{
		auto server = std::make_shared<messaging_quic_server>("server");
	}
	// No crash = success
}
