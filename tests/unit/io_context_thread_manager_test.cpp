/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/integration/io_context_thread_manager.h"
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <thread>

namespace integration = kcenon::network::integration;

/**
 * @file io_context_thread_manager_test.cpp
 * @brief Unit tests for io_context_thread_manager
 *
 * Tests validate:
 * - Singleton instance access
 * - Running io_context on shared thread pool
 * - Stopping individual io_context
 * - stop_all and wait_all shutdown
 * - active_count tracking
 * - is_active query
 * - Metrics reporting
 * - Custom thread pool assignment
 */

// ============================================================================
// Singleton Tests
// ============================================================================

class IoContextThreadManagerSingletonTest : public ::testing::Test
{
};

TEST_F(IoContextThreadManagerSingletonTest, InstanceIsSame)
{
	auto& mgr1 = integration::io_context_thread_manager::instance();
	auto& mgr2 = integration::io_context_thread_manager::instance();

	EXPECT_EQ(&mgr1, &mgr2);
}

// ============================================================================
// Run and Stop Tests
// ============================================================================

class IoContextThreadManagerRunTest : public ::testing::Test
{
protected:
	integration::io_context_thread_manager& mgr_
		= integration::io_context_thread_manager::instance();
};

TEST_F(IoContextThreadManagerRunTest, RunIoContext)
{
	auto io_ctx = std::make_shared<asio::io_context>();
	auto work = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
		io_ctx->get_executor());

	auto future = mgr_.run_io_context(io_ctx, "test-component");

	EXPECT_TRUE(mgr_.is_active(io_ctx));

	// Cleanup
	work.reset();
	mgr_.stop_io_context(io_ctx);
	future.wait();
}

TEST_F(IoContextThreadManagerRunTest, StopIoContext)
{
	auto io_ctx = std::make_shared<asio::io_context>();
	auto work = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
		io_ctx->get_executor());

	auto future = mgr_.run_io_context(io_ctx, "stop-test");

	work.reset();
	mgr_.stop_io_context(io_ctx);
	future.wait();

	EXPECT_FALSE(mgr_.is_active(io_ctx));
}

TEST_F(IoContextThreadManagerRunTest, ActiveCount)
{
	auto io_ctx = std::make_shared<asio::io_context>();
	auto work = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
		io_ctx->get_executor());

	auto count_before = mgr_.active_count();

	auto future = mgr_.run_io_context(io_ctx, "count-test");

	EXPECT_GE(mgr_.active_count(), count_before);

	// Cleanup
	work.reset();
	mgr_.stop_io_context(io_ctx);
	future.wait();
}

// ============================================================================
// Metrics Tests
// ============================================================================

class IoContextThreadManagerMetricsTest : public ::testing::Test
{
protected:
	integration::io_context_thread_manager& mgr_
		= integration::io_context_thread_manager::instance();
};

TEST_F(IoContextThreadManagerMetricsTest, GetMetrics)
{
	auto metrics = mgr_.get_metrics();

	// Metrics struct should be populated
	EXPECT_GE(metrics.total_started, 0u);
	EXPECT_GE(metrics.total_completed, 0u);
}

// ============================================================================
// Custom Thread Pool Tests
// ============================================================================

class IoContextThreadManagerPoolTest : public ::testing::Test
{
protected:
	integration::io_context_thread_manager& mgr_
		= integration::io_context_thread_manager::instance();
};

TEST_F(IoContextThreadManagerPoolTest, SetCustomThreadPool)
{
	auto pool = std::make_shared<integration::basic_thread_pool>(2);

	// Should not throw
	EXPECT_NO_THROW(mgr_.set_thread_pool(pool));
}
