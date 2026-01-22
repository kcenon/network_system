/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file unified_interfaces_test.cpp
 * @brief Tests for unified interface definitions
 *
 * These tests verify that the unified interfaces compile correctly and
 * that the type definitions are usable.
 */

#include <gtest/gtest.h>

#include "kcenon/network/unified/unified.h"

#include <memory>
#include <vector>

namespace kcenon::network::unified::test {

// Test that types compile and can be instantiated
class UnifiedTypesTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(UnifiedTypesTest, EndpointInfoDefaultConstruction) {
    endpoint_info ep;
    EXPECT_TRUE(ep.host.empty());
    EXPECT_EQ(ep.port, 0);
    EXPECT_FALSE(ep.is_valid());
}

TEST_F(UnifiedTypesTest, EndpointInfoHostPortConstruction) {
    endpoint_info ep("localhost", 8080);
    EXPECT_EQ(ep.host, "localhost");
    EXPECT_EQ(ep.port, 8080);
    EXPECT_TRUE(ep.is_valid());
}

TEST_F(UnifiedTypesTest, EndpointInfoUrlConstruction) {
    endpoint_info ep(std::string("wss://example.com/ws"));
    EXPECT_EQ(ep.host, "wss://example.com/ws");
    EXPECT_EQ(ep.port, 0);
    EXPECT_TRUE(ep.is_valid());
}

TEST_F(UnifiedTypesTest, EndpointInfoStringViewConstruction) {
    std::string host = "192.168.1.1";
    endpoint_info ep{host, 443};
    EXPECT_EQ(ep.host, "192.168.1.1");
    EXPECT_EQ(ep.port, 443);
}

TEST_F(UnifiedTypesTest, EndpointInfoToString) {
    endpoint_info ep1{"localhost", 8080};
    EXPECT_EQ(ep1.to_string(), "localhost:8080");

    endpoint_info ep2{"wss://example.com/ws"};
    EXPECT_EQ(ep2.to_string(), "wss://example.com/ws");
}

TEST_F(UnifiedTypesTest, EndpointInfoEquality) {
    endpoint_info ep1{"localhost", 8080};
    endpoint_info ep2{"localhost", 8080};
    endpoint_info ep3{"localhost", 9090};
    endpoint_info ep4{"127.0.0.1", 8080};

    EXPECT_EQ(ep1, ep2);
    EXPECT_NE(ep1, ep3);
    EXPECT_NE(ep1, ep4);
}

TEST_F(UnifiedTypesTest, ConnectionCallbacksDefaultConstruction) {
    connection_callbacks cbs;
    EXPECT_FALSE(cbs.on_connected);
    EXPECT_FALSE(cbs.on_data);
    EXPECT_FALSE(cbs.on_disconnected);
    EXPECT_FALSE(cbs.on_error);
}

TEST_F(UnifiedTypesTest, ConnectionCallbacksAssignment) {
    bool connected_called = false;
    bool data_called = false;

    connection_callbacks cbs;
    cbs.on_connected = [&]() { connected_called = true; };
    cbs.on_data = [&](std::span<const std::byte>) { data_called = true; };

    EXPECT_TRUE(cbs.on_connected);
    EXPECT_TRUE(cbs.on_data);

    cbs.on_connected();
    EXPECT_TRUE(connected_called);

    std::vector<std::byte> data;
    cbs.on_data(data);
    EXPECT_TRUE(data_called);
}

TEST_F(UnifiedTypesTest, ListenerCallbacksDefaultConstruction) {
    listener_callbacks cbs;
    EXPECT_FALSE(cbs.on_accept);
    EXPECT_FALSE(cbs.on_data);
    EXPECT_FALSE(cbs.on_disconnect);
    EXPECT_FALSE(cbs.on_error);
}

TEST_F(UnifiedTypesTest, ConnectionOptionsDefaultValues) {
    connection_options opts;
    EXPECT_EQ(opts.connect_timeout.count(), 0);
    EXPECT_EQ(opts.read_timeout.count(), 0);
    EXPECT_EQ(opts.write_timeout.count(), 0);
    EXPECT_FALSE(opts.keep_alive);
    EXPECT_FALSE(opts.no_delay);
}

TEST_F(UnifiedTypesTest, ConnectionOptionsCustomValues) {
    connection_options opts;
    opts.connect_timeout = std::chrono::milliseconds{5000};
    opts.read_timeout = std::chrono::milliseconds{30000};
    opts.keep_alive = true;
    opts.no_delay = true;

    EXPECT_EQ(opts.connect_timeout.count(), 5000);
    EXPECT_EQ(opts.read_timeout.count(), 30000);
    EXPECT_TRUE(opts.keep_alive);
    EXPECT_TRUE(opts.no_delay);
}

// Test that interfaces can be used as types (compile-time check)
class InterfaceTypeTest : public ::testing::Test {};

TEST_F(InterfaceTypeTest, TransportPointerType) {
    // Verify i_transport can be used as a pointer type
    i_transport* transport_ptr = nullptr;
    EXPECT_EQ(transport_ptr, nullptr);

    std::unique_ptr<i_transport> transport_uptr;
    EXPECT_EQ(transport_uptr, nullptr);
}

TEST_F(InterfaceTypeTest, ConnectionPointerType) {
    // Verify i_connection can be used as a pointer type
    i_connection* connection_ptr = nullptr;
    EXPECT_EQ(connection_ptr, nullptr);

    std::unique_ptr<i_connection> connection_uptr;
    EXPECT_EQ(connection_uptr, nullptr);
}

TEST_F(InterfaceTypeTest, ListenerPointerType) {
    // Verify i_listener can be used as a pointer type
    i_listener* listener_ptr = nullptr;
    EXPECT_EQ(listener_ptr, nullptr);

    std::unique_ptr<i_listener> listener_uptr;
    EXPECT_EQ(listener_uptr, nullptr);
}

TEST_F(InterfaceTypeTest, ConnectionInheritsFromTransport) {
    // Verify i_connection inherits from i_transport (compile-time)
    // This compiles only if i_connection publicly inherits from i_transport
    auto check_inheritance = [](i_transport*) {};
    i_connection* conn_ptr = nullptr;
    check_inheritance(conn_ptr);
    SUCCEED();
}

}  // namespace kcenon::network::unified::test
