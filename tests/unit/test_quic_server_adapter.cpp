/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_quic_server_adapter.cpp
 * @brief Unit tests for QUIC server adapter (i_protocol_server bridge)
 */

#include "internal/adapters/quic_server_adapter.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace kcenon::network::internal::adapters {
namespace {

// ============================================================================
// Construction Tests
// ============================================================================

TEST(QuicServerAdapterTest, ConstructWithExplicitId) {
    quic_server_adapter adapter("test-quic-server");
    SUCCEED();
}

TEST(QuicServerAdapterTest, ConstructWithEmptyId) {
    quic_server_adapter adapter("");
    SUCCEED();
}

// ============================================================================
// Initial State Tests
// ============================================================================

TEST(QuicServerAdapterTest, IsNotRunningBeforeStart) {
    quic_server_adapter adapter("test-server");
    EXPECT_FALSE(adapter.is_running());
}

TEST(QuicServerAdapterTest, ConnectionCountZeroBeforeStart) {
    quic_server_adapter adapter("test-server");
    EXPECT_EQ(adapter.connection_count(), 0u);
}

// ============================================================================
// QUIC-Specific Configuration Tests
// ============================================================================

TEST(QuicServerAdapterTest, SetCertPath) {
    quic_server_adapter adapter("test-server");
    adapter.set_cert_path("/path/to/cert.pem");
    SUCCEED();
}

TEST(QuicServerAdapterTest, SetKeyPath) {
    quic_server_adapter adapter("test-server");
    adapter.set_key_path("/path/to/key.pem");
    SUCCEED();
}

TEST(QuicServerAdapterTest, SetAlpnProtocols) {
    quic_server_adapter adapter("test-server");
    adapter.set_alpn_protocols({"h3", "h3-29"});
    SUCCEED();
}

TEST(QuicServerAdapterTest, SetCaCertPath) {
    quic_server_adapter adapter("test-server");
    adapter.set_ca_cert_path("/path/to/ca.pem");
    SUCCEED();
}

TEST(QuicServerAdapterTest, SetRequireClientCert) {
    quic_server_adapter adapter("test-server");
    adapter.set_require_client_cert(true);
    SUCCEED();
}

TEST(QuicServerAdapterTest, SetMaxIdleTimeout) {
    quic_server_adapter adapter("test-server");
    adapter.set_max_idle_timeout(30000);
    SUCCEED();
}

TEST(QuicServerAdapterTest, SetMaxConnections) {
    quic_server_adapter adapter("test-server");
    adapter.set_max_connections(100);
    SUCCEED();
}

TEST(QuicServerAdapterTest, ConfigureAllSettingsBeforeStart) {
    quic_server_adapter adapter("test-server");
    adapter.set_cert_path("/path/to/cert.pem");
    adapter.set_key_path("/path/to/key.pem");
    adapter.set_alpn_protocols({"h3"});
    adapter.set_ca_cert_path("/path/to/ca.pem");
    adapter.set_require_client_cert(false);
    adapter.set_max_idle_timeout(60000);
    adapter.set_max_connections(50);
    EXPECT_FALSE(adapter.is_running());
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST(QuicServerAdapterTest, SetAllCallbacks) {
    quic_server_adapter adapter("test-server");
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

TEST(QuicServerAdapterTest, StopBeforeStartIsIdempotent) {
    quic_server_adapter adapter("test-server");
    auto result = adapter.stop();
    SUCCEED();
}

}  // namespace
}  // namespace kcenon::network::internal::adapters
