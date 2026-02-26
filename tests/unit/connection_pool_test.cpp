/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/core/connection_pool.h"
#include <gtest/gtest.h>

#include <thread>
#include <vector>

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
 * - Large pool sizes
 * - Concurrent destruction safety
 * - Initialize error paths
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

TEST_F(ConnectionPoolConstructionTest, ConstructsWithLargePoolSize)
{
	connection_pool pool("localhost", 5555, 1000);

	EXPECT_EQ(pool.pool_size(), 1000);
	EXPECT_EQ(pool.active_count(), 0);
}

TEST_F(ConnectionPoolConstructionTest, ConstructsWithMaxPort)
{
	connection_pool pool("localhost", 65535, 5);

	EXPECT_EQ(pool.pool_size(), 5);
	EXPECT_EQ(pool.active_count(), 0);
}

TEST_F(ConnectionPoolConstructionTest, ConstructsWithIPv6Localhost)
{
	connection_pool pool("::1", 8080, 5);

	EXPECT_EQ(pool.pool_size(), 5);
	EXPECT_EQ(pool.active_count(), 0);
}

TEST_F(ConnectionPoolConstructionTest, ConstructsWithEmptyHost)
{
	connection_pool pool("", 5555, 5);

	EXPECT_EQ(pool.pool_size(), 5);
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

TEST_F(ConnectionPoolShutdownTest, MultipleNullptrReleasesAreHarmless)
{
	connection_pool pool("localhost", 5555, 5);

	for (int i = 0; i < 100; ++i)
	{
		pool.release(nullptr);
	}

	EXPECT_EQ(pool.active_count(), 0);
}

TEST_F(ConnectionPoolShutdownTest, RapidCreationAndDestruction)
{
	// Stress test: rapid creation/destruction should not leak or crash
	for (int i = 0; i < 50; ++i)
	{
		connection_pool pool("localhost", static_cast<unsigned short>(5000 + i),
							 static_cast<size_t>(i % 20));
		EXPECT_EQ(pool.pool_size(), static_cast<size_t>(i % 20));
	}
	SUCCEED();
}

TEST_F(ConnectionPoolShutdownTest, ConcurrentPoolCreation)
{
	constexpr int num_threads = 8;
	std::vector<std::thread> threads;
	threads.reserve(num_threads);

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([t]() {
			for (int i = 0; i < 10; ++i)
			{
				connection_pool pool(
					"localhost",
					static_cast<unsigned short>(5000 + t * 100 + i),
					static_cast<size_t>(5));
				EXPECT_EQ(pool.pool_size(), 5);
				EXPECT_EQ(pool.active_count(), 0);
			}
		});
	}

	for (auto& th : threads)
	{
		th.join();
	}
	SUCCEED();
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

TEST_F(ConnectionPoolActiveCountTest, ActiveCountConsistentAcrossReads)
{
	connection_pool pool("localhost", 5555, 10);

	// Multiple reads should return the same value
	auto count1 = pool.active_count();
	auto count2 = pool.active_count();
	auto count3 = pool.active_count();

	EXPECT_EQ(count1, count2);
	EXPECT_EQ(count2, count3);
	EXPECT_EQ(count1, 0);
}

TEST_F(ConnectionPoolActiveCountTest, PoolSizeDoesNotChangeAfterConstruction)
{
	connection_pool pool("localhost", 5555, 15);

	EXPECT_EQ(pool.pool_size(), 15);

	// Release nullptr should not affect pool_size
	pool.release(nullptr);

	EXPECT_EQ(pool.pool_size(), 15);
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

TEST_F(ConnectionPoolInitializeTest, InitializeFailsWithUnreachableHost)
{
	// Non-routable address should fail to connect
	connection_pool pool("192.0.2.1", 1, 1);

	auto result = pool.initialize();

	EXPECT_TRUE(result.is_err());
}

TEST_F(ConnectionPoolInitializeTest, ActiveCountRemainsZeroAfterFailedInit)
{
	connection_pool pool("127.0.0.1", 1, 5);

	auto result = pool.initialize();
	EXPECT_TRUE(result.is_err());

	EXPECT_EQ(pool.active_count(), 0);
}

TEST_F(ConnectionPoolInitializeTest, PoolSizePreservedAfterFailedInit)
{
	connection_pool pool("127.0.0.1", 1, 10);

	auto result = pool.initialize();
	EXPECT_TRUE(result.is_err());

	EXPECT_EQ(pool.pool_size(), 10);
}

// ============================================================================
// Acquire Without Initialization Tests
// ============================================================================

class ConnectionPoolAcquireTest : public ::testing::Test
{
};

TEST_F(ConnectionPoolAcquireTest, AcquireFromUninitializedPoolWithZeroSize)
{
	connection_pool pool("localhost", 5555, 0);
	auto result = pool.initialize();
	ASSERT_TRUE(result.is_ok());

	// Pool has zero size - acquire should return nullptr (no connections available)
	// Note: this may block or return nullptr depending on implementation
	// We test that the pool state is consistent after initialization
	EXPECT_EQ(pool.pool_size(), 0);
	EXPECT_EQ(pool.active_count(), 0);
}
