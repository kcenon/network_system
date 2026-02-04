/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "kcenon/network/utils/callback_manager.h"
#include "internal/utils/connection_state.h"
#include "kcenon/network/utils/lifecycle_manager.h"

using namespace kcenon::network::utils;

// ============================================================================
// LifecycleManager Tests
// ============================================================================

class LifecycleManagerTest : public ::testing::Test
{
protected:
	void SetUp() override {}
	void TearDown() override {}
};

TEST_F(LifecycleManagerTest, InitialStateIsNotRunning)
{
	lifecycle_manager manager;
	EXPECT_FALSE(manager.is_running());
}

TEST_F(LifecycleManagerTest, TryStartSucceedsWhenNotRunning)
{
	lifecycle_manager manager;
	EXPECT_TRUE(manager.try_start());
	EXPECT_TRUE(manager.is_running());
}

TEST_F(LifecycleManagerTest, TryStartFailsWhenAlreadyRunning)
{
	lifecycle_manager manager;
	EXPECT_TRUE(manager.try_start());
	EXPECT_FALSE(manager.try_start());
	EXPECT_TRUE(manager.is_running());
}

TEST_F(LifecycleManagerTest, MarkStoppedSetsNotRunning)
{
	lifecycle_manager manager;
	(void)manager.try_start();
	EXPECT_TRUE(manager.is_running());

	manager.mark_stopped();
	EXPECT_FALSE(manager.is_running());
}

TEST_F(LifecycleManagerTest, PrepareStopSucceedsWhenRunning)
{
	lifecycle_manager manager;
	(void)manager.try_start();
	EXPECT_TRUE(manager.prepare_stop());
}

TEST_F(LifecycleManagerTest, PrepareStopFailsWhenNotRunning)
{
	lifecycle_manager manager;
	EXPECT_FALSE(manager.prepare_stop());
}

TEST_F(LifecycleManagerTest, PrepareStopFailsWhenAlreadyStopping)
{
	lifecycle_manager manager;
	(void)manager.try_start();
	EXPECT_TRUE(manager.prepare_stop());
	EXPECT_FALSE(manager.prepare_stop());
}

TEST_F(LifecycleManagerTest, WaitForStopCompletesAfterMarkStopped)
{
	lifecycle_manager manager;
	(void)manager.try_start();
	(void)manager.prepare_stop();

	std::thread stopper([&manager]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		manager.mark_stopped();
	});

	auto start = std::chrono::steady_clock::now();
	manager.wait_for_stop();
	auto elapsed = std::chrono::steady_clock::now() - start;

	EXPECT_GE(elapsed, std::chrono::milliseconds(40));
	EXPECT_FALSE(manager.is_running());

	stopper.join();
}

TEST_F(LifecycleManagerTest, ResetAllowsReuse)
{
	lifecycle_manager manager;
	(void)manager.try_start();
	(void)manager.prepare_stop();
	manager.mark_stopped();

	manager.reset();

	EXPECT_FALSE(manager.is_running());
	EXPECT_TRUE(manager.try_start());
	EXPECT_TRUE(manager.is_running());
}

TEST_F(LifecycleManagerTest, SetRunningDirectlyWorks)
{
	lifecycle_manager manager;
	manager.set_running();
	EXPECT_TRUE(manager.is_running());
}

TEST_F(LifecycleManagerTest, MoveConstructorTransfersState)
{
	lifecycle_manager manager1;
	(void)manager1.try_start();

	lifecycle_manager manager2(std::move(manager1));

	EXPECT_TRUE(manager2.is_running());
}

TEST_F(LifecycleManagerTest, MoveAssignmentTransfersState)
{
	lifecycle_manager manager1;
	(void)manager1.try_start();

	lifecycle_manager manager2;
	manager2 = std::move(manager1);

	EXPECT_TRUE(manager2.is_running());
}

TEST_F(LifecycleManagerTest, ConcurrentTryStartOnlyOneSucceeds)
{
	lifecycle_manager manager;
	std::atomic<int> success_count{0};
	constexpr int thread_count = 10;
	std::vector<std::thread> threads;
	threads.reserve(thread_count);

	for (int i = 0; i < thread_count; ++i)
	{
		threads.emplace_back([&manager, &success_count]() {
			if (manager.try_start())
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
	EXPECT_TRUE(manager.is_running());
}

// ============================================================================
// ConnectionState Tests
// ============================================================================

class ConnectionStateTest : public ::testing::Test
{
protected:
	void SetUp() override {}
	void TearDown() override {}
};

TEST_F(ConnectionStateTest, InitialStateIsDisconnected)
{
	connection_state state;
	EXPECT_EQ(state.status(), connection_status::disconnected);
	EXPECT_TRUE(state.is_disconnected());
	EXPECT_FALSE(state.is_connecting());
	EXPECT_FALSE(state.is_connected());
	EXPECT_FALSE(state.is_disconnecting());
}

TEST_F(ConnectionStateTest, SetConnectingFromDisconnected)
{
	connection_state state;
	EXPECT_TRUE(state.set_connecting());
	EXPECT_EQ(state.status(), connection_status::connecting);
	EXPECT_TRUE(state.is_connecting());
}

TEST_F(ConnectionStateTest, SetConnectingFailsWhenNotDisconnected)
{
	connection_state state;
	(void)state.set_connecting();
	EXPECT_FALSE(state.set_connecting());

	connection_state state2;
	(void)state2.set_connecting();
	state2.set_connected();
	EXPECT_FALSE(state2.set_connecting());
}

TEST_F(ConnectionStateTest, SetConnectedWorks)
{
	connection_state state;
	(void)state.set_connecting();
	state.set_connected();
	EXPECT_EQ(state.status(), connection_status::connected);
	EXPECT_TRUE(state.is_connected());
}

TEST_F(ConnectionStateTest, SetDisconnectingFromConnected)
{
	connection_state state;
	(void)state.set_connecting();
	state.set_connected();
	EXPECT_TRUE(state.set_disconnecting());
	EXPECT_EQ(state.status(), connection_status::disconnecting);
	EXPECT_TRUE(state.is_disconnecting());
}

TEST_F(ConnectionStateTest, SetDisconnectingFailsWhenNotConnected)
{
	connection_state state;
	EXPECT_FALSE(state.set_disconnecting());

	connection_state state2;
	(void)state2.set_connecting();
	EXPECT_FALSE(state2.set_disconnecting());
}

TEST_F(ConnectionStateTest, SetDisconnectedFromAnyState)
{
	connection_state state;
	(void)state.set_connecting();
	state.set_connected();
	(void)state.set_disconnecting();
	state.set_disconnected();

	EXPECT_EQ(state.status(), connection_status::disconnected);
	EXPECT_TRUE(state.is_disconnected());
}

TEST_F(ConnectionStateTest, ResetSetsDisconnected)
{
	connection_state state;
	(void)state.set_connecting();
	state.set_connected();
	state.reset();

	EXPECT_TRUE(state.is_disconnected());
}

TEST_F(ConnectionStateTest, MoveConstructorTransfersState)
{
	connection_state state1;
	(void)state1.set_connecting();
	state1.set_connected();

	connection_state state2(std::move(state1));

	EXPECT_TRUE(state2.is_connected());
}

TEST_F(ConnectionStateTest, MoveAssignmentTransfersState)
{
	connection_state state1;
	(void)state1.set_connecting();
	state1.set_connected();

	connection_state state2;
	state2 = std::move(state1);

	EXPECT_TRUE(state2.is_connected());
}

TEST_F(ConnectionStateTest, ConcurrentSetConnectingOnlyOneSucceeds)
{
	connection_state state;
	std::atomic<int> success_count{0};
	constexpr int thread_count = 10;
	std::vector<std::thread> threads;
	threads.reserve(thread_count);

	for (int i = 0; i < thread_count; ++i)
	{
		threads.emplace_back([&state, &success_count]() {
			if (state.set_connecting())
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
	EXPECT_TRUE(state.is_connecting());
}

// ============================================================================
// CallbackManager Tests
// ============================================================================

class CallbackManagerTest : public ::testing::Test
{
protected:
	void SetUp() override {}
	void TearDown() override {}
};

TEST_F(CallbackManagerTest, InitialCallbacksAreEmpty)
{
	callback_manager<std::function<void()>, std::function<void(int)>> manager;

	auto cb0 = manager.get<0>();
	auto cb1 = manager.get<1>();

	EXPECT_FALSE(cb0);
	EXPECT_FALSE(cb1);
}

TEST_F(CallbackManagerTest, SetAndGetByIndex)
{
	callback_manager<std::function<void()>, std::function<void(int)>> manager;

	bool called = false;
	manager.set<0>([&called]() { called = true; });

	auto cb = manager.get<0>();
	EXPECT_TRUE(cb);

	cb();
	EXPECT_TRUE(called);
}

TEST_F(CallbackManagerTest, InvokeByIndex)
{
	callback_manager<std::function<void()>, std::function<void(int)>> manager;

	bool called = false;
	manager.set<0>([&called]() { called = true; });

	manager.invoke<0>();
	EXPECT_TRUE(called);
}

TEST_F(CallbackManagerTest, InvokeWithArguments)
{
	callback_manager<std::function<void()>, std::function<void(int)>> manager;

	int received_value = 0;
	manager.set<1>([&received_value](int v) { received_value = v; });

	manager.invoke<1>(42);
	EXPECT_EQ(received_value, 42);
}

TEST_F(CallbackManagerTest, InvokeIfConditionTrue)
{
	callback_manager<std::function<void()>> manager;

	bool called = false;
	manager.set<0>([&called]() { called = true; });

	manager.invoke_if<0>(true);
	EXPECT_TRUE(called);
}

TEST_F(CallbackManagerTest, InvokeIfConditionFalse)
{
	callback_manager<std::function<void()>> manager;

	bool called = false;
	manager.set<0>([&called]() { called = true; });

	manager.invoke_if<0>(false);
	EXPECT_FALSE(called);
}

TEST_F(CallbackManagerTest, InvokeDoesNothingWhenCallbackNotSet)
{
	callback_manager<std::function<void()>> manager;

	// Should not crash
	manager.invoke<0>();
}

TEST_F(CallbackManagerTest, ClearAllCallbacks)
{
	callback_manager<std::function<void()>, std::function<void(int)>> manager;

	manager.set<0>([]() {});
	manager.set<1>([](int) {});

	manager.clear();

	EXPECT_FALSE(manager.get<0>());
	EXPECT_FALSE(manager.get<1>());
}

TEST_F(CallbackManagerTest, ClearSpecificCallback)
{
	callback_manager<std::function<void()>, std::function<void(int)>> manager;

	manager.set<0>([]() {});
	manager.set<1>([](int) {});

	manager.clear<0>();

	EXPECT_FALSE(manager.get<0>());
	EXPECT_TRUE(manager.get<1>());
}

TEST_F(CallbackManagerTest, TcpClientCallbacksTypeAlias)
{
	tcp_client_callbacks callbacks;

	bool receive_called = false;
	bool connected_called = false;
	bool disconnected_called = false;
	bool error_called = false;

	callbacks.set<tcp_client_callback_index::receive>(
	    [&receive_called](const std::vector<uint8_t>&) { receive_called = true; });
	callbacks.set<tcp_client_callback_index::connected>([&connected_called]() { connected_called = true; });
	callbacks.set<tcp_client_callback_index::disconnected>([&disconnected_called]() { disconnected_called = true; });
	callbacks.set<tcp_client_callback_index::error>(
	    [&error_called](std::error_code) { error_called = true; });

	std::vector<uint8_t> data{1, 2, 3};
	callbacks.invoke<tcp_client_callback_index::receive>(data);
	callbacks.invoke<tcp_client_callback_index::connected>();
	callbacks.invoke<tcp_client_callback_index::disconnected>();
	callbacks.invoke<tcp_client_callback_index::error>(std::error_code{});

	EXPECT_TRUE(receive_called);
	EXPECT_TRUE(connected_called);
	EXPECT_TRUE(disconnected_called);
	EXPECT_TRUE(error_called);
}

TEST_F(CallbackManagerTest, ConcurrentSetAndInvoke)
{
	callback_manager<std::function<void()>> manager;
	std::atomic<int> invoke_count{0};
	constexpr int iterations = 100;

	std::thread setter([&manager, &invoke_count]() {
		for (int i = 0; i < iterations; ++i)
		{
			manager.set<0>([&invoke_count]() { ++invoke_count; });
			std::this_thread::yield();
		}
	});

	std::thread invoker([&manager]() {
		for (int i = 0; i < iterations; ++i)
		{
			manager.invoke<0>();
			std::this_thread::yield();
		}
	});

	setter.join();
	invoker.join();

	// Just verify no crashes - exact count depends on timing
	EXPECT_GE(invoke_count.load(), 0);
}
