/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/core/secure_messaging_udp_server.h"
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace core = kcenon::network::core;

/**
 * @file secure_messaging_udp_server_test.cpp
 * @brief Unit tests for secure_messaging_udp_server (DTLS server)
 *
 * Tests validate:
 * - Construction with server_id
 * - server_id() accessor
 * - is_running() initial state
 * - Callback registration (receive, error, client_connected, client_disconnected)
 * - set_certificate_chain_file() / set_private_key_file() with invalid paths
 * - stop_server() while not running
 * - start_server() without certificates
 */

// ============================================================================
// Construction Tests
// ============================================================================

class SecureMessagingUdpServerTest : public ::testing::Test
{
protected:
	std::shared_ptr<core::secure_messaging_udp_server> server_
		= std::make_shared<core::secure_messaging_udp_server>("test-dtls-server");
};

TEST_F(SecureMessagingUdpServerTest, Construction)
{
	EXPECT_NE(server_, nullptr);
}

TEST_F(SecureMessagingUdpServerTest, ServerId)
{
	EXPECT_EQ(server_->server_id(), "test-dtls-server");
}

TEST_F(SecureMessagingUdpServerTest, InitiallyNotRunning)
{
	EXPECT_FALSE(server_->is_running());
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST_F(SecureMessagingUdpServerTest, SetReceiveCallback)
{
	bool called = false;
	server_->set_receive_callback(
		[&called](const std::vector<uint8_t>&,
				  const asio::ip::udp::endpoint&) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(SecureMessagingUdpServerTest, SetErrorCallback)
{
	bool called = false;
	server_->set_error_callback(
		[&called](std::error_code) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(SecureMessagingUdpServerTest, SetClientConnectedCallback)
{
	bool called = false;
	server_->set_client_connected_callback(
		[&called](const asio::ip::udp::endpoint&) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(SecureMessagingUdpServerTest, SetClientDisconnectedCallback)
{
	bool called = false;
	server_->set_client_disconnected_callback(
		[&called](const asio::ip::udp::endpoint&) {
			called = true;
		});
	EXPECT_FALSE(called);
}

// ============================================================================
// Certificate Configuration Tests
// ============================================================================

TEST_F(SecureMessagingUdpServerTest, SetCertificateChainFileInvalidPath)
{
	auto result = server_->set_certificate_chain_file("/nonexistent/cert.pem");
	EXPECT_FALSE(result);
}

TEST_F(SecureMessagingUdpServerTest, SetPrivateKeyFileInvalidPath)
{
	auto result = server_->set_private_key_file("/nonexistent/key.pem");
	EXPECT_FALSE(result);
}

// ============================================================================
// Error Path Tests (not running)
// ============================================================================

TEST_F(SecureMessagingUdpServerTest, StopWhileNotRunning)
{
	auto result = server_->stop_server();
	// Should not crash; may succeed as no-op or return error
	(void)result;
}

TEST_F(SecureMessagingUdpServerTest, StartWithoutCertificates)
{
	// Starting without setting certificates should fail
	auto result = server_->start_server(5556);
	EXPECT_FALSE(result);
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

class SecureMessagingUdpServerMultiInstanceTest : public ::testing::Test
{
};

TEST_F(SecureMessagingUdpServerMultiInstanceTest, DifferentIds)
{
	auto server1 = std::make_shared<core::secure_messaging_udp_server>("server-1");
	auto server2 = std::make_shared<core::secure_messaging_udp_server>("server-2");

	EXPECT_EQ(server1->server_id(), "server-1");
	EXPECT_EQ(server2->server_id(), "server-2");
	EXPECT_NE(server1->server_id(), server2->server_id());
}

TEST_F(SecureMessagingUdpServerMultiInstanceTest, IndependentState)
{
	auto server1 = std::make_shared<core::secure_messaging_udp_server>("s1");
	auto server2 = std::make_shared<core::secure_messaging_udp_server>("s2");

	EXPECT_FALSE(server1->is_running());
	EXPECT_FALSE(server2->is_running());
}
