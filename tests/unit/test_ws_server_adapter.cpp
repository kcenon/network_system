/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_ws_server_adapter.cpp
 * @brief Unit tests for WebSocket server adapter (i_protocol_server bridge)
 */

#include "internal/adapters/ws_server_adapter.h"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace kcenon::network::internal::adapters {
namespace {

TEST(WsServerAdapterTest, ConstructWithExplicitId) {
    ws_server_adapter adapter("test-ws-server");
    SUCCEED();
}

TEST(WsServerAdapterTest, IsNotRunningBeforeStart) {
    ws_server_adapter adapter("test-server");
    EXPECT_FALSE(adapter.is_running());
}

TEST(WsServerAdapterTest, ConnectionCountZeroBeforeStart) {
    ws_server_adapter adapter("test-server");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

TEST(WsServerAdapterTest, SetPath) {
    ws_server_adapter adapter("test-server");
    adapter.set_path("/ws");
    SUCCEED();
}

TEST(WsServerAdapterTest, SetPathMultipleTimes) {
    ws_server_adapter adapter("test-server");
    adapter.set_path("/ws");
    adapter.set_path("/chat");
    adapter.set_path("/notifications");
    SUCCEED();
}

TEST(WsServerAdapterTest, SetAllCallbacks) {
    ws_server_adapter adapter("test-server");
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

TEST(WsServerAdapterTest, StopBeforeStartIsIdempotent) {
    ws_server_adapter adapter("test-server");
    auto result = adapter.stop();
    SUCCEED();
}

TEST(WsServerAdapterTest, ConfigurePathAndCallbacksBeforeStart) {
    ws_server_adapter adapter("test-server");
    adapter.set_path("/ws");
    adapter.set_connection_callback(
        [](std::shared_ptr<interfaces::i_session>) {});
    adapter.set_receive_callback(
        [](std::string_view, const std::vector<uint8_t>&) {});
    EXPECT_FALSE(adapter.is_running());
    EXPECT_EQ(adapter.connection_count(), 0u);
}

}  // namespace
}  // namespace kcenon::network::internal::adapters
