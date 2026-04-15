/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file secure_messaging_udp_server_test.cpp
 * @brief Unit tests for secure_messaging_udp_server class (DTLS)
 *
 * Tests validate:
 * - Construction with server ID
 * - Initial state (not running)
 * - Server ID accessor
 * - Certificate configuration with invalid paths
 * - Callback registration
 * - Stop when not started
 */

#include "internal/core/secure_messaging_udp_server.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::core;

// ============================================================================
// Construction Tests
// ============================================================================

TEST(SecureMessagingUdpServerTest, ConstructWithServerId)
{
	auto server = std::make_shared<secure_messaging_udp_server>("dtls-server-1");

	EXPECT_EQ(server->server_id(), "dtls-server-1");
	EXPECT_FALSE(server->is_running());
}

TEST(SecureMessagingUdpServerTest, EmptyServerId)
{
	auto server = std::make_shared<secure_messaging_udp_server>("");

	EXPECT_TRUE(server->server_id().empty());
}

// ============================================================================
// Certificate Configuration Tests
// ============================================================================

TEST(SecureMessagingUdpServerTest, SetCertificateWithInvalidPath)
{
	auto server = std::make_shared<secure_messaging_udp_server>("server");

	auto result = server->set_certificate_chain_file("/nonexistent/cert.pem");
	EXPECT_TRUE(result.is_err());
}

TEST(SecureMessagingUdpServerTest, SetPrivateKeyWithInvalidPath)
{
	auto server = std::make_shared<secure_messaging_udp_server>("server");

	auto result = server->set_private_key_file("/nonexistent/key.pem");
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST(SecureMessagingUdpServerTest, StopWhenNotStartedSucceeds)
{
	auto server = std::make_shared<secure_messaging_udp_server>("server");

	auto result = server->stop_server();
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST(SecureMessagingUdpServerTest, SetReceiveCallback)
{
	auto server = std::make_shared<secure_messaging_udp_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_receive_callback(
			[](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {}));
}

TEST(SecureMessagingUdpServerTest, SetErrorCallback)
{
	auto server = std::make_shared<secure_messaging_udp_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_error_callback([](std::error_code) {}));
}

TEST(SecureMessagingUdpServerTest, SetClientConnectedCallback)
{
	auto server = std::make_shared<secure_messaging_udp_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_client_connected_callback(
			[](const asio::ip::udp::endpoint&) {}));
}

TEST(SecureMessagingUdpServerTest, SetClientDisconnectedCallback)
{
	auto server = std::make_shared<secure_messaging_udp_server>("server");
	EXPECT_NO_FATAL_FAILURE(
		server->set_client_disconnected_callback(
			[](const asio::ip::udp::endpoint&) {}));
}

TEST(SecureMessagingUdpServerTest, SetNullCallbacks)
{
	auto server = std::make_shared<secure_messaging_udp_server>("server");
	EXPECT_NO_FATAL_FAILURE(server->set_receive_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_error_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_client_connected_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(server->set_client_disconnected_callback(nullptr));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST(SecureMessagingUdpServerTest, DestructorWhenNotRunning)
{
	{ auto server = std::make_shared<secure_messaging_udp_server>("server"); }
}
