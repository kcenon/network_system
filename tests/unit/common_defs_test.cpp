/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/utils/common_defs.h"
#include <gtest/gtest.h>

#include <thread>
#include <type_traits>

namespace internal = kcenon::network::internal;

/**
 * @file common_defs_test.cpp
 * @brief Unit tests for common_defs.h configuration structs and enums
 *
 * Tests validate:
 * - socket_config default values and water mark semantics
 * - socket_metrics atomic counter operations and reset()
 * - data_mode enum values
 * - tls_version enum values
 * - certificate_verification enum values
 * - tls_config::is_valid() validation logic
 * - tls_config factory methods (insecure_for_testing, secure_defaults, legacy_compatible)
 * - Inline constants (buffer size, timeout, IDs, cipher list)
 */

// ============================================================================
// socket_config Tests
// ============================================================================

class SocketConfigTest : public ::testing::Test
{
};

TEST_F(SocketConfigTest, DefaultValues)
{
	internal::socket_config config;
	EXPECT_EQ(config.max_pending_bytes, 0u);
	EXPECT_EQ(config.high_water_mark, 1024u * 1024u);
	EXPECT_EQ(config.low_water_mark, 256u * 1024u);
}

TEST_F(SocketConfigTest, HighWaterMarkGreaterThanLowWaterMark)
{
	internal::socket_config config;
	EXPECT_GT(config.high_water_mark, config.low_water_mark);
}

TEST_F(SocketConfigTest, CustomValues)
{
	internal::socket_config config;
	config.max_pending_bytes = 4096;
	config.high_water_mark = 2048;
	config.low_water_mark = 512;

	EXPECT_EQ(config.max_pending_bytes, 4096u);
	EXPECT_EQ(config.high_water_mark, 2048u);
	EXPECT_EQ(config.low_water_mark, 512u);
}

TEST_F(SocketConfigTest, ZeroMaxPendingBytesDisablesBackpressure)
{
	internal::socket_config config;
	EXPECT_EQ(config.max_pending_bytes, 0u);
}

// ============================================================================
// socket_metrics Tests
// ============================================================================

class SocketMetricsTest : public ::testing::Test
{
};

TEST_F(SocketMetricsTest, DefaultValuesAreZero)
{
	internal::socket_metrics metrics;
	EXPECT_EQ(metrics.total_bytes_sent.load(), 0u);
	EXPECT_EQ(metrics.total_bytes_received.load(), 0u);
	EXPECT_EQ(metrics.current_pending_bytes.load(), 0u);
	EXPECT_EQ(metrics.peak_pending_bytes.load(), 0u);
	EXPECT_EQ(metrics.backpressure_events.load(), 0u);
	EXPECT_EQ(metrics.rejected_sends.load(), 0u);
	EXPECT_EQ(metrics.send_count.load(), 0u);
	EXPECT_EQ(metrics.receive_count.load(), 0u);
}

TEST_F(SocketMetricsTest, AtomicStoreAndLoad)
{
	internal::socket_metrics metrics;
	metrics.total_bytes_sent.store(1000);
	metrics.total_bytes_received.store(2000);
	metrics.send_count.store(10);
	metrics.receive_count.store(20);

	EXPECT_EQ(metrics.total_bytes_sent.load(), 1000u);
	EXPECT_EQ(metrics.total_bytes_received.load(), 2000u);
	EXPECT_EQ(metrics.send_count.load(), 10u);
	EXPECT_EQ(metrics.receive_count.load(), 20u);
}

TEST_F(SocketMetricsTest, AtomicIncrementWithFetchAdd)
{
	internal::socket_metrics metrics;
	metrics.send_count.fetch_add(1);
	metrics.send_count.fetch_add(1);
	metrics.send_count.fetch_add(1);

	EXPECT_EQ(metrics.send_count.load(), 3u);
}

TEST_F(SocketMetricsTest, ResetClearsAllCounters)
{
	internal::socket_metrics metrics;
	metrics.total_bytes_sent.store(500);
	metrics.total_bytes_received.store(600);
	metrics.current_pending_bytes.store(100);
	metrics.peak_pending_bytes.store(200);
	metrics.backpressure_events.store(3);
	metrics.rejected_sends.store(2);
	metrics.send_count.store(50);
	metrics.receive_count.store(40);

	metrics.reset();

	EXPECT_EQ(metrics.total_bytes_sent.load(), 0u);
	EXPECT_EQ(metrics.total_bytes_received.load(), 0u);
	EXPECT_EQ(metrics.current_pending_bytes.load(), 0u);
	EXPECT_EQ(metrics.peak_pending_bytes.load(), 0u);
	EXPECT_EQ(metrics.backpressure_events.load(), 0u);
	EXPECT_EQ(metrics.rejected_sends.load(), 0u);
	EXPECT_EQ(metrics.send_count.load(), 0u);
	EXPECT_EQ(metrics.receive_count.load(), 0u);
}

TEST_F(SocketMetricsTest, PeakTrackingPattern)
{
	internal::socket_metrics metrics;

	auto update_pending = [&](std::size_t bytes)
	{
		metrics.current_pending_bytes.store(bytes);
		auto current_peak = metrics.peak_pending_bytes.load();
		while (bytes > current_peak)
		{
			if (metrics.peak_pending_bytes.compare_exchange_weak(current_peak, bytes))
			{
				break;
			}
		}
	};

	update_pending(100);
	update_pending(500);
	update_pending(300);

	EXPECT_EQ(metrics.current_pending_bytes.load(), 300u);
	EXPECT_EQ(metrics.peak_pending_bytes.load(), 500u);
}

// ============================================================================
// data_mode Enum Tests
// ============================================================================

class DataModeTest : public ::testing::Test
{
};

TEST_F(DataModeTest, EnumValues)
{
	EXPECT_EQ(static_cast<uint8_t>(internal::data_mode::packet_mode), 1);
	EXPECT_EQ(static_cast<uint8_t>(internal::data_mode::file_mode), 2);
	EXPECT_EQ(static_cast<uint8_t>(internal::data_mode::binary_mode), 3);
}

TEST_F(DataModeTest, EnumValuesAreDistinct)
{
	EXPECT_NE(internal::data_mode::packet_mode, internal::data_mode::file_mode);
	EXPECT_NE(internal::data_mode::file_mode, internal::data_mode::binary_mode);
	EXPECT_NE(internal::data_mode::packet_mode, internal::data_mode::binary_mode);
}

// ============================================================================
// tls_version Enum Tests
// ============================================================================

class TlsVersionTest : public ::testing::Test
{
};

TEST_F(TlsVersionTest, EnumValues)
{
	EXPECT_EQ(static_cast<uint8_t>(internal::tls_version::tls_1_0), 10);
	EXPECT_EQ(static_cast<uint8_t>(internal::tls_version::tls_1_1), 11);
	EXPECT_EQ(static_cast<uint8_t>(internal::tls_version::tls_1_2), 12);
	EXPECT_EQ(static_cast<uint8_t>(internal::tls_version::tls_1_3), 13);
}

TEST_F(TlsVersionTest, VersionOrdering)
{
	EXPECT_LT(static_cast<uint8_t>(internal::tls_version::tls_1_0),
			   static_cast<uint8_t>(internal::tls_version::tls_1_1));
	EXPECT_LT(static_cast<uint8_t>(internal::tls_version::tls_1_1),
			   static_cast<uint8_t>(internal::tls_version::tls_1_2));
	EXPECT_LT(static_cast<uint8_t>(internal::tls_version::tls_1_2),
			   static_cast<uint8_t>(internal::tls_version::tls_1_3));
}

// ============================================================================
// certificate_verification Enum Tests
// ============================================================================

class CertificateVerificationTest : public ::testing::Test
{
};

TEST_F(CertificateVerificationTest, EnumValues)
{
	EXPECT_EQ(static_cast<uint8_t>(internal::certificate_verification::none), 0);
	EXPECT_EQ(static_cast<uint8_t>(internal::certificate_verification::verify_peer), 1);
	EXPECT_EQ(static_cast<uint8_t>(internal::certificate_verification::verify_fail_if_no_peer_cert),
			   2);
}

// ============================================================================
// tls_config Tests
// ============================================================================

class TlsConfigTest : public ::testing::Test
{
};

TEST_F(TlsConfigTest, DefaultValues)
{
	internal::tls_config config;
	EXPECT_FALSE(config.enabled);
	EXPECT_EQ(config.min_version, internal::tls_version::tls_1_3);
	EXPECT_EQ(config.verify_mode, internal::certificate_verification::verify_peer);
	EXPECT_FALSE(config.certificate_file.has_value());
	EXPECT_FALSE(config.private_key_file.has_value());
	EXPECT_FALSE(config.private_key_password.has_value());
	EXPECT_FALSE(config.ca_file.has_value());
	EXPECT_FALSE(config.ca_path.has_value());
	EXPECT_FALSE(config.cipher_list.has_value());
	EXPECT_FALSE(config.sni_hostname.has_value());
	EXPECT_TRUE(config.enable_session_resumption);
	EXPECT_EQ(config.handshake_timeout_ms, 10000u);
}

TEST_F(TlsConfigTest, IsValidWhenDisabled)
{
	internal::tls_config config;
	config.enabled = false;
	EXPECT_TRUE(config.is_valid());
}

TEST_F(TlsConfigTest, IsValidWithVerificationNone)
{
	internal::tls_config config;
	config.enabled = true;
	config.verify_mode = internal::certificate_verification::none;
	EXPECT_TRUE(config.is_valid());
}

TEST_F(TlsConfigTest, IsInvalidWithVerificationButNoCa)
{
	internal::tls_config config;
	config.enabled = true;
	config.verify_mode = internal::certificate_verification::verify_peer;
	// No ca_file or ca_path set
	EXPECT_FALSE(config.is_valid());
}

TEST_F(TlsConfigTest, IsValidWithCaFile)
{
	internal::tls_config config;
	config.enabled = true;
	config.verify_mode = internal::certificate_verification::verify_peer;
	config.ca_file = "/path/to/ca.crt";
	EXPECT_TRUE(config.is_valid());
}

TEST_F(TlsConfigTest, IsValidWithCaPath)
{
	internal::tls_config config;
	config.enabled = true;
	config.verify_mode = internal::certificate_verification::verify_peer;
	config.ca_path = "/path/to/ca/dir";
	EXPECT_TRUE(config.is_valid());
}

TEST_F(TlsConfigTest, IsValidWithVerifyFailIfNoPeerCertAndCa)
{
	internal::tls_config config;
	config.enabled = true;
	config.verify_mode = internal::certificate_verification::verify_fail_if_no_peer_cert;
	config.ca_file = "/path/to/ca.crt";
	EXPECT_TRUE(config.is_valid());
}

TEST_F(TlsConfigTest, IsInvalidWithVerifyFailIfNoPeerCertWithoutCa)
{
	internal::tls_config config;
	config.enabled = true;
	config.verify_mode = internal::certificate_verification::verify_fail_if_no_peer_cert;
	EXPECT_FALSE(config.is_valid());
}

TEST_F(TlsConfigTest, InsecureForTesting)
{
	auto config = internal::tls_config::insecure_for_testing();
	EXPECT_TRUE(config.enabled);
	EXPECT_EQ(config.verify_mode, internal::certificate_verification::none);
	EXPECT_TRUE(config.is_valid());
}

TEST_F(TlsConfigTest, SecureDefaults)
{
	auto config = internal::tls_config::secure_defaults();
	EXPECT_TRUE(config.enabled);
	EXPECT_EQ(config.min_version, internal::tls_version::tls_1_3);
	EXPECT_EQ(config.verify_mode, internal::certificate_verification::verify_peer);
	EXPECT_TRUE(config.enable_session_resumption);
}

TEST_F(TlsConfigTest, SecureDefaultsRequiresCaForValidity)
{
	auto config = internal::tls_config::secure_defaults();
	// Without CA file, verification enabled config is invalid
	EXPECT_FALSE(config.is_valid());

	config.ca_file = "/path/to/ca.crt";
	EXPECT_TRUE(config.is_valid());
}

TEST_F(TlsConfigTest, LegacyCompatible)
{
	auto config = internal::tls_config::legacy_compatible();
	EXPECT_TRUE(config.enabled);
	EXPECT_EQ(config.min_version, internal::tls_version::tls_1_2);
	EXPECT_EQ(config.verify_mode, internal::certificate_verification::verify_peer);
	EXPECT_TRUE(config.enable_session_resumption);
}

TEST_F(TlsConfigTest, LegacyCompatibleAllowsTls12)
{
	auto config = internal::tls_config::legacy_compatible();
	EXPECT_EQ(config.min_version, internal::tls_version::tls_1_2);

	auto secure = internal::tls_config::secure_defaults();
	EXPECT_LT(static_cast<uint8_t>(config.min_version),
			   static_cast<uint8_t>(secure.min_version));
}

TEST_F(TlsConfigTest, OptionalFieldsAssignment)
{
	internal::tls_config config;
	config.certificate_file = "/path/to/cert.pem";
	config.private_key_file = "/path/to/key.pem";
	config.private_key_password = "secret";
	config.cipher_list = "ECDHE-RSA-AES256-GCM-SHA384";
	config.sni_hostname = "example.com";

	EXPECT_EQ(config.certificate_file.value(), "/path/to/cert.pem");
	EXPECT_EQ(config.private_key_file.value(), "/path/to/key.pem");
	EXPECT_EQ(config.private_key_password.value(), "secret");
	EXPECT_EQ(config.cipher_list.value(), "ECDHE-RSA-AES256-GCM-SHA384");
	EXPECT_EQ(config.sni_hostname.value(), "example.com");
}

// ============================================================================
// Inline Constants Tests
// ============================================================================

class CommonDefsConstantsTest : public ::testing::Test
{
};

TEST_F(CommonDefsConstantsTest, DefaultBufferSize)
{
	EXPECT_EQ(internal::default_buffer_size, 4096u);
}

TEST_F(CommonDefsConstantsTest, DefaultTimeoutMs)
{
	EXPECT_EQ(internal::default_timeout_ms, 5000u);
}

TEST_F(CommonDefsConstantsTest, DefaultClientId)
{
	EXPECT_EQ(internal::default_client_id, "default_client");
}

TEST_F(CommonDefsConstantsTest, DefaultServerId)
{
	EXPECT_EQ(internal::default_server_id, "default_server");
}

TEST_F(CommonDefsConstantsTest, DefaultTlsCipherList)
{
	auto cipher_list = internal::default_tls_cipher_list;
	EXPECT_FALSE(cipher_list.empty());
	// Verify it contains expected strong cipher suites
	EXPECT_NE(cipher_list.find("ECDHE-RSA-AES256-GCM-SHA384"), std::string_view::npos);
	EXPECT_NE(cipher_list.find("ECDHE-RSA-AES128-GCM-SHA256"), std::string_view::npos);
	EXPECT_NE(cipher_list.find("ECDHE-RSA-CHACHA20-POLY1305"), std::string_view::npos);
}

TEST_F(CommonDefsConstantsTest, ConstantsAreConstexpr)
{
	static_assert(internal::default_buffer_size == 4096);
	static_assert(internal::default_timeout_ms == 5000);
}
