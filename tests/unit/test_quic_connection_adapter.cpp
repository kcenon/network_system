/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_quic_connection_adapter.cpp
 * @brief Unit tests for unified QUIC connection adapter (i_connection bridge)
 *
 * Tests validate:
 * - Construction with quic_config and explicit connection ID
 * - id() accessor returns constructor value
 * - Initial state (not connected, not connecting)
 * - Callback, options, timeout registration
 * - Close idempotency before connect
 * - Shutdown safety (destructor does not deadlock nor leak resources)
 *
 * Note: Live connect()/send() require UDP socket binding and a running QUIC
 * server. Those paths are covered by integration tests.
 */

#include "internal/unified/adapters/quic_connection_adapter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace kcenon::network::unified::adapters {
namespace {

namespace quic_ns = kcenon::network::protocol::quic;

// ============================================================================
// Helpers
// ============================================================================

quic_ns::quic_config make_default_config() {
    quic_ns::quic_config cfg;
    cfg.server_name = "example.com";
    cfg.alpn_protocols = {"h3"};
    return cfg;
}

// ============================================================================
// Construction Tests
// ============================================================================

TEST(QuicConnectionAdapterTest, ConstructWithExplicitId) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-quic-conn");
    SUCCEED();
}

TEST(QuicConnectionAdapterTest, ConstructWithEmptyId) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "");
    SUCCEED();
}

TEST(QuicConnectionAdapterTest, ConstructWithDefaultConfig) {
    quic_ns::quic_config cfg;
    quic_connection_adapter adapter(cfg, "test-conn");
    SUCCEED();
}

TEST(QuicConnectionAdapterTest, IdReturnsConstructorValue) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "my-quic-connection");
    EXPECT_EQ(adapter.id(), "my-quic-connection");
}

TEST(QuicConnectionAdapterTest, ConstructWithInsecureConfig) {
    quic_ns::quic_config cfg;
    cfg.server_name = "localhost";
    cfg.insecure_skip_verify = true;
    quic_connection_adapter adapter(cfg, "test-conn");
    SUCCEED();
}

TEST(QuicConnectionAdapterTest, ConstructWithCustomStreamLimits) {
    quic_ns::quic_config cfg;
    cfg.server_name = "example.com";
    cfg.max_bidi_streams = 50;
    cfg.max_uni_streams = 50;
    quic_connection_adapter adapter(cfg, "test-conn");
    SUCCEED();
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(QuicConnectionAdapterTest, IsNotConnectedBeforeConnect) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-conn");
    EXPECT_FALSE(adapter.is_connected());
}

TEST(QuicConnectionAdapterTest, IsNotConnectingBeforeConnect) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-conn");
    EXPECT_FALSE(adapter.is_connecting());
}

// ============================================================================
// Configuration Tests
// ============================================================================

TEST(QuicConnectionAdapterTest, SetCallbacks) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-conn");
    connection_callbacks cbs;
    cbs.on_connected = []() {};
    cbs.on_disconnected = []() {};
    cbs.on_data = [](std::span<const std::byte>) {};
    cbs.on_error = [](std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    SUCCEED();
}

TEST(QuicConnectionAdapterTest, SetOptions) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-conn");
    connection_options opts;
    adapter.set_options(std::move(opts));
    SUCCEED();
}

TEST(QuicConnectionAdapterTest, SetTimeout) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-conn");
    adapter.set_timeout(std::chrono::seconds(30));
    SUCCEED();
}

TEST(QuicConnectionAdapterTest, SetTimeoutMultipleTimes) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-conn");
    adapter.set_timeout(std::chrono::seconds(5));
    adapter.set_timeout(std::chrono::milliseconds(500));
    adapter.set_timeout(std::chrono::minutes(1));
    SUCCEED();
}

// ============================================================================
// Shutdown Safety Tests
// ============================================================================

TEST(QuicConnectionAdapterTest, CloseBeforeConnectIsIdempotent) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-conn");
    adapter.close();
    EXPECT_FALSE(adapter.is_connected());
}

TEST(QuicConnectionAdapterTest, MultipleCloseCallsAreHarmless) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-conn");
    adapter.close();
    adapter.close();
    adapter.close();
    EXPECT_FALSE(adapter.is_connected());
}

TEST(QuicConnectionAdapterTest, DestructorDoesNotDeadlock) {
    {
        auto cfg = make_default_config();
        quic_connection_adapter adapter(cfg, "test-conn");
        connection_callbacks cbs;
        cbs.on_connected = []() {};
        adapter.set_callbacks(std::move(cbs));
    }
    SUCCEED();
}

TEST(QuicConnectionAdapterTest, RapidCreationAndDestruction) {
    for (int i = 0; i < 10; ++i) {
        auto cfg = make_default_config();
        quic_connection_adapter adapter(cfg, "conn-" + std::to_string(i));
        EXPECT_FALSE(adapter.is_connected());
    }
    SUCCEED();
}

// ============================================================================
// Full Configuration Tests
// ============================================================================

TEST(QuicConnectionAdapterTest, ConfigureAllSettingsBeforeConnect) {
    auto cfg = make_default_config();
    quic_connection_adapter adapter(cfg, "test-conn");
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
