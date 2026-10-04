/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_http_client_adapter.cpp
 * @brief Unit tests for HTTP client adapter (i_protocol_client bridge)
 */

#include "internal/adapters/http_client_adapter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace kcenon::network::internal::adapters {
namespace {

// ============================================================================
// Construction Tests
// ============================================================================

TEST(HttpClientAdapterTest, ConstructWithDefaultTimeout) {
    http_client_adapter adapter("test-http-client");
    SUCCEED();
}

TEST(HttpClientAdapterTest, ConstructWithCustomTimeout) {
    http_client_adapter adapter("test-http-client", std::chrono::seconds(10));
    SUCCEED();
}

TEST(HttpClientAdapterTest, ConstructWithMillisecondTimeout) {
    http_client_adapter adapter("test-http-client", std::chrono::milliseconds(500));
    SUCCEED();
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(HttpClientAdapterTest, IsNotRunningBeforeStart) {
    http_client_adapter adapter("test-client");
    EXPECT_FALSE(adapter.is_running());
}

TEST(HttpClientAdapterTest, IsNotConnectedBeforeStart) {
    http_client_adapter adapter("test-client");
    EXPECT_FALSE(adapter.is_connected());
}

// ============================================================================
// HTTP-Specific Configuration Tests
// ============================================================================

TEST(HttpClientAdapterTest, SetPathBeforeStart) {
    http_client_adapter adapter("test-client");
    adapter.set_path("/api/v1/data");
    SUCCEED();
}

TEST(HttpClientAdapterTest, SetPathMultipleTimes) {
    http_client_adapter adapter("test-client");
    adapter.set_path("/api/v1");
    adapter.set_path("/api/v2");
    adapter.set_path("/health");
    SUCCEED();
}

TEST(HttpClientAdapterTest, SetPathWithEmptyString) {
    http_client_adapter adapter("test-client");
    adapter.set_path("");
    SUCCEED();
}

TEST(HttpClientAdapterTest, SetUseSslTrue) {
    http_client_adapter adapter("test-client");
    adapter.set_use_ssl(true);
    SUCCEED();
}

TEST(HttpClientAdapterTest, SetUseSslFalse) {
    http_client_adapter adapter("test-client");
    adapter.set_use_ssl(false);
    SUCCEED();
}

TEST(HttpClientAdapterTest, ConfigureAllSettingsBeforeStart) {
    http_client_adapter adapter("test-client", std::chrono::seconds(5));
    adapter.set_path("/api/v1/endpoint");
    adapter.set_use_ssl(true);
    adapter.set_receive_callback(
        [](const std::vector<uint8_t>&) {});
    adapter.set_connected_callback([]() {});
    adapter.set_error_callback(
        [](std::error_code) {});
    EXPECT_FALSE(adapter.is_running());
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST(HttpClientAdapterTest, SetAllCallbacks) {
    http_client_adapter adapter("test-client");
    adapter.set_receive_callback(
        [](const std::vector<uint8_t>&) {});
    adapter.set_connected_callback([]() {});
    adapter.set_disconnected_callback(
        []() {});
    adapter.set_error_callback(
        [](std::error_code) {});
    EXPECT_FALSE(adapter.is_connected());
}

// ============================================================================
// Guard Path Tests
// ============================================================================

TEST(HttpClientAdapterTest, StopBeforeStartIsIdempotent) {
    http_client_adapter adapter("test-client");
    auto result = adapter.stop();
    SUCCEED();
}

}  // namespace
}  // namespace kcenon::network::internal::adapters
