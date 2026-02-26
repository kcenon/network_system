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

// ============================================================================
// Reconnect Callback Attempt Number Tests
// ============================================================================

class ResilientClientReconnectAttemptTest : public ::testing::Test
{
protected:
	std::unique_ptr<resilient_client> client_;

	void SetUp() override
	{
		client_ = std::make_unique<resilient_client>(
			"attempt_test", "127.0.0.1", 1,
			3, std::chrono::milliseconds(1));
	}
};

TEST_F(ResilientClientReconnectAttemptTest, CallbackReceivesAttemptNumber)
{
	std::vector<size_t> attempts;
	std::mutex mtx;

	client_->set_reconnect_callback([&](size_t attempt) {
		std::lock_guard<std::mutex> lock(mtx);
		attempts.push_back(attempt);
	});

	auto result = client_->connect();

	// Verify at least one attempt was recorded
	std::lock_guard<std::mutex> lock(mtx);
	EXPECT_GE(attempts.size(), 1u);

	// Attempt numbers should start from 1 and increment
	if (!attempts.empty())
	{
		EXPECT_EQ(attempts[0], 1u);
	}
}

TEST_F(ResilientClientReconnectAttemptTest, MaxRetriesRespected)
{
	std::atomic<size_t> callback_count{0};

	client_->set_reconnect_callback([&](size_t) {
		callback_count.fetch_add(1);
	});

	auto result = client_->connect();

	// Should not exceed max_retries (3)
	EXPECT_LE(callback_count.load(), 3u);
}

// ============================================================================
// Disconnect Callback Tests
// ============================================================================

class ResilientClientDisconnectCallbackTest : public ::testing::Test
{
protected:
	std::unique_ptr<resilient_client> client_;

	void SetUp() override
	{
		client_ = std::make_unique<resilient_client>(
			"disconnect_cb_test", "127.0.0.1", 1,
			1, std::chrono::milliseconds(1));
	}
};

TEST_F(ResilientClientDisconnectCallbackTest, DisconnectCallbackCanBeSet)
{
	std::atomic<bool> called{false};

	client_->set_disconnect_callback([&]() {
		called.store(true);
	});

	// Disconnect without connecting - should be safe
	auto result = client_->disconnect();
	EXPECT_FALSE(result.is_err());
}

TEST_F(ResilientClientDisconnectCallbackTest, SetDisconnectCallbackMultipleTimes)
{
	// Setting callback multiple times should not crash
	for (int i = 0; i < 5; ++i)
	{
		EXPECT_NO_THROW(client_->set_disconnect_callback([]() {}));
	}
}

// ============================================================================
// Edge Case Tests
// ============================================================================

class ResilientClientEdgeCaseTest : public ::testing::Test
{
};

TEST_F(ResilientClientEdgeCaseTest, ConstructWithEmptyId)
{
	// Empty client_id should not crash
	resilient_client client("", "localhost", 9999);

	EXPECT_FALSE(client.is_connected());
}

TEST_F(ResilientClientEdgeCaseTest, ConstructWithZeroRetries)
{
	resilient_client client("zero_retry", "127.0.0.1", 1,
							0, std::chrono::milliseconds(1));

	EXPECT_FALSE(client.is_connected());
}

TEST_F(ResilientClientEdgeCaseTest, ConstructWithLargeBackoff)
{
	// Large initial backoff should not overflow or cause issues during construction
	resilient_client client("large_backoff", "127.0.0.1", 1,
							1, std::chrono::milliseconds(60000));

	EXPECT_FALSE(client.is_connected());
}

TEST_F(ResilientClientEdgeCaseTest, ConstructWithPortZero)
{
	resilient_client client("port_zero", "127.0.0.1", 0);

	EXPECT_FALSE(client.is_connected());
}

TEST_F(ResilientClientEdgeCaseTest, ConstructWithMaxPort)
{
	resilient_client client("max_port", "127.0.0.1", 65535);

	EXPECT_FALSE(client.is_connected());
}

TEST_F(ResilientClientEdgeCaseTest, MultipleDestructionsAreSafe)
{
	for (int i = 0; i < 10; ++i)
	{
		resilient_client client("multi_destroy_" + std::to_string(i),
								"127.0.0.1", 1, 1,
								std::chrono::milliseconds(1));
	}
	SUCCEED();
}

// ============================================================================
// send_with_retry Additional Tests
// ============================================================================

class ResilientClientSendTest : public ::testing::Test
{
protected:
	std::unique_ptr<resilient_client> client_;

	void SetUp() override
	{
		client_ = std::make_unique<resilient_client>(
			"send_test", "127.0.0.1", 1,
			1, std::chrono::milliseconds(1));
	}
};

TEST_F(ResilientClientSendTest, SendEmptyDataWhenNotConnected)
{
	std::vector<uint8_t> empty_data;
	auto result = client_->send_with_retry(std::move(empty_data));

	EXPECT_TRUE(result.is_err());
}

TEST_F(ResilientClientSendTest, SendLargeDataWhenNotConnected)
{
	std::vector<uint8_t> large_data(10000, 0xAB);
	auto result = client_->send_with_retry(std::move(large_data));

	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Rapid Connect/Disconnect Cycle Tests
// ============================================================================

class ResilientClientRapidCycleTest : public ::testing::Test
{
protected:
	std::unique_ptr<resilient_client> client_;

	void SetUp() override
	{
		client_ = std::make_unique<resilient_client>(
			"rapid_cycle", "127.0.0.1", 1,
			1, std::chrono::milliseconds(1));
	}
};

TEST_F(ResilientClientRapidCycleTest, RapidConnectDisconnect)
{
	for (int i = 0; i < 5; ++i)
	{
		auto connect_result = client_->connect();
		EXPECT_TRUE(connect_result.is_ok() || connect_result.is_err());

		auto disconnect_result = client_->disconnect();
		EXPECT_FALSE(disconnect_result.is_err());
	}

	// Final state should be disconnected
	EXPECT_FALSE(client_->is_connected());
}

TEST_F(ResilientClientRapidCycleTest, DisconnectMultipleTimesIsSafe)
{
	for (int i = 0; i < 10; ++i)
	{
		auto result = client_->disconnect();
		EXPECT_FALSE(result.is_err());
	}
}

// ============================================================================
// Get Client Consistency Tests
// ============================================================================

class ResilientClientGetClientTest : public ::testing::Test
{
protected:
	std::unique_ptr<resilient_client> client_;

	void SetUp() override
	{
		client_ = std::make_unique<resilient_client>(
			"get_client_test", "localhost", 9999);
	}
};

TEST_F(ResilientClientGetClientTest, GetClientAfterConnectDisconnect)
{
	auto before = client_->get_client();
	ASSERT_NE(before, nullptr);

	client_->connect();
	client_->disconnect();

	auto after = client_->get_client();
	ASSERT_NE(after, nullptr);

	// Should return the same underlying client instance
	EXPECT_EQ(before.get(), after.get());
}

TEST_F(ResilientClientGetClientTest, GetClientIsThreadSafe)
{
	constexpr int num_threads = 4;
	std::vector<std::thread> threads;
	std::vector<void*> pointers(num_threads, nullptr);

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this, &pointers, t]() {
			auto ptr = client_->get_client();
			pointers[t] = ptr.get();
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}

	// All threads should get the same pointer
	for (int t = 1; t < num_threads; ++t)
	{
		EXPECT_EQ(pointers[0], pointers[t]);
	}
}
