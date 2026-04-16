/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/http/websocket_client.h"
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
 * @file websocket_client_test.cpp
 * @brief Unit tests for messaging_ws_client (WebSocket client)
 *
 * Tests validate:
 * - ws_client_config struct default values and custom values
 * - messaging_ws_client construction and initial state
 * - client_id() accessor
 * - is_running() and is_connected() initial state
 * - Callback registration (text, binary, connected, disconnected, error)
 * - stop_client() while not running
 * - send_text() / send_binary() while not connected
 * - close() while not connected
 * - ping() while not connected
 * - Type aliases (ws_client, secure_ws_client)
 */

// ============================================================================
// ws_client_config Tests
// ============================================================================

class WsClientConfigTest : public ::testing::Test
{
};

TEST_F(WsClientConfigTest, DefaultValues)
{
	core::ws_client_config config;
	EXPECT_TRUE(config.host.empty());
	EXPECT_EQ(config.port, 80u);
	EXPECT_EQ(config.path, "/");
	EXPECT_TRUE(config.headers.empty());
	EXPECT_EQ(config.connect_timeout, std::chrono::milliseconds{10000});
	EXPECT_EQ(config.ping_interval, std::chrono::milliseconds{30000});
	EXPECT_TRUE(config.auto_pong);
	EXPECT_FALSE(config.auto_reconnect);
	EXPECT_EQ(config.max_message_size, 10u * 1024 * 1024);
}

TEST_F(WsClientConfigTest, CustomValues)
{
	core::ws_client_config config;
	config.host = "example.com";
	config.port = 443;
	config.path = "/ws";
	config.headers["Authorization"] = "Bearer token";
	config.connect_timeout = std::chrono::milliseconds{5000};
	config.ping_interval = std::chrono::milliseconds{15000};
	config.auto_pong = false;
	config.auto_reconnect = true;
	config.max_message_size = 1024 * 1024;

	EXPECT_EQ(config.host, "example.com");
	EXPECT_EQ(config.port, 443u);
	EXPECT_EQ(config.path, "/ws");
	EXPECT_EQ(config.headers.size(), 1u);
	EXPECT_EQ(config.connect_timeout, std::chrono::milliseconds{5000});
	EXPECT_EQ(config.ping_interval, std::chrono::milliseconds{15000});
	EXPECT_FALSE(config.auto_pong);
	EXPECT_TRUE(config.auto_reconnect);
	EXPECT_EQ(config.max_message_size, 1024u * 1024);
}

// ============================================================================
// messaging_ws_client Construction and State Tests
// ============================================================================

class MessagingWsClientTest : public ::testing::Test
{
protected:
	std::shared_ptr<core::messaging_ws_client> client_
		= std::make_shared<core::messaging_ws_client>("test-ws-client");
};

TEST_F(MessagingWsClientTest, Construction)
{
	EXPECT_NE(client_, nullptr);
}

TEST_F(MessagingWsClientTest, ClientId)
{
	EXPECT_EQ(client_->client_id(), "test-ws-client");
}

TEST_F(MessagingWsClientTest, InitiallyNotRunning)
{
	EXPECT_FALSE(client_->is_running());
}

TEST_F(MessagingWsClientTest, InitiallyNotConnected)
{
	EXPECT_FALSE(client_->is_connected());
}

// ============================================================================
// Callback Registration Tests (interface version)
// ============================================================================

TEST_F(MessagingWsClientTest, SetTextCallback)
{
	bool called = false;
	client_->set_text_callback(
		[&called](const std::string&) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(MessagingWsClientTest, SetBinaryCallback)
{
	bool called = false;
	client_->set_binary_callback(
		[&called](const std::vector<uint8_t>&) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(MessagingWsClientTest, SetConnectedCallback)
{
	bool called = false;
	client_->set_connected_callback(
		[&called]() {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(MessagingWsClientTest, SetDisconnectedCallback)
{
	bool called = false;
	client_->set_disconnected_callback(
		[&called](uint16_t, std::string_view) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(MessagingWsClientTest, SetErrorCallback)
{
	bool called = false;
	client_->set_error_callback(
		[&called](std::error_code) {
			called = true;
		});
	EXPECT_FALSE(called);
}

// ============================================================================
// Error Path Tests (not connected)
// ============================================================================

TEST_F(MessagingWsClientTest, StopWhileNotRunning)
{
	auto result = client_->stop_client();
	// Should not crash; may succeed as no-op or return error
	(void)result;
}

TEST_F(MessagingWsClientTest, SendTextWhileNotConnected)
{
	auto result = client_->send_text(std::string("hello"));
	EXPECT_FALSE(result);
}

TEST_F(MessagingWsClientTest, SendBinaryWhileNotConnected)
{
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	auto result = client_->send_binary(std::move(data));
	EXPECT_FALSE(result);
}

TEST_F(MessagingWsClientTest, CloseWhileNotConnected)
{
	auto result = client_->close(1000, "normal");
	// May succeed or return error; should not crash
	(void)result;
}

TEST_F(MessagingWsClientTest, PingWhileNotConnected)
{
	auto result = client_->ping();
	EXPECT_FALSE(result);
}

TEST_F(MessagingWsClientTest, SendPingWhileNotConnected)
{
	auto result = client_->send_ping();
	EXPECT_FALSE(result);
}

// ============================================================================
// Interface Method Tests
// ============================================================================

TEST_F(MessagingWsClientTest, StartInterfaceWhileAlreadyStopped)
{
	// stop() delegates to stop_client()
	auto result = client_->stop();
	(void)result;
}

// ============================================================================
// Type Alias Tests
// ============================================================================

class WsClientTypeAliasTest : public ::testing::Test
{
};

TEST_F(WsClientTypeAliasTest, WsClientAlias)
{
	// ws_client should be usable as an alias for messaging_ws_client
	auto client = std::make_shared<core::ws_client>("alias-client");
	EXPECT_EQ(client->client_id(), "alias-client");
	EXPECT_FALSE(client->is_running());
}

TEST_F(WsClientTypeAliasTest, SecureWsClientAlias)
{
	// secure_ws_client should also be an alias for messaging_ws_client
	auto client = std::make_shared<core::secure_ws_client>("secure-alias");
	EXPECT_EQ(client->client_id(), "secure-alias");
	EXPECT_FALSE(client->is_connected());
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

class WsClientMultiInstanceTest : public ::testing::Test
{
};

TEST_F(WsClientMultiInstanceTest, DifferentIds)
{
	auto client1 = std::make_shared<core::messaging_ws_client>("ws-1");
	auto client2 = std::make_shared<core::messaging_ws_client>("ws-2");

	EXPECT_EQ(client1->client_id(), "ws-1");
	EXPECT_EQ(client2->client_id(), "ws-2");
	EXPECT_NE(client1->client_id(), client2->client_id());
}

TEST_F(WsClientMultiInstanceTest, IndependentState)
{
	auto client1 = std::make_shared<core::messaging_ws_client>("c1");
	auto client2 = std::make_shared<core::messaging_ws_client>("c2");

	EXPECT_FALSE(client1->is_running());
	EXPECT_FALSE(client2->is_running());
	EXPECT_FALSE(client1->is_connected());
	EXPECT_FALSE(client2->is_connected());
}
