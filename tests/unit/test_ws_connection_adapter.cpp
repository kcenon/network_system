/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_ws_connection_adapter.cpp
 * @brief Unit tests for unified WebSocket connection adapter (i_connection bridge)
 *
 * Tests validate:
 * - Construction with explicit connection ID
 * - id() accessor returns constructor value
 * - Initial state (not connected, not connecting)
 * - Callback registration
 * - Options and timeout setting
 * - WebSocket path configuration
 * - Close idempotency before connect
 * - Shutdown safety (destructor does not deadlock)
 *
 * Note: Live connect()/send() require a running WebSocket server.
 * Those paths are covered by integration tests.
 */

#include "internal/unified/adapters/ws_connection_adapter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace kcenon::network::unified::adapters {
namespace {

// ============================================================================
// Construction Tests
// ============================================================================

TEST(WsConnectionAdapterTest, ConstructWithExplicitId) {
    ws_connection_adapter adapter("test-ws-conn");
    SUCCEED();
}

TEST(WsConnectionAdapterTest, ConstructWithEmptyId) {
    ws_connection_adapter adapter("");
    SUCCEED();
}

TEST(WsConnectionAdapterTest, IdReturnsConstructorValue) {
    ws_connection_adapter adapter("my-ws-connection-id");
    EXPECT_EQ(adapter.id(), "my-ws-connection-id");
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(WsConnectionAdapterTest, IsNotConnectedBeforeConnect) {
    ws_connection_adapter adapter("test-conn");
    EXPECT_FALSE(adapter.is_connected());
}

TEST(WsConnectionAdapterTest, IsNotConnectingBeforeConnect) {
    ws_connection_adapter adapter("test-conn");
    EXPECT_FALSE(adapter.is_connecting());
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST(WsConnectionAdapterTest, SetCallbacks) {
    ws_connection_adapter adapter("test-conn");
    connection_callbacks cbs;
    cbs.on_connected = []() {};
    cbs.on_disconnected = []() {};
    cbs.on_data = [](std::span<const std::byte>) {};
    cbs.on_error = [](std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    SUCCEED();
}

TEST(WsConnectionAdapterTest, SetOptions) {
    ws_connection_adapter adapter("test-conn");
    connection_options opts;
    adapter.set_options(std::move(opts));
    SUCCEED();
}

TEST(WsConnectionAdapterTest, SetTimeout) {
    ws_connection_adapter adapter("test-conn");
    adapter.set_timeout(std::chrono::seconds(15));
    SUCCEED();
}

TEST(WsConnectionAdapterTest, SetTimeoutMultipleTimes) {
    ws_connection_adapter adapter("test-conn");
    adapter.set_timeout(std::chrono::seconds(5));
    adapter.set_timeout(std::chrono::milliseconds(250));
    adapter.set_timeout(std::chrono::minutes(1));
    SUCCEED();
}

// ============================================================================
// WebSocket-Specific Configuration Tests
// ============================================================================

TEST(WsConnectionAdapterTest, SetPath) {
    ws_connection_adapter adapter("test-conn");
    adapter.set_path("/ws");
    SUCCEED();
}

TEST(WsConnectionAdapterTest, SetPathMultipleTimes) {
    ws_connection_adapter adapter("test-conn");
    adapter.set_path("/ws/v1");
    adapter.set_path("/ws/v2");
    adapter.set_path("/socket.io/");
    SUCCEED();
}

TEST(WsConnectionAdapterTest, SetPathWithEmptyString) {
    ws_connection_adapter adapter("test-conn");
    adapter.set_path("");
    SUCCEED();
}

// ============================================================================
// Shutdown Safety Tests
// ============================================================================

TEST(WsConnectionAdapterTest, CloseBeforeConnectIsIdempotent) {
    ws_connection_adapter adapter("test-conn");
    adapter.close();
    EXPECT_FALSE(adapter.is_connected());
}

TEST(WsConnectionAdapterTest, MultipleCloseCallsAreHarmless) {
    ws_connection_adapter adapter("test-conn");
    adapter.close();
    adapter.close();
    adapter.close();
    EXPECT_FALSE(adapter.is_connected());
}

TEST(WsConnectionAdapterTest, DestructorDoesNotDeadlock) {
    {
        ws_connection_adapter adapter("test-conn");
        adapter.set_path("/ws");
        connection_callbacks cbs;
        cbs.on_connected = []() {};
        adapter.set_callbacks(std::move(cbs));
    }
    SUCCEED();
}

TEST(WsConnectionAdapterTest, RapidCreationAndDestruction) {
    for (int i = 0; i < 20; ++i) {
        ws_connection_adapter adapter("conn-" + std::to_string(i));
        EXPECT_FALSE(adapter.is_connected());
    }
    SUCCEED();
}

// ============================================================================
// Full Configuration Tests
// ============================================================================

TEST(WsConnectionAdapterTest, ConfigureAllSettingsBeforeConnect) {
    ws_connection_adapter adapter("test-conn");
    adapter.set_path("/ws/api");
    adapter.set_timeout(std::chrono::seconds(30));
    connection_options opts;
    adapter.set_options(std::move(opts));
    connection_callbacks cbs;
    cbs.on_connected = []() {};
    cbs.on_disconnected = []() {};
    cbs.on_data = [](std::span<const std::byte>) {};
    cbs.on_error = [](std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    EXPECT_FALSE(adapter.is_connected());
    EXPECT_FALSE(adapter.is_connecting());
}

}  // namespace
}  // namespace kcenon::network::unified::adapters
