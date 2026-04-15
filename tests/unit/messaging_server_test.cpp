/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file messaging_server_test.cpp
 * @brief Unit tests for messaging_server class
 *
 * Tests validate:
 * - Construction with server ID
 * - Initial state (not running)
 * - Server ID accessor
 * - Callback registration
 * - Stop when not started
 */

#include "internal/core/messaging_server.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Construction Tests
// ============================================================================

TEST(MessagingServerTest, ConstructWithServerId)
{
	auto server = std::make_shared<messaging_server>("tcp-server-1");

	EXPECT_EQ(server->server_id(), "tcp-server-1");
	EXPECT_FALSE(server->is_running());
}

TEST(MessagingServerTest, EmptyServerId)
{
	auto server = std::make_shared<messaging_server>("");

	EXPECT_TRUE(server->server_id().empty());
	EXPECT_FALSE(server->is_running());
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST(MessagingServerTest, StopWhenNotStartedSucceeds)
{
	auto server = std::make_shared<messaging_server>("server");

	auto result = server->stop_server();
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST(MessagingServerTest, SetConnectionCallback)
{
	auto server = std::make_shared<messaging_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_connection_callback(
			[](std::shared_ptr<kcenon::network::session::messaging_session>) {}));
}

TEST(MessagingServerTest, SetDisconnectionCallback)
{
	auto server = std::make_shared<messaging_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_disconnection_callback([](const std::string&) {}));
}

TEST(MessagingServerTest, SetReceiveCallback)
{
	auto server = std::make_shared<messaging_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_receive_callback(
			[](std::shared_ptr<kcenon::network::session::messaging_session>,
			   const std::vector<uint8_t>&) {}));
}

TEST(MessagingServerTest, SetErrorCallback)
{
	auto server = std::make_shared<messaging_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_error_callback(
			[](std::shared_ptr<kcenon::network::session::messaging_session>,
			   std::error_code) {}));
}

TEST(MessagingServerTest, SetNullCallbacks)
{
	auto server = std::make_shared<messaging_server>("server");
	EXPECT_NO_FATAL_FAILURE(server->set_connection_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_disconnection_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_receive_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_error_callback(nullptr));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST(MessagingServerTest, DestructorWhenNotRunning)
{
	{ auto server = std::make_shared<messaging_server>("server"); }
}
