/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_udp_listener_adapter.cpp
 * @brief Unit tests for unified UDP listener adapter (i_listener bridge)
 */

#include "internal/unified/adapters/udp_listener_adapter.h"

#include <gtest/gtest.h>

#include <string>

namespace kcenon::network::unified::adapters {
namespace {

TEST(UdpListenerAdapterTest, ConstructWithExplicitId) {
    udp_listener_adapter adapter("test-udp-listener");
    SUCCEED();
}

TEST(UdpListenerAdapterTest, IsNotListeningBeforeStart) {
    udp_listener_adapter adapter("test-listener");
    EXPECT_FALSE(adapter.is_listening());
}

TEST(UdpListenerAdapterTest, ConnectionCountZeroBeforeStart) {
    udp_listener_adapter adapter("test-listener");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

TEST(UdpListenerAdapterTest, SetCallbacks) {
    udp_listener_adapter adapter("test-listener");
    listener_callbacks cbs;
    cbs.on_data = [](std::string_view, std::span<const std::byte>) {};
    cbs.on_disconnected = [](std::string_view) {};
    cbs.on_error = [](std::string_view, std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    SUCCEED();
}

TEST(UdpListenerAdapterTest, SetAcceptCallback) {
    udp_listener_adapter adapter("test-listener");
    adapter.set_accept_callback([](std::string_view) {});
    SUCCEED();
}

TEST(UdpListenerAdapterTest, StopBeforeStartIsIdempotent) {
    udp_listener_adapter adapter("test-listener");
    adapter.stop();
    EXPECT_FALSE(adapter.is_listening());
}

TEST(UdpListenerAdapterTest, CloseNonexistentConnectionIsIdempotent) {
    udp_listener_adapter adapter("test-listener");
    adapter.close_connection("nonexistent-id");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

}  // namespace
}  // namespace kcenon::network::unified::adapters
