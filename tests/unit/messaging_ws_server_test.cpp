/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file messaging_ws_server_test.cpp
 * @brief Unit tests for messaging_ws_server class
 *
 * Tests validate:
 * - Construction with server ID
 * - Initial state (not running, zero connections)
 * - Server ID accessor
 * - ws_server_config default values
 * - Callback registration
 * - Connection management when no connections
 * - Broadcast when not running
 * - Stop when not started
 * - Interface compliance (i_websocket_server)
 */

#include "internal/http/websocket_server.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Config Tests
// ============================================================================

TEST(WsServerConfigTest, DefaultValues)
{
	ws_server_config config;

	EXPECT_EQ(config.port, 8080);
	EXPECT_EQ(config.path, "/");
	EXPECT_EQ(config.max_connections, 1000u);
	EXPECT_EQ(config.ping_interval, std::chrono::milliseconds{30000});
	EXPECT_TRUE(config.auto_pong);
	EXPECT_EQ(config.max_message_size, 10u * 1024 * 1024);
}

// ============================================================================
// Construction Tests
// ============================================================================

TEST(MessagingWsServerTest, ConstructWithServerId)
{
	auto server = std::make_shared<messaging_ws_server>("ws-server-1");

	EXPECT_EQ(server->server_id(), "ws-server-1");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->connection_count(), 0u);
}

TEST(MessagingWsServerTest, EmptyServerId)
{
	auto server = std::make_shared<messaging_ws_server>("");

	EXPECT_TRUE(server->server_id().empty());
}

// ============================================================================
// Connection Management Tests
// ============================================================================

TEST(MessagingWsServerTest, GetConnectionWithInvalidId)
{
	auto server = std::make_shared<messaging_ws_server>("server");

	auto conn = server->get_connection("nonexistent");
	EXPECT_EQ(conn, nullptr);
}

TEST(MessagingWsServerTest, GetAllConnectionsInitiallyEmpty)
{
	auto server = std::make_shared<messaging_ws_server>("server");

	auto conns = server->get_all_connections();
	EXPECT_TRUE(conns.empty());
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST(MessagingWsServerTest, StopWhenNotStartedSucceeds)
{
	auto server = std::make_shared<messaging_ws_server>("server");

	auto result = server->stop_server();
	EXPECT_TRUE(result.is_ok());
}

TEST(MessagingWsServerTest, BroadcastTextWhenNotRunning)
{
	auto server = std::make_shared<messaging_ws_server>("server");
	EXPECT_NO_FATAL_FAILURE(server->broadcast_text("hello"));
}

TEST(MessagingWsServerTest, BroadcastBinaryWhenNotRunning)
{
	auto server = std::make_shared<messaging_ws_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->broadcast_binary(std::vector<uint8_t>{0x01, 0x02}));
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST(MessagingWsServerTest, SetConnectionCallback)
{
	auto server = std::make_shared<messaging_ws_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_connection_callback(
			[](std::shared_ptr<ws_connection>) {}));
}

TEST(MessagingWsServerTest, SetTextCallback)
{
	auto server = std::make_shared<messaging_ws_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_text_callback(
			[](std::shared_ptr<ws_connection>, const std::string&) {}));
}

TEST(MessagingWsServerTest, SetBinaryCallback)
{
	auto server = std::make_shared<messaging_ws_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_binary_callback(
			[](std::shared_ptr<ws_connection>, const std::vector<uint8_t>&) {}));
}

TEST(MessagingWsServerTest, SetErrorCallback)
{
	auto server = std::make_shared<messaging_ws_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_error_callback(
			[](const std::string&, std::error_code) {}));
}

TEST(MessagingWsServerTest, SetNullCallbacks)
{
	auto server = std::make_shared<messaging_ws_server>("server");
	EXPECT_NO_FATAL_FAILURE(server->set_connection_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_text_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_binary_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_error_callback(nullptr));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST(MessagingWsServerTest, DestructorWhenNotRunning)
{
	{ auto server = std::make_shared<messaging_ws_server>("server"); }
}
