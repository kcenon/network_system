// BSD 3-Clause License
// Copyright (c) 2024, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file messaging_stress_test.cpp
 * @brief Stress tests for messaging_client / messaging_server (Issue #1088).
 *
 * Covers (per Issue #1088 plan): NetworkStressTest.RapidConnectionDisconnection,
 * NetworkStressTest.ConcurrentClients.
 *
 * Uses bare @c TEST() (not @c TEST_F) to preserve the original test naming and
 * does not derive from @c messaging_loopback_fixture because the original two
 * cases construct everything inline. The @c pick_ephemeral_port() helper is
 * still reused via the support library so port allocation is parallel-safe.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "internal/core/messaging_client.h"
#include "internal/core/messaging_server.h"
#include "internal/integration/container_integration.h"
#include "kcenon/network/detail/utils/result_types.h"
#include "support/messaging_loopback_fixture.h"

namespace
{

using kcenon::network::tests::support::is_sanitizer_run;
using kcenon::network::tests::support::pick_ephemeral_port;
using kcenon::network::tests::support::wait_for_ready;
using messaging_client = kcenon::network::core::messaging_client;
using messaging_server = kcenon::network::core::messaging_server;
namespace integration = kcenon::network::integration;

} // namespace

TEST(NetworkStressTest, RapidConnectionDisconnection)
{
    if (is_sanitizer_run())
    {
        GTEST_SKIP() << "Skipping under sanitizer due to asio false positives";
    }

    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto server = std::make_shared<messaging_server>("stress_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully";

    wait_for_ready();

    constexpr int kCycles = 10;
    for (int i = 0; i < kCycles; ++i)
    {
        auto client =
            std::make_shared<messaging_client>("rapid_client_" + std::to_string(i));
        auto client_start = client->start_client("127.0.0.1", port);
        EXPECT_TRUE(client_start.is_ok())
            << "Client " << i << " should start successfully";
        wait_for_ready();
        auto client_stop = client->stop_client();
        EXPECT_TRUE(client_stop.is_ok())
            << "Client " << i << " should stop successfully";
    }

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server should stop successfully";
}

TEST(NetworkStressTest, ConcurrentClients)
{
    if (is_sanitizer_run())
    {
        GTEST_SKIP() << "Skipping under sanitizer due to timing sensitivity";
    }

    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto server = std::make_shared<messaging_server>("concurrent_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully";

    wait_for_ready();

    constexpr int kNumThreads = 5;
    constexpr int kClientsPerThread = 2;
    std::vector<std::thread> threads;
    std::vector<std::shared_ptr<messaging_client>> all_clients;
    std::mutex clients_mutex;
    std::atomic<int> successful_connections{0};
    std::atomic<int> successful_sends{0};

    for (int t = 0; t < kNumThreads; ++t)
    {
        threads.emplace_back([port, t, &all_clients, &clients_mutex,
                              &successful_connections, &successful_sends]() {
            for (int c = 0; c < kClientsPerThread; ++c)
            {
                auto client = std::make_shared<messaging_client>(
                    "thread_" + std::to_string(t) + "_client_"
                    + std::to_string(c));

                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    all_clients.push_back(client);
                }

                auto client_start = client->start_client("127.0.0.1", port);
                if (client_start.is_ok())
                {
                    ++successful_connections;
                    wait_for_ready();

                    const std::string message = "thread:" + std::to_string(t)
                                                + ":client:" + std::to_string(c);
                    auto& manager = integration::container_manager::instance();
                    auto serialized = manager.serialize(message);
                    auto send_result = client->send_packet(std::move(serialized));
                    if (send_result.is_ok())
                    {
                        ++successful_sends;
                    }
                }
                wait_for_ready();
            }
        });
    }

    for (auto& th : threads)
    {
        th.join();
    }

    EXPECT_GT(successful_connections.load(), 0)
        << "At least some clients should connect";
    EXPECT_GT(successful_sends.load(), 0)
        << "At least some messages should be sent";

    for (auto& client : all_clients)
    {
        client->stop_client();
    }

    wait_for_ready();
    all_clients.clear();

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server should stop successfully";
}
