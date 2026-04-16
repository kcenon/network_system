/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/http/websocket_server.h"
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace core = kcenon::network::core;
namespace internal = kcenon::network::internal;

/**
 * @file websocket_server_test.cpp
 * @brief Unit tests for messaging_ws_server (WebSocket server)
 *
 * Tests validate:
 * - ws_server_config struct default values and custom values
 * - messaging_ws_server construction and initial state
 * - server_id() accessor
 * - is_running() initial state
 * - connection_count() initial value
 * - Callback registration (connection, disconnection, text, binary, error)
 * - stop_server() while not running
 * - broadcast while not running
 * - get_connection() / get_all_connections() while empty
 * - Type aliases (ws_server, secure_ws_server)
 */

// ============================================================================
// ws_server_config Tests
// ============================================================================

class WsServerConfigTest : public ::testing::Test
{
};

TEST_F(WsServerConfigTest, DefaultValues)
{
	core::ws_server_config config;
	EXPECT_EQ(config.port, 8080u);
	EXPECT_EQ(config.path, "/");
	EXPECT_EQ(config.max_connections, 1000u);
	EXPECT_EQ(config.ping_interval, std::chrono::milliseconds{30000});
	EXPECT_TRUE(config.auto_pong);
	EXPECT_EQ(config.max_message_size, 10u * 1024 * 1024);
}

TEST_F(WsServerConfigTest, CustomValues)
{
	core::ws_server_config config;
	config.port = 9090;
	config.path = "/chat";
	config.max_connections = 500;
	config.ping_interval = std::chrono::milliseconds{15000};
	config.auto_pong = false;
	config.max_message_size = 1024 * 1024;

	EXPECT_EQ(config.port, 9090u);
	EXPECT_EQ(config.path, "/chat");
	EXPECT_EQ(config.max_connections, 500u);
	EXPECT_EQ(config.ping_interval, std::chrono::milliseconds{15000});
	EXPECT_FALSE(config.auto_pong);
	EXPECT_EQ(config.max_message_size, 1024u * 1024);
}

// ============================================================================
// messaging_ws_server Construction and State Tests
// ============================================================================

class MessagingWsServerTest : public ::testing::Test
{
protected:
	std::shared_ptr<core::messaging_ws_server> server_
		= std::make_shared<core::messaging_ws_server>("test-ws-server");
};

TEST_F(MessagingWsServerTest, Construction)
{
	EXPECT_NE(server_, nullptr);
}

TEST_F(MessagingWsServerTest, ServerId)
{
	EXPECT_EQ(server_->server_id(), "test-ws-server");
}

TEST_F(MessagingWsServerTest, InitiallyNotRunning)
{
	EXPECT_FALSE(server_->is_running());
}

TEST_F(MessagingWsServerTest, ConnectionCountInitially)
{
	EXPECT_EQ(server_->connection_count(), 0u);
}

// ============================================================================
// Callback Registration Tests (interface version)
// ============================================================================

TEST_F(MessagingWsServerTest, SetConnectionCallback)
{
	bool called = false;
	server_->set_connection_callback(
		[&called](std::shared_ptr<kcenon::network::interfaces::i_websocket_session>) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(MessagingWsServerTest, SetDisconnectionCallback)
{
	bool called = false;
	server_->set_disconnection_callback(
		[&called](std::string_view, uint16_t, std::string_view) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(MessagingWsServerTest, SetTextCallback)
{
	bool called = false;
	server_->set_text_callback(
		[&called](std::string_view, const std::string&) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(MessagingWsServerTest, SetBinaryCallback)
{
	bool called = false;
	server_->set_binary_callback(
		[&called](std::string_view, const std::vector<uint8_t>&) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(MessagingWsServerTest, SetErrorCallback)
{
	bool called = false;
	server_->set_error_callback(
		[&called](std::string_view, std::error_code) {
			called = true;
		});
	EXPECT_FALSE(called);
}

// ============================================================================
// Error Path Tests (not running)
// ============================================================================

TEST_F(MessagingWsServerTest, StopWhileNotRunning)
{
	auto result = server_->stop_server();
	// Should not crash; may succeed as no-op or return error
	(void)result;
}

TEST_F(MessagingWsServerTest, StopInterfaceWhileNotRunning)
{
	auto result = server_->stop();
	(void)result;
}

TEST_F(MessagingWsServerTest, BroadcastTextWhileNotRunning)
{
	// Broadcasting while server is not running should not crash
	server_->broadcast_text("hello");
}

TEST_F(MessagingWsServerTest, BroadcastBinaryWhileNotRunning)
{
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	server_->broadcast_binary(data);
}

TEST_F(MessagingWsServerTest, GetConnectionNotFound)
{
	auto conn = server_->get_connection("nonexistent");
	EXPECT_EQ(conn, nullptr);
}

TEST_F(MessagingWsServerTest, GetAllConnectionsEmpty)
{
	auto connections = server_->get_all_connections();
	EXPECT_TRUE(connections.empty());
}

// ============================================================================
// Type Alias Tests
// ============================================================================

class WsServerTypeAliasTest : public ::testing::Test
{
};

TEST_F(WsServerTypeAliasTest, WsServerAlias)
{
	auto server = std::make_shared<core::ws_server>("alias-server");
	EXPECT_EQ(server->server_id(), "alias-server");
	EXPECT_FALSE(server->is_running());
}

TEST_F(WsServerTypeAliasTest, SecureWsServerAlias)
{
	auto server = std::make_shared<core::secure_ws_server>("secure-alias");
	EXPECT_EQ(server->server_id(), "secure-alias");
	EXPECT_EQ(server->connection_count(), 0u);
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

class WsServerMultiInstanceTest : public ::testing::Test
{
};

TEST_F(WsServerMultiInstanceTest, DifferentIds)
{
	auto server1 = std::make_shared<core::messaging_ws_server>("ws-server-1");
	auto server2 = std::make_shared<core::messaging_ws_server>("ws-server-2");

	EXPECT_EQ(server1->server_id(), "ws-server-1");
	EXPECT_EQ(server2->server_id(), "ws-server-2");
	EXPECT_NE(server1->server_id(), server2->server_id());
}

TEST_F(WsServerMultiInstanceTest, IndependentState)
{
	auto server1 = std::make_shared<core::messaging_ws_server>("s1");
	auto server2 = std::make_shared<core::messaging_ws_server>("s2");

	EXPECT_FALSE(server1->is_running());
	EXPECT_FALSE(server2->is_running());
	EXPECT_EQ(server1->connection_count(), 0u);
	EXPECT_EQ(server2->connection_count(), 0u);
}
