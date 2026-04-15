/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_quic_listener_adapter.cpp
 * @brief Unit tests for unified QUIC listener adapter (i_listener bridge)
 *
 * Tests validate:
 * - Construction with quic_config and explicit listener ID
 * - Initial state (not listening, zero connections)
 * - Callback registration (listener + accept callbacks)
 * - Stop idempotency before start
 * - close_connection() on unknown id is harmless
 * - Shutdown safety (destructor does not deadlock)
 *
 * Note: Live start() requires cert/key files and UDP socket binding.
 * Those paths are covered by integration tests.
 */

#include "internal/unified/adapters/quic_listener_adapter.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

namespace kcenon::network::unified::adapters {
namespace {

namespace quic_ns = kcenon::network::protocol::quic;

// ============================================================================
// Helpers
// ============================================================================

quic_ns::quic_config make_default_config() {
    quic_ns::quic_config cfg;
    cfg.server_name = "localhost";
    cfg.alpn_protocols = {"h3"};
    return cfg;
}

// ============================================================================
// Construction Tests
// ============================================================================

TEST(QuicListenerAdapterTest, ConstructWithExplicitId) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-quic-listener");
    SUCCEED();
}

TEST(QuicListenerAdapterTest, ConstructWithEmptyId) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "");
    SUCCEED();
}

TEST(QuicListenerAdapterTest, ConstructWithDefaultConfig) {
    quic_ns::quic_config cfg;
    quic_listener_adapter adapter(cfg, "test-listener");
    SUCCEED();
}

TEST(QuicListenerAdapterTest, ConstructWithCertAndKey) {
    quic_ns::quic_config cfg;
    cfg.server_name = "localhost";
    cfg.cert_file = "/path/to/cert.pem";
    cfg.key_file = "/path/to/key.pem";
    quic_listener_adapter adapter(cfg, "test-listener");
    SUCCEED();
}

TEST(QuicListenerAdapterTest, ConstructWithEarlyDataEnabled) {
    quic_ns::quic_config cfg;
    cfg.server_name = "localhost";
    cfg.enable_early_data = true;
    quic_listener_adapter adapter(cfg, "test-listener");
    SUCCEED();
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(QuicListenerAdapterTest, IsNotListeningBeforeStart) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
    EXPECT_FALSE(adapter.is_listening());
}

TEST(QuicListenerAdapterTest, ConnectionCountZeroBeforeStart) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST(QuicListenerAdapterTest, SetCallbacks) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
    listener_callbacks cbs;
    cbs.on_data = [](std::string_view, std::span<const std::byte>) {};
    cbs.on_disconnect = [](std::string_view) {};
    cbs.on_error = [](std::string_view, std::error_code) {};
    adapter.set_callbacks(std::move(cbs));
    SUCCEED();
}

TEST(QuicListenerAdapterTest, SetAcceptCallback) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
    adapter.set_accept_callback([](std::unique_ptr<i_connection>) {});
    SUCCEED();
}

TEST(QuicListenerAdapterTest, SetCallbacksMultipleTimes) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
    listener_callbacks cbs1;
    cbs1.on_data = [](std::string_view, std::span<const std::byte>) {};
    adapter.set_callbacks(std::move(cbs1));

    listener_callbacks cbs2;
    cbs2.on_error = [](std::string_view, std::error_code) {};
    adapter.set_callbacks(std::move(cbs2));
    SUCCEED();
}

// ============================================================================
// Shutdown Safety Tests
// ============================================================================

TEST(QuicListenerAdapterTest, StopBeforeStartIsIdempotent) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
    adapter.stop();
    EXPECT_FALSE(adapter.is_listening());
}

TEST(QuicListenerAdapterTest, MultipleStopCallsAreHarmless) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
    adapter.stop();
    adapter.stop();
    adapter.stop();
    EXPECT_FALSE(adapter.is_listening());
    EXPECT_EQ(adapter.connection_count(), 0u);
}

TEST(QuicListenerAdapterTest, CloseNonexistentConnectionIsIdempotent) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
    adapter.close_connection("nonexistent-id");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

TEST(QuicListenerAdapterTest, CloseEmptyIdIsHarmless) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
    adapter.close_connection("");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

TEST(QuicListenerAdapterTest, DestructorDoesNotDeadlock) {
    {
        auto cfg = make_default_config();
        quic_listener_adapter adapter(cfg, "test-listener");
        listener_callbacks cbs;
        cbs.on_data = [](std::string_view, std::span<const std::byte>) {};
        adapter.set_callbacks(std::move(cbs));
    }
    SUCCEED();
}

TEST(QuicListenerAdapterTest, RapidCreationAndDestruction) {
    for (int i = 0; i < 10; ++i) {
        auto cfg = make_default_config();
        quic_listener_adapter adapter(cfg, "listener-" + std::to_string(i));
        EXPECT_FALSE(adapter.is_listening());
    }
    SUCCEED();
}

// ============================================================================
// Full Configuration Tests
// ============================================================================

TEST(QuicListenerAdapterTest, ConfigureAllSettingsBeforeStart) {
    auto cfg = make_default_config();
    quic_listener_adapter adapter(cfg, "test-listener");
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
