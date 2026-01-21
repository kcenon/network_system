/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#include "kcenon/network/compat/unified_compat.h"

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>

using namespace kcenon::network;

/**
 * @file test_unified_compat.cpp
 * @brief Unit tests for backward-compatible type aliases in unified_compat.h
 *
 * Tests validate:
 * - Type aliases resolve to correct unified template instantiations
 * - Convenience aliases work without deprecation warnings
 * - Protocol and policy types are accessible via core namespace
 */

// ============================================================================
// Type Alias Resolution Tests
// ============================================================================

class UnifiedCompatTypeAliasTest : public ::testing::Test {};

TEST_F(UnifiedCompatTypeAliasTest, PlainTcpClientAliasEquivalentToTcpClient) {
    // compat::plain_tcp_client should be the same type as core::tcp_client
    static_assert(
        std::is_same_v<compat::plain_tcp_client, core::tcp_client>,
        "plain_tcp_client should be alias for tcp_client");
}

TEST_F(UnifiedCompatTypeAliasTest, PlainTcpServerAliasEquivalentToTcpServer) {
    // compat::plain_tcp_server should be the same type as core::tcp_server
    static_assert(
        std::is_same_v<compat::plain_tcp_server, core::tcp_server>,
        "plain_tcp_server should be alias for tcp_server");
}

#ifdef BUILD_TLS_SUPPORT
TEST_F(UnifiedCompatTypeAliasTest, TlsTcpClientAliasEquivalentToSecureTcpClient) {
    // compat::tls_tcp_client should be the same type as core::secure_tcp_client
    static_assert(
        std::is_same_v<compat::tls_tcp_client, core::secure_tcp_client>,
        "tls_tcp_client should be alias for secure_tcp_client");
}

TEST_F(UnifiedCompatTypeAliasTest, TlsTcpServerAliasEquivalentToSecureTcpServer) {
    // compat::tls_tcp_server should be the same type as core::secure_tcp_server
    static_assert(
        std::is_same_v<compat::tls_tcp_server, core::secure_tcp_server>,
        "tls_tcp_server should be alias for secure_tcp_server");
}
#endif

// ============================================================================
// Instantiation Tests
// ============================================================================

TEST_F(UnifiedCompatTypeAliasTest, PlainTcpClientCanBeInstantiated) {
    auto client = std::make_shared<compat::plain_tcp_client>("test_client");
    EXPECT_FALSE(client->is_running());
    EXPECT_FALSE(client->is_connected());
}

TEST_F(UnifiedCompatTypeAliasTest, PlainTcpServerCanBeInstantiated) {
    auto server = std::make_shared<compat::plain_tcp_server>("test_server");
    EXPECT_FALSE(server->is_running());
}

#ifdef BUILD_TLS_SUPPORT
TEST_F(UnifiedCompatTypeAliasTest, TlsTcpClientCanBeInstantiated) {
    // Note: Client doesn't load certificates at construction time, so this works
    // even without real certificate files
    policy::tls_enabled tls_config{
        .cert_path = "",
        .key_path = "",
        .ca_path = "",
        .verify_peer = false};
    auto client = std::make_shared<compat::tls_tcp_client>("secure_client", tls_config);
    EXPECT_FALSE(client->is_running());
    EXPECT_FALSE(client->is_connected());
}

// Note: TLS server requires valid certificate files at construction, so we skip
// the instantiation test. The type alias correctness is verified by the
// TlsTcpServerAliasEquivalentToSecureTcpServer static_assert test above.
#endif

// ============================================================================
// Protocol and Policy Access Tests
// ============================================================================

TEST_F(UnifiedCompatTypeAliasTest, ProtocolTypesAccessibleViaCore) {
    // Protocol types should be accessible via core namespace after including unified_compat.h
    [[maybe_unused]] auto tcp_name = core::tcp_protocol::name;
    [[maybe_unused]] auto udp_name = core::udp_protocol::name;
    [[maybe_unused]] auto ws_name = core::websocket_protocol::name;
    [[maybe_unused]] auto quic_name = core::quic_protocol::name;

    EXPECT_EQ(tcp_name, "tcp");
    EXPECT_EQ(udp_name, "udp");
    EXPECT_EQ(ws_name, "websocket");
    EXPECT_EQ(quic_name, "quic");
}

TEST_F(UnifiedCompatTypeAliasTest, PolicyTypesAccessibleViaCore) {
    // Policy types should be accessible via core namespace
    static_assert(core::no_tls::enabled == false, "no_tls should have enabled=false");
    static_assert(core::tls_enabled::enabled == true, "tls_enabled should have enabled=true");
}

// ============================================================================
// API Compatibility Tests
// ============================================================================

TEST_F(UnifiedCompatTypeAliasTest, PlainTcpClientHasExpectedApi) {
    auto client = std::make_shared<compat::plain_tcp_client>("api_test");

    // Verify expected methods exist and return expected types
    EXPECT_FALSE(client->is_running());
    EXPECT_FALSE(client->is_connected());
    EXPECT_EQ(client->client_id(), "api_test");

    // Callback setters should work
    client->set_receive_callback([](const std::vector<uint8_t>&) {});
    client->set_connected_callback([]() {});
    client->set_disconnected_callback([]() {});
    client->set_error_callback([](std::error_code) {});
}

TEST_F(UnifiedCompatTypeAliasTest, PlainTcpServerHasExpectedApi) {
    auto server = std::make_shared<compat::plain_tcp_server>("api_test_server");

    // Verify expected methods exist and return expected types
    EXPECT_FALSE(server->is_running());
    EXPECT_EQ(server->server_id(), "api_test_server");

    // Callback setters should work
    server->set_connection_callback([](auto) {});
    server->set_disconnection_callback([](const std::string&) {});
    server->set_receive_callback([](auto, const std::vector<uint8_t>&) {});
    server->set_error_callback([](auto, std::error_code) {});
}
