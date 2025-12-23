// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file test_thread_system_adapter.cpp
 * @brief Unit tests for thread_system_pool_adapter
 */

#include <kcenon/network/config/feature_flags.h>

#include <gtest/gtest.h>

#include "kcenon/network/integration/thread_system_adapter.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#if KCENON_WITH_THREAD_SYSTEM

namespace kcenon::network::integration {
namespace {

class ThreadSystemAdapterTest : public ::testing::Test {
protected:
    void SetUp() override {
        adapter_ = thread_system_pool_adapter::create_default("test_pool");
        ASSERT_NE(adapter_, nullptr);
        ASSERT_TRUE(adapter_->is_running());
    }

    void TearDown() override {
        adapter_.reset();
    }

    std::shared_ptr<thread_system_pool_adapter> adapter_;
};

TEST_F(ThreadSystemAdapterTest, CreateDefault) {
    EXPECT_TRUE(adapter_->is_running());
    EXPECT_GT(adapter_->worker_count(), 0u);
}

TEST_F(ThreadSystemAdapterTest, SubmitBasicTask) {
    std::atomic<bool> executed{false};

    auto future = adapter_->submit([&executed]() {
        executed = true;
    });

    future.wait();
    EXPECT_TRUE(executed);
}

TEST_F(ThreadSystemAdapterTest, SubmitMultipleTasks) {
    constexpr int num_tasks = 100;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;

    for (int i = 0; i < num_tasks; ++i) {
        futures.push_back(adapter_->submit([&counter]() {
            counter++;
        }));
    }

    for (auto& f : futures) {
        f.wait();
    }

    EXPECT_EQ(counter.load(), num_tasks);
}

TEST_F(ThreadSystemAdapterTest, SubmitDelayedBasic) {
    std::atomic<bool> executed{false};
    auto start = std::chrono::steady_clock::now();

    auto future = adapter_->submit_delayed(
        [&executed]() { executed = true; },
        std::chrono::milliseconds(50)
    );

    future.wait();
    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_TRUE(executed);
    EXPECT_GE(elapsed, std::chrono::milliseconds(45)); // Allow small tolerance
}

TEST_F(ThreadSystemAdapterTest, SubmitDelayedMultiple) {
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    auto start = std::chrono::steady_clock::now();

    // Submit tasks with different delays
    futures.push_back(adapter_->submit_delayed(
        [&counter]() { counter++; },
        std::chrono::milliseconds(100)
    ));
    futures.push_back(adapter_->submit_delayed(
        [&counter]() { counter++; },
        std::chrono::milliseconds(50)
    ));
    futures.push_back(adapter_->submit_delayed(
        [&counter]() { counter++; },
        std::chrono::milliseconds(150)
    ));

    for (auto& f : futures) {
        f.wait();
    }

    auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_EQ(counter.load(), 3);
    EXPECT_GE(elapsed, std::chrono::milliseconds(140)); // ~150ms for longest
}

TEST_F(ThreadSystemAdapterTest, SubmitDelayedOrdering) {
    std::vector<int> order;
    std::mutex order_mutex;

    auto add_to_order = [&order, &order_mutex](int value) {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back(value);
    };

    // Submit in reverse order of execution
    auto f1 = adapter_->submit_delayed(
        [&add_to_order]() { add_to_order(3); },
        std::chrono::milliseconds(150)
    );
    auto f2 = adapter_->submit_delayed(
        [&add_to_order]() { add_to_order(1); },
        std::chrono::milliseconds(50)
    );
    auto f3 = adapter_->submit_delayed(
        [&add_to_order]() { add_to_order(2); },
        std::chrono::milliseconds(100)
    );

    f1.wait();
    f2.wait();
    f3.wait();

    // Give a small buffer for any in-flight tasks
    std::this_thread::yield();

    std::lock_guard<std::mutex> lock(order_mutex);
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST_F(ThreadSystemAdapterTest, SubmitDelayedZeroDelay) {
    std::atomic<bool> executed{false};

    auto future = adapter_->submit_delayed(
        [&executed]() { executed = true; },
        std::chrono::milliseconds(0)
    );

    future.wait();
    EXPECT_TRUE(executed);
}

TEST_F(ThreadSystemAdapterTest, SubmitDelayedWithException) {
    auto future = adapter_->submit_delayed(
        []() { throw std::runtime_error("test exception"); },
        std::chrono::milliseconds(10)
    );

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(ThreadSystemAdapterTest, PendingTasksCount) {
    // Initially should have no pending tasks (or very few)
    EXPECT_LE(adapter_->pending_tasks(), 1u);
}

TEST_F(ThreadSystemAdapterTest, ConcurrentDelayedSubmissions) {
    constexpr int num_submitters = 10;
    constexpr int tasks_per_submitter = 10;
    std::atomic<int> counter{0};

    std::vector<std::thread> submitters;
    std::vector<std::future<void>> all_futures;
    std::mutex futures_mutex;

    for (int i = 0; i < num_submitters; ++i) {
        submitters.emplace_back([this, &counter, &all_futures, &futures_mutex, i]() {
            for (int j = 0; j < tasks_per_submitter; ++j) {
                auto delay = std::chrono::milliseconds(10 + (i * j) % 50);
                auto future = adapter_->submit_delayed(
                    [&counter]() { counter++; },
                    delay
                );

                std::lock_guard<std::mutex> lock(futures_mutex);
                all_futures.push_back(std::move(future));
            }
        });
    }

    for (auto& t : submitters) {
        t.join();
    }

    for (auto& f : all_futures) {
        f.wait();
    }

    EXPECT_EQ(counter.load(), num_submitters * tasks_per_submitter);
}

} // namespace
} // namespace kcenon::network::integration

#else // !KCENON_WITH_THREAD_SYSTEM

// Placeholder test when thread_system is not available
TEST(ThreadSystemAdapterTest, NotAvailable) {
    GTEST_SKIP() << "thread_system not available, skipping adapter tests";
}

#endif // KCENON_WITH_THREAD_SYSTEM
