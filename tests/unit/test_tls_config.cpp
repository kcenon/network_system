/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include <kcenon/network/internal/common_defs.h>

using namespace kcenon::network::internal;

/**
 * @file test_tls_config.cpp
 * @brief Unit tests for TLS/SSL configuration structures
 *
 * Tests validate:
 * - Configuration validation logic
 * - Factory methods (secure_defaults, insecure_for_testing)
 * - Security enum values
 * - Default values and optional fields
 */

// ============================================================================
// TLS Version Tests
// ============================================================================

TEST(TLSVersionTest, EnumValues) {
    EXPECT_EQ(static_cast<std::uint8_t>(tls_version::tls_1_0), 10);
    EXPECT_EQ(static_cast<std::uint8_t>(tls_version::tls_1_1), 11);
    EXPECT_EQ(static_cast<std::uint8_t>(tls_version::tls_1_2), 12);
    EXPECT_EQ(static_cast<std::uint8_t>(tls_version::tls_1_3), 13);
}

TEST(TLSVersionTest, Comparison) {
    EXPECT_LT(tls_version::tls_1_0, tls_version::tls_1_2);
    EXPECT_LT(tls_version::tls_1_2, tls_version::tls_1_3);
    EXPECT_GT(tls_version::tls_1_3, tls_version::tls_1_0);
}

// ============================================================================
// Certificate Verification Tests
// ============================================================================

TEST(CertificateVerificationTest, EnumValues) {
    EXPECT_EQ(static_cast<std::uint8_t>(certificate_verification::none), 0);
    EXPECT_EQ(static_cast<std::uint8_t>(certificate_verification::verify_peer), 1);
    EXPECT_EQ(static_cast<std::uint8_t>(certificate_verification::verify_fail_if_no_peer_cert), 2);
}

// ============================================================================
// TLS Config - Default Constructor Tests
// ============================================================================

TEST(TLSConfigTest, DefaultConstruction) {
    tls_config config;

    // Default values (TLS 1.3 enforced by default - TICKET-009)
    EXPECT_FALSE(config.enabled);
    EXPECT_EQ(config.min_version, tls_version::tls_1_3);
    EXPECT_EQ(config.verify_mode, certificate_verification::verify_peer);
    EXPECT_TRUE(config.enable_session_resumption);
    EXPECT_EQ(config.handshake_timeout_ms, 10000);

    // Optional fields should be empty
    EXPECT_FALSE(config.certificate_file.has_value());
    EXPECT_FALSE(config.private_key_file.has_value());
    EXPECT_FALSE(config.private_key_password.has_value());
    EXPECT_FALSE(config.ca_file.has_value());
    EXPECT_FALSE(config.ca_path.has_value());
    EXPECT_FALSE(config.cipher_list.has_value());
    EXPECT_FALSE(config.sni_hostname.has_value());
}

// ============================================================================
// TLS Config - Validation Tests
// ============================================================================

TEST(TLSConfigTest, ValidationWhenDisabled) {
    tls_config config;
    config.enabled = false;

    // Should be valid even without certificates when disabled
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, ValidationWithVerificationNone) {
    tls_config config;
    config.enabled = true;
    config.verify_mode = certificate_verification::none;

    // Valid without CA when verification is disabled (testing only)
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, ValidationRequiresCAForVerification) {
    tls_config config;
    config.enabled = true;
    config.verify_mode = certificate_verification::verify_peer;

    // Invalid without CA file or path when verification is enabled
    EXPECT_FALSE(config.is_valid());

    // Valid with CA file
    config.ca_file = "/path/to/ca.crt";
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, ValidationWithCAPath) {
    tls_config config;
    config.enabled = true;
    config.verify_mode = certificate_verification::verify_peer;
    config.ca_path = "/etc/ssl/certs";

    // Valid with CA path instead of file
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, ValidationWithBothCAFileAndPath) {
    tls_config config;
    config.enabled = true;
    config.verify_mode = certificate_verification::verify_peer;
    config.ca_file = "/path/to/ca.crt";
    config.ca_path = "/etc/ssl/certs";

    // Valid with both
    EXPECT_TRUE(config.is_valid());
}

// ============================================================================
// TLS Config - Factory Methods Tests
// ============================================================================

TEST(TLSConfigTest, SecureDefaults) {
    auto config = tls_config::secure_defaults();

    // Should have secure defaults (TLS 1.3 enforced by default - TICKET-009)
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.min_version, tls_version::tls_1_3);
    EXPECT_EQ(config.verify_mode, certificate_verification::verify_peer);
    EXPECT_TRUE(config.enable_session_resumption);

    // Should NOT be valid without certificates
    EXPECT_FALSE(config.is_valid());

    // Should become valid with CA
    config.ca_file = "/path/to/ca.crt";
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, LegacyCompatible) {
    auto config = tls_config::legacy_compatible();

    // Should allow TLS 1.2 for backwards compatibility
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.min_version, tls_version::tls_1_2);
    EXPECT_EQ(config.verify_mode, certificate_verification::verify_peer);
    EXPECT_TRUE(config.enable_session_resumption);
}

TEST(TLSConfigTest, InsecureForTesting) {
    auto config = tls_config::insecure_for_testing();

    // Should be enabled but without verification
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.verify_mode, certificate_verification::none);

    // Should be valid even without certificates (testing only!)
    EXPECT_TRUE(config.is_valid());
}

// ============================================================================
// TLS Config - Server Configuration Tests
// ============================================================================

TEST(TLSConfigTest, ServerConfiguration) {
    auto config = tls_config::secure_defaults();
    config.certificate_file = "/etc/ssl/certs/server.crt";
    config.private_key_file = "/etc/ssl/private/server.key";
    config.ca_file = "/etc/ssl/certs/ca.crt";

    EXPECT_TRUE(config.is_valid());
    EXPECT_TRUE(config.certificate_file.has_value());
    EXPECT_TRUE(config.private_key_file.has_value());
    EXPECT_TRUE(config.ca_file.has_value());
}

TEST(TLSConfigTest, ServerWithEncryptedKey) {
    auto config = tls_config::secure_defaults();
    config.certificate_file = "/etc/ssl/certs/server.crt";
    config.private_key_file = "/etc/ssl/private/server.key";
    config.private_key_password = "secret_password";
    config.ca_file = "/etc/ssl/certs/ca.crt";

    EXPECT_TRUE(config.is_valid());
    EXPECT_TRUE(config.private_key_password.has_value());
    EXPECT_EQ(config.private_key_password.value(), "secret_password");
}

// ============================================================================
// TLS Config - Client Configuration Tests
// ============================================================================

TEST(TLSConfigTest, ClientConfiguration) {
    auto config = tls_config::secure_defaults();
    config.ca_file = "/etc/ssl/certs/ca-bundle.crt";
    config.sni_hostname = "api.example.com";

    EXPECT_TRUE(config.is_valid());
    EXPECT_FALSE(config.certificate_file.has_value()); // Client doesn't need cert
    EXPECT_TRUE(config.sni_hostname.has_value());
    EXPECT_EQ(config.sni_hostname.value(), "api.example.com");
}

TEST(TLSConfigTest, ClientWithMutualTLS) {
    auto config = tls_config::secure_defaults();
    config.certificate_file = "/path/to/client.crt";
    config.private_key_file = "/path/to/client.key";
    config.ca_file = "/etc/ssl/certs/ca.crt";
    config.verify_mode = certificate_verification::verify_fail_if_no_peer_cert;
    config.sni_hostname = "api.example.com";

    EXPECT_TRUE(config.is_valid());
    EXPECT_EQ(config.verify_mode, certificate_verification::verify_fail_if_no_peer_cert);
}

// ============================================================================
// TLS Config - Advanced Features Tests
// ============================================================================

TEST(TLSConfigTest, CustomCipherList) {
    auto config = tls_config::secure_defaults();
    config.ca_file = "/path/to/ca.crt";
    config.cipher_list = "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256";

    EXPECT_TRUE(config.is_valid());
    EXPECT_TRUE(config.cipher_list.has_value());
    EXPECT_EQ(config.cipher_list.value(), "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256");
}

TEST(TLSConfigTest, SessionResumption) {
    auto config = tls_config::secure_defaults();

    // Enabled by default
    EXPECT_TRUE(config.enable_session_resumption);

    // Can be disabled
    config.enable_session_resumption = false;
    EXPECT_FALSE(config.enable_session_resumption);
}

TEST(TLSConfigTest, HandshakeTimeout) {
    auto config = tls_config::secure_defaults();

    // Default timeout
    EXPECT_EQ(config.handshake_timeout_ms, 10000);

    // Custom timeout
    config.handshake_timeout_ms = 5000;
    EXPECT_EQ(config.handshake_timeout_ms, 5000);
}

TEST(TLSConfigTest, TLS13Configuration) {
    auto config = tls_config::secure_defaults();
    config.min_version = tls_version::tls_1_3;
    config.ca_file = "/path/to/ca.crt";

    EXPECT_TRUE(config.is_valid());
    EXPECT_EQ(config.min_version, tls_version::tls_1_3);
}

// ============================================================================
// TLS Config - Security Edge Cases
// ============================================================================

TEST(TLSConfigTest, DeprecatedTLSVersions) {
    auto config = tls_config::secure_defaults();
    config.ca_file = "/path/to/ca.crt";

    // TLS 1.0 and 1.1 are deprecated but still configurable
    config.min_version = tls_version::tls_1_0;
    EXPECT_TRUE(config.is_valid()); // Valid but insecure

    config.min_version = tls_version::tls_1_1;
    EXPECT_TRUE(config.is_valid()); // Valid but insecure
}

TEST(TLSConfigTest, EmptyStringOptionals) {
    auto config = tls_config::secure_defaults();

    // Empty strings should be treated as valid (though may fail at runtime)
    config.certificate_file = "";
    config.ca_file = "";

    // is_valid() checks presence, not content
    EXPECT_TRUE(config.is_valid());
}

// ============================================================================
// Constants Tests
// ============================================================================

TEST(TLSConstantsTest, DefaultCipherList) {
    std::string_view cipher_list = default_tls_cipher_list;

    EXPECT_FALSE(cipher_list.empty());
    EXPECT_NE(cipher_list.find("ECDHE"), std::string_view::npos);
    EXPECT_NE(cipher_list.find("AES"), std::string_view::npos);
    EXPECT_NE(cipher_list.find("GCM"), std::string_view::npos);
}

// ============================================================================
// Integration Scenario Tests
// ============================================================================

TEST(TLSConfigTest, ProductionServerScenario) {
    // Typical production server configuration
    auto config = tls_config::secure_defaults();
    config.min_version = tls_version::tls_1_3;
    config.certificate_file = "/etc/ssl/certs/server.crt";
    config.private_key_file = "/etc/ssl/private/server.key";
    config.ca_file = "/etc/ssl/certs/ca-bundle.crt";
    config.verify_mode = certificate_verification::verify_fail_if_no_peer_cert;
    config.cipher_list = default_tls_cipher_list;
    config.enable_session_resumption = true;
    config.handshake_timeout_ms = 5000;

    EXPECT_TRUE(config.is_valid());
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.min_version, tls_version::tls_1_3);
}

TEST(TLSConfigTest, DevelopmentClientScenario) {
    // Development client with relaxed security (testing only)
    auto config = tls_config::insecure_for_testing();
    config.sni_hostname = "localhost";

    EXPECT_TRUE(config.is_valid());
    EXPECT_TRUE(config.enabled);
    EXPECT_EQ(config.verify_mode, certificate_verification::none);
}

TEST(TLSConfigTest, MutualTLSAPIScenario) {
    // API client with mutual TLS authentication
    auto config = tls_config::secure_defaults();
    config.min_version = tls_version::tls_1_2;
    config.certificate_file = "/path/to/client.crt";
    config.private_key_file = "/path/to/client.key";
    config.ca_file = "/path/to/api-ca.crt";
    config.verify_mode = certificate_verification::verify_peer;
    config.sni_hostname = "api.production.example.com";
    config.handshake_timeout_ms = 10000;

    EXPECT_TRUE(config.is_valid());
    EXPECT_TRUE(config.certificate_file.has_value());
    EXPECT_TRUE(config.sni_hostname.has_value());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
