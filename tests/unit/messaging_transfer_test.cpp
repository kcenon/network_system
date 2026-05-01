// BSD 3-Clause License
// Copyright (c) 2024, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file messaging_transfer_test.cpp
 * @brief Message transfer tests for messaging_client / messaging_server
 *        (Issue #1088).
 *
 * Covers: BasicMessageTransfer, LargeMessageTransfer, MultipleMessageTransfer.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "internal/core/messaging_client.h"
#include "internal/core/messaging_server.h"
#include "internal/integration/container_integration.h"
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
namespace integration = kcenon::network::integration;
using namespace std::chrono_literals;

class MessagingTransferTest : public messaging_loopback_fixture
{
};

TEST_F(MessagingTransferTest, BasicMessageTransfer)
{
    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto server = std::make_shared<messaging_server>("test_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok())
        << "Server should start successfully: "
        << (server_start.is_err() ? server_start.error().message : "");

    wait_for_ready();

    auto client = std::make_shared<messaging_client>("test_client");

    std::promise<bool> connected_promise;
    auto connected_future = connected_promise.get_future();
    bool promise_set = false;
    std::mutex promise_mutex;

    client->set_connected_callback(
        [&connected_promise, &promise_set, &promise_mutex]() {
            std::lock_guard<std::mutex> lock(promise_mutex);
            if (!promise_set)
            {
                promise_set = true;
                connected_promise.set_value(true);
            }
        });

    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok())
        << "Client should start successfully: "
        << (client_start.is_err() ? client_start.error().message : "");

    auto status = connected_future.wait_for(5s);
    ASSERT_EQ(status, std::future_status::ready)
        << "Client should connect within timeout";

    const std::string test_message = "test_message:Hello, Server!:1";

    auto& manager = integration::container_manager::instance();
    auto serialized = manager.serialize(test_message);

    auto send_result = client->send_packet(std::move(serialized));
    EXPECT_TRUE(send_result.is_ok())
        << "Message send should succeed: "
        << (send_result.is_err() ? send_result.error().message : "");

    wait_for_ready();

    auto client_stop = client->stop_client();
    EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";
    wait_for_ready();

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";

    wait_for_ready();
}

TEST_F(MessagingTransferTest, LargeMessageTransfer)
{
    if (is_sanitizer_run())
    {
        GTEST_SKIP() << "Skipping under sanitizer due to asio race condition";
    }

    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto server = std::make_shared<messaging_server>("test_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok())
        << "Server should start successfully: "
        << (server_start.is_err() ? server_start.error().message : "");

    wait_for_ready();

    auto client = std::make_shared<messaging_client>("test_client");

    std::promise<bool> connected_promise;
    auto connected_future = connected_promise.get_future();
    bool promise_set = false;
    std::mutex promise_mutex;

    client->set_connected_callback(
        [&connected_promise, &promise_set, &promise_mutex]() {
            std::lock_guard<std::mutex> lock(promise_mutex);
            if (!promise_set)
            {
                promise_set = true;
                connected_promise.set_value(true);
            }
        });

    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok())
        << "Client should start successfully: "
        << (client_start.is_err() ? client_start.error().message : "");

    auto status = connected_future.wait_for(5s);
    ASSERT_EQ(status, std::future_status::ready)
        << "Client should connect within timeout";

    const std::string large_message =
        "large_message:" + std::string(1024UL * 1024UL, 'X');

    auto& manager = integration::container_manager::instance();
    auto serialized = manager.serialize(large_message);

    auto send_result = client->send_packet(std::move(serialized));
    EXPECT_TRUE(send_result.is_ok())
        << "Large message send should succeed: "
        << (send_result.is_err() ? send_result.error().message : "");

    wait_for_ready();

    auto client_stop = client->stop_client();
    EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";
    wait_for_ready();

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";

    wait_for_ready();
}

TEST_F(MessagingTransferTest, MultipleMessageTransfer)
{
    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto server = std::make_shared<messaging_server>("test_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok())
        << "Server should start successfully: "
        << (server_start.is_err() ? server_start.error().message : "");

    wait_for_ready();

    auto client = std::make_shared<messaging_client>("test_client");

    std::promise<bool> connected_promise;
    auto connected_future = connected_promise.get_future();
    bool promise_set = false;
    std::mutex promise_mutex;

    client->set_connected_callback(
        [&connected_promise, &promise_set, &promise_mutex]() {
            std::lock_guard<std::mutex> lock(promise_mutex);
            if (!promise_set)
            {
                promise_set = true;
                connected_promise.set_value(true);
            }
        });

    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok())
        << "Client should start successfully: "
        << (client_start.is_err() ? client_start.error().message : "");

    auto status = connected_future.wait_for(5s);
    ASSERT_EQ(status, std::future_status::ready)
        << "Client should connect within timeout";

    auto& manager = integration::container_manager::instance();

    constexpr int kMessageCount = 10;
    for (int i = 0; i < kMessageCount; ++i)
    {
        const std::string message = "sequence_message:" + std::to_string(i)
                                    + ":Message " + std::to_string(i);

        auto serialized = manager.serialize(message);
        auto send_result = client->send_packet(std::move(serialized));
        EXPECT_TRUE(send_result.is_ok())
            << "Message " << i << " send should succeed: "
            << (send_result.is_err() ? send_result.error().message : "");

        std::this_thread::yield();
    }

    wait_for_ready();

    auto client_stop = client->stop_client();
    EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";
    wait_for_ready();

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";

    wait_for_ready();
}

} // namespace
