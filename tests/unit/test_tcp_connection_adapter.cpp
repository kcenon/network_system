/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_tcp_connection_adapter.cpp
 * @brief Unit tests for unified TCP connection adapter (i_connection bridge)
 */

#include "internal/unified/adapters/tcp_connection_adapter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace kcenon::network::unified::adapters {
namespace {

TEST(TcpConnectionAdapterTest, ConstructWithExplicitId) {
    tcp_connection_adapter adapter("test-tcp-conn");
    SUCCEED();
}

TEST(TcpConnectionAdapterTest, IdReturnsConstructorValue) {
    tcp_connection_adapter adapter("my-connection-id");
    EXPECT_EQ(adapter.id(), "my-connection-id");
}

TEST(TcpConnectionAdapterTest, IsNotConnectedBeforeConnect) {
    tcp_connection_adapter adapter("test-conn");
    EXPECT_FALSE(adapter.is_connected());
}

TEST(TcpConnectionAdapterTest, IsNotConnectingBeforeConnect) {
    tcp_connection_adapter adapter("test-conn");
    EXPECT_FALSE(adapter.is_connecting());
}

TEST(TcpConnectionAdapterTest, SetCallbacks) {
    tcp_connection_adapter adapter("test-conn");
    connection_callbacks cbs;
    cbs.on_connected = []() {};
    cbs.on_disconnected = []() {};
    cbs.on_data = [](std::span<const std::byte>) {};
    cbs.on_error = [](std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    SUCCEED();
}

TEST(TcpConnectionAdapterTest, SetOptions) {
    tcp_connection_adapter adapter("test-conn");
    connection_options opts;
    adapter.set_options(std::move(opts));
    SUCCEED();
}

TEST(TcpConnectionAdapterTest, SetTimeout) {
    tcp_connection_adapter adapter("test-conn");
    adapter.set_timeout(std::chrono::seconds(30));
    SUCCEED();
}

TEST(TcpConnectionAdapterTest, SetTimeoutMultipleTimes) {
    tcp_connection_adapter adapter("test-conn");
    adapter.set_timeout(std::chrono::seconds(5));
    adapter.set_timeout(std::chrono::seconds(30));
    adapter.set_timeout(std::chrono::milliseconds(500));
    SUCCEED();
}

TEST(TcpConnectionAdapterTest, CloseBeforeConnectIsIdempotent) {
    tcp_connection_adapter adapter("test-conn");
    adapter.close();
    EXPECT_FALSE(adapter.is_connected());
}

}  // namespace
}  // namespace kcenon::network::unified::adapters
