// BSD 3-Clause License
// Copyright (c) 2024, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file messaging_server_session_test.cpp
 * @brief Server-side session lifecycle tests for messaging_server (Issue #1088).
 *
 * Covers (per Issue #1088 plan): ServerStopWhileClientsConnected,
 * CleanupDeadSessionsOnNewConnection, WaitForStopIsCallableWhileRunning,
 * WaitForStopOnNonRunningServerReturnsImmediately.
 *
 * Named with the @c messaging_server_session_ prefix to avoid colliding with
 * the existing @c tests/unit/messaging_session_test.cpp which targets the
 * @c messaging_session class directly.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "internal/core/messaging_client.h"
#include "internal/core/messaging_server.h"
#include "kcenon/network/detail/utils/result_types.h"
#include "support/messaging_loopback_fixture.h"

namespace
{

using kcenon::network::tests::support::is_sanitizer_run;
using kcenon::network::tests::support::messaging_loopback_fixture;
using kcenon::network::tests::support::pick_ephemeral_port;
using kcenon::network::tests::support::wait_for_ready;
using messaging_client = kcenon::network::core::messaging_client;
using messaging_server = kcenon::network::core::messaging_server;
using namespace std::chrono_literals;

// Wait for a condition with timeout (replaces arbitrary sleep_for).
template <typename Predicate>
bool WaitForCondition(Predicate&& condition,
                      std::chrono::milliseconds timeout = 5000ms)
{
    std::mutex mtx;
    std::condition_variable cv;
    bool result = false;

    std::thread checker([&]() {
        while (!result)
        {
            if (condition())
            {
                std::lock_guard<std::mutex> lock(mtx);
                result = true;
                cv.notify_one();
                return;
            }
            std::this_thread::yield();
        }
    });

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait_for(lock, timeout, [&] { return result; });
    }

    if (checker.joinable())
    {
        checker.join();
    }
    return result;
}

class MessagingServerSessionTest : public messaging_loopback_fixture
{
};

TEST_F(MessagingServerSessionTest, ServerStopWhileClientsConnected)
{
    if (is_sanitizer_run())
    {
        GTEST_SKIP() << "Skipping under sanitizer due to asio false positives";
    }

    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto server = std::make_shared<messaging_server>("stopping_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully";

    wait_for_ready();

    std::vector<std::shared_ptr<messaging_client>> clients;
    for (int i = 0; i < 3; ++i)
    {
        auto client =
            std::make_shared<messaging_client>("client_" + std::to_string(i));
        auto client_start = client->start_client("127.0.0.1", port);
        EXPECT_TRUE(client_start.is_ok())
            << "Client " << i << " should start successfully";
        clients.push_back(client);
    }

    wait_for_ready();

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok())
        << "Server should stop successfully even with connected clients";

    wait_for_ready();

    for (size_t i = 0; i < clients.size(); ++i)
    {
        auto client_stop = clients[i]->stop_client();
        if (client_stop.is_err())
        {
            EXPECT_NE(client_stop.error().message, "")
                << "Client " << i << " stop returned error (expected)";
        }
    }

    wait_for_ready();
    clients.clear();
}

TEST_F(MessagingServerSessionTest, CleanupDeadSessionsOnNewConnection)
{
    if (is_sanitizer_run())
    {
        GTEST_SKIP() << "Skipping under sanitizer due to timing sensitivity";
    }

    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto server = std::make_shared<messaging_server>("cleanup_server");

    std::atomic<int> connection_count{0};

    server->set_connection_callback(
        [&connection_count](
            std::shared_ptr<kcenon::network::session::messaging_session>) {
            connection_count.fetch_add(1);
        });

    auto server_start = server->start_server(port);
    ASSERT_TRUE(server_start.is_ok()) << "Server should start";

    wait_for_ready();

    auto client1 = std::make_shared<messaging_client>("cleanup_client_1");
    auto start1 = client1->start_client("127.0.0.1", port);
    ASSERT_TRUE(start1.is_ok()) << "First client should start";

    WaitForCondition([&]() { return connection_count.load() >= 1; }, 5000ms);

    (void)client1->stop_client();
    wait_for_ready();

    auto client2 = std::make_shared<messaging_client>("cleanup_client_2");
    auto start2 = client2->start_client("127.0.0.1", port);
    EXPECT_TRUE(start2.is_ok()) << "Second client should start";

    WaitForCondition([&]() { return connection_count.load() >= 2; }, 5000ms);

    EXPECT_GE(connection_count.load(), 2)
        << "Server should have accepted both connections";

    (void)client2->stop_client();
    wait_for_ready();
    (void)server->stop_server();
    wait_for_ready();
}

TEST_F(MessagingServerSessionTest, WaitForStopIsCallableWhileRunning)
{
    auto server = std::make_shared<messaging_server>("wait_server");

    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto start_result = server->start_server(port);
    ASSERT_TRUE(start_result.is_ok()) << "Server start should succeed";

    wait_for_ready();

    std::atomic<bool> wait_completed{false};

    std::thread waiter([&server, &wait_completed]() {
        server->wait_for_stop();
        wait_completed.store(true);
    });

    auto stop_result = server->stop_server();
    EXPECT_TRUE(stop_result.is_ok());

    if (waiter.joinable())
    {
        waiter.join();
    }

    EXPECT_TRUE(wait_completed.load())
        << "wait_for_stop should have returned after stop_server";
}

TEST_F(MessagingServerSessionTest, WaitForStopOnNonRunningServerReturnsImmediately)
{
    auto server = std::make_shared<messaging_server>("idle_server");

    server->wait_for_stop();

    EXPECT_FALSE(server->is_running());
}

} // namespace
