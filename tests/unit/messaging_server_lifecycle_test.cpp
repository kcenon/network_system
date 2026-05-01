// BSD 3-Clause License
// Copyright (c) 2024, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file messaging_server_lifecycle_test.cpp
 * @brief Server-only lifecycle tests for messaging_server (Issue #1088).
 *
 * Split out of the monolithic tests/unit_tests.cpp so each concern owns its
 * own executable and ctest -j N can run them in parallel without port
 * collisions. The shared fixture in @c support/messaging_loopback_fixture.h
 * supplies an OS-assigned ephemeral port for every test.
 *
 * Covers (per Issue #1088): ServerConstruction, ServerStartStop,
 * ServerMultipleStartStop, ServerPortAlreadyInUse,
 * DoubleStartReturnsAlreadyExistsError.
 */

#include <gtest/gtest.h>

#include <memory>

#include "internal/core/messaging_server.h"
#include "kcenon/network/detail/utils/result_types.h"
#include "support/messaging_loopback_fixture.h"

namespace
{

using kcenon::network::tests::support::messaging_loopback_fixture;
using kcenon::network::tests::support::pick_ephemeral_port;
using kcenon::network::tests::support::wait_for_ready;
using messaging_server = kcenon::network::core::messaging_server;
namespace error_codes = kcenon::network::error_codes;

class MessagingServerLifecycleTest : public messaging_loopback_fixture
{
};

TEST_F(MessagingServerLifecycleTest, ServerConstruction)
{
    auto server = std::make_shared<messaging_server>("test_server");
    EXPECT_NE(server, nullptr);
}

TEST_F(MessagingServerLifecycleTest, ServerStartStop)
{
    auto server = std::make_shared<messaging_server>("test_server");

    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto start_result = server->start_server(port);
    EXPECT_TRUE(start_result.is_ok())
        << "Server start should succeed: "
        << (start_result.is_err() ? start_result.error().message : "");

    wait_for_ready();

    auto stop_result = server->stop_server();
    EXPECT_TRUE(stop_result.is_ok())
        << "Server stop should succeed: "
        << (stop_result.is_err() ? stop_result.error().message : "");
}

TEST_F(MessagingServerLifecycleTest, ServerMultipleStartStop)
{
    auto server = std::make_shared<messaging_server>("test_server");

    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    for (int i = 0; i < 3; ++i)
    {
        auto start_result = server->start_server(port);
        EXPECT_TRUE(start_result.is_ok())
            << "Server start cycle " << i << " should succeed: "
            << (start_result.is_err() ? start_result.error().message : "");
        wait_for_ready();

        auto stop_result = server->stop_server();
        EXPECT_TRUE(stop_result.is_ok())
            << "Server stop cycle " << i << " should succeed: "
            << (stop_result.is_err() ? stop_result.error().message : "");
        wait_for_ready();
    }
}

TEST_F(MessagingServerLifecycleTest, ServerPortAlreadyInUse)
{
    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto server1 = std::make_shared<messaging_server>("server1");

    auto result1 = server1->start_server(port);
    EXPECT_TRUE(result1.is_ok())
        << "First server should start successfully: "
        << (result1.is_err() ? result1.error().message : "");
    wait_for_ready();

    auto server2 = std::make_shared<messaging_server>("server2");
    auto result2 = server2->start_server(port);
    EXPECT_TRUE(result2.is_err())
        << "Second server should fail to start on same port";

    if (result2.is_err())
    {
        EXPECT_EQ(result2.error().code, error_codes::network_system::bind_failed)
            << "Should return bind_failed error code, got: "
            << result2.error().code;
        EXPECT_FALSE(result2.error().message.empty())
            << "Error message should not be empty";
    }

    auto stop_result = server1->stop_server();
    EXPECT_TRUE(stop_result.is_ok()) << "Server stop should succeed";
    wait_for_ready();
}

TEST_F(MessagingServerLifecycleTest, DoubleStartReturnsAlreadyExistsError)
{
    auto server = std::make_shared<messaging_server>("double_start_server");

    const auto port = pick_ephemeral_port();
    ASSERT_NE(port, 0u) << "No available port found";

    auto result1 = server->start_server(port);
    ASSERT_TRUE(result1.is_ok()) << "First start should succeed";

    wait_for_ready();

    auto result2 = server->start_server(port);
    EXPECT_TRUE(result2.is_err()) << "Double start should return error";

    if (result2.is_err())
    {
        EXPECT_EQ(result2.error().code, error_codes::common_errors::already_exists)
            << "Should return already_exists error code";
        EXPECT_FALSE(result2.error().message.empty())
            << "Error message should not be empty";
    }

    auto stop_result = server->stop_server();
    EXPECT_TRUE(stop_result.is_ok());
}

} // namespace
