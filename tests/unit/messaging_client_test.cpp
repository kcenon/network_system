/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file messaging_client_test.cpp
 * @brief Unit tests for messaging_client class
 *
 * Tests validate:
 * - Construction with client ID
 * - Initial state (not running, not connected)
 * - Client ID accessor
 * - Callback registration
 * - Error paths (send before connect, empty host)
 * - Stop when not started
 * - Interface compliance (i_protocol_client)
 */

#include "internal/core/messaging_client.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Construction Tests
// ============================================================================

TEST(MessagingClientTest, ConstructWithClientId)
{
	auto client = std::make_shared<messaging_client>("tcp-client-1");

	EXPECT_EQ(client->client_id(), "tcp-client-1");
	EXPECT_FALSE(client->is_running());
	EXPECT_FALSE(client->is_connected());
}

TEST(MessagingClientTest, EmptyClientId)
{
	auto client = std::make_shared<messaging_client>("");

	EXPECT_TRUE(client->client_id().empty());
	EXPECT_FALSE(client->is_running());
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST(MessagingClientTest, SendBeforeConnectFails)
{
	auto client = std::make_shared<messaging_client>("client");

	auto result = client->send_packet(std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_err());
}

TEST(MessagingClientTest, SendViaInterfaceBeforeConnectFails)
{
	auto client = std::make_shared<messaging_client>("client");

	auto result = client->send(std::vector<uint8_t>{0x01});
	EXPECT_TRUE(result.is_err());
}

TEST(MessagingClientTest, StartWithEmptyHostFails)
{
	auto client = std::make_shared<messaging_client>("client");

	auto result = client->start_client("", 8080);
	EXPECT_TRUE(result.is_err());
}

TEST(MessagingClientTest, StopWhenNotStartedSucceeds)
{
	auto client = std::make_shared<messaging_client>("client");

	auto result = client->stop_client();
	EXPECT_TRUE(result.is_ok());
}

TEST(MessagingClientTest, StopViaInterfaceWhenNotStarted)
{
	auto client = std::make_shared<messaging_client>("client");

	auto result = client->stop();
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST(MessagingClientTest, SetReceiveCallback)
{
	auto client = std::make_shared<messaging_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_receive_callback([](const std::vector<uint8_t>&) {}));
}

TEST(MessagingClientTest, SetConnectedCallback)
{
	auto client = std::make_shared<messaging_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_connected_callback([]() {}));
}

TEST(MessagingClientTest, SetDisconnectedCallback)
{
	auto client = std::make_shared<messaging_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_disconnected_callback([]() {}));
}

TEST(MessagingClientTest, SetErrorCallback)
{
	auto client = std::make_shared<messaging_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_error_callback([](std::error_code) {}));
}

TEST(MessagingClientTest, SetNullCallbacks)
{
	auto client = std::make_shared<messaging_client>("client");
	EXPECT_NO_FATAL_FAILURE(client->set_receive_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_connected_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_disconnected_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_error_callback(nullptr));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST(MessagingClientTest, DestructorWhenNotRunning)
{
	{ auto client = std::make_shared<messaging_client>("client"); }
}
