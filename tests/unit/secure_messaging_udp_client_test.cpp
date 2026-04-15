/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file secure_messaging_udp_client_test.cpp
 * @brief Unit tests for secure_messaging_udp_client class (DTLS)
 *
 * Tests validate:
 * - Construction with client ID and verify_cert flag
 * - Initial state (not running, not connected)
 * - Client ID accessor
 * - Callback registration (receive, connected, disconnected, error, udp_receive)
 * - Error paths (send before connect, empty host)
 * - Stop when not started
 */

#include "internal/core/secure_messaging_udp_client.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Construction Tests
// ============================================================================

TEST(SecureMessagingUdpClientTest, ConstructWithDefaultVerify)
{
	auto client = std::make_shared<secure_messaging_udp_client>("dtls-client-1");

	EXPECT_EQ(client->client_id(), "dtls-client-1");
	EXPECT_FALSE(client->is_running());
	EXPECT_FALSE(client->is_connected());
}

TEST(SecureMessagingUdpClientTest, ConstructWithVerifyDisabled)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client", false);

	EXPECT_EQ(client->client_id(), "client");
	EXPECT_FALSE(client->is_running());
}

TEST(SecureMessagingUdpClientTest, EmptyClientId)
{
	auto client = std::make_shared<secure_messaging_udp_client>("");

	EXPECT_TRUE(client->client_id().empty());
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST(SecureMessagingUdpClientTest, SendBeforeConnectFails)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client");

	auto result = client->send_packet(std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_err());
}

TEST(SecureMessagingUdpClientTest, StartWithEmptyHostFails)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client");

	auto result = client->start_client("", 443);
	EXPECT_TRUE(result.is_err());
}

TEST(SecureMessagingUdpClientTest, StopWhenNotStartedSucceeds)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client");

	auto result = client->stop_client();
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST(SecureMessagingUdpClientTest, SetReceiveCallback)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_receive_callback([](const std::vector<uint8_t>&) {}));
}

TEST(SecureMessagingUdpClientTest, SetUdpReceiveCallback)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_udp_receive_callback(
			[](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {}));
}

TEST(SecureMessagingUdpClientTest, SetConnectedCallback)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_connected_callback([]() {}));
}

TEST(SecureMessagingUdpClientTest, SetDisconnectedCallback)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_disconnected_callback([]() {}));
}

TEST(SecureMessagingUdpClientTest, SetErrorCallback)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client");
	EXPECT_NO_FATAL_FAILURE(
		client->set_error_callback([](std::error_code) {}));
}

TEST(SecureMessagingUdpClientTest, SetNullCallbacks)
{
	auto client = std::make_shared<secure_messaging_udp_client>("client");
	EXPECT_NO_FATAL_FAILURE(client->set_receive_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_connected_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_disconnected_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(client->set_error_callback(nullptr));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST(SecureMessagingUdpClientTest, DestructorWhenNotRunning)
{
	{ auto client = std::make_shared<secure_messaging_udp_client>("client"); }
}
