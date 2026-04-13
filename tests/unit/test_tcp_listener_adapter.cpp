/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_tcp_listener_adapter.cpp
 * @brief Unit tests for unified TCP listener adapter (i_listener bridge)
 */

#include "internal/unified/adapters/tcp_listener_adapter.h"

#include <gtest/gtest.h>

#include <string>

namespace kcenon::network::unified::adapters {
namespace {

TEST(TcpListenerAdapterTest, ConstructWithExplicitId) {
    tcp_listener_adapter adapter("test-tcp-listener");
    SUCCEED();
}

TEST(TcpListenerAdapterTest, IsNotListeningBeforeStart) {
    tcp_listener_adapter adapter("test-listener");
    EXPECT_FALSE(adapter.is_listening());
}

TEST(TcpListenerAdapterTest, ConnectionCountZeroBeforeStart) {
    tcp_listener_adapter adapter("test-listener");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

TEST(TcpListenerAdapterTest, SetCallbacks) {
    tcp_listener_adapter adapter("test-listener");
    listener_callbacks cbs;
    cbs.on_data = [](std::string_view, std::span<const std::byte>) {};
    cbs.on_disconnect = [](std::string_view) {};
    cbs.on_error = [](std::string_view, std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    SUCCEED();
}

TEST(TcpListenerAdapterTest, SetAcceptCallback) {
    tcp_listener_adapter adapter("test-listener");
    adapter.set_accept_callback([](std::unique_ptr<i_connection>) {});
    SUCCEED();
}

TEST(TcpListenerAdapterTest, StopBeforeStartIsIdempotent) {
    tcp_listener_adapter adapter("test-listener");
    adapter.stop();
    EXPECT_FALSE(adapter.is_listening());
}

TEST(TcpListenerAdapterTest, CloseNonexistentConnectionIsIdempotent) {
    tcp_listener_adapter adapter("test-listener");
    adapter.close_connection("nonexistent-id");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

}  // namespace
}  // namespace kcenon::network::unified::adapters
