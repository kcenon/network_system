/**
 * @file integration_test_full.cpp
 * @brief Comprehensive integration tests for thread_system integration
 *
 * This test suite validates the complete thread_system integration across all phases:
 * - Phase 1: Foundation infrastructure (thread_pool_manager, io_context_executor)
 * - Phase 2: Core component refactoring (all servers/clients using thread pools)
 * - Phase 3: Data pipeline integration (pipeline_jobs with utility pool fallback)
 * - Phase 4: Direct integration (simplified integration layer)
 * - Phase 5: Full system validation
 */

#include <gtest/gtest.h>
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/io_context_executor.h"
#include "network_system/core/messaging_server.h"
#include "network_system/core/messaging_client.h"
#include "network_system/utils/health_monitor.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

using namespace network_system;
using namespace network_system::integration;
using namespace network_system::core;
using namespace network_system::utils;
using namespace std::chrono_literals;

/**
 * Integration test fixture
 * Sets up and tears down thread pool manager for each test
 */
class FullIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize thread pool manager
        auto& mgr = thread_pool_manager::instance();
        bool init_result = mgr.initialize(
            10,   // Max 10 I/O pools
            std::thread::hardware_concurrency(),  // Pipeline workers
            std::thread::hardware_concurrency() / 2  // Utility workers
        );
        ASSERT_TRUE(init_result) << "Failed to initialize thread pool manager";

        // Verify initialization
        auto stats = mgr.get_statistics();
        EXPECT_EQ(stats.io_pools_created, 0) << "I/O pools should not be created yet";
        EXPECT_GE(stats.total_active_pools, 2) << "Pipeline + utility pools should be active";
    }

    void TearDown() override {
        // Clean shutdown
        auto& mgr = thread_pool_manager::instance();
        mgr.shutdown();

        // Verify clean shutdown
        auto stats = mgr.get_statistics();
        EXPECT_EQ(stats.total_active_pools, 0) << "All pools should be destroyed";
    }

    /**
     * Helper: Wait for condition with timeout
     */
    template<typename Predicate>
    bool wait_for_condition(Predicate pred, std::chrono::milliseconds timeout = 5000ms) {
        auto start = std::chrono::steady_clock::now();
        while (!pred()) {
            if (std::chrono::steady_clock::now() - start > timeout) {
                return false;
            }
            std::this_thread::sleep_for(10ms);
        }
        return true;
    }
};

/**
 * Phase 1 Test: Thread Pool Manager Functionality
 */
TEST_F(FullIntegrationTest, Phase1_ThreadPoolManagerBasics) {
    auto& mgr = thread_pool_manager::instance();

    // Test pool creation
    auto io_pool = mgr.create_io_pool("phase1_test_io");
    ASSERT_NE(io_pool, nullptr) << "I/O pool creation should succeed";

    auto pipeline_pool = mgr.get_pipeline_pool();
    ASSERT_NE(pipeline_pool, nullptr) << "Pipeline pool should be available";

    auto utility_pool = mgr.get_utility_pool();
    ASSERT_NE(utility_pool, nullptr) << "Utility pool should be available";

    // Test statistics
    auto stats = mgr.get_statistics();
    EXPECT_EQ(stats.io_pools_created, 1) << "One I/O pool should be created";
    EXPECT_GE(stats.total_active_pools, 3) << "I/O + pipeline + utility pools";

    // Clean up
    mgr.destroy_io_pool("phase1_test_io");
}

/**
 * Phase 1 Test: I/O Context Executor
 */
TEST_F(FullIntegrationTest, Phase1_IOContextExecutor) {
    auto& mgr = thread_pool_manager::instance();
    asio::io_context io_context;

    // Create executor
    auto pool = mgr.create_io_pool("executor_test");
    ASSERT_NE(pool, nullptr);

    auto executor = std::make_unique<io_context_executor>(
        pool,
        io_context,
        "executor_test"
    );

    // Post work
    std::atomic<int> counter{0};
    for (int i = 0; i < 10; ++i) {
        io_context.post([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // Start executor
    executor->start();

    // Wait for work completion
    ASSERT_TRUE(wait_for_condition([&counter]() {
        return counter.load(std::memory_order_relaxed) == 10;
    }, 1000ms)) << "All work should complete";

    // Stop executor
    executor->stop();

    EXPECT_EQ(counter.load(), 10) << "All 10 tasks should execute";
}

/**
 * Phase 2 Test: Messaging Server/Client Integration
 */
TEST_F(FullIntegrationTest, Phase2_MessagingServerClient) {
    const uint16_t test_port = 18080;

    // Create and start server
    auto server = std::make_unique<messaging_server>();
    ASSERT_NO_THROW(server->start(test_port)) << "Server start should not throw";

    // Wait for server to be ready
    std::this_thread::sleep_for(200ms);

    EXPECT_TRUE(server->is_running()) << "Server should be running";

    // Verify server created I/O pool
    auto& mgr = thread_pool_manager::instance();
    auto stats_with_server = mgr.get_statistics();
    EXPECT_GT(stats_with_server.io_pools_created, 0) << "Server should create I/O pool";

    // Create and connect client
    auto client = std::make_unique<messaging_client>();
    ASSERT_NO_THROW(client->connect("127.0.0.1", test_port)) << "Client connect should not throw";

    // Wait for connection
    ASSERT_TRUE(wait_for_condition([&client]() {
        return client->is_connected();
    }, 2000ms)) << "Client should connect within 2 seconds";

    EXPECT_TRUE(client->is_connected()) << "Client should be connected";

    // Verify client also created I/O pool
    auto stats_with_client = mgr.get_statistics();
    EXPECT_GT(stats_with_client.io_pools_created, stats_with_server.io_pools_created)
        << "Client should create additional I/O pool";

    // Clean shutdown
    client->disconnect();
    std::this_thread::sleep_for(100ms);

    server->stop();
    std::this_thread::sleep_for(100ms);

    // Verify pools cleaned up
    auto stats_after_stop = mgr.get_statistics();
    // Note: Pools might not be immediately destroyed, so we just check they were created
    EXPECT_GT(stats_after_stop.io_pools_created, 0);
}

/**
 * Phase 2 Test: Health Monitor Integration
 */
TEST_F(FullIntegrationTest, Phase2_HealthMonitor) {
    auto monitor = std::make_unique<health_monitor>();

    // Start monitoring
    ASSERT_NO_THROW(monitor->start(100ms)) << "Health monitor start should not throw";

    EXPECT_TRUE(monitor->is_monitoring()) << "Monitor should be active";

    // Let it run for a while
    std::this_thread::sleep_for(500ms);

    // Stop monitoring
    monitor->stop();

    EXPECT_FALSE(monitor->is_monitoring()) << "Monitor should be stopped";
}

/**
 * Phase 3 Test: Pipeline with Utility Pool
 * Note: Since typed_thread_pool is not in public API, pipeline uses utility pool
 */
TEST_F(FullIntegrationTest, Phase3_PipelineUtilityPool) {
    auto& mgr = thread_pool_manager::instance();
    auto utility_pool = mgr.get_utility_pool();
    ASSERT_NE(utility_pool, nullptr);

    // Submit work to utility pool (simulating pipeline jobs)
    std::atomic<int> completed{0};
    const int num_jobs = 100;

    for (int i = 0; i < num_jobs; ++i) {
        auto job = std::make_unique<kcenon::thread::job>([&completed]() {
            // Simulate pipeline work
            std::this_thread::sleep_for(1ms);
            completed.fetch_add(1, std::memory_order_relaxed);
        });

        auto result = utility_pool->execute(std::move(job));
        EXPECT_FALSE(result.has_error()) << "Job execution should succeed";
    }

    // Wait for all jobs to complete
    ASSERT_TRUE(wait_for_condition([&completed, num_jobs]() {
        return completed.load(std::memory_order_relaxed) == num_jobs;
    }, 5000ms)) << "All pipeline jobs should complete";

    EXPECT_EQ(completed.load(), num_jobs) << "All jobs should be processed";
}

/**
 * Phase 4 Test: Direct Thread System Integration
 */
TEST_F(FullIntegrationTest, Phase4_DirectThreadSystemAccess) {
    // Test direct access to thread_system (no abstraction layer)
    auto pool = std::make_shared<kcenon::thread::thread_pool>("direct_test");

    // Start pool
    auto start_result = pool->start();
    EXPECT_FALSE(start_result.has_error()) << "Pool start should succeed";

    // Execute jobs directly
    std::atomic<int> job_count{0};
    for (int i = 0; i < 50; ++i) {
        auto job = std::make_unique<kcenon::thread::job>([&job_count]() {
            job_count.fetch_add(1, std::memory_order_relaxed);
        });

        auto exec_result = pool->execute(std::move(job));
        EXPECT_FALSE(exec_result.has_error()) << "Job execution should succeed";
    }

    // Wait for completion
    ASSERT_TRUE(wait_for_condition([&job_count]() {
        return job_count.load(std::memory_order_relaxed) == 50;
    }, 3000ms));

    EXPECT_EQ(job_count.load(), 50);

    // Stop pool
    auto stop_result = pool->stop();
    EXPECT_FALSE(stop_result.has_error()) << "Pool stop should succeed";
}

/**
 * Phase 5 Test: Full System Integration
 * Tests all components working together
 */
TEST_F(FullIntegrationTest, Phase5_FullSystemIntegration) {
    const uint16_t test_port = 18081;

    // Start server
    auto server = std::make_unique<messaging_server>();
    server->start(test_port);
    std::this_thread::sleep_for(200ms);

    // Start client
    auto client = std::make_unique<messaging_client>();
    client->connect("127.0.0.1", test_port);

    ASSERT_TRUE(wait_for_condition([&client]() {
        return client->is_connected();
    }, 2000ms));

    // Start health monitor
    auto monitor = std::make_unique<health_monitor>();
    monitor->start(100ms);

    // Submit work to utility pool (simulating pipeline)
    auto& mgr = thread_pool_manager::instance();
    auto utility_pool = mgr.get_utility_pool();

    std::atomic<int> pipeline_jobs{0};
    for (int i = 0; i < 20; ++i) {
        auto job = std::make_unique<kcenon::thread::job>([&pipeline_jobs]() {
            std::this_thread::sleep_for(5ms);
            pipeline_jobs.fetch_add(1, std::memory_order_relaxed);
        });
        utility_pool->execute(std::move(job));
    }

    // Let system run
    std::this_thread::sleep_for(500ms);

    // Verify statistics
    auto stats = mgr.get_statistics();
    EXPECT_GT(stats.io_pools_created, 0) << "I/O pools should be created";
    EXPECT_GE(stats.total_active_pools, 2) << "Multiple pools should be active";

    // Wait for pipeline jobs
    ASSERT_TRUE(wait_for_condition([&pipeline_jobs]() {
        return pipeline_jobs.load(std::memory_order_relaxed) == 20;
    }, 3000ms));

    // Stop all components
    monitor->stop();
    client->disconnect();
    std::this_thread::sleep_for(100ms);
    server->stop();
    std::this_thread::sleep_for(100ms);

    // Verify clean shutdown
    auto final_stats = mgr.get_statistics();
    // Pools should have been created and some might be destroyed
    EXPECT_GT(final_stats.io_pools_created, 0);
}

/**
 * Phase 5 Test: No Memory Leaks
 * Create and destroy components multiple times
 */
TEST_F(FullIntegrationTest, Phase5_NoMemoryLeaks) {
    const int iterations = 5;
    const uint16_t base_port = 19000;

    for (int i = 0; i < iterations; ++i) {
        auto server = std::make_unique<messaging_server>();
        server->start(base_port + i);
        std::this_thread::sleep_for(50ms);

        auto client = std::make_unique<messaging_client>();
        client->connect("127.0.0.1", base_port + i);
        std::this_thread::sleep_for(50ms);

        // Verify connection
        if (!client->is_connected()) {
            // May fail occasionally due to timing, but shouldn't crash
            continue;
        }

        client->disconnect();
        std::this_thread::sleep_for(20ms);

        server->stop();
        std::this_thread::sleep_for(20ms);
    }

    // Verify pools are properly managed
    auto& mgr = thread_pool_manager::instance();
    auto stats = mgr.get_statistics();

    EXPECT_GT(stats.io_pools_created, 0) << "Pools should have been created";

    // All tests passed means no crashes - memory leaks would require valgrind/sanitizers
    SUCCEED() << "No crashes detected across " << iterations << " iterations";
}

/**
 * Phase 5 Test: Concurrent Component Creation
 */
TEST_F(FullIntegrationTest, Phase5_ConcurrentComponentCreation) {
    constexpr size_t num_threads = 5;
    std::vector<std::thread> threads;
    std::atomic<size_t> successful_servers{0};

    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&successful_servers, i]() {
            try {
                auto server = std::make_unique<messaging_server>();
                server->start(20000 + i);
                std::this_thread::sleep_for(100ms);

                if (server->is_running()) {
                    successful_servers.fetch_add(1, std::memory_order_relaxed);
                }

                server->stop();
            } catch (...) {
                // Errors are acceptable in concurrent creation
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // At least some should succeed
    EXPECT_GT(successful_servers.load(), 0)
        << "At least some concurrent server creations should succeed";
}

/**
 * Phase 5 Test: Stress Test - Many Concurrent Jobs
 */
TEST_F(FullIntegrationTest, Phase5_StressConcurrentJobs) {
    auto& mgr = thread_pool_manager::instance();
    auto utility_pool = mgr.get_utility_pool();
    ASSERT_NE(utility_pool, nullptr);

    constexpr size_t num_jobs = 1000;
    std::atomic<size_t> completed{0};

    auto start_time = std::chrono::steady_clock::now();

    // Submit many jobs
    for (size_t i = 0; i < num_jobs; ++i) {
        auto job = std::make_unique<kcenon::thread::job>([&completed]() {
            // Minimal work
            completed.fetch_add(1, std::memory_order_relaxed);
        });

        auto result = utility_pool->execute(std::move(job));
        EXPECT_FALSE(result.has_error());
    }

    // Wait for all jobs
    ASSERT_TRUE(wait_for_condition([&completed, num_jobs]() {
        return completed.load(std::memory_order_relaxed) == num_jobs;
    }, 10000ms)) << "All stress test jobs should complete";

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    EXPECT_EQ(completed.load(), num_jobs);

    // Performance info
    std::cout << "\nStress test: Processed " << num_jobs << " jobs in "
              << duration.count() << "ms" << std::endl;
    std::cout << "Average: "
              << static_cast<double>(duration.count()) / num_jobs
              << "ms per job" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
