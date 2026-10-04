/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_udp_server_adapter.cpp
 * @brief Unit tests for UDP server adapter (i_protocol_server bridge)
 */

#include "internal/adapters/udp_server_adapter.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace kcenon::network::internal::adapters {
namespace {

// ============================================================================
// Construction Tests
// ============================================================================

TEST(UdpServerAdapterTest, ConstructWithExplicitId) {
    udp_server_adapter adapter("test-udp-server");
    SUCCEED();
}

TEST(UdpServerAdapterTest, ConstructWithEmptyId) {
    udp_server_adapter adapter("");
    SUCCEED();
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(UdpServerAdapterTest, IsNotRunningBeforeStart) {
    udp_server_adapter adapter("test-server");
    EXPECT_FALSE(adapter.is_running());
}

TEST(UdpServerAdapterTest, ConnectionCountZeroBeforeStart) {
    udp_server_adapter adapter("test-server");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST(UdpServerAdapterTest, SetConnectionCallback) {
    udp_server_adapter adapter("test-server");
    adapter.set_connection_callback(
        [](std::shared_ptr<interfaces::i_session>) {});
    SUCCEED();
}

TEST(UdpServerAdapterTest, SetDisconnectionCallback) {
    udp_server_adapter adapter("test-server");
    adapter.set_disconnection_callback(
        [](std::string_view) {});
    SUCCEED();
}

TEST(UdpServerAdapterTest, SetReceiveCallback) {
    udp_server_adapter adapter("test-server");
    adapter.set_receive_callback(
        [](std::string_view, const std::vector<uint8_t>&) {});
    SUCCEED();
}

TEST(UdpServerAdapterTest, SetErrorCallback) {
    udp_server_adapter adapter("test-server");
    adapter.set_error_callback(
        [](std::string_view, std::error_code) {});
    SUCCEED();
}

TEST(UdpServerAdapterTest, SetAllCallbacksSequentially) {
    udp_server_adapter adapter("test-server");
    adapter.set_connection_callback(
        [](std::shared_ptr<interfaces::i_session>) {});
    adapter.set_disconnection_callback(
        [](std::string_view) {});
    adapter.set_receive_callback(
        [](std::string_view, const std::vector<uint8_t>&) {});
    adapter.set_error_callback(
        [](std::string_view, std::error_code) {});
    EXPECT_FALSE(adapter.is_running());
}

// ============================================================================
// Guard Path Tests
// ============================================================================

TEST(UdpServerAdapterTest, StopBeforeStartIsIdempotent) {
    udp_server_adapter adapter("test-server");
    auto result = adapter.stop();
    SUCCEED();
}

}  // namespace
}  // namespace kcenon::network::internal::adapters
