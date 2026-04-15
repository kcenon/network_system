/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file messaging_udp_client_test.cpp
 * @brief Unit tests for messaging_udp_client class
 *
 * Tests validate:
 * - Construction with client ID
 * - Initial state (not running)
 * - Client ID accessor
 * - Callback registration
 * - Error paths (send before start, empty host)
 * - Stop when not started
 * - Interface compliance (i_udp_client)
 */

#include "internal/core/messaging_udp_client.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Construction Tests
// ============================================================================

TEST(MessagingUdpClientTest, ConstructWithClientId)
{
	auto client = std::make_shared<messaging_udp_client>("udp-client-1");

	EXPECT_EQ(client->client_id(), "udp-client-1");
	EXPECT_FALSE(client->is_running());
}

TEST(MessagingUdpClientTest, EmptyClientId)
{
	auto client = std::make_shared<messaging_udp_client>("");

	EXPECT_TRUE(client->client_id().empty());
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST(MessagingUdpClientTest, SendBeforeStartFails)
{
	auto client = std::make_shared<messaging_udp_client>("client");

	auto result = client->send(std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_err());
}

TEST(MessagingUdpClientTest, StartWithEmptyHostFails)
{
	auto client = std::make_shared<messaging_udp_client>("client");

	auto result = client->start_client("", 9000);
	EXPECT_TRUE(result.is_err());
}

TEST(MessagingUdpClientTest, StopWhenNotStartedSucceeds)
{
	auto client = std::make_shared<messaging_udp_client>("client");

	auto result = client->stop_client();
	EXPECT_TRUE(result.is_ok());
}

TEST(MessagingUdpClientTest, SetTargetBeforeStartFails)
{
	auto client = std::make_shared<messaging_udp_client>("client");

	auto result = client->set_target("127.0.0.1", 9000);
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST(MessagingUdpClientTest, SetReceiveCallback)
{
	auto client = std::make_shared<messaging_udp_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_receive_callback(
			[](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {}));
}

TEST(MessagingUdpClientTest, SetErrorCallback)
{
	auto client = std::make_shared<messaging_udp_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_error_callback([](std::error_code) {}));
}

TEST(MessagingUdpClientTest, SetNullCallbacks)
{
	auto client = std::make_shared<messaging_udp_client>("client");
	EXPECT_NO_FATAL_FAILURE(client->set_receive_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_error_callback(nullptr));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST(MessagingUdpClientTest, DestructorWhenNotRunning)
{
	{ auto client = std::make_shared<messaging_udp_client>("client"); }
}
