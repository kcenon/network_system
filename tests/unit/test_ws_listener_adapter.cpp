/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_ws_listener_adapter.cpp
 * @brief Unit tests for unified WebSocket listener adapter (i_listener bridge)
 *
 * Tests validate:
 * - Construction with explicit listener ID
 * - Initial state (not listening, zero connections)
 * - Callback registration (listener + accept callbacks)
 * - WebSocket path configuration
 * - Stop idempotency before start
 * - close_connection() on unknown id is harmless
 * - Shutdown safety (destructor does not deadlock)
 *
 * Note: Live start()/send_to()/broadcast() require an actual bind.
 * Those paths are covered by integration tests.
 */

#include "internal/unified/adapters/ws_listener_adapter.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace kcenon::network::unified::adapters {
namespace {

// ============================================================================
// Construction Tests
// ============================================================================

TEST(WsListenerAdapterTest, ConstructWithExplicitId) {
    ws_listener_adapter adapter("test-ws-listener");
    SUCCEED();
}

TEST(WsListenerAdapterTest, ConstructWithEmptyId) {
    ws_listener_adapter adapter("");
    SUCCEED();
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(WsListenerAdapterTest, IsNotListeningBeforeStart) {
    ws_listener_adapter adapter("test-listener");
    EXPECT_FALSE(adapter.is_listening());
}

TEST(WsListenerAdapterTest, ConnectionCountZeroBeforeStart) {
    ws_listener_adapter adapter("test-listener");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST(WsListenerAdapterTest, SetCallbacks) {
    ws_listener_adapter adapter("test-listener");
    listener_callbacks cbs;
    cbs.on_data = [](std::string_view, std::span<const std::byte>) {};
    cbs.on_disconnect = [](std::string_view) {};
    cbs.on_error = [](std::string_view, std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    SUCCEED();
}

TEST(WsListenerAdapterTest, SetAcceptCallback) {
    ws_listener_adapter adapter("test-listener");
    adapter.set_accept_callback([](std::unique_ptr<i_connection>) {});
    SUCCEED();
}

TEST(WsListenerAdapterTest, SetCallbacksMultipleTimes) {
    ws_listener_adapter adapter("test-listener");
    listener_callbacks cbs1;
    cbs1.on_data = [](std::string_view, std::span<const std::byte>) {};
    adapter.set_callbacks(std::move(cbs1));

    listener_callbacks cbs2;
    cbs2.on_error = [](std::string_view, std::error_code) {};
    adapter.set_callbacks(std::move(cbs2));
    SUCCEED();
}

// ============================================================================
// WebSocket-Specific Configuration Tests
// ============================================================================

TEST(WsListenerAdapterTest, SetPath) {
    ws_listener_adapter adapter("test-listener");
    adapter.set_path("/ws");
    SUCCEED();
}

TEST(WsListenerAdapterTest, SetPathMultipleTimes) {
    ws_listener_adapter adapter("test-listener");
    adapter.set_path("/ws/v1");
    adapter.set_path("/ws/v2");
    adapter.set_path("/socket.io/");
    SUCCEED();
}

TEST(WsListenerAdapterTest, SetPathWithEmptyString) {
    ws_listener_adapter adapter("test-listener");
    adapter.set_path("");
    SUCCEED();
}

// ============================================================================
// Shutdown Safety Tests
// ============================================================================

TEST(WsListenerAdapterTest, StopBeforeStartIsIdempotent) {
    ws_listener_adapter adapter("test-listener");
    adapter.stop();
    EXPECT_FALSE(adapter.is_listening());
}

TEST(WsListenerAdapterTest, MultipleStopCallsAreHarmless) {
    ws_listener_adapter adapter("test-listener");
    adapter.stop();
    adapter.stop();
    adapter.stop();
    EXPECT_FALSE(adapter.is_listening());
    EXPECT_EQ(adapter.connection_count(), 0u);
}

TEST(WsListenerAdapterTest, CloseNonexistentConnectionIsIdempotent) {
    ws_listener_adapter adapter("test-listener");
    adapter.close_connection("nonexistent-id");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

TEST(WsListenerAdapterTest, CloseEmptyIdIsHarmless) {
    ws_listener_adapter adapter("test-listener");
    adapter.close_connection("");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

TEST(WsListenerAdapterTest, DestructorDoesNotDeadlock) {
    {
        ws_listener_adapter adapter("test-listener");
        adapter.set_path("/ws");
        listener_callbacks cbs;
        cbs.on_data = [](std::string_view, std::span<const std::byte>) {};
        adapter.set_callbacks(std::move(cbs));
    }
    SUCCEED();
}

TEST(WsListenerAdapterTest, RapidCreationAndDestruction) {
    for (int i = 0; i < 20; ++i) {
        ws_listener_adapter adapter("listener-" + std::to_string(i));
        EXPECT_FALSE(adapter.is_listening());
    }
    SUCCEED();
}

// ============================================================================
// Full Configuration Tests
// ============================================================================

TEST(WsListenerAdapterTest, ConfigureAllSettingsBeforeStart) {
    ws_listener_adapter adapter("test-listener");
    adapter.set_path("/ws/api");
    listener_callbacks cbs;
    cbs.on_data = [](std::string_view, std::span<const std::byte>) {};
    cbs.on_disconnect = [](std::string_view) {};
    cbs.on_error = [](std::string_view, std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    adapter.set_accept_callback([](std::unique_ptr<i_connection>) {});
    EXPECT_FALSE(adapter.is_listening());
    EXPECT_EQ(adapter.connection_count(), 0u);
}

}  // namespace
}  // namespace kcenon::network::unified::adapters
