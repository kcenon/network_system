// BSD 3-Clause License
// Copyright (c) 2024, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file messaging_client_lifecycle_test.cpp
 * @brief Client-only lifecycle and connection tests for messaging_client
 *        (Issue #1088).
 *
 * Covers: ClientConstruction, ClientConnectToNonExistentServer,
 * ClientServerBasicConnection, MultipleClientsConnection, SendWithoutConnection.
 */

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

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
namespace error_codes = kcenon::network::error_codes;
namespace integration = kcenon::network::integration;

class MessagingClientLifecycleTest : public messaging_loopback_fixture
{
};

TEST_F(MessagingClientLifecycleTest, ClientConstruction)
{
    auto client = std::make_shared<messaging_client>("test_client");
    EXPECT_NE(client, nullptr);
}

TEST_F(MessagingClientLifecycleTest, ClientConnectToNonExistentServer)
{
    auto client = std::make_shared<messaging_client>("test_client");

    // Use a port that's almost certainly free; the connection will fail in
    // the background, but start_client only kicks off the async work and
    // should return ok().
    const auto unused_port = pick_ephemeral_port();
    ASSERT_NE(unused_port, 0u);

    auto start_result = client->start_client("127.0.0.1", unused_port);
    EXPECT_TRUE(start_result.is_ok())
        << "Client start should succeed (connection is async): "
        << (start_result.is_err() ? start_result.error().message : "");

    wait_for_ready();

    auto stop_result = client->stop_client();
    EXPECT_TRUE(stop_result.is_ok())
        << "Client stop should succeed: "
        << (stop_result.is_err() ? stop_result.error().message : "");
}

TEST_F(MessagingClientLifecycleTest, ClientServerBasicConnection)
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
    auto client_start = client->start_client("127.0.0.1", port);
    EXPECT_TRUE(client_start.is_ok())
        << "Client should start successfully: "
        << (client_start.is_err() ? client_start.error().message : "");

    wait_for_ready();

    auto client_stop = client->stop_client();
    EXPECT_TRUE(client_stop.is_ok()) << "Client stop should succeed";

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";
}

TEST_F(MessagingClientLifecycleTest, MultipleClientsConnection)
{
    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto server = std::make_shared<messaging_server>("test_server");
    auto server_start = server->start_server(port);
    EXPECT_TRUE(server_start.is_ok()) << "Server should start successfully";

    wait_for_ready();

    constexpr int kClientCount = 5;
    std::vector<std::shared_ptr<messaging_client>> clients;

    for (int i = 0; i < kClientCount; ++i)
    {
        auto client =
            std::make_shared<messaging_client>("client_" + std::to_string(i));
        auto client_start = client->start_client("127.0.0.1", port);
        EXPECT_TRUE(client_start.is_ok())
            << "Client " << i << " should start successfully: "
            << (client_start.is_err() ? client_start.error().message : "");
        clients.push_back(client);
    }

    wait_for_ready();

    for (size_t i = 0; i < clients.size(); ++i)
    {
        auto client_stop = clients[i]->stop_client();
        EXPECT_TRUE(client_stop.is_ok())
            << "Client " << i << " stop should succeed";
    }

    auto server_stop = server->stop_server();
    EXPECT_TRUE(server_stop.is_ok()) << "Server stop should succeed";
}

TEST_F(MessagingClientLifecycleTest, SendWithoutConnection)
{
    auto client = std::make_shared<messaging_client>("disconnected_client");

    const std::string test_data = "test:data";
    auto& manager = integration::container_manager::instance();
    auto serialized = manager.serialize(test_data);

    auto send_result = client->send_packet(std::move(serialized));
    EXPECT_TRUE(send_result.is_err())
        << "Send without connection should return error";

    if (send_result.is_err())
    {
        EXPECT_EQ(send_result.error().code,
                  error_codes::network_system::connection_closed)
            << "Should return connection_closed error code";
    }
}

} // namespace
