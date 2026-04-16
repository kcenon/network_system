/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/core/secure_messaging_udp_client.h"
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace core = kcenon::network::core;

/**
 * @file secure_messaging_udp_client_test.cpp
 * @brief Unit tests for secure_messaging_udp_client (DTLS client)
 *
 * Tests validate:
 * - Construction with client_id and verify_cert flag
 * - client_id() accessor
 * - is_running() and is_connected() initial state
 * - Callback registration (receive, connected, disconnected, error, udp_receive)
 * - stop_client() while not running
 * - send_packet() while not running
 * - start_client() with invalid arguments
 */

// ============================================================================
// Construction Tests
// ============================================================================

class SecureMessagingUdpClientTest : public ::testing::Test
{
protected:
	std::shared_ptr<core::secure_messaging_udp_client> client_
		= std::make_shared<core::secure_messaging_udp_client>("test-dtls-client");
};

TEST_F(SecureMessagingUdpClientTest, Construction)
{
	EXPECT_NE(client_, nullptr);
}

TEST_F(SecureMessagingUdpClientTest, ClientId)
{
	EXPECT_EQ(client_->client_id(), "test-dtls-client");
}

TEST_F(SecureMessagingUdpClientTest, InitiallyNotRunning)
{
	EXPECT_FALSE(client_->is_running());
}

TEST_F(SecureMessagingUdpClientTest, InitiallyNotConnected)
{
	EXPECT_FALSE(client_->is_connected());
}

TEST_F(SecureMessagingUdpClientTest, ConstructWithVerifyCertFalse)
{
	auto client = std::make_shared<core::secure_messaging_udp_client>(
		"no-verify-client", false);
	EXPECT_EQ(client->client_id(), "no-verify-client");
	EXPECT_FALSE(client->is_running());
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST_F(SecureMessagingUdpClientTest, SetReceiveCallback)
{
	bool called = false;
	client_->set_receive_callback(
		[&called](const std::vector<uint8_t>&) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(SecureMessagingUdpClientTest, SetConnectedCallback)
{
	bool called = false;
	client_->set_connected_callback(
		[&called]() {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(SecureMessagingUdpClientTest, SetDisconnectedCallback)
{
	bool called = false;
	client_->set_disconnected_callback(
		[&called]() {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(SecureMessagingUdpClientTest, SetErrorCallback)
{
	bool called = false;
	client_->set_error_callback(
		[&called](std::error_code) {
			called = true;
		});
	EXPECT_FALSE(called);
}

TEST_F(SecureMessagingUdpClientTest, SetUdpReceiveCallback)
{
	bool called = false;
	client_->set_udp_receive_callback(
		[&called](const std::vector<uint8_t>&,
				  const asio::ip::udp::endpoint&) {
			called = true;
		});
	EXPECT_FALSE(called);
}

// ============================================================================
// Error Path Tests (not running)
// ============================================================================

TEST_F(SecureMessagingUdpClientTest, StopWhileNotRunning)
{
	auto result = client_->stop_client();
	// Should not crash; may succeed as no-op or return error
	(void)result;
}

TEST_F(SecureMessagingUdpClientTest, SendWhileNotRunning)
{
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	auto result = client_->send_packet(std::move(data));
	EXPECT_TRUE(result.is_err());
}

TEST_F(SecureMessagingUdpClientTest, StartWithEmptyHost)
{
	auto result = client_->start_client("", 5555);
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Multiple Instance Tests
// ============================================================================

class SecureMessagingUdpClientMultiInstanceTest : public ::testing::Test
{
};

TEST_F(SecureMessagingUdpClientMultiInstanceTest, DifferentIds)
{
	auto client1 = std::make_shared<core::secure_messaging_udp_client>("client-1");
	auto client2 = std::make_shared<core::secure_messaging_udp_client>("client-2");

	EXPECT_EQ(client1->client_id(), "client-1");
	EXPECT_EQ(client2->client_id(), "client-2");
	EXPECT_NE(client1->client_id(), client2->client_id());
}

TEST_F(SecureMessagingUdpClientMultiInstanceTest, IndependentState)
{
	auto client1 = std::make_shared<core::secure_messaging_udp_client>("c1");
	auto client2 = std::make_shared<core::secure_messaging_udp_client>("c2");

	EXPECT_FALSE(client1->is_running());
	EXPECT_FALSE(client2->is_running());
	EXPECT_FALSE(client1->is_connected());
	EXPECT_FALSE(client2->is_connected());
}
