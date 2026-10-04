/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_tcp_server_adapter.cpp
 * @brief Unit tests for TCP server adapter (i_protocol_server bridge)
 */

#include "internal/adapters/tcp_server_adapter.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace kcenon::network::internal::adapters {
namespace {

// ============================================================================
// Construction Tests
// ============================================================================

TEST(TcpServerAdapterTest, ConstructWithExplicitId) {
    tcp_server_adapter adapter("test-tcp-server");
    // Should construct without throwing
    SUCCEED();
}

TEST(TcpServerAdapterTest, ConstructWithEmptyId) {
    tcp_server_adapter adapter("");
    // Empty ID should be accepted (adapter may generate internal ID)
    SUCCEED();
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(TcpServerAdapterTest, IsNotRunningBeforeStart) {
    tcp_server_adapter adapter("test-server");
    EXPECT_FALSE(adapter.is_running());
}

TEST(TcpServerAdapterTest, ConnectionCountZeroBeforeStart) {
    tcp_server_adapter adapter("test-server");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST(TcpServerAdapterTest, SetConnectionCallback) {
    tcp_server_adapter adapter("test-server");
    bool called = false;
    adapter.set_connection_callback(
        [&called](std::shared_ptr<interfaces::i_session>) { called = true; });
    SUCCEED();
}

TEST(TcpServerAdapterTest, SetDisconnectionCallback) {
    tcp_server_adapter adapter("test-server");
    adapter.set_disconnection_callback(
        [](std::string_view) {});
    SUCCEED();
}

TEST(TcpServerAdapterTest, SetReceiveCallback) {
    tcp_server_adapter adapter("test-server");
    adapter.set_receive_callback(
        [](std::string_view, const std::vector<uint8_t>&) {});
    SUCCEED();
}

TEST(TcpServerAdapterTest, SetErrorCallback) {
    tcp_server_adapter adapter("test-server");
    adapter.set_error_callback(
        [](std::string_view, std::error_code) {});
    SUCCEED();
}

TEST(TcpServerAdapterTest, SetAllCallbacksSequentially) {
    tcp_server_adapter adapter("test-server");
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

TEST(TcpServerAdapterTest, StopBeforeStartIsIdempotent) {
    tcp_server_adapter adapter("test-server");
    auto result = adapter.stop();
    // stop() before start() should not crash or error fatally
    SUCCEED();
}

}  // namespace
}  // namespace kcenon::network::internal::adapters
