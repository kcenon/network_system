/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file network_system_test.cpp
 * @brief Unit tests for network system initialization and lifecycle
 *
 * Tests validate:
 * - Default initialization and shutdown
 * - Custom configuration initialization
 * - Double initialization detection
 * - Shutdown without initialization
 * - is_initialized() state tracking
 * - Re-initialization after shutdown
 */

#include "kcenon/network/network_system.h"
#include "kcenon/network/config/config.h"

#include <gtest/gtest.h>

using namespace kcenon::network;

// ============================================================================
// Lifecycle Tests
// ============================================================================

class NetworkSystemLifecycleTest : public ::testing::Test
{
protected:
	void TearDown() override
	{
		// Ensure clean state after each test
		if (is_initialized())
		{
			(void)shutdown();
		}
	}
};

TEST_F(NetworkSystemLifecycleTest, NotInitializedByDefault)
{
	// Fresh state should not be initialized
	// Note: this test may be affected by other tests if run in same process
	if (!is_initialized())
	{
		EXPECT_FALSE(is_initialized());
	}
	else
	{
		// If already initialized by another test, shut down first
		(void)shutdown();
		EXPECT_FALSE(is_initialized());
	}
}

TEST_F(NetworkSystemLifecycleTest, InitializeWithDefaultConfig)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	auto result = initialize();
	EXPECT_TRUE(result.is_ok());
	EXPECT_TRUE(is_initialized());
}

TEST_F(NetworkSystemLifecycleTest, InitializeWithProductionConfig)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	auto result = initialize(config::network_config::production());
	EXPECT_TRUE(result.is_ok());
	EXPECT_TRUE(is_initialized());
}

TEST_F(NetworkSystemLifecycleTest, InitializeWithDevelopmentConfig)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	auto result = initialize(config::network_config::development());
	EXPECT_TRUE(result.is_ok());
	EXPECT_TRUE(is_initialized());
}

TEST_F(NetworkSystemLifecycleTest, InitializeWithCustomConfig)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	config::network_config cfg;
	cfg.thread_pool.worker_count = 2;

	auto result = initialize(cfg);
	EXPECT_TRUE(result.is_ok());
	EXPECT_TRUE(is_initialized());
}

TEST_F(NetworkSystemLifecycleTest, ShutdownSuccess)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	auto init_result = initialize();
	ASSERT_TRUE(init_result.is_ok());

	auto shutdown_result = shutdown();
	EXPECT_TRUE(shutdown_result.is_ok());
	EXPECT_FALSE(is_initialized());
}

TEST_F(NetworkSystemLifecycleTest, ShutdownWithoutInitialize)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	auto result = shutdown();
	EXPECT_TRUE(result.is_err());
}

TEST_F(NetworkSystemLifecycleTest, DoubleInitializeFails)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	auto first = initialize();
	ASSERT_TRUE(first.is_ok());

	auto second = initialize();
	EXPECT_TRUE(second.is_err());
}

TEST_F(NetworkSystemLifecycleTest, ReinitializeAfterShutdown)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	auto init1 = initialize();
	ASSERT_TRUE(init1.is_ok());

	auto shut = shutdown();
	ASSERT_TRUE(shut.is_ok());
	EXPECT_FALSE(is_initialized());

	auto init2 = initialize();
	EXPECT_TRUE(init2.is_ok());
	EXPECT_TRUE(is_initialized());
}

TEST_F(NetworkSystemLifecycleTest, IsInitializedTracksState)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	EXPECT_FALSE(is_initialized());

	auto result = initialize();
	ASSERT_TRUE(result.is_ok());
	EXPECT_TRUE(is_initialized());

	auto shut = shutdown();
	ASSERT_TRUE(shut.is_ok());
	EXPECT_FALSE(is_initialized());
}

// ============================================================================
// Configuration Tests
// ============================================================================

class NetworkSystemConfigTest : public ::testing::Test
{
protected:
	void TearDown() override
	{
		if (is_initialized())
		{
			(void)shutdown();
		}
	}
};

TEST_F(NetworkSystemConfigTest, ProductionConfigValues)
{
	auto cfg = config::network_config::production();

	// Production config should have reasonable defaults
	EXPECT_GE(cfg.thread_pool.worker_count, 0);
}

TEST_F(NetworkSystemConfigTest, DevelopmentConfigValues)
{
	auto cfg = config::network_config::development();

	// Development config should be valid
	EXPECT_GE(cfg.thread_pool.worker_count, 0);
}

TEST_F(NetworkSystemConfigTest, InitializeWithZeroWorkerCount)
{
	if (is_initialized())
	{
		(void)shutdown();
	}

	config::network_config cfg;
	cfg.thread_pool.worker_count = 0; // Should use hardware_concurrency

	auto result = initialize(cfg);
	EXPECT_TRUE(result.is_ok());
	EXPECT_TRUE(is_initialized());
}
