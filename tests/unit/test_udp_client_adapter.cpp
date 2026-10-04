/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_udp_client_adapter.cpp
 * @brief Unit tests for UDP client adapter (i_protocol_client bridge)
 */

#include "internal/adapters/udp_client_adapter.h"

#include <gtest/gtest.h>

#include <string>

namespace kcenon::network::internal::adapters {
namespace {

TEST(UdpClientAdapterTest, ConstructWithExplicitId) {
    udp_client_adapter adapter("test-udp-client");
    SUCCEED();
}

TEST(UdpClientAdapterTest, ConstructWithEmptyId) {
    udp_client_adapter adapter("");
    SUCCEED();
}

TEST(UdpClientAdapterTest, IsNotRunningBeforeStart) {
    udp_client_adapter adapter("test-client");
    EXPECT_FALSE(adapter.is_running());
}

TEST(UdpClientAdapterTest, IsNotConnectedBeforeStart) {
    udp_client_adapter adapter("test-client");
    EXPECT_FALSE(adapter.is_connected());
}

TEST(UdpClientAdapterTest, SetReceiveCallback) {
    udp_client_adapter adapter("test-client");
    adapter.set_receive_callback(
        [](const std::vector<uint8_t>&) {});
    SUCCEED();
}

TEST(UdpClientAdapterTest, SetConnectedCallback) {
    udp_client_adapter adapter("test-client");
    adapter.set_connected_callback([]() {});
    SUCCEED();
}

TEST(UdpClientAdapterTest, SetDisconnectedCallback) {
    udp_client_adapter adapter("test-client");
    adapter.set_disconnected_callback(
        []() {});
    SUCCEED();
}

TEST(UdpClientAdapterTest, SetErrorCallback) {
    udp_client_adapter adapter("test-client");
    adapter.set_error_callback(
        [](std::error_code) {});
    SUCCEED();
}

TEST(UdpClientAdapterTest, SetAllCallbacksSequentially) {
    udp_client_adapter adapter("test-client");
    adapter.set_receive_callback(
        [](const std::vector<uint8_t>&) {});
    adapter.set_connected_callback([]() {});
    adapter.set_disconnected_callback(
        []() {});
    adapter.set_error_callback(
        [](std::error_code) {});
    EXPECT_FALSE(adapter.is_connected());
}

TEST(UdpClientAdapterTest, StopBeforeStartIsIdempotent) {
    udp_client_adapter adapter("test-client");
    auto result = adapter.stop();
    SUCCEED();
}

}  // namespace
}  // namespace kcenon::network::internal::adapters
