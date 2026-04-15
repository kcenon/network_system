/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file messaging_ws_client_test.cpp
 * @brief Unit tests for messaging_ws_client class
 *
 * Tests validate:
 * - Construction with client ID
 * - Initial state (not running, not connected)
 * - Client ID accessor
 * - ws_client_config default values
 * - Callback registration
 * - Error paths (send before connect, close before connect)
 * - Stop when not started
 * - Interface compliance (i_websocket_client)
 */

#include "internal/http/websocket_client.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Config Tests
// ============================================================================

TEST(WsClientConfigTest, DefaultValues)
{
	ws_client_config config;

	EXPECT_TRUE(config.host.empty());
	EXPECT_EQ(config.port, 80);
	EXPECT_EQ(config.path, "/");
	EXPECT_TRUE(config.headers.empty());
	EXPECT_EQ(config.connect_timeout, std::chrono::milliseconds{10000});
	EXPECT_EQ(config.ping_interval, std::chrono::milliseconds{30000});
	EXPECT_TRUE(config.auto_pong);
	EXPECT_FALSE(config.auto_reconnect);
	EXPECT_EQ(config.max_message_size, 10u * 1024 * 1024);
}

// ============================================================================
// Construction Tests
// ============================================================================

TEST(MessagingWsClientTest, ConstructWithClientId)
{
	auto client = std::make_shared<messaging_ws_client>("ws-client-1");

	EXPECT_EQ(client->client_id(), "ws-client-1");
	EXPECT_FALSE(client->is_running());
	EXPECT_FALSE(client->is_connected());
}

TEST(MessagingWsClientTest, EmptyClientId)
{
	auto client = std::make_shared<messaging_ws_client>("");

	EXPECT_TRUE(client->client_id().empty());
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST(MessagingWsClientTest, SendTextBeforeConnectFails)
{
	auto client = std::make_shared<messaging_ws_client>("client");

	auto result = client->send_text(std::string("hello"));
	EXPECT_TRUE(result.is_err());
}

TEST(MessagingWsClientTest, SendBinaryBeforeConnectFails)
{
	auto client = std::make_shared<messaging_ws_client>("client");

	auto result = client->send_binary(std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_err());
}

TEST(MessagingWsClientTest, SendPingBeforeConnectFails)
{
	auto client = std::make_shared<messaging_ws_client>("client");

	auto result = client->send_ping();
	EXPECT_TRUE(result.is_err());
}

TEST(MessagingWsClientTest, CloseBeforeConnectFails)
{
	auto client = std::make_shared<messaging_ws_client>("client");

	auto result = client->close();
	EXPECT_TRUE(result.is_err());
}

TEST(MessagingWsClientTest, StopWhenNotStartedSucceeds)
{
	auto client = std::make_shared<messaging_ws_client>("client");

	auto result = client->stop_client();
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST(MessagingWsClientTest, SetTextCallback)
{
	auto client = std::make_shared<messaging_ws_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_text_callback([](const std::string&) {}));
}

TEST(MessagingWsClientTest, SetBinaryCallback)
{
	auto client = std::make_shared<messaging_ws_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_binary_callback([](const std::vector<uint8_t>&) {}));
}

TEST(MessagingWsClientTest, SetConnectedCallback)
{
	auto client = std::make_shared<messaging_ws_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_connected_callback([]() {}));
}

TEST(MessagingWsClientTest, SetErrorCallback)
{
	auto client = std::make_shared<messaging_ws_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_error_callback([](std::error_code) {}));
}

TEST(MessagingWsClientTest, SetNullCallbacks)
{
	auto client = std::make_shared<messaging_ws_client>("client");
	EXPECT_NO_FATAL_FAILURE(client->set_text_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_binary_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_connected_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_error_callback(nullptr));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST(MessagingWsClientTest, DestructorWhenNotRunning)
{
	{ auto client = std::make_shared<messaging_ws_client>("client"); }
}
