/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/transport_params.h"
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_transport_params_test.cpp
 * @brief Unit tests for QUIC transport parameters (RFC 9000 Section 18)
 *
 * Tests validate:
 * - transport_param_id constant values (RFC 9000 §18.2)
 * - transport_param_error constant values
 * - preferred_address_info equality operator
 * - transport_parameters default member values
 * - encode() / decode() round-trip for various parameter combinations
 * - validate() for server vs client parameter restrictions
 * - apply_defaults() zero-value override behavior
 * - make_default_client_params() factory
 * - make_default_server_params() factory
 * - Decode error handling (truncated, duplicate, invalid values)
 */

// ============================================================================
// Transport Parameter ID Constants Tests
// ============================================================================

class TransportParamIdTest : public ::testing::Test
{
};

TEST_F(TransportParamIdTest, ConstantValues)
{
	EXPECT_EQ(quic::transport_param_id::original_destination_connection_id, 0x00u);
	EXPECT_EQ(quic::transport_param_id::max_idle_timeout, 0x01u);
	EXPECT_EQ(quic::transport_param_id::stateless_reset_token, 0x02u);
	EXPECT_EQ(quic::transport_param_id::max_udp_payload_size, 0x03u);
	EXPECT_EQ(quic::transport_param_id::initial_max_data, 0x04u);
	EXPECT_EQ(quic::transport_param_id::initial_max_stream_data_bidi_local, 0x05u);
	EXPECT_EQ(quic::transport_param_id::initial_max_stream_data_bidi_remote, 0x06u);
	EXPECT_EQ(quic::transport_param_id::initial_max_stream_data_uni, 0x07u);
	EXPECT_EQ(quic::transport_param_id::initial_max_streams_bidi, 0x08u);
	EXPECT_EQ(quic::transport_param_id::initial_max_streams_uni, 0x09u);
	EXPECT_EQ(quic::transport_param_id::ack_delay_exponent, 0x0au);
	EXPECT_EQ(quic::transport_param_id::max_ack_delay, 0x0bu);
	EXPECT_EQ(quic::transport_param_id::disable_active_migration, 0x0cu);
	EXPECT_EQ(quic::transport_param_id::preferred_address, 0x0du);
	EXPECT_EQ(quic::transport_param_id::active_connection_id_limit, 0x0eu);
	EXPECT_EQ(quic::transport_param_id::initial_source_connection_id, 0x0fu);
	EXPECT_EQ(quic::transport_param_id::retry_source_connection_id, 0x10u);
}

// ============================================================================
// Transport Parameter Error Constants Tests
// ============================================================================

class TransportParamErrorTest : public ::testing::Test
{
};

TEST_F(TransportParamErrorTest, ErrorCodeValues)
{
	EXPECT_EQ(quic::transport_param_error::invalid_parameter, -720);
	EXPECT_EQ(quic::transport_param_error::decode_error, -721);
	EXPECT_EQ(quic::transport_param_error::duplicate_parameter, -722);
	EXPECT_EQ(quic::transport_param_error::missing_required_parameter, -723);
	EXPECT_EQ(quic::transport_param_error::invalid_value, -724);
}

TEST_F(TransportParamErrorTest, ErrorCodesAreDistinct)
{
	EXPECT_NE(quic::transport_param_error::invalid_parameter,
			  quic::transport_param_error::decode_error);
	EXPECT_NE(quic::transport_param_error::decode_error,
			  quic::transport_param_error::duplicate_parameter);
	EXPECT_NE(quic::transport_param_error::duplicate_parameter,
			  quic::transport_param_error::missing_required_parameter);
	EXPECT_NE(quic::transport_param_error::missing_required_parameter,
			  quic::transport_param_error::invalid_value);
}

// ============================================================================
// preferred_address_info Tests
// ============================================================================

class PreferredAddressInfoTest : public ::testing::Test
{
};

TEST_F(PreferredAddressInfoTest, DefaultConstruction)
{
	quic::preferred_address_info addr;
	EXPECT_EQ(addr.ipv4_port, 0);
	EXPECT_EQ(addr.ipv6_port, 0);
	EXPECT_TRUE(addr.connection_id.empty());
}

TEST_F(PreferredAddressInfoTest, EqualityWithSameValues)
{
	quic::preferred_address_info a;
	a.ipv4_address = {192, 168, 1, 1};
	a.ipv4_port = 443;
	a.ipv6_port = 8443;

	quic::preferred_address_info b = a;
	EXPECT_EQ(a, b);
}

TEST_F(PreferredAddressInfoTest, InequalityWithDifferentPort)
{
	quic::preferred_address_info a;
	a.ipv4_port = 443;

	quic::preferred_address_info b;
	b.ipv4_port = 8443;

	EXPECT_NE(a, b);
}

// ============================================================================
// transport_parameters Default Values Tests
// ============================================================================

class TransportParametersDefaultTest : public ::testing::Test
{
};

TEST_F(TransportParametersDefaultTest, DefaultMemberValues)
{
	quic::transport_parameters params;

	EXPECT_FALSE(params.original_destination_connection_id.has_value());
	EXPECT_FALSE(params.initial_source_connection_id.has_value());
	EXPECT_FALSE(params.retry_source_connection_id.has_value());
	EXPECT_FALSE(params.stateless_reset_token.has_value());

	EXPECT_EQ(params.max_idle_timeout, 0u);
	EXPECT_EQ(params.ack_delay_exponent, 3u);
	EXPECT_EQ(params.max_ack_delay, 25u);

	EXPECT_EQ(params.max_udp_payload_size, 65527u);
	EXPECT_EQ(params.initial_max_data, 0u);
	EXPECT_EQ(params.initial_max_stream_data_bidi_local, 0u);
	EXPECT_EQ(params.initial_max_stream_data_bidi_remote, 0u);
	EXPECT_EQ(params.initial_max_stream_data_uni, 0u);

	EXPECT_EQ(params.initial_max_streams_bidi, 0u);
	EXPECT_EQ(params.initial_max_streams_uni, 0u);

	EXPECT_FALSE(params.disable_active_migration);
	EXPECT_EQ(params.active_connection_id_limit, 2u);
	EXPECT_FALSE(params.preferred_address.has_value());
}

TEST_F(TransportParametersDefaultTest, DefaultEquality)
{
	quic::transport_parameters a;
	quic::transport_parameters b;
	EXPECT_EQ(a, b);
}

// ============================================================================
// Encode/Decode Round-Trip Tests
// ============================================================================

class TransportParamsEncodeDecodeTest : public ::testing::Test
{
};

TEST_F(TransportParamsEncodeDecodeTest, DefaultParametersRoundTrip)
{
	quic::transport_parameters original;
	auto encoded = original.encode();

	// Default parameters with all-default values encode to empty
	// since encode() skips default-valued fields
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));
	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(original, decoded.value());
}

TEST_F(TransportParamsEncodeDecodeTest, TimingParametersRoundTrip)
{
	quic::transport_parameters original;
	original.max_idle_timeout = 30000;
	original.ack_delay_exponent = 5;
	original.max_ack_delay = 100;

	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(decoded.value().max_idle_timeout, 30000u);
	EXPECT_EQ(decoded.value().ack_delay_exponent, 5u);
	EXPECT_EQ(decoded.value().max_ack_delay, 100u);
}

TEST_F(TransportParamsEncodeDecodeTest, FlowControlParametersRoundTrip)
{
	quic::transport_parameters original;
	original.max_udp_payload_size = 1400;
	original.initial_max_data = 1048576;
	original.initial_max_stream_data_bidi_local = 262144;
	original.initial_max_stream_data_bidi_remote = 131072;
	original.initial_max_stream_data_uni = 65536;

	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(decoded.value().max_udp_payload_size, 1400u);
	EXPECT_EQ(decoded.value().initial_max_data, 1048576u);
	EXPECT_EQ(decoded.value().initial_max_stream_data_bidi_local, 262144u);
	EXPECT_EQ(decoded.value().initial_max_stream_data_bidi_remote, 131072u);
	EXPECT_EQ(decoded.value().initial_max_stream_data_uni, 65536u);
}

TEST_F(TransportParamsEncodeDecodeTest, StreamLimitsRoundTrip)
{
	quic::transport_parameters original;
	original.initial_max_streams_bidi = 100;
	original.initial_max_streams_uni = 50;

	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(decoded.value().initial_max_streams_bidi, 100u);
	EXPECT_EQ(decoded.value().initial_max_streams_uni, 50u);
}

TEST_F(TransportParamsEncodeDecodeTest, DisableActiveMigrationRoundTrip)
{
	quic::transport_parameters original;
	original.disable_active_migration = true;

	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	EXPECT_TRUE(decoded.value().disable_active_migration);
}

TEST_F(TransportParamsEncodeDecodeTest, ActiveConnectionIdLimitRoundTrip)
{
	quic::transport_parameters original;
	original.active_connection_id_limit = 8;

	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(decoded.value().active_connection_id_limit, 8u);
}

TEST_F(TransportParamsEncodeDecodeTest, ConnectionIdParametersRoundTrip)
{
	quic::transport_parameters original;

	std::array<uint8_t, 8> cid_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	original.initial_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_data.data(), cid_data.size()));

	std::array<uint8_t, 4> odcid_data = {0xAA, 0xBB, 0xCC, 0xDD};
	original.original_destination_connection_id =
		quic::connection_id(std::span<const uint8_t>(odcid_data.data(), odcid_data.size()));

	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	ASSERT_TRUE(decoded.value().initial_source_connection_id.has_value());
	EXPECT_EQ(decoded.value().initial_source_connection_id->length(), 8u);
	ASSERT_TRUE(decoded.value().original_destination_connection_id.has_value());
	EXPECT_EQ(decoded.value().original_destination_connection_id->length(), 4u);
}

TEST_F(TransportParamsEncodeDecodeTest, StatelessResetTokenRoundTrip)
{
	quic::transport_parameters original;
	std::array<uint8_t, 16> token = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
	original.stateless_reset_token = token;

	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	ASSERT_TRUE(decoded.value().stateless_reset_token.has_value());
	EXPECT_EQ(decoded.value().stateless_reset_token.value(), token);
}

TEST_F(TransportParamsEncodeDecodeTest, AllParametersRoundTrip)
{
	quic::transport_parameters original;
	original.max_idle_timeout = 30000;
	original.ack_delay_exponent = 5;
	original.max_ack_delay = 100;
	original.max_udp_payload_size = 1400;
	original.initial_max_data = 1048576;
	original.initial_max_stream_data_bidi_local = 262144;
	original.initial_max_stream_data_bidi_remote = 131072;
	original.initial_max_stream_data_uni = 65536;
	original.initial_max_streams_bidi = 100;
	original.initial_max_streams_uni = 50;
	original.disable_active_migration = true;
	original.active_connection_id_limit = 8;

	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(original, decoded.value());
}

TEST_F(TransportParamsEncodeDecodeTest, PreferredAddressRoundTrip)
{
	quic::transport_parameters original;

	quic::preferred_address_info addr;
	addr.ipv4_address = {192, 168, 1, 100};
	addr.ipv4_port = 443;
	addr.ipv6_address = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
	addr.ipv6_port = 8443;

	std::array<uint8_t, 4> cid_data = {0xAA, 0xBB, 0xCC, 0xDD};
	addr.connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_data.data(), cid_data.size()));

	addr.stateless_reset_token = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
	original.preferred_address = addr;

	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	ASSERT_TRUE(decoded.value().preferred_address.has_value());

	auto& decoded_addr = decoded.value().preferred_address.value();
	EXPECT_EQ(decoded_addr.ipv4_address, addr.ipv4_address);
	EXPECT_EQ(decoded_addr.ipv4_port, 443);
	EXPECT_EQ(decoded_addr.ipv6_address, addr.ipv6_address);
	EXPECT_EQ(decoded_addr.ipv6_port, 8443);
	EXPECT_EQ(decoded_addr.stateless_reset_token, addr.stateless_reset_token);
}

TEST_F(TransportParamsEncodeDecodeTest, EmptyBufferDecodes)
{
	std::vector<uint8_t> empty;
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(empty.data(), empty.size()));

	ASSERT_TRUE(decoded.is_ok());
	quic::transport_parameters defaults;
	EXPECT_EQ(decoded.value(), defaults);
}

// ============================================================================
// Decode Error Tests
// ============================================================================

class TransportParamsDecodeErrorTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeErrorTest, TruncatedParameterLength)
{
	// Parameter ID only, no length field
	std::vector<uint8_t> data = {0x01};
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, ParameterLengthExceedsBuffer)
{
	// Parameter ID=0x01, length=0xFF but no data follows
	std::vector<uint8_t> data = {0x01, 0x40, 0xFF};
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, DuplicateParameter)
{
	// Encode max_idle_timeout twice
	quic::transport_parameters params;
	params.max_idle_timeout = 1000;
	auto encoded = params.encode();

	// Append the same parameter again
	std::vector<uint8_t> doubled;
	doubled.insert(doubled.end(), encoded.begin(), encoded.end());
	doubled.insert(doubled.end(), encoded.begin(), encoded.end());

	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(doubled.data(), doubled.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, InvalidStatelessResetTokenLength)
{
	// stateless_reset_token must be exactly 16 bytes
	// Encode param_id=0x02, length=8, then 8 bytes
	std::vector<uint8_t> data = {0x02, 0x08, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, AckDelayExponentExceedsMax)
{
	// ack_delay_exponent > 20 should fail
	// param_id=0x0a, length=1, value=21
	std::vector<uint8_t> data = {0x0a, 0x01, 0x15};
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, MaxAckDelayExceedsMax)
{
	// max_ack_delay > 16383 should fail
	// param_id=0x0b, length=2, value=16384 (varint: 0x80, 0x00 for 2^14)
	// varint encoding of 16384: 0x80 0x40 0x00 (2-byte encoding)
	std::vector<uint8_t> data = {0x0b, 0x04, 0x80, 0x00, 0x40, 0x00};
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, MaxUdpPayloadSizeBelowMinimum)
{
	// max_udp_payload_size < 1200 should fail
	// param_id=0x03, length=2, value=1199 (varint 2-byte: 0x44, 0xAF)
	std::vector<uint8_t> data = {0x03, 0x02, 0x44, 0xAF};
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, ActiveConnectionIdLimitBelowMinimum)
{
	// active_connection_id_limit < 2 should fail
	// param_id=0x0e, length=1, value=1
	std::vector<uint8_t> data = {0x0e, 0x01, 0x01};
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, DisableActiveMigrationNonZeroLength)
{
	// disable_active_migration must have zero-length value
	// param_id=0x0c, length=1, value=0x01
	std::vector<uint8_t> data = {0x0c, 0x01, 0x01};
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, ConnectionIdTooLong)
{
	// original_destination_connection_id > 20 bytes should fail
	// param_id=0x00, length=21, then 21 bytes
	std::vector<uint8_t> data = {0x00, 0x15};
	for (int i = 0; i < 21; ++i)
	{
		data.push_back(static_cast<uint8_t>(i));
	}
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsDecodeErrorTest, UnknownParameterIsIgnored)
{
	// Unknown param IDs should be silently ignored per RFC 9000 §18.1
	// Use a large param_id to ensure it's unknown
	// param_id=0xFF (varint: 0x40, 0xFF), length=2, value=0xAB, 0xCD
	std::vector<uint8_t> data = {0x40, 0xFF, 0x02, 0xAB, 0xCD};
	auto result = quic::transport_parameters::decode(
		std::span<const uint8_t>(data.data(), data.size()));
	ASSERT_TRUE(result.is_ok());
}

// ============================================================================
// Validate Tests
// ============================================================================

class TransportParamsValidateTest : public ::testing::Test
{
};

TEST_F(TransportParamsValidateTest, DefaultParametersValidForBoth)
{
	quic::transport_parameters params;

	auto server_result = params.validate(true);
	EXPECT_TRUE(server_result.is_ok());

	auto client_result = params.validate(false);
	EXPECT_TRUE(client_result.is_ok());
}

TEST_F(TransportParamsValidateTest, AckDelayExponentExceedsMaxInvalid)
{
	quic::transport_parameters params;
	params.ack_delay_exponent = 21;

	auto result = params.validate(true);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsValidateTest, MaxAckDelayExceedsMaxInvalid)
{
	quic::transport_parameters params;
	params.max_ack_delay = 16384;

	auto result = params.validate(true);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsValidateTest, MaxUdpPayloadSizeBelowMinInvalid)
{
	quic::transport_parameters params;
	params.max_udp_payload_size = 1199;

	auto result = params.validate(true);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsValidateTest, ActiveConnectionIdLimitBelowMinInvalid)
{
	quic::transport_parameters params;
	params.active_connection_id_limit = 1;

	auto result = params.validate(true);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParamsValidateTest, ClientMustNotSendServerOnlyParams)
{
	// original_destination_connection_id
	{
		quic::transport_parameters params;
		std::array<uint8_t, 4> cid_data = {0x01, 0x02, 0x03, 0x04};
		params.original_destination_connection_id =
			quic::connection_id(std::span<const uint8_t>(cid_data.data(), cid_data.size()));
		auto result = params.validate(false);
		EXPECT_TRUE(result.is_err());
	}

	// retry_source_connection_id
	{
		quic::transport_parameters params;
		std::array<uint8_t, 4> cid_data = {0x01, 0x02, 0x03, 0x04};
		params.retry_source_connection_id =
			quic::connection_id(std::span<const uint8_t>(cid_data.data(), cid_data.size()));
		auto result = params.validate(false);
		EXPECT_TRUE(result.is_err());
	}

	// stateless_reset_token
	{
		quic::transport_parameters params;
		params.stateless_reset_token = std::array<uint8_t, 16>{};
		auto result = params.validate(false);
		EXPECT_TRUE(result.is_err());
	}

	// preferred_address
	{
		quic::transport_parameters params;
		params.preferred_address = quic::preferred_address_info{};
		auto result = params.validate(false);
		EXPECT_TRUE(result.is_err());
	}
}

TEST_F(TransportParamsValidateTest, ServerCanSendServerOnlyParams)
{
	quic::transport_parameters params;
	std::array<uint8_t, 4> cid_data = {0x01, 0x02, 0x03, 0x04};
	params.original_destination_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_data.data(), cid_data.size()));
	params.stateless_reset_token = std::array<uint8_t, 16>{};

	auto result = params.validate(true);
	EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// apply_defaults Tests
// ============================================================================

class TransportParamsApplyDefaultsTest : public ::testing::Test
{
};

TEST_F(TransportParamsApplyDefaultsTest, OverridesZeroValues)
{
	quic::transport_parameters params;
	params.max_udp_payload_size = 0;
	params.ack_delay_exponent = 0;
	params.max_ack_delay = 0;
	params.active_connection_id_limit = 0;

	params.apply_defaults();

	EXPECT_EQ(params.max_udp_payload_size, 65527u);
	EXPECT_EQ(params.ack_delay_exponent, 3u);
	EXPECT_EQ(params.max_ack_delay, 25u);
	EXPECT_EQ(params.active_connection_id_limit, 2u);
}

TEST_F(TransportParamsApplyDefaultsTest, PreservesNonZeroValues)
{
	quic::transport_parameters params;
	params.max_udp_payload_size = 1400;
	params.ack_delay_exponent = 5;
	params.max_ack_delay = 100;
	params.active_connection_id_limit = 8;

	params.apply_defaults();

	EXPECT_EQ(params.max_udp_payload_size, 1400u);
	EXPECT_EQ(params.ack_delay_exponent, 5u);
	EXPECT_EQ(params.max_ack_delay, 100u);
	EXPECT_EQ(params.active_connection_id_limit, 8u);
}

// ============================================================================
// Factory Function Tests
// ============================================================================

class TransportParamsFactoryTest : public ::testing::Test
{
};

TEST_F(TransportParamsFactoryTest, DefaultClientParams)
{
	auto params = quic::make_default_client_params();

	EXPECT_EQ(params.max_idle_timeout, 30000u);
	EXPECT_EQ(params.max_udp_payload_size, 65527u);
	EXPECT_EQ(params.initial_max_data, 1048576u);
	EXPECT_EQ(params.initial_max_stream_data_bidi_local, 262144u);
	EXPECT_EQ(params.initial_max_stream_data_bidi_remote, 262144u);
	EXPECT_EQ(params.initial_max_stream_data_uni, 262144u);
	EXPECT_EQ(params.initial_max_streams_bidi, 100u);
	EXPECT_EQ(params.initial_max_streams_uni, 100u);
	EXPECT_EQ(params.ack_delay_exponent, 3u);
	EXPECT_EQ(params.max_ack_delay, 25u);
	EXPECT_EQ(params.active_connection_id_limit, 8u);
}

TEST_F(TransportParamsFactoryTest, DefaultServerParams)
{
	auto params = quic::make_default_server_params();

	EXPECT_EQ(params.max_idle_timeout, 30000u);
	EXPECT_EQ(params.max_udp_payload_size, 65527u);
	EXPECT_EQ(params.initial_max_data, 1048576u);
	EXPECT_EQ(params.initial_max_stream_data_bidi_local, 262144u);
	EXPECT_EQ(params.initial_max_stream_data_bidi_remote, 262144u);
	EXPECT_EQ(params.initial_max_stream_data_uni, 262144u);
	EXPECT_EQ(params.initial_max_streams_bidi, 100u);
	EXPECT_EQ(params.initial_max_streams_uni, 100u);
	EXPECT_EQ(params.ack_delay_exponent, 3u);
	EXPECT_EQ(params.max_ack_delay, 25u);
	EXPECT_EQ(params.active_connection_id_limit, 8u);
}

TEST_F(TransportParamsFactoryTest, DefaultClientParamsAreValid)
{
	auto params = quic::make_default_client_params();
	auto result = params.validate(false);
	EXPECT_TRUE(result.is_ok());
}

TEST_F(TransportParamsFactoryTest, DefaultServerParamsAreValid)
{
	auto params = quic::make_default_server_params();
	auto result = params.validate(true);
	EXPECT_TRUE(result.is_ok());
}

TEST_F(TransportParamsFactoryTest, DefaultClientParamsEncodeDecodeRoundTrip)
{
	auto original = quic::make_default_client_params();
	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(original, decoded.value());
}

TEST_F(TransportParamsFactoryTest, DefaultServerParamsEncodeDecodeRoundTrip)
{
	auto original = quic::make_default_server_params();
	auto encoded = original.encode();
	auto decoded = quic::transport_parameters::decode(
		std::span<const uint8_t>(encoded.data(), encoded.size()));

	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(original, decoded.value());
}
