/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file messaging_udp_server_test.cpp
 * @brief Unit tests for messaging_udp_server class
 *
 * Tests validate:
 * - Construction with server ID
 * - Initial state (not running)
 * - Server ID accessor
 * - Callback registration
 * - Stop when not started
 */

#include "internal/core/messaging_udp_server.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Construction Tests
// ============================================================================

TEST(MessagingUdpServerTest, ConstructWithServerId)
{
	auto server = std::make_shared<messaging_udp_server>("udp-server-1");

	EXPECT_EQ(server->server_id(), "udp-server-1");
	EXPECT_FALSE(server->is_running());
}

TEST(MessagingUdpServerTest, EmptyServerId)
{
	auto server = std::make_shared<messaging_udp_server>("");

	EXPECT_TRUE(server->server_id().empty());
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST(MessagingUdpServerTest, StopWhenNotStartedSucceeds)
{
	auto server = std::make_shared<messaging_udp_server>("server");

	auto result = server->stop_server();
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST(MessagingUdpServerTest, SetReceiveCallback)
{
	auto server = std::make_shared<messaging_udp_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_receive_callback(
			[](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {}));
}

TEST(MessagingUdpServerTest, SetErrorCallback)
{
	auto server = std::make_shared<messaging_udp_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_error_callback([](std::error_code) {}));
}

TEST(MessagingUdpServerTest, SetNullCallbacks)
{
	auto server = std::make_shared<messaging_udp_server>("server");
	EXPECT_NO_FATAL_FAILURE(server->set_receive_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_error_callback(nullptr));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST(MessagingUdpServerTest, DestructorWhenNotRunning)
{
	{ auto server = std::make_shared<messaging_udp_server>("server"); }
}
