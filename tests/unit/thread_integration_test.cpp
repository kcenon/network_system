/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/integration/thread_integration.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

namespace integration = kcenon::network::integration;

/**
 * @file thread_integration_test.cpp
 * @brief Unit tests for thread_integration module
 *
 * Tests validate:
 * - basic_thread_pool construction and configuration
 * - Task submission and execution
 * - Delayed task submission
 * - Worker count and running state queries
 * - thread_integration_manager singleton access
 * - thread_integration_manager task submission
 * - thread_integration_manager metrics
 */

// ============================================================================
// basic_thread_pool Constructor Tests
// ============================================================================

class BasicThreadPoolConstructorTest : public ::testing::Test
{
};

TEST_F(BasicThreadPoolConstructorTest, DefaultConstruction)
{
	integration::basic_thread_pool pool;

	EXPECT_TRUE(pool.is_running());
	EXPECT_GT(pool.worker_count(), 0u);
}

TEST_F(BasicThreadPoolConstructorTest, SpecifiedThreadCount)
{
	integration::basic_thread_pool pool(2);

	EXPECT_TRUE(pool.is_running());
	EXPECT_EQ(pool.worker_count(), 2u);
}

TEST_F(BasicThreadPoolConstructorTest, InitialPendingTasksZero)
{
	integration::basic_thread_pool pool(1);

	// No tasks submitted yet
	EXPECT_EQ(pool.pending_tasks(), 0u);
}

// ============================================================================
// basic_thread_pool Task Submission Tests
// ============================================================================

class BasicThreadPoolSubmitTest : public ::testing::Test
{
protected:
	integration::basic_thread_pool pool_{2};
};

TEST_F(BasicThreadPoolSubmitTest, SubmitTaskExecutes)
{
	std::atomic<bool> executed{false};

	auto future = pool_.submit([&executed]() {
		executed.store(true);
	});

	future.wait();
	EXPECT_TRUE(executed.load());
}

TEST_F(BasicThreadPoolSubmitTest, SubmitMultipleTasks)
{
	std::atomic<int> counter{0};
	std::vector<std::future<void>> futures;

	for (int i = 0; i < 10; ++i)
	{
		futures.push_back(pool_.submit([&counter]() {
			counter.fetch_add(1);
		}));
	}

	for (auto& f : futures)
	{
		f.wait();
	}

	EXPECT_EQ(counter.load(), 10);
}

TEST_F(BasicThreadPoolSubmitTest, SubmitDelayedTask)
{
	std::atomic<bool> executed{false};
	auto start = std::chrono::steady_clock::now();

	auto future = pool_.submit_delayed(
		[&executed]() { executed.store(true); },
		std::chrono::milliseconds(50));

	future.wait();
	auto elapsed = std::chrono::steady_clock::now() - start;

	EXPECT_TRUE(executed.load());
	EXPECT_GE(elapsed, std::chrono::milliseconds(40));
}

// ============================================================================
// basic_thread_pool Lifecycle Tests
// ============================================================================

class BasicThreadPoolLifecycleTest : public ::testing::Test
{
};

TEST_F(BasicThreadPoolLifecycleTest, StopPool)
{
	auto pool = std::make_unique<integration::basic_thread_pool>(2);
	EXPECT_TRUE(pool->is_running());

	pool->stop();

	EXPECT_FALSE(pool->is_running());
}

TEST_F(BasicThreadPoolLifecycleTest, CompletedTasksCount)
{
	integration::basic_thread_pool pool(1);
	std::atomic<int> counter{0};

	auto f1 = pool.submit([&counter]() { counter.fetch_add(1); });
	auto f2 = pool.submit([&counter]() { counter.fetch_add(1); });

	f1.wait();
	f2.wait();

	// Give a brief moment for internal counter to update
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	EXPECT_GE(pool.completed_tasks(), 2u);
}

// ============================================================================
// thread_integration_manager Tests
// ============================================================================

class ThreadIntegrationManagerTest : public ::testing::Test
{
};

TEST_F(ThreadIntegrationManagerTest, SingletonAccess)
{
	auto& mgr1 = integration::thread_integration_manager::instance();
	auto& mgr2 = integration::thread_integration_manager::instance();

	EXPECT_EQ(&mgr1, &mgr2);
}

TEST_F(ThreadIntegrationManagerTest, GetThreadPoolNotNull)
{
	auto& mgr = integration::thread_integration_manager::instance();

	auto pool = mgr.get_thread_pool();

	EXPECT_NE(pool, nullptr);
}

TEST_F(ThreadIntegrationManagerTest, SubmitTask)
{
	auto& mgr = integration::thread_integration_manager::instance();
	std::atomic<bool> executed{false};

	auto future = mgr.submit_task([&executed]() {
		executed.store(true);
	});

	future.wait();
	EXPECT_TRUE(executed.load());
}

TEST_F(ThreadIntegrationManagerTest, SubmitDelayedTask)
{
	auto& mgr = integration::thread_integration_manager::instance();
	std::atomic<bool> executed{false};

	auto future = mgr.submit_delayed_task(
		[&executed]() { executed.store(true); },
		std::chrono::milliseconds(10));

	future.wait();
	EXPECT_TRUE(executed.load());
}

TEST_F(ThreadIntegrationManagerTest, GetMetrics)
{
	auto& mgr = integration::thread_integration_manager::instance();

	auto metrics = mgr.get_metrics();

	EXPECT_GT(metrics.worker_threads, 0u);
	EXPECT_TRUE(metrics.is_running);
}

TEST_F(ThreadIntegrationManagerTest, SetCustomThreadPool)
{
	auto& mgr = integration::thread_integration_manager::instance();
	auto custom_pool = std::make_shared<integration::basic_thread_pool>(1);

	mgr.set_thread_pool(custom_pool);

	auto retrieved = mgr.get_thread_pool();
	EXPECT_EQ(retrieved->worker_count(), 1u);
}
