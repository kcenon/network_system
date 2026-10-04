/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_udp_connection_adapter.cpp
 * @brief Unit tests for unified UDP connection adapter (i_connection bridge)
 */

#include "internal/unified/adapters/udp_connection_adapter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace kcenon::network::unified::adapters {
namespace {

TEST(UdpConnectionAdapterTest, ConstructWithExplicitId) {
    udp_connection_adapter adapter("test-udp-conn");
    SUCCEED();
}

TEST(UdpConnectionAdapterTest, IdReturnsConstructorValue) {
    udp_connection_adapter adapter("my-udp-connection");
    EXPECT_EQ(adapter.id(), "my-udp-connection");
}

TEST(UdpConnectionAdapterTest, IsNotConnectedBeforeConnect) {
    udp_connection_adapter adapter("test-conn");
    EXPECT_FALSE(adapter.is_connected());
}

TEST(UdpConnectionAdapterTest, IsNotConnectingBeforeConnect) {
    udp_connection_adapter adapter("test-conn");
    EXPECT_FALSE(adapter.is_connecting());
}

TEST(UdpConnectionAdapterTest, SetCallbacks) {
    udp_connection_adapter adapter("test-conn");
    connection_callbacks cbs;
    cbs.on_connected = []() {};
    cbs.on_disconnected = []() {};
    cbs.on_data = [](std::span<const std::byte>) {};
    cbs.on_error = [](std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    SUCCEED();
}

TEST(UdpConnectionAdapterTest, SetOptions) {
    udp_connection_adapter adapter("test-conn");
    connection_options opts;
    adapter.set_options(std::move(opts));
    SUCCEED();
}

TEST(UdpConnectionAdapterTest, SetTimeout) {
    udp_connection_adapter adapter("test-conn");
    adapter.set_timeout(std::chrono::seconds(15));
    SUCCEED();
}

TEST(UdpConnectionAdapterTest, CloseBeforeConnectIsIdempotent) {
    udp_connection_adapter adapter("test-conn");
    adapter.close();
    EXPECT_FALSE(adapter.is_connected());
}

}  // namespace
}  // namespace kcenon::network::unified::adapters
