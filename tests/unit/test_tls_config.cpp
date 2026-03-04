/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include <gtest/gtest.h>
#include <internal/utils/common_defs.h>

#include <limits>
#include <string>

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

// ============================================================================
// Invalid Configuration Combinations
// ============================================================================

TEST(TLSConfigTest, EnabledWithVerifyPeerButNoCA) {
    // Enabled + verify_peer without CA file/path is invalid
    tls_config config;
    config.enabled = true;
    config.verify_mode = certificate_verification::verify_peer;
    // No ca_file or ca_path set
    EXPECT_FALSE(config.is_valid());
}

TEST(TLSConfigTest, EnabledWithVerifyFailIfNoPeerCertButNoCA) {
    // Strictest verification mode also requires CA
    tls_config config;
    config.enabled = true;
    config.verify_mode = certificate_verification::verify_fail_if_no_peer_cert;
    EXPECT_FALSE(config.is_valid());

    // Becomes valid with ca_path alone
    config.ca_path = "/etc/ssl/certs";
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, DisabledIgnoresAllOtherFields) {
    // When disabled, any combination of fields is considered valid
    tls_config config;
    config.enabled = false;
    config.verify_mode = certificate_verification::verify_fail_if_no_peer_cert;
    // No CA provided, but doesn't matter when disabled
    EXPECT_TRUE(config.is_valid());

    config.certificate_file = "/nonexistent/path";
    config.private_key_file = "/nonexistent/key";
    config.cipher_list = "INVALID_CIPHER";
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, KeyWithoutCertificateIsValid) {
    // Having a private key without a certificate is structurally valid
    // (server-specific validation is deferred to the server class)
    tls_config config;
    config.enabled = true;
    config.verify_mode = certificate_verification::none;
    config.private_key_file = "/path/to/key.pem";
    // No certificate_file
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, CertificateWithoutKeyIsValid) {
    // Having a certificate without a key is structurally valid
    tls_config config;
    config.enabled = true;
    config.verify_mode = certificate_verification::none;
    config.certificate_file = "/path/to/cert.pem";
    // No private_key_file
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, AllFieldsPopulated) {
    // Every optional field set simultaneously
    tls_config config;
    config.enabled = true;
    config.min_version = tls_version::tls_1_3;
    config.verify_mode = certificate_verification::verify_fail_if_no_peer_cert;
    config.certificate_file = "/path/to/cert.pem";
    config.private_key_file = "/path/to/key.pem";
    config.private_key_password = "password123";
    config.ca_file = "/path/to/ca.pem";
    config.ca_path = "/etc/ssl/certs";
    config.cipher_list = "ECDHE-RSA-AES256-GCM-SHA384";
    config.sni_hostname = "example.com";
    config.enable_session_resumption = true;
    config.handshake_timeout_ms = 3000;

    EXPECT_TRUE(config.is_valid());
}

// ============================================================================
// Nonexistent File Path Validation (structural, not filesystem)
// ============================================================================

TEST(TLSConfigTest, NonexistentFilePathsStillStructurallyValid) {
    // is_valid() checks field presence, not filesystem existence
    auto config = tls_config::secure_defaults();
    config.certificate_file = "/absolutely/nonexistent/cert.pem";
    config.private_key_file = "/absolutely/nonexistent/key.pem";
    config.ca_file = "/absolutely/nonexistent/ca.pem";

    // Structurally valid (filesystem validation is deferred to runtime)
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, NonexistentCAPathStillStructurallyValid) {
    auto config = tls_config::secure_defaults();
    config.ca_path = "/absolutely/nonexistent/ca_dir/";

    EXPECT_TRUE(config.is_valid());
}

// ============================================================================
// Cipher Suite Validation Edge Cases
// ============================================================================

TEST(TLSConfigTest, EmptyCipherListIsValid) {
    auto config = tls_config::secure_defaults();
    config.ca_file = "/path/to/ca.crt";
    config.cipher_list = "";
    // Empty cipher_list is structurally valid (has_value() returns true)
    EXPECT_TRUE(config.is_valid());
    EXPECT_TRUE(config.cipher_list.has_value());
    EXPECT_TRUE(config.cipher_list.value().empty());
}

TEST(TLSConfigTest, InvalidCipherStringIsStructurallyValid) {
    // is_valid() does not validate cipher syntax — that's OpenSSL's job
    auto config = tls_config::secure_defaults();
    config.ca_file = "/path/to/ca.crt";
    config.cipher_list = "COMPLETELY_BOGUS_CIPHER";
    EXPECT_TRUE(config.is_valid());
}

TEST(TLSConfigTest, MultipleCipherSuitesDelimitedByColon) {
    auto config = tls_config::secure_defaults();
    config.ca_file = "/path/to/ca.crt";
    config.cipher_list = "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-CHACHA20-POLY1305";

    EXPECT_TRUE(config.is_valid());
    auto& cl = config.cipher_list.value();
    // Verify colon-separated format
    EXPECT_NE(cl.find(':'), std::string::npos);
}

TEST(TLSConfigTest, DefaultCipherListContainsForwardSecrecyCiphers) {
    // The default cipher list should contain ECDHE for forward secrecy
    std::string_view ciphers = default_tls_cipher_list;
    EXPECT_NE(ciphers.find("ECDHE"), std::string_view::npos);
    // Should not contain insecure ciphers
    EXPECT_EQ(ciphers.find("RC4"), std::string_view::npos);
    EXPECT_EQ(ciphers.find("DES"), std::string_view::npos);
    EXPECT_EQ(ciphers.find("MD5"), std::string_view::npos);
    EXPECT_EQ(ciphers.find("NULL"), std::string_view::npos);
}

// ============================================================================
// Handshake Timeout Edge Cases
// ============================================================================

TEST(TLSConfigTest, ZeroHandshakeTimeout) {
    tls_config config;
    config.handshake_timeout_ms = 0;
    // Zero timeout is allowed at the struct level
    EXPECT_EQ(config.handshake_timeout_ms, 0);
}

TEST(TLSConfigTest, VeryLargeHandshakeTimeout) {
    tls_config config;
    config.handshake_timeout_ms = std::numeric_limits<std::size_t>::max();
    EXPECT_EQ(config.handshake_timeout_ms, std::numeric_limits<std::size_t>::max());
}

// ============================================================================
// Copy and Move Semantics
// ============================================================================

TEST(TLSConfigTest, CopyConstruction) {
    auto original = tls_config::secure_defaults();
    original.ca_file = "/path/to/ca.crt";
    original.sni_hostname = "example.com";
    original.cipher_list = "ECDHE-RSA-AES256-GCM-SHA384";

    tls_config copy = original;

    EXPECT_EQ(copy.enabled, original.enabled);
    EXPECT_EQ(copy.min_version, original.min_version);
    EXPECT_EQ(copy.verify_mode, original.verify_mode);
    EXPECT_EQ(copy.ca_file, original.ca_file);
    EXPECT_EQ(copy.sni_hostname, original.sni_hostname);
    EXPECT_EQ(copy.cipher_list, original.cipher_list);
    EXPECT_TRUE(copy.is_valid());
}

TEST(TLSConfigTest, MoveConstruction) {
    auto original = tls_config::secure_defaults();
    original.ca_file = "/path/to/ca.crt";
    original.sni_hostname = "example.com";

    tls_config moved = std::move(original);

    EXPECT_TRUE(moved.enabled);
    EXPECT_TRUE(moved.ca_file.has_value());
    EXPECT_EQ(moved.sni_hostname.value(), "example.com");
    EXPECT_TRUE(moved.is_valid());
}

// ============================================================================
// Factory Method Consistency Tests
// ============================================================================

TEST(TLSConfigTest, FactoryMethodsAllReturnEnabled) {
    auto secure = tls_config::secure_defaults();
    auto legacy = tls_config::legacy_compatible();
    auto insecure = tls_config::insecure_for_testing();

    // All factory methods produce enabled configs
    EXPECT_TRUE(secure.enabled);
    EXPECT_TRUE(legacy.enabled);
    EXPECT_TRUE(insecure.enabled);
}

TEST(TLSConfigTest, SecureDefaultsStricterThanLegacy) {
    auto secure = tls_config::secure_defaults();
    auto legacy = tls_config::legacy_compatible();

    // secure_defaults should require higher TLS version
    EXPECT_GT(secure.min_version, legacy.min_version);

    // Both should verify peers
    EXPECT_EQ(secure.verify_mode, certificate_verification::verify_peer);
    EXPECT_EQ(legacy.verify_mode, certificate_verification::verify_peer);
}

TEST(TLSConfigTest, InsecureForTestingIsLeastSecure) {
    auto insecure = tls_config::insecure_for_testing();

    EXPECT_EQ(insecure.verify_mode, certificate_verification::none);
    // Should be valid without any certificates
    EXPECT_TRUE(insecure.is_valid());
}

// ============================================================================
// TLS Context Error Tests (OpenSSL SSL_CTX)
// ============================================================================

#include "../helpers/dtls_test_helpers.h"
#include <openssl/ssl.h>
#include <openssl/err.h>

using namespace kcenon::network::testing;

TEST(TLSContextErrorTest, SelfSignedCertWithVerifyPeerFails) {
    // A self-signed certificate cannot pass peer verification
    // when the client doesn't trust the CA
    auto cert = test_certificate_generator::generate("localhost");

    // Create server context with self-signed cert
    SSL_CTX* server_ctx = SSL_CTX_new(TLS_server_method());
    ASSERT_NE(server_ctx, nullptr);

    BIO* cert_bio = BIO_new_mem_buf(cert.certificate_pem.data(),
                                     static_cast<int>(cert.certificate_pem.size()));
    X509* x509 = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
    BIO_free(cert_bio);
    ASSERT_NE(x509, nullptr);
    EXPECT_EQ(SSL_CTX_use_certificate(server_ctx, x509), 1);

    BIO* key_bio = BIO_new_mem_buf(cert.private_key_pem.data(),
                                    static_cast<int>(cert.private_key_pem.size()));
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
    BIO_free(key_bio);
    ASSERT_NE(pkey, nullptr);
    EXPECT_EQ(SSL_CTX_use_PrivateKey(server_ctx, pkey), 1);
    EXPECT_EQ(SSL_CTX_check_private_key(server_ctx), 1);

    // Create client context that verifies peer (without trusting the CA)
    SSL_CTX* client_ctx = SSL_CTX_new(TLS_client_method());
    ASSERT_NE(client_ctx, nullptr);
    SSL_CTX_set_verify(client_ctx, SSL_VERIFY_PEER, nullptr);
    // Deliberately NOT loading the self-signed cert as a trusted CA

    // Create memory BIO pair to simulate handshake without real sockets
    BIO* server_rbio = BIO_new(BIO_s_mem());
    BIO* server_wbio = BIO_new(BIO_s_mem());
    BIO* client_rbio = BIO_new(BIO_s_mem());
    BIO* client_wbio = BIO_new(BIO_s_mem());

    SSL* server_ssl = SSL_new(server_ctx);
    SSL_set_accept_state(server_ssl);
    SSL_set_bio(server_ssl, server_rbio, server_wbio);

    SSL* client_ssl = SSL_new(client_ctx);
    SSL_set_connect_state(client_ssl);
    SSL_set_bio(client_ssl, client_rbio, client_wbio);

    // Attempt handshake by pumping data between BIO pairs
    bool handshake_failed = false;
    for (int i = 0; i < 20; ++i) {
        int client_ret = SSL_do_handshake(client_ssl);
        if (client_ret == 1) break;

        int client_err = SSL_get_error(client_ssl, client_ret);
        if (client_err != SSL_ERROR_WANT_READ && client_err != SSL_ERROR_WANT_WRITE) {
            handshake_failed = true;
            break;
        }

        // Pump client_wbio -> server_rbio
        char buf[4096];
        int n = BIO_read(client_wbio, buf, sizeof(buf));
        if (n > 0) BIO_write(server_rbio, buf, n);

        int server_ret = SSL_do_handshake(server_ssl);
        if (server_ret == 1) break;

        int server_err = SSL_get_error(server_ssl, server_ret);
        if (server_err != SSL_ERROR_WANT_READ && server_err != SSL_ERROR_WANT_WRITE) {
            handshake_failed = true;
            break;
        }

        // Pump server_wbio -> client_rbio
        n = BIO_read(server_wbio, buf, sizeof(buf));
        if (n > 0) BIO_write(client_rbio, buf, n);
    }

    // The handshake should fail because client doesn't trust the self-signed cert
    EXPECT_TRUE(handshake_failed);

    SSL_free(server_ssl);
    SSL_free(client_ssl);
    X509_free(x509);
    EVP_PKEY_free(pkey);
    SSL_CTX_free(server_ctx);
    SSL_CTX_free(client_ctx);
}

TEST(TLSContextErrorTest, InvalidCipherSuiteRejectedByOpenSSL) {
    // OpenSSL should reject completely invalid cipher strings
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    ASSERT_NE(ctx, nullptr);

    int result = SSL_CTX_set_cipher_list(ctx, "COMPLETELY_INVALID_CIPHER");
    EXPECT_EQ(result, 0);

    SSL_CTX_free(ctx);
}

TEST(TLSContextErrorTest, ValidCipherSuiteAcceptedByOpenSSL) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    ASSERT_NE(ctx, nullptr);

    int result = SSL_CTX_set_cipher_list(ctx, "ECDHE-RSA-AES256-GCM-SHA384");
    EXPECT_EQ(result, 1);

    SSL_CTX_free(ctx);
}

TEST(TLSContextErrorTest, CertificateGeneratorProducesValidPair) {
    auto cert = test_certificate_generator::generate("test.example.com");

    EXPECT_FALSE(cert.certificate_pem.empty());
    EXPECT_FALSE(cert.private_key_pem.empty());

    // Verify the certificate can be loaded into an SSL context
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    ASSERT_NE(ctx, nullptr);

    BIO* cert_bio = BIO_new_mem_buf(cert.certificate_pem.data(),
                                     static_cast<int>(cert.certificate_pem.size()));
    X509* x509 = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
    BIO_free(cert_bio);
    ASSERT_NE(x509, nullptr);

    EXPECT_EQ(SSL_CTX_use_certificate(ctx, x509), 1);

    BIO* key_bio = BIO_new_mem_buf(cert.private_key_pem.data(),
                                    static_cast<int>(cert.private_key_pem.size()));
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
    BIO_free(key_bio);
    ASSERT_NE(pkey, nullptr);

    EXPECT_EQ(SSL_CTX_use_PrivateKey(ctx, pkey), 1);
    EXPECT_EQ(SSL_CTX_check_private_key(ctx), 1);

    X509_free(x509);
    EVP_PKEY_free(pkey);
    SSL_CTX_free(ctx);
}

TEST(TLSContextErrorTest, MismatchedKeyAndCertificateRejected) {
    // Generate two different certificates — use key from one with cert from another
    auto cert_a = test_certificate_generator::generate("host-a.test");
    auto cert_b = test_certificate_generator::generate("host-b.test");

    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    ASSERT_NE(ctx, nullptr);

    // Load certificate from cert_a
    BIO* cert_bio = BIO_new_mem_buf(cert_a.certificate_pem.data(),
                                     static_cast<int>(cert_a.certificate_pem.size()));
    X509* x509 = PEM_read_bio_X509(cert_bio, nullptr, nullptr, nullptr);
    BIO_free(cert_bio);
    ASSERT_NE(x509, nullptr);
    EXPECT_EQ(SSL_CTX_use_certificate(ctx, x509), 1);

    // Load private key from cert_b (mismatched!)
    BIO* key_bio = BIO_new_mem_buf(cert_b.private_key_pem.data(),
                                    static_cast<int>(cert_b.private_key_pem.size()));
    EVP_PKEY* pkey = PEM_read_bio_PrivateKey(key_bio, nullptr, nullptr, nullptr);
    BIO_free(key_bio);
    ASSERT_NE(pkey, nullptr);

    // OpenSSL 3.x may reject the mismatched key at use_PrivateKey() time
    // or at check_private_key() time — either way, the mismatch is detected
    int use_result = SSL_CTX_use_PrivateKey(ctx, pkey);
    if (use_result == 1) {
        // Key was loaded; mismatch detected at check_private_key
        EXPECT_NE(SSL_CTX_check_private_key(ctx), 1);
    } else {
        // Key loading itself failed due to mismatch (OpenSSL 3.x behavior)
        EXPECT_EQ(use_result, 0);
    }

    X509_free(x509);
    EVP_PKEY_free(pkey);
    SSL_CTX_free(ctx);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
