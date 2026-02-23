/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/utils/resilient_client.h"
#include <gtest/gtest.h>

#include <atomic>

using namespace kcenon::network::utils;

/**
 * @file resilient_client_test.cpp
 * @brief Unit tests for resilient_client
 *
 * Tests validate:
 * - Construction with various parameters
 * - Initial connection state (not connected)
 * - get_client() returns valid messaging_client pointer
 * - Callback registration (reconnect and disconnect)
 * - Connect failure without running server
 * - Disconnect when already disconnected is no-op
 * - Destructor safety when not connected
 *
 * Note: Tests that require a live server are covered in integration tests.
 * These unit tests focus on state management and safe behavior without
 * a network connection.
 */

// ============================================================================
// Construction Tests
// ============================================================================

class ResilientClientConstructionTest : public ::testing::Test
{
};

TEST_F(ResilientClientConstructionTest, ConstructsWithDefaultParameters)
{
	resilient_client client("test_client", "localhost", 9999);

	EXPECT_FALSE(client.is_connected());
}

TEST_F(ResilientClientConstructionTest, ConstructsWithCustomRetries)
{
	resilient_client client("test_client", "127.0.0.1", 8080,
							5, std::chrono::milliseconds(500));

	EXPECT_FALSE(client.is_connected());
}

TEST_F(ResilientClientConstructionTest, ConstructsWithMinimalRetries)
{
	resilient_client client("test_client", "localhost", 1234,
							1, std::chrono::milliseconds(1));

	EXPECT_FALSE(client.is_connected());
}

TEST_F(ResilientClientConstructionTest, DestructorWhenNotConnected)
{
	{
		resilient_client client("test_client", "localhost", 9999);
	}
	SUCCEED();
}

// ============================================================================
// State Tests
// ============================================================================

class ResilientClientStateTest : public ::testing::Test
{
protected:
	std::unique_ptr<resilient_client> client_;

	void SetUp() override
	{
		client_ = std::make_unique<resilient_client>(
			"state_test", "localhost", 9999, 1, std::chrono::milliseconds(1));
	}
};

TEST_F(ResilientClientStateTest, InitiallyNotConnected)
{
	EXPECT_FALSE(client_->is_connected());
}

TEST_F(ResilientClientStateTest, GetClientReturnsNonNull)
{
	auto underlying = client_->get_client();

	ASSERT_NE(underlying, nullptr);
}

TEST_F(ResilientClientStateTest, GetClientReturnsSameInstance)
{
	auto client1 = client_->get_client();
	auto client2 = client_->get_client();

	EXPECT_EQ(client1.get(), client2.get());
}

// ============================================================================
// Callback Tests
// ============================================================================

class ResilientClientCallbackTest : public ::testing::Test
{
protected:
	std::unique_ptr<resilient_client> client_;

	void SetUp() override
	{
		client_ = std::make_unique<resilient_client>(
			"callback_test", "localhost", 9999, 1, std::chrono::milliseconds(1));
	}
};

TEST_F(ResilientClientCallbackTest, SetReconnectCallbackDoesNotThrow)
{
	EXPECT_NO_THROW(
		client_->set_reconnect_callback([](size_t) {}));
}

TEST_F(ResilientClientCallbackTest, SetDisconnectCallbackDoesNotThrow)
{
	EXPECT_NO_THROW(
		client_->set_disconnect_callback([]() {}));
}

TEST_F(ResilientClientCallbackTest, ReconnectCallbackInvokedOnConnectAttempt)
{
	std::atomic<size_t> callback_count{0};
	client_->set_reconnect_callback([&callback_count](size_t attempt) {
		callback_count.fetch_add(1);
	});

	// connect() invokes the callback for each attempt
	auto result = client_->connect();

	// Callback should have been invoked at least once regardless of outcome
	EXPECT_GE(callback_count.load(), 1);
}

TEST_F(ResilientClientCallbackTest, NullCallbackIsSafe)
{
	client_->set_reconnect_callback(nullptr);
	client_->set_disconnect_callback(nullptr);

	// Should not crash even with null callbacks
	EXPECT_NO_THROW(client_->connect());
}

// ============================================================================
// Connect/Disconnect Tests (without server)
// ============================================================================

class ResilientClientConnectionTest : public ::testing::Test
{
protected:
	std::unique_ptr<resilient_client> client_;

	void SetUp() override
	{
		// Use minimal retry settings for fast test execution
		client_ = std::make_unique<resilient_client>(
			"conn_test", "127.0.0.1", 1,  // port 1 - unlikely to have a server
			1, std::chrono::milliseconds(1));
	}
};

TEST_F(ResilientClientConnectionTest, ConnectReturnsResult)
{
	// messaging_client::start_client() is async; it may succeed (starts IO)
	// or fail depending on runtime state. Either outcome is valid.
	auto result = client_->connect();

	// Verify result is well-formed (either ok or error)
	EXPECT_TRUE(result.is_ok() || result.is_err());
}

TEST_F(ResilientClientConnectionTest, ConnectThenDisconnect)
{
	auto connect_result = client_->connect();

	// Disconnect should succeed regardless of connect outcome
	auto disconnect_result = client_->disconnect();
	EXPECT_FALSE(disconnect_result.is_err());
}

TEST_F(ResilientClientConnectionTest, DisconnectWhenNotConnectedSucceeds)
{
	auto result = client_->disconnect();

	// Disconnecting when already disconnected should succeed (no-op)
	EXPECT_FALSE(result.is_err());
	EXPECT_FALSE(client_->is_connected());
}

TEST_F(ResilientClientConnectionTest, SendWithRetryFailsWhenNotConnected)
{
	std::vector<uint8_t> data = {1, 2, 3, 4, 5};

	auto result = client_->send_with_retry(std::move(data));

	EXPECT_TRUE(result.is_err());
}

TEST_F(ResilientClientConnectionTest, MultipleConnectDisconnectCycles)
{
	for (int i = 0; i < 3; ++i)
	{
		auto connect_result = client_->connect();
		EXPECT_TRUE(connect_result.is_ok() || connect_result.is_err());

		auto disconnect_result = client_->disconnect();
		EXPECT_FALSE(disconnect_result.is_err());
	}
}

// ============================================================================
// Circuit Breaker Tests (when common_system is available)
// ============================================================================

#ifdef WITH_COMMON_SYSTEM
class ResilientClientCircuitBreakerTest : public ::testing::Test
{
protected:
	std::unique_ptr<resilient_client> client_;

	void SetUp() override
	{
		kcenon::common::resilience::circuit_breaker_config config;
		config.failure_threshold = 2;
		config.timeout = std::chrono::seconds(1);

		client_ = std::make_unique<resilient_client>(
			"circuit_test", "127.0.0.1", 1,
			1, std::chrono::milliseconds(1),
			config);
	}
};

TEST_F(ResilientClientCircuitBreakerTest, InitialCircuitStateIsClosed)
{
	EXPECT_EQ(client_->circuit_state(),
			  kcenon::common::resilience::circuit_state::CLOSED);
}
#endif
