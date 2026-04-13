/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_ws_client_adapter.cpp
 * @brief Unit tests for WebSocket client adapter (i_protocol_client bridge)
 */

#include "internal/adapters/ws_client_adapter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace kcenon::network::internal::adapters {
namespace {

TEST(WsClientAdapterTest, ConstructWithDefaultPingInterval) {
    ws_client_adapter adapter("test-ws-client");
    SUCCEED();
}

TEST(WsClientAdapterTest, ConstructWithCustomPingInterval) {
    ws_client_adapter adapter("test-ws-client", std::chrono::seconds(10));
    SUCCEED();
}

TEST(WsClientAdapterTest, IsNotRunningBeforeStart) {
    ws_client_adapter adapter("test-client");
    EXPECT_FALSE(adapter.is_running());
}

TEST(WsClientAdapterTest, IsNotConnectedBeforeStart) {
    ws_client_adapter adapter("test-client");
    EXPECT_FALSE(adapter.is_connected());
}

TEST(WsClientAdapterTest, SetPath) {
    ws_client_adapter adapter("test-client");
    adapter.set_path("/ws/chat");
    SUCCEED();
}

TEST(WsClientAdapterTest, SetPathMultipleTimes) {
    ws_client_adapter adapter("test-client");
    adapter.set_path("/ws/v1");
    adapter.set_path("/ws/v2");
    SUCCEED();
}

TEST(WsClientAdapterTest, SetAllCallbacks) {
    ws_client_adapter adapter("test-client");
    adapter.set_receive_callback(
        [](std::shared_ptr<interfaces::i_session>, std::vector<uint8_t>&&) {});
    adapter.set_connected_callback([](std::shared_ptr<interfaces::i_session>) {});
    adapter.set_disconnected_callback(
        [](std::shared_ptr<interfaces::i_session>) {});
    adapter.set_error_callback(
        [](std::shared_ptr<interfaces::i_session>, std::error_code) {});
    EXPECT_FALSE(adapter.is_connected());
}

TEST(WsClientAdapterTest, ConfigurePathAndCallbacksBeforeStart) {
    ws_client_adapter adapter("test-client", std::chrono::seconds(15));
    adapter.set_path("/ws");
    adapter.set_receive_callback(
        [](std::shared_ptr<interfaces::i_session>, std::vector<uint8_t>&&) {});
    adapter.set_error_callback(
        [](std::shared_ptr<interfaces::i_session>, std::error_code) {});
    EXPECT_FALSE(adapter.is_running());
}

TEST(WsClientAdapterTest, StopBeforeStartIsIdempotent) {
    ws_client_adapter adapter("test-client");
    auto result = adapter.stop();
    SUCCEED();
}

}  // namespace
}  // namespace kcenon::network::internal::adapters
