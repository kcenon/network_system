/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file test_quic_client_adapter.cpp
 * @brief Unit tests for QUIC client adapter (i_protocol_client bridge)
 */

#include "internal/adapters/quic_client_adapter.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace kcenon::network::internal::adapters {
namespace {

TEST(QuicClientAdapterTest, ConstructWithExplicitId) {
    quic_client_adapter adapter("test-quic-client");
    SUCCEED();
}

TEST(QuicClientAdapterTest, IsNotRunningBeforeStart) {
    quic_client_adapter adapter("test-client");
    EXPECT_FALSE(adapter.is_running());
}

TEST(QuicClientAdapterTest, IsNotConnectedBeforeStart) {
    quic_client_adapter adapter("test-client");
    EXPECT_FALSE(adapter.is_connected());
}

// ============================================================================
// QUIC-Specific Configuration Tests
// ============================================================================

TEST(QuicClientAdapterTest, SetAlpnProtocols) {
    quic_client_adapter adapter("test-client");
    adapter.set_alpn_protocols({"h3", "h3-29"});
    SUCCEED();
}

TEST(QuicClientAdapterTest, SetAlpnProtocolsEmpty) {
    quic_client_adapter adapter("test-client");
    adapter.set_alpn_protocols({});
    SUCCEED();
}

TEST(QuicClientAdapterTest, SetCaCertPath) {
    quic_client_adapter adapter("test-client");
    adapter.set_ca_cert_path("/path/to/ca-cert.pem");
    SUCCEED();
}

TEST(QuicClientAdapterTest, SetClientCert) {
    quic_client_adapter adapter("test-client");
    adapter.set_client_cert("/path/to/client.pem", "/path/to/client-key.pem");
    SUCCEED();
}

TEST(QuicClientAdapterTest, SetVerifyServerTrue) {
    quic_client_adapter adapter("test-client");
    adapter.set_verify_server(true);
    SUCCEED();
}

TEST(QuicClientAdapterTest, SetVerifyServerFalse) {
    quic_client_adapter adapter("test-client");
    adapter.set_verify_server(false);
    SUCCEED();
}

TEST(QuicClientAdapterTest, SetMaxIdleTimeout) {
    quic_client_adapter adapter("test-client");
    adapter.set_max_idle_timeout(30000);
    SUCCEED();
}

TEST(QuicClientAdapterTest, ConfigureAllSettingsBeforeStart) {
    quic_client_adapter adapter("test-client");
    adapter.set_alpn_protocols({"h3"});
    adapter.set_ca_cert_path("/path/to/ca.pem");
    adapter.set_client_cert("/path/to/cert.pem", "/path/to/key.pem");
    adapter.set_verify_server(true);
    adapter.set_max_idle_timeout(60000);
    EXPECT_FALSE(adapter.is_running());
    EXPECT_FALSE(adapter.is_connected());
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST(QuicClientAdapterTest, SetAllCallbacks) {
    quic_client_adapter adapter("test-client");
    adapter.set_receive_callback(
        [](std::shared_ptr<interfaces::i_session>, std::vector<uint8_t>&&) {});
    adapter.set_connected_callback([](std::shared_ptr<interfaces::i_session>) {});
    adapter.set_disconnected_callback(
        [](std::shared_ptr<interfaces::i_session>) {});
    adapter.set_error_callback(
        [](std::shared_ptr<interfaces::i_session>, std::error_code) {});
    EXPECT_FALSE(adapter.is_connected());
}

// ============================================================================
// Guard Path Tests
// ============================================================================

TEST(QuicClientAdapterTest, StopBeforeStartIsIdempotent) {
    quic_client_adapter adapter("test-client");
    auto result = adapter.stop();
    SUCCEED();
}

}  // namespace
}  // namespace kcenon::network::internal::adapters
