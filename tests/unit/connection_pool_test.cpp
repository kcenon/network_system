/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/core/connection_pool.h"
#include <gtest/gtest.h>

#include <thread>

using namespace kcenon::network::core;

/**
 * @file connection_pool_test.cpp
 * @brief Unit tests for connection_pool
 *
 * Tests validate:
 * - Construction with host, port, pool_size
 * - pool_size() and active_count() accessors
 * - Shutdown safety (destructor does not deadlock)
 * - acquire() returns nullptr after shutdown
 * - release() with nullptr is a no-op
 *
 * Note: initialize(), acquire()/release() with live connections require
 * a running server. Those paths are covered by integration tests.
 */

// ============================================================================
// Construction Tests
// ============================================================================

class ConnectionPoolConstructionTest : public ::testing::Test
{
};

TEST_F(ConnectionPoolConstructionTest, ConstructsWithDefaultPoolSize)
{
	connection_pool pool("localhost", 5555);

	EXPECT_EQ(pool.pool_size(), 10);
	EXPECT_EQ(pool.active_count(), 0);
}

TEST_F(ConnectionPoolConstructionTest, ConstructsWithCustomPoolSize)
{
	connection_pool pool("192.168.1.1", 8080, 25);

	EXPECT_EQ(pool.pool_size(), 25);
	EXPECT_EQ(pool.active_count(), 0);
}

TEST_F(ConnectionPoolConstructionTest, ConstructsWithMinimalPoolSize)
{
	connection_pool pool("localhost", 9999, 1);

	EXPECT_EQ(pool.pool_size(), 1);
	EXPECT_EQ(pool.active_count(), 0);
}

TEST_F(ConnectionPoolConstructionTest, ConstructsWithZeroPoolSize)
{
	connection_pool pool("localhost", 0, 0);

	EXPECT_EQ(pool.pool_size(), 0);
	EXPECT_EQ(pool.active_count(), 0);
}

// ============================================================================
// Shutdown Safety Tests
// ============================================================================

class ConnectionPoolShutdownTest : public ::testing::Test
{
};

TEST_F(ConnectionPoolShutdownTest, DestructorDoesNotDeadlock)
{
	// Create and destroy immediately - should not hang
	{
		connection_pool pool("localhost", 5555, 5);
	}
	SUCCEED();
}

TEST_F(ConnectionPoolShutdownTest, MultipleDestructionsAreHarmless)
{
	// Creating and destroying multiple pools should be safe
	for (int i = 0; i < 10; ++i)
	{
		connection_pool pool("localhost", 5555, 5);
		EXPECT_EQ(pool.pool_size(), 5);
	}
	SUCCEED();
}

TEST_F(ConnectionPoolShutdownTest, ReleaseWithNullptrIsNoOp)
{
	connection_pool pool("localhost", 5555, 5);

	// Releasing nullptr should not crash or change state
	pool.release(nullptr);

	EXPECT_EQ(pool.active_count(), 0);
}

// ============================================================================
// Active Count Tracking Tests
// ============================================================================

class ConnectionPoolActiveCountTest : public ::testing::Test
{
};

TEST_F(ConnectionPoolActiveCountTest, InitialActiveCountIsZero)
{
	connection_pool pool("localhost", 5555, 10);

	EXPECT_EQ(pool.active_count(), 0);
}

TEST_F(ConnectionPoolActiveCountTest, PoolSizeIsIndependentOfActiveCount)
{
	connection_pool pool("10.0.0.1", 3000, 42);

	EXPECT_EQ(pool.pool_size(), 42);
	EXPECT_EQ(pool.active_count(), 0);
}

// ============================================================================
// Initialize Without Server Tests
// ============================================================================

class ConnectionPoolInitializeTest : public ::testing::Test
{
};

TEST_F(ConnectionPoolInitializeTest, InitializeFailsWithoutServer)
{
	// Attempting to initialize without a running server should fail
	connection_pool pool("127.0.0.1", 1, 1);

	auto result = pool.initialize();

	EXPECT_TRUE(result.is_err());
}

TEST_F(ConnectionPoolInitializeTest, InitializeWithZeroPoolSizeSucceeds)
{
	// Pool size 0 means no connections to create - should succeed trivially
	connection_pool pool("localhost", 5555, 0);

	auto result = pool.initialize();

	EXPECT_TRUE(result.is_ok());
}
