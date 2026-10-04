// BSD 3-Clause License
// Copyright (c) 2024, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file messaging_id_test.cpp
 * @brief Identifier and monitor accessor tests for messaging_server
 *        (Issue #1088).
 *
 * Covers (per Issue #1088 plan): ServerIdReturnsConstructorValue,
 * ServerIdEmptyStringIsValid, SetAndGetMonitor.
 */

#include <gtest/gtest.h>

#include <memory>

#include "internal/core/messaging_server.h"
#include "support/messaging_loopback_fixture.h"

namespace
{

using kcenon::network::tests::support::messaging_loopback_fixture;
using messaging_server = kcenon::network::core::messaging_server;

class MessagingIdTest : public messaging_loopback_fixture
{
};

TEST_F(MessagingIdTest, ServerIdReturnsConstructorValue)
{
    auto server = std::make_shared<messaging_server>("my_custom_id");
    EXPECT_EQ(server->server_id(), "my_custom_id");
}

TEST_F(MessagingIdTest, ServerIdEmptyStringIsValid)
{
    auto server = std::make_shared<messaging_server>("");
    EXPECT_EQ(server->server_id(), "");
}

#if KCENON_WITH_COMMON_SYSTEM
TEST_F(MessagingIdTest, SetAndGetMonitor)
{
    auto server = std::make_shared<messaging_server>("monitor_server");

    EXPECT_EQ(server->get_monitor(), nullptr);

    server->set_monitor(nullptr);
    EXPECT_EQ(server->get_monitor(), nullptr);
}
#endif // KCENON_WITH_COMMON_SYSTEM

} // namespace
