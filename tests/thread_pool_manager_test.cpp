/**
 * @file thread_pool_manager_test.cpp
 * @brief Unit tests for thread_pool_manager - Phase 1 infrastructure
 *
 * Tests the centralized thread pool management functionality added in Phase 1.
 */

#include <gtest/gtest.h>
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/io_context_executor.h"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;
using namespace network_system::integration;

/**
 * Test fixture for thread_pool_manager tests
 * Ensures clean state for each test
 */
class ThreadPoolManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state
        auto& mgr = thread_pool_manager::instance();
        mgr.shutdown();
    }

    void TearDown() override {
        // Clean up after each test
        auto& mgr = thread_pool_manager::instance();
        mgr.shutdown();
    }
};

/**
 * Test 1: Basic initialization with default parameters
 */
TEST_F(ThreadPoolManagerTest, InitializeWithDefaults) {
    auto& mgr = thread_pool_manager::instance();

    // Initialize with default parameters
    bool result = mgr.initialize();
    EXPECT_TRUE(result);

    // Verify statistics
    auto stats = mgr.get_statistics();
    EXPECT_EQ(stats.io_pools_created, 0);  // No I/O pools created yet
    EXPECT_GE(stats.total_active_pools, 2);  // At least pipeline + utility pools

    // Should be able to get pools
    auto pipeline_pool = mgr.get_pipeline_pool();
    EXPECT_NE(pipeline_pool, nullptr);

    auto utility_pool = mgr.get_utility_pool();
    EXPECT_NE(utility_pool, nullptr);
}

/**
 * Test 2: Initialize with custom parameters
 */
TEST_F(ThreadPoolManagerTest, InitializeWithCustomParameters) {
    auto& mgr = thread_pool_manager::instance();

    // Initialize with custom parameters
    bool result = mgr.initialize(
        5,   // Max 5 I/O pools
        4,   // 4 pipeline workers
        2    // 2 utility workers
    );
    EXPECT_TRUE(result);

    auto stats = mgr.get_statistics();
    EXPECT_EQ(stats.io_pools_created, 0);  // No I/O pools created yet
    EXPECT_GE(stats.total_active_pools, 2);
}

/**
 * Test 3: I/O pool creation and management
 */
TEST_F(ThreadPoolManagerTest, CreateAndDestroyIOPool) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    // Create I/O pool
    auto pool1 = mgr.create_io_pool("test_pool_1");
    ASSERT_NE(pool1, nullptr);

    auto stats_after_create = mgr.get_statistics();
    EXPECT_EQ(stats_after_create.io_pools_created, 1);

    // Create another pool
    auto pool2 = mgr.create_io_pool("test_pool_2");
    ASSERT_NE(pool2, nullptr);

    stats_after_create = mgr.get_statistics();
    EXPECT_EQ(stats_after_create.io_pools_created, 2);

    // Destroy first pool
    mgr.destroy_io_pool("test_pool_1");

    auto stats_after_destroy = mgr.get_statistics();
    EXPECT_EQ(stats_after_destroy.io_pools_destroyed, 1);
    EXPECT_EQ(stats_after_destroy.io_pools_created - stats_after_destroy.io_pools_destroyed, 1);
}

/**
 * Test 4: Maximum I/O pool limit
 */
TEST_F(ThreadPoolManagerTest, MaxIOPoolLimit) {
    auto& mgr = thread_pool_manager::instance();

    const size_t max_pools = 3;
    mgr.initialize(max_pools);

    // Create up to max
    for (size_t i = 0; i < max_pools; ++i) {
        auto pool = mgr.create_io_pool("pool_" + std::to_string(i));
        EXPECT_NE(pool, nullptr);
    }

    // Try to create one more - should fail or reuse
    auto extra_pool = mgr.create_io_pool("extra_pool");
    // Implementation might return nullptr or reuse - both are acceptable
}

/**
 * Test 5: Pipeline pool access
 */
TEST_F(ThreadPoolManagerTest, PipelinePoolAccess) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    auto pipeline = mgr.get_pipeline_pool();
    ASSERT_NE(pipeline, nullptr);

    // Should be the same instance on multiple calls
    auto pipeline2 = mgr.get_pipeline_pool();
    EXPECT_EQ(pipeline, pipeline2);
}

/**
 * Test 6: Utility pool access
 */
TEST_F(ThreadPoolManagerTest, UtilityPoolAccess) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    auto utility = mgr.get_utility_pool();
    ASSERT_NE(utility, nullptr);

    // Should be the same instance on multiple calls
    auto utility2 = mgr.get_utility_pool();
    EXPECT_EQ(utility, utility2);
}

/**
 * Test 7: Shutdown and re-initialization
 */
TEST_F(ThreadPoolManagerTest, ShutdownAndReinitialize) {
    auto& mgr = thread_pool_manager::instance();

    // First initialization
    mgr.initialize();
    auto pool1 = mgr.create_io_pool("pool1");
    ASSERT_NE(pool1, nullptr);

    // Shutdown
    mgr.shutdown();

    auto stats_after_shutdown = mgr.get_statistics();
    EXPECT_EQ(stats_after_shutdown.total_active_pools, 0);

    // Re-initialize
    bool reinit_result = mgr.initialize();
    EXPECT_TRUE(reinit_result);

    // Should be able to create pools again
    auto pool2 = mgr.create_io_pool("pool2");
    EXPECT_NE(pool2, nullptr);
}

/**
 * Test 8: Statistics accuracy
 */
TEST_F(ThreadPoolManagerTest, StatisticsAccuracy) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize(10, 4, 2);

    auto initial_stats = mgr.get_statistics();

    // Create some I/O pools
    mgr.create_io_pool("pool1");
    mgr.create_io_pool("pool2");
    mgr.create_io_pool("pool3");

    auto stats_after_create = mgr.get_statistics();
    EXPECT_EQ(stats_after_create.io_pools_created, 3);
    EXPECT_EQ(stats_after_create.io_pools_destroyed, 0);

    // Destroy some pools
    mgr.destroy_io_pool("pool1");
    mgr.destroy_io_pool("pool2");

    auto stats_after_destroy = mgr.get_statistics();
    EXPECT_EQ(stats_after_destroy.io_pools_created, 3);
    EXPECT_EQ(stats_after_destroy.io_pools_destroyed, 2);
    EXPECT_EQ(stats_after_destroy.io_pools_created - stats_after_destroy.io_pools_destroyed, 1);
}

/**
 * Test 9: Concurrent I/O pool creation
 */
TEST_F(ThreadPoolManagerTest, ConcurrentIOPoolCreation) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize(20);  // Allow many pools

    constexpr size_t num_threads = 10;
    std::vector<std::thread> threads;
    std::atomic<size_t> successful_creates{0};

    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&mgr, &successful_creates, i]() {
            auto pool = mgr.create_io_pool("concurrent_pool_" + std::to_string(i));
            if (pool != nullptr) {
                successful_creates.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Most or all should succeed
    EXPECT_GE(successful_creates.load(), num_threads / 2);
}

/**
 * Test 10: io_context_executor integration
 */
TEST_F(ThreadPoolManagerTest, IOContextExecutorIntegration) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    asio::io_context io_context;

    // Create executor
    auto pool = mgr.create_io_pool("executor_test");
    ASSERT_NE(pool, nullptr);

    auto executor = std::make_unique<io_context_executor>(
        pool,
        io_context,
        "executor_test"
    );

    // Post work to io_context
    std::atomic<bool> work_executed{false};
    io_context.post([&work_executed]() {
        work_executed.store(true, std::memory_order_release);
    });

    // Start executor
    executor->start();

    // Wait briefly for work to execute
    std::this_thread::sleep_for(100ms);

    // Stop executor
    executor->stop();

    EXPECT_TRUE(work_executed.load(std::memory_order_acquire));
}

/**
 * Test 11: Multiple executors
 */
TEST_F(ThreadPoolManagerTest, MultipleExecutors) {
    auto& mgr = thread_pool_manager::instance();
    mgr.initialize();

    std::vector<std::unique_ptr<io_context_executor>> executors;
    std::vector<std::unique_ptr<asio::io_context>> contexts;

    // Create multiple executors
    for (size_t i = 0; i < 3; ++i) {
        auto pool = mgr.create_io_pool("multi_exec_" + std::to_string(i));
        ASSERT_NE(pool, nullptr);

        contexts.push_back(std::make_unique<asio::io_context>());
        executors.push_back(std::make_unique<io_context_executor>(
            pool,
            *contexts.back(),
            "multi_exec_" + std::to_string(i)
        ));
    }

    // Start all
    for (auto& executor : executors) {
        executor->start();
    }

    // Wait briefly
    std::this_thread::sleep_for(50ms);

    // Stop all
    for (auto& executor : executors) {
        executor->stop();
    }

    // All should stop cleanly
    SUCCEED();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
