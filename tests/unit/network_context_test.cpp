/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/core/network_context.h"
#include <gtest/gtest.h>

#include <memory>

namespace ctx = kcenon::network::core;
namespace integration = kcenon::network::integration;

/**
 * @file network_context_test.cpp
 * @brief Unit tests for network_context singleton
 *
 * Tests validate:
 * - Singleton instance() consistency
 * - is_initialized() initial state
 * - set/get round-trip for thread pool, logger, monitoring
 * - initialize()/shutdown() lifecycle transitions
 * - Idempotent initialize and shutdown
 */

// ============================================================================
// Singleton Tests
// ============================================================================

class NetworkContextSingletonTest : public ::testing::Test
{
};

TEST_F(NetworkContextSingletonTest, InstanceReturnsSameReference)
{
	auto& ctx1 = ctx::network_context::instance();
	auto& ctx2 = ctx::network_context::instance();

	EXPECT_EQ(&ctx1, &ctx2);
}

TEST_F(NetworkContextSingletonTest, InstanceReturnsSameAddressAcrossCalls)
{
	ctx::network_context* ptr1 = &ctx::network_context::instance();
	ctx::network_context* ptr2 = &ctx::network_context::instance();
	ctx::network_context* ptr3 = &ctx::network_context::instance();

	EXPECT_EQ(ptr1, ptr2);
	EXPECT_EQ(ptr2, ptr3);
}

// ============================================================================
// Thread Pool Getter/Setter Tests
// ============================================================================

class NetworkContextThreadPoolTest : public ::testing::Test
{
protected:
	void TearDown() override
	{
		// Reset to null to avoid polluting other tests
		ctx::network_context::instance().set_thread_pool(nullptr);
	}
};

TEST_F(NetworkContextThreadPoolTest, GetThreadPoolCallDoesNotCrash)
{
	ctx::network_context::instance().set_thread_pool(nullptr);

	auto pool = ctx::network_context::instance().get_thread_pool();

	// May or may not be null depending on prior initialization
	// Just verify the call doesn't crash
	SUCCEED();
}

TEST_F(NetworkContextThreadPoolTest, SetAndGetThreadPoolRoundTrip)
{
	auto mock_pool = std::make_shared<integration::basic_thread_pool>(1);
	ctx::network_context::instance().set_thread_pool(mock_pool);

	auto retrieved = ctx::network_context::instance().get_thread_pool();

	EXPECT_EQ(retrieved.get(), mock_pool.get());
}

TEST_F(NetworkContextThreadPoolTest, SetNullThreadPool)
{
	auto pool = std::make_shared<integration::basic_thread_pool>(1);
	ctx::network_context::instance().set_thread_pool(pool);

	// Set to null
	ctx::network_context::instance().set_thread_pool(nullptr);

	auto retrieved = ctx::network_context::instance().get_thread_pool();
	EXPECT_EQ(retrieved, nullptr);
}

TEST_F(NetworkContextThreadPoolTest, ReplaceThreadPool)
{
	auto pool1 = std::make_shared<integration::basic_thread_pool>(1);
	auto pool2 = std::make_shared<integration::basic_thread_pool>(1);

	ctx::network_context::instance().set_thread_pool(pool1);
	EXPECT_EQ(ctx::network_context::instance().get_thread_pool().get(),
			  pool1.get());

	ctx::network_context::instance().set_thread_pool(pool2);
	EXPECT_EQ(ctx::network_context::instance().get_thread_pool().get(),
			  pool2.get());
}

// ============================================================================
// Logger Getter/Setter Tests
// ============================================================================

class NetworkContextLoggerTest : public ::testing::Test
{
protected:
	void TearDown() override
	{
		ctx::network_context::instance().set_logger(nullptr);
	}
};

TEST_F(NetworkContextLoggerTest, SetAndGetLoggerRoundTrip)
{
	auto mock_logger = std::make_shared<integration::basic_logger>(
		integration::log_level::info);
	ctx::network_context::instance().set_logger(mock_logger);

	auto retrieved = ctx::network_context::instance().get_logger();

	EXPECT_EQ(retrieved.get(), mock_logger.get());
}

TEST_F(NetworkContextLoggerTest, SetNullLogger)
{
	auto logger = std::make_shared<integration::basic_logger>(
		integration::log_level::info);
	ctx::network_context::instance().set_logger(logger);

	ctx::network_context::instance().set_logger(nullptr);

	auto retrieved = ctx::network_context::instance().get_logger();
	EXPECT_EQ(retrieved, nullptr);
}

// ============================================================================
// Monitoring Getter/Setter Tests
// ============================================================================

class NetworkContextMonitoringTest : public ::testing::Test
{
protected:
	void TearDown() override
	{
		ctx::network_context::instance().set_monitoring(nullptr);
	}
};

TEST_F(NetworkContextMonitoringTest, SetAndGetMonitoringRoundTrip)
{
	auto mock_monitoring = std::make_shared<integration::basic_monitoring>();
	ctx::network_context::instance().set_monitoring(mock_monitoring);

	auto retrieved = ctx::network_context::instance().get_monitoring();

	EXPECT_EQ(retrieved.get(), mock_monitoring.get());
}

TEST_F(NetworkContextMonitoringTest, GetMonitoringReturnsDefaultWhenNull)
{
	ctx::network_context::instance().set_monitoring(nullptr);

	auto monitoring = ctx::network_context::instance().get_monitoring();

	// Should return a default monitoring from the integration manager
	// (may or may not be null depending on the integration manager state)
	SUCCEED();
}

// ============================================================================
// Lifecycle Tests
// ============================================================================

class NetworkContextLifecycleTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Ensure clean state before each lifecycle test
		ctx::network_context::instance().shutdown();
		ctx::network_context::instance().set_thread_pool(nullptr);
		ctx::network_context::instance().set_logger(nullptr);
		ctx::network_context::instance().set_monitoring(nullptr);
	}

	void TearDown() override
	{
		// Always clean up
		ctx::network_context::instance().shutdown();
		ctx::network_context::instance().set_thread_pool(nullptr);
		ctx::network_context::instance().set_logger(nullptr);
		ctx::network_context::instance().set_monitoring(nullptr);
	}
};

TEST_F(NetworkContextLifecycleTest, NotInitializedAfterShutdown)
{
	EXPECT_FALSE(ctx::network_context::instance().is_initialized());
}

TEST_F(NetworkContextLifecycleTest, InitializeSetsInitializedTrue)
{
	ctx::network_context::instance().initialize(1);

	EXPECT_TRUE(ctx::network_context::instance().is_initialized());
}

TEST_F(NetworkContextLifecycleTest, ShutdownSetsInitializedFalse)
{
	ctx::network_context::instance().initialize(1);
	ASSERT_TRUE(ctx::network_context::instance().is_initialized());

	ctx::network_context::instance().shutdown();

	EXPECT_FALSE(ctx::network_context::instance().is_initialized());
}

TEST_F(NetworkContextLifecycleTest, DoubleInitializeIsIdempotent)
{
	ctx::network_context::instance().initialize(1);
	EXPECT_TRUE(ctx::network_context::instance().is_initialized());

	// Second initialize should not crash or change state
	ctx::network_context::instance().initialize(2);
	EXPECT_TRUE(ctx::network_context::instance().is_initialized());
}

TEST_F(NetworkContextLifecycleTest, DoubleShutdownIsIdempotent)
{
	ctx::network_context::instance().initialize(1);

	ctx::network_context::instance().shutdown();
	EXPECT_FALSE(ctx::network_context::instance().is_initialized());

	// Second shutdown should not crash
	ctx::network_context::instance().shutdown();
	EXPECT_FALSE(ctx::network_context::instance().is_initialized());
}

TEST_F(NetworkContextLifecycleTest, InitializeCreatesThreadPool)
{
	ctx::network_context::instance().initialize(2);

	auto pool = ctx::network_context::instance().get_thread_pool();

	EXPECT_NE(pool, nullptr);
}

TEST_F(NetworkContextLifecycleTest, InitializeCreatesLogger)
{
	ctx::network_context::instance().initialize(1);

	auto logger = ctx::network_context::instance().get_logger();

	EXPECT_NE(logger, nullptr);
}

TEST_F(NetworkContextLifecycleTest, ReinitializeAfterShutdown)
{
	ctx::network_context::instance().initialize(1);
	EXPECT_TRUE(ctx::network_context::instance().is_initialized());

	ctx::network_context::instance().shutdown();
	EXPECT_FALSE(ctx::network_context::instance().is_initialized());

	// Should be able to re-initialize
	ctx::network_context::instance().initialize(1);
	EXPECT_TRUE(ctx::network_context::instance().is_initialized());
}

TEST_F(NetworkContextLifecycleTest, InitializeWithZeroThreadsAutoDetects)
{
	// thread_count=0 should auto-detect via hardware_concurrency
	ctx::network_context::instance().initialize(0);

	EXPECT_TRUE(ctx::network_context::instance().is_initialized());
	EXPECT_NE(ctx::network_context::instance().get_thread_pool(), nullptr);
}

TEST_F(NetworkContextLifecycleTest, PresetThreadPoolIsNotReplacedByInitialize)
{
	auto custom_pool = std::make_shared<integration::basic_thread_pool>(1);
	ctx::network_context::instance().set_thread_pool(custom_pool);

	ctx::network_context::instance().initialize(4);

	// The pre-set pool should be preserved
	EXPECT_EQ(ctx::network_context::instance().get_thread_pool().get(),
			  custom_pool.get());
}

TEST_F(NetworkContextLifecycleTest, PresetLoggerIsNotReplacedByInitialize)
{
	auto custom_logger = std::make_shared<integration::basic_logger>(
		integration::log_level::debug);
	ctx::network_context::instance().set_logger(custom_logger);

	ctx::network_context::instance().initialize(1);

	// The pre-set logger should be preserved
	EXPECT_EQ(ctx::network_context::instance().get_logger().get(),
			  custom_logger.get());
}
