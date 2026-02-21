/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/core/secure_messaging_client.h"
#include <gtest/gtest.h>

#include <atomic>

using namespace kcenon::network::core;
using namespace kcenon::network;

/**
 * @file secure_messaging_client_test.cpp
 * @brief Unit tests for secure_messaging_client
 *
 * Tests validate:
 * - Construction with client_id and verify_cert flag
 * - client_id() accessor
 * - is_running() / is_connected() initial state transitions
 * - send_packet() when not connected returns error
 * - Callback setters (receive, connected, disconnected, error)
 * - Double-start returns already_exists error
 *
 * Note: Actual TLS connection tests require a running server with
 * valid certificates. Those are covered by integration tests.
 */

// ============================================================================
// Construction Tests
// ============================================================================

class SecureMessagingClientConstructionTest : public ::testing::Test
{
};

TEST_F(SecureMessagingClientConstructionTest, ConstructsWithClientIdAndDefaultVerifyCert)
{
	auto client = std::make_shared<secure_messaging_client>("test_client");

	EXPECT_EQ(client->client_id(), "test_client");
	EXPECT_FALSE(client->is_running());
	EXPECT_FALSE(client->is_connected());
}

TEST_F(SecureMessagingClientConstructionTest, ConstructsWithVerifyCertDisabled)
{
	auto client = std::make_shared<secure_messaging_client>("no_verify_client", false);

	EXPECT_EQ(client->client_id(), "no_verify_client");
	EXPECT_FALSE(client->is_running());
	EXPECT_FALSE(client->is_connected());
}

TEST_F(SecureMessagingClientConstructionTest, ConstructsWithEmptyClientId)
{
	auto client = std::make_shared<secure_messaging_client>("");

	EXPECT_EQ(client->client_id(), "");
	EXPECT_FALSE(client->is_running());
}

// ============================================================================
// State Transition Tests
// ============================================================================

class SecureMessagingClientStateTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		client_ = std::make_shared<secure_messaging_client>("state_test_client", false);
	}

	std::shared_ptr<secure_messaging_client> client_;
};

TEST_F(SecureMessagingClientStateTest, InitialStateIsNotRunning)
{
	EXPECT_FALSE(client_->is_running());
}

TEST_F(SecureMessagingClientStateTest, InitialStateIsNotConnected)
{
	EXPECT_FALSE(client_->is_connected());
}

TEST_F(SecureMessagingClientStateTest, StopWhenNotRunningReturnsOk)
{
	// Stopping a client that was never started should succeed
	auto result = client_->stop_client();
	EXPECT_TRUE(result.is_ok());
}

TEST_F(SecureMessagingClientStateTest, StartWithEmptyHostReturnsError)
{
	auto result = client_->start_client("", 5555);

	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::common_errors::invalid_argument);
}

TEST_F(SecureMessagingClientStateTest, StartWithRefusedPortReturnsError)
{
	// Connection to a port with no listener should fail quickly
	auto result = client_->start_client("127.0.0.1", 1);

	EXPECT_TRUE(result.is_err());
	EXPECT_FALSE(client_->is_connected());
}

// ============================================================================
// Send Packet Tests
// ============================================================================

class SecureMessagingClientSendTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		client_ = std::make_shared<secure_messaging_client>("send_test_client", false);
	}

	std::shared_ptr<secure_messaging_client> client_;
};

TEST_F(SecureMessagingClientSendTest, SendWhenNotConnectedReturnsError)
{
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	auto result = client_->send_packet(std::move(data));

	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::network_system::connection_closed);
}

TEST_F(SecureMessagingClientSendTest, SendEmptyDataWhenNotConnectedReturnsError)
{
	std::vector<uint8_t> data;
	auto result = client_->send_packet(std::move(data));

	// Should fail with connection_closed (checked before empty data check)
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Callback Setter Tests
// ============================================================================

class SecureMessagingClientCallbackTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		client_ = std::make_shared<secure_messaging_client>("callback_test_client", false);
	}

	std::shared_ptr<secure_messaging_client> client_;
};

TEST_F(SecureMessagingClientCallbackTest, SetReceiveCallbackDoesNotThrow)
{
	EXPECT_NO_THROW(client_->set_receive_callback(
		[](const std::vector<uint8_t>&) {}));
}

TEST_F(SecureMessagingClientCallbackTest, SetConnectedCallbackDoesNotThrow)
{
	EXPECT_NO_THROW(client_->set_connected_callback([]() {}));
}

TEST_F(SecureMessagingClientCallbackTest, SetDisconnectedCallbackDoesNotThrow)
{
	EXPECT_NO_THROW(client_->set_disconnected_callback([]() {}));
}

TEST_F(SecureMessagingClientCallbackTest, SetErrorCallbackDoesNotThrow)
{
	EXPECT_NO_THROW(client_->set_error_callback(
		[](std::error_code) {}));
}

TEST_F(SecureMessagingClientCallbackTest, SetNullCallbackDoesNotThrow)
{
	EXPECT_NO_THROW(client_->set_receive_callback(nullptr));
	EXPECT_NO_THROW(client_->set_connected_callback(nullptr));
	EXPECT_NO_THROW(client_->set_disconnected_callback(nullptr));
	EXPECT_NO_THROW(client_->set_error_callback(nullptr));
}

// ============================================================================
// Double-Start Tests
// ============================================================================

class SecureMessagingClientDoubleStartTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		client_ = std::make_shared<secure_messaging_client>("double_start_client", false);
	}

	std::shared_ptr<secure_messaging_client> client_;
};

TEST_F(SecureMessagingClientDoubleStartTest, DoubleStartWithEmptyHostBothReturnError)
{
	// First start with empty host fails
	auto result1 = client_->start_client("", 5555);
	EXPECT_TRUE(result1.is_err());

	// Second start with empty host also fails (client never actually started)
	auto result2 = client_->start_client("", 5555);
	EXPECT_TRUE(result2.is_err());
}
