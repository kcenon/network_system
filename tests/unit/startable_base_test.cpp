/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/detail/utils/startable_base.h"
#include <gtest/gtest.h>

#include <atomic>
#include <string_view>
#include <thread>
#include <vector>

namespace utils = kcenon::network::utils;
using kcenon::network::VoidResult;

/**
 * @file startable_base_test.cpp
 * @brief Unit tests for startable_base<Derived> CRTP lifecycle template
 *
 * Tests validate:
 * - do_start() success when not running
 * - do_start() returns already_exists when already running
 * - do_start() rolls back state on implementation failure
 * - do_stop() idempotent when not running
 * - do_stop() calls do_stop_impl and on_stopped hook
 * - Atomic double-stop prevention
 * - is_running() and is_stop_initiated() state tracking
 * - Full lifecycle: start → stop → restart
 * - Variadic template argument forwarding in do_start
 * - Concurrent start/stop scenarios
 */

// ============================================================================
// Mock Derived Class
// ============================================================================

/**
 * @brief Mock component for testing startable_base CRTP template
 *
 * Tracks call counts and allows configurable failure injection.
 */
class mock_component : public utils::startable_base<mock_component>
{
public:
	static constexpr std::string_view component_name() { return "MockComponent"; }

	// Public wrappers for protected do_start/do_stop
	auto start() -> VoidResult { return do_start(); }

	auto start_with_args(std::string_view host, unsigned short port)
		-> VoidResult
	{
		return do_start(host, port);
	}

	auto stop() -> VoidResult { return do_stop(); }

	// State inspection
	int start_impl_calls() const { return start_impl_calls_; }
	int stop_impl_calls() const { return stop_impl_calls_; }
	int on_stopped_calls() const { return on_stopped_calls_; }
	std::string last_host() const { return last_host_; }
	unsigned short last_port() const { return last_port_; }
	bool stop_initiated() const { return is_stop_initiated(); }

	// Failure injection
	void set_start_should_fail(bool fail) { start_should_fail_ = fail; }
	void set_stop_should_fail(bool fail) { stop_should_fail_ = fail; }

protected:
	friend class utils::startable_base<mock_component>;

	auto do_start_impl() -> VoidResult
	{
		++start_impl_calls_;
		if (start_should_fail_)
		{
			return kcenon::network::error_void(
				kcenon::network::error_codes::common_errors::internal_error,
				"simulated start failure", "mock_component");
		}
		return kcenon::network::ok();
	}

	auto do_start_impl(std::string_view host, unsigned short port)
		-> VoidResult
	{
		last_host_ = std::string(host);
		last_port_ = port;
		++start_impl_calls_;
		if (start_should_fail_)
		{
			return kcenon::network::error_void(
				kcenon::network::error_codes::common_errors::internal_error,
				"simulated start failure", "mock_component");
		}
		return kcenon::network::ok();
	}

	auto do_stop_impl() -> VoidResult
	{
		++stop_impl_calls_;
		if (stop_should_fail_)
		{
			return kcenon::network::error_void(
				kcenon::network::error_codes::common_errors::internal_error,
				"simulated stop failure", "mock_component");
		}
		return kcenon::network::ok();
	}

	auto on_stopped() -> void { ++on_stopped_calls_; }

private:
	int start_impl_calls_{0};
	int stop_impl_calls_{0};
	int on_stopped_calls_{0};
	bool start_should_fail_{false};
	bool stop_should_fail_{false};
	std::string last_host_;
	unsigned short last_port_{0};
};

// ============================================================================
// Initial State Tests
// ============================================================================

class StartableBaseInitialStateTest : public ::testing::Test
{
};

TEST_F(StartableBaseInitialStateTest, NotRunningByDefault)
{
	mock_component comp;

	EXPECT_FALSE(comp.is_running());
}

TEST_F(StartableBaseInitialStateTest, NoCalls)
{
	mock_component comp;

	EXPECT_EQ(comp.start_impl_calls(), 0);
	EXPECT_EQ(comp.stop_impl_calls(), 0);
	EXPECT_EQ(comp.on_stopped_calls(), 0);
}

// ============================================================================
// do_start() Success Tests
// ============================================================================

class StartableBaseStartTest : public ::testing::Test
{
};

TEST_F(StartableBaseStartTest, StartSucceedsWhenNotRunning)
{
	mock_component comp;

	auto result = comp.start();

	EXPECT_TRUE(result.is_ok());
	EXPECT_TRUE(comp.is_running());
	EXPECT_EQ(comp.start_impl_calls(), 1);
}

TEST_F(StartableBaseStartTest, StartForwardsArguments)
{
	mock_component comp;

	auto result = comp.start_with_args("localhost", 8080);

	EXPECT_TRUE(result.is_ok());
	EXPECT_TRUE(comp.is_running());
	EXPECT_EQ(comp.last_host(), "localhost");
	EXPECT_EQ(comp.last_port(), 8080);
}

TEST_F(StartableBaseStartTest, StartForwardsDifferentArguments)
{
	mock_component comp;

	auto result = comp.start_with_args("192.168.1.1", 443);

	EXPECT_TRUE(result.is_ok());
	EXPECT_EQ(comp.last_host(), "192.168.1.1");
	EXPECT_EQ(comp.last_port(), 443);
}

// ============================================================================
// do_start() Error Tests
// ============================================================================

class StartableBaseStartErrorTest : public ::testing::Test
{
};

TEST_F(StartableBaseStartErrorTest, DoubleStartReturnsAlreadyExists)
{
	mock_component comp;
	auto first = comp.start();
	ASSERT_TRUE(first.is_ok());

	auto second = comp.start();

	EXPECT_TRUE(second.is_err());
	EXPECT_EQ(second.error().code,
			  kcenon::network::error_codes::common_errors::already_exists);
}

TEST_F(StartableBaseStartErrorTest, DoubleStartDoesNotCallImplAgain)
{
	mock_component comp;
	(void)comp.start();

	(void)comp.start();

	// do_start_impl should only be called once
	EXPECT_EQ(comp.start_impl_calls(), 1);
}

TEST_F(StartableBaseStartErrorTest, DoubleStartKeepsRunning)
{
	mock_component comp;
	(void)comp.start();

	(void)comp.start();

	EXPECT_TRUE(comp.is_running());
}

TEST_F(StartableBaseStartErrorTest, DoubleStartErrorMessageContainsComponentName)
{
	mock_component comp;
	(void)comp.start();

	auto result = comp.start();

	EXPECT_TRUE(result.is_err());
	EXPECT_NE(result.error().message.find("MockComponent"), std::string::npos);
}

// ============================================================================
// do_start() Rollback Tests
// ============================================================================

class StartableBaseStartRollbackTest : public ::testing::Test
{
};

TEST_F(StartableBaseStartRollbackTest, FailedStartRollsBackToNotRunning)
{
	mock_component comp;
	comp.set_start_should_fail(true);

	auto result = comp.start();

	EXPECT_TRUE(result.is_err());
	EXPECT_FALSE(comp.is_running());
}

TEST_F(StartableBaseStartRollbackTest, FailedStartStillCallsImpl)
{
	mock_component comp;
	comp.set_start_should_fail(true);

	(void)comp.start();

	EXPECT_EQ(comp.start_impl_calls(), 1);
}

TEST_F(StartableBaseStartRollbackTest, CanRetryStartAfterFailure)
{
	mock_component comp;
	comp.set_start_should_fail(true);

	auto first = comp.start();
	EXPECT_TRUE(first.is_err());
	EXPECT_FALSE(comp.is_running());

	// Fix the failure and retry
	comp.set_start_should_fail(false);
	auto second = comp.start();

	EXPECT_TRUE(second.is_ok());
	EXPECT_TRUE(comp.is_running());
	EXPECT_EQ(comp.start_impl_calls(), 2);
}

TEST_F(StartableBaseStartRollbackTest,
	   FailedStartWithArgsRollsBackToNotRunning)
{
	mock_component comp;
	comp.set_start_should_fail(true);

	auto result = comp.start_with_args("localhost", 9090);

	EXPECT_TRUE(result.is_err());
	EXPECT_FALSE(comp.is_running());
	// Arguments were still forwarded before failure
	EXPECT_EQ(comp.last_host(), "localhost");
	EXPECT_EQ(comp.last_port(), 9090);
}

// ============================================================================
// do_stop() Tests
// ============================================================================

class StartableBaseStopTest : public ::testing::Test
{
};

TEST_F(StartableBaseStopTest, StopWhenNotRunningReturnsOk)
{
	mock_component comp;

	auto result = comp.stop();

	EXPECT_TRUE(result.is_ok());
}

TEST_F(StartableBaseStopTest, StopWhenNotRunningDoesNotCallImpl)
{
	mock_component comp;

	(void)comp.stop();

	EXPECT_EQ(comp.stop_impl_calls(), 0);
	EXPECT_EQ(comp.on_stopped_calls(), 0);
}

TEST_F(StartableBaseStopTest, StopWhenRunningCallsImplAndOnStopped)
{
	mock_component comp;
	(void)comp.start();

	auto result = comp.stop();

	EXPECT_TRUE(result.is_ok());
	EXPECT_FALSE(comp.is_running());
	EXPECT_EQ(comp.stop_impl_calls(), 1);
	EXPECT_EQ(comp.on_stopped_calls(), 1);
}

TEST_F(StartableBaseStopTest, StopSetsNotRunning)
{
	mock_component comp;
	(void)comp.start();
	ASSERT_TRUE(comp.is_running());

	(void)comp.stop();

	EXPECT_FALSE(comp.is_running());
}

TEST_F(StartableBaseStopTest, StopCallsOnStoppedEvenWhenImplFails)
{
	mock_component comp;
	(void)comp.start();
	comp.set_stop_should_fail(true);

	auto result = comp.stop();

	// do_stop_impl failure is returned, but on_stopped is still called
	EXPECT_TRUE(result.is_err());
	EXPECT_FALSE(comp.is_running());
	EXPECT_EQ(comp.stop_impl_calls(), 1);
	EXPECT_EQ(comp.on_stopped_calls(), 1);
}

// ============================================================================
// Double Stop Prevention Tests
// ============================================================================

class StartableBaseDoubleStopTest : public ::testing::Test
{
};

TEST_F(StartableBaseDoubleStopTest, SecondStopIsNoOp)
{
	mock_component comp;
	(void)comp.start();

	auto first = comp.stop();
	EXPECT_TRUE(first.is_ok());

	auto second = comp.stop();
	EXPECT_TRUE(second.is_ok());

	// do_stop_impl should only be called once
	EXPECT_EQ(comp.stop_impl_calls(), 1);
	EXPECT_EQ(comp.on_stopped_calls(), 1);
}

TEST_F(StartableBaseDoubleStopTest, ConcurrentStopsOnlyOneProceeds)
{
	mock_component comp;
	(void)comp.start();

	constexpr int thread_count = 10;
	std::vector<std::thread> threads;
	threads.reserve(thread_count);
	std::atomic<int> ok_count{0};

	for (int i = 0; i < thread_count; ++i)
	{
		threads.emplace_back([&comp, &ok_count]() {
			auto result = comp.stop();
			if (result.is_ok())
			{
				++ok_count;
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// All returns should be ok (idempotent), but impl called only once
	EXPECT_EQ(ok_count.load(), thread_count);
	EXPECT_EQ(comp.stop_impl_calls(), 1);
	EXPECT_EQ(comp.on_stopped_calls(), 1);
	EXPECT_FALSE(comp.is_running());
}

// ============================================================================
// Full Lifecycle Tests
// ============================================================================

class StartableBaseLifecycleTest : public ::testing::Test
{
};

TEST_F(StartableBaseLifecycleTest, StartStopCycle)
{
	mock_component comp;

	EXPECT_FALSE(comp.is_running());

	auto start_result = comp.start();
	EXPECT_TRUE(start_result.is_ok());
	EXPECT_TRUE(comp.is_running());

	auto stop_result = comp.stop();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(comp.is_running());
}

TEST_F(StartableBaseLifecycleTest, RestartAfterStop)
{
	mock_component comp;

	(void)comp.start();
	(void)comp.stop();

	auto result = comp.start();

	EXPECT_TRUE(result.is_ok());
	EXPECT_TRUE(comp.is_running());
	EXPECT_EQ(comp.start_impl_calls(), 2);
}

TEST_F(StartableBaseLifecycleTest, MultipleStartStopCycles)
{
	mock_component comp;

	for (int i = 0; i < 5; ++i)
	{
		auto start_result = comp.start();
		EXPECT_TRUE(start_result.is_ok()) << "Cycle " << i;
		EXPECT_TRUE(comp.is_running()) << "Cycle " << i;

		auto stop_result = comp.stop();
		EXPECT_TRUE(stop_result.is_ok()) << "Cycle " << i;
		EXPECT_FALSE(comp.is_running()) << "Cycle " << i;
	}

	EXPECT_EQ(comp.start_impl_calls(), 5);
	EXPECT_EQ(comp.stop_impl_calls(), 5);
	EXPECT_EQ(comp.on_stopped_calls(), 5);
}

TEST_F(StartableBaseLifecycleTest, StartWithArgsStopRestart)
{
	mock_component comp;

	(void)comp.start_with_args("host1", 1000);
	EXPECT_EQ(comp.last_host(), "host1");
	EXPECT_EQ(comp.last_port(), 1000);

	(void)comp.stop();

	(void)comp.start_with_args("host2", 2000);
	EXPECT_EQ(comp.last_host(), "host2");
	EXPECT_EQ(comp.last_port(), 2000);
	EXPECT_TRUE(comp.is_running());
}

// ============================================================================
// Concurrent Start Tests
// ============================================================================

class StartableBaseConcurrentStartTest : public ::testing::Test
{
};

TEST_F(StartableBaseConcurrentStartTest, OnlyOneStartSucceeds)
{
	mock_component comp;
	constexpr int thread_count = 10;
	std::vector<std::thread> threads;
	threads.reserve(thread_count);
	std::atomic<int> success_count{0};

	for (int i = 0; i < thread_count; ++i)
	{
		threads.emplace_back([&comp, &success_count]() {
			auto result = comp.start();
			if (result.is_ok())
			{
				++success_count;
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_EQ(success_count.load(), 1);
	EXPECT_TRUE(comp.is_running());
	EXPECT_EQ(comp.start_impl_calls(), 1);
}

// ============================================================================
// is_stop_initiated() Tests
// ============================================================================

class StartableBaseStopInitiatedTest : public ::testing::Test
{
};

TEST_F(StartableBaseStopInitiatedTest, NotInitiatedByDefault)
{
	mock_component comp;

	EXPECT_FALSE(comp.stop_initiated());
}

TEST_F(StartableBaseStopInitiatedTest, NotInitiatedAfterStart)
{
	mock_component comp;
	(void)comp.start();

	EXPECT_FALSE(comp.stop_initiated());
}

// ============================================================================
// get_lifecycle() Access Tests
// ============================================================================

/**
 * @brief Mock that exposes lifecycle access for testing
 */
class lifecycle_access_mock
	: public utils::startable_base<lifecycle_access_mock>
{
public:
	static constexpr std::string_view component_name()
	{
		return "LifecycleAccessMock";
	}

	auto start() -> VoidResult { return do_start(); }

	auto stop() -> VoidResult { return do_stop(); }

	// Expose lifecycle for testing
	auto lifecycle() -> utils::lifecycle_manager&
	{
		return get_lifecycle();
	}

	auto lifecycle() const -> const utils::lifecycle_manager&
	{
		return get_lifecycle();
	}

protected:
	friend class utils::startable_base<lifecycle_access_mock>;

	auto do_start_impl() -> VoidResult { return kcenon::network::ok(); }

	auto do_stop_impl() -> VoidResult { return kcenon::network::ok(); }

	auto on_stopped() -> void {}
};

class StartableBaseLifecycleAccessTest : public ::testing::Test
{
};

TEST_F(StartableBaseLifecycleAccessTest, LifecycleManagerAccessible)
{
	lifecycle_access_mock comp;

	EXPECT_FALSE(comp.lifecycle().is_running());
}

TEST_F(StartableBaseLifecycleAccessTest,
	   LifecycleReflectsStartableBaseState)
{
	lifecycle_access_mock comp;

	(void)comp.start();
	EXPECT_TRUE(comp.lifecycle().is_running());

	(void)comp.stop();
	EXPECT_FALSE(comp.lifecycle().is_running());
}

TEST_F(StartableBaseLifecycleAccessTest, ConstLifecycleAccessWorks)
{
	const lifecycle_access_mock comp;

	EXPECT_FALSE(comp.lifecycle().is_running());
}

// ============================================================================
// wait_for_stop() Integration Tests
// ============================================================================

class StartableBaseWaitForStopTest : public ::testing::Test
{
};

TEST_F(StartableBaseWaitForStopTest, WaitForStopDoesNotBlockWhenNotRunning)
{
	mock_component comp;

	// Should return immediately
	auto start_time = std::chrono::steady_clock::now();
	comp.wait_for_stop();
	auto elapsed = std::chrono::steady_clock::now() - start_time;

	EXPECT_LT(elapsed, std::chrono::milliseconds(100));
}

// ============================================================================
// Error Propagation Tests
// ============================================================================

class StartableBaseErrorPropagationTest : public ::testing::Test
{
};

TEST_F(StartableBaseErrorPropagationTest, StartErrorCodePropagated)
{
	mock_component comp;
	comp.set_start_should_fail(true);

	auto result = comp.start();

	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code,
			  kcenon::network::error_codes::common_errors::internal_error);
}

TEST_F(StartableBaseErrorPropagationTest, StopErrorCodePropagated)
{
	mock_component comp;
	(void)comp.start();
	comp.set_stop_should_fail(true);

	auto result = comp.stop();

	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code,
			  kcenon::network::error_codes::common_errors::internal_error);
}

TEST_F(StartableBaseErrorPropagationTest, AlreadyExistsErrorHasSource)
{
	mock_component comp;
	(void)comp.start();

	auto result = comp.start();

	EXPECT_TRUE(result.is_err());
	auto source = kcenon::network::get_error_source(result.error());
	EXPECT_FALSE(source.empty());
}
