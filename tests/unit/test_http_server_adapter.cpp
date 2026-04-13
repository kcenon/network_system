/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_http_server_adapter.cpp
 * @brief Unit tests for HTTP server adapter (i_protocol_server bridge)
 */

#include "internal/adapters/http_server_adapter.h"

#include <gtest/gtest.h>

#include <string>

namespace kcenon::network::internal::adapters {
namespace {

// ============================================================================
// Construction Tests
// ============================================================================

TEST(HttpServerAdapterTest, ConstructWithExplicitId) {
    http_server_adapter adapter("test-http-server");
    SUCCEED();
}

TEST(HttpServerAdapterTest, ConstructWithEmptyId) {
    http_server_adapter adapter("");
    SUCCEED();
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(HttpServerAdapterTest, IsNotRunningBeforeStart) {
    http_server_adapter adapter("test-server");
    EXPECT_FALSE(adapter.is_running());
}

TEST(HttpServerAdapterTest, ConnectionCountZeroBeforeStart) {
    http_server_adapter adapter("test-server");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST(HttpServerAdapterTest, SetConnectionCallback) {
    http_server_adapter adapter("test-server");
    adapter.set_connection_callback(
        [](std::shared_ptr<interfaces::i_session>) {});
    SUCCEED();
}

TEST(HttpServerAdapterTest, SetDisconnectionCallback) {
    http_server_adapter adapter("test-server");
    adapter.set_disconnection_callback(
        [](std::shared_ptr<interfaces::i_session>) {});
    SUCCEED();
}

TEST(HttpServerAdapterTest, SetReceiveCallback) {
    http_server_adapter adapter("test-server");
    adapter.set_receive_callback(
        [](std::shared_ptr<interfaces::i_session>, std::vector<uint8_t>&&) {});
    SUCCEED();
}

TEST(HttpServerAdapterTest, SetErrorCallback) {
    http_server_adapter adapter("test-server");
    adapter.set_error_callback(
        [](std::shared_ptr<interfaces::i_session>, std::error_code) {});
    SUCCEED();
}

TEST(HttpServerAdapterTest, SetAllCallbacksSequentially) {
    http_server_adapter adapter("test-server");
    adapter.set_connection_callback(
        [](std::shared_ptr<interfaces::i_session>) {});
    adapter.set_disconnection_callback(
        [](std::shared_ptr<interfaces::i_session>) {});
    adapter.set_receive_callback(
        [](std::shared_ptr<interfaces::i_session>, std::vector<uint8_t>&&) {});
    adapter.set_error_callback(
        [](std::shared_ptr<interfaces::i_session>, std::error_code) {});
    EXPECT_FALSE(adapter.is_running());
}

// ============================================================================
// Guard Path Tests
// ============================================================================

TEST(HttpServerAdapterTest, StopBeforeStartIsIdempotent) {
    http_server_adapter adapter("test-server");
    auto result = adapter.stop();
    SUCCEED();
}

}  // namespace
}  // namespace kcenon::network::internal::adapters
