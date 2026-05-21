/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

// Branch-coverage gap tests for src/protocols/quic/transport_params.cpp.
//
// The existing quic_transport_params_test.cpp and
// quic_transport_params_coverage_test.cpp suites bring line coverage for this
// file above the issue #1024 target. The remaining uncovered logical branches
// (per the latest coverage.yml run on develop) are concentrated in:
//
//   * encode() server-only retry_source_connection_id emission path
//     (transport_params.cpp:87-89)
//   * decode() per-case malformed-varint error returns for parameters
//     not previously truncation-tested individually:
//       initial_max_stream_data_bidi_remote (line 409)
//       initial_max_stream_data_uni        (line 423)
//       initial_max_streams_uni            (line 451)
//
// These tests target those branches and add a small set of round-trip
// assertions covering the server-only encode emission so the new path is
// exercised end-to-end.
//
// Part of epic #953 (expand unit test coverage from 40% to 80%). Sub-issue
// #1024 follow-up after #1015.

#include "internal/protocols/quic/transport_params.h"
#include "internal/protocols/quic/varint.h"
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

namespace {

auto make_param(uint64_t id, std::span<const uint8_t> value) -> std::vector<uint8_t>
{
	auto id_bytes = quic::varint::encode(id);
	auto len_bytes = quic::varint::encode(value.size());
	std::vector<uint8_t> out;
	out.reserve(id_bytes.size() + len_bytes.size() + value.size());
	out.insert(out.end(), id_bytes.begin(), id_bytes.end());
	out.insert(out.end(), len_bytes.begin(), len_bytes.end());
	out.insert(out.end(), value.begin(), value.end());
	return out;
}

auto as_span(const std::vector<uint8_t>& v) -> std::span<const uint8_t>
{
	return std::span<const uint8_t>(v.data(), v.size());
}

}  // namespace

// ============================================================================
// Per-case malformed-varint coverage for the three decode branches not
// individually exercised by quic_transport_params_coverage_test.cpp.
// ============================================================================

class TransportParamsDecodeMalformedVarintGapTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeMalformedVarintGapTest,
	InitialMaxStreamDataBidiRemoteTruncatedVarint)
{
	// Length says 1 byte; value is the prefix of a 2-byte varint (0x40)
	// without its trailing byte -> inner varint::decode() must fail and the
	// switch arm at line 409 must take its decode_error branch.
	std::vector<uint8_t> truncated{0x40};
	auto buf = make_param(
		quic::transport_param_id::initial_max_stream_data_bidi_remote,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintGapTest,
	InitialMaxStreamDataUniTruncatedVarint)
{
	// 4-byte varint prefix (0x80) with only 3 bytes delivered.
	std::vector<uint8_t> truncated{0x80, 0x00, 0x00};
	auto buf = make_param(
		quic::transport_param_id::initial_max_stream_data_uni,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintGapTest,
	InitialMaxStreamsUniTruncatedVarint)
{
	// 8-byte varint prefix (0xC0) with only 7 bytes of payload.
	std::vector<uint8_t> truncated{0xC0, 0, 0, 0, 0, 0, 0};
	auto buf = make_param(
		quic::transport_param_id::initial_max_streams_uni,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

// ============================================================================
// encode() emission of server-only retry_source_connection_id (line 87).
// The existing tests set this field for validate() rejection paths but never
// invoke encode() with it set, so the optional<>::operator bool true branch
// at line 87 and the inner append_parameter call at line 89 are unexercised.
// ============================================================================

class TransportParamsEncodeRetrySourceTest : public ::testing::Test
{
};

TEST_F(TransportParamsEncodeRetrySourceTest, EmitsParameterWhenPresent)
{
	quic::transport_parameters p;
	std::array<uint8_t, 8> cid_bytes{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
	p.retry_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));

	auto encoded = p.encode();

	// Locate the parameter id byte. retry_source_connection_id == 0x10, fits
	// into a single varint byte.
	bool found = false;
	for (size_t i = 0; i + 1 + cid_bytes.size() < encoded.size(); ++i)
	{
		if (encoded[i] == quic::transport_param_id::retry_source_connection_id)
		{
			// Length prefix is a single byte equal to cid_bytes.size().
			if (encoded[i + 1] == cid_bytes.size())
			{
				found = true;
				for (size_t k = 0; k < cid_bytes.size(); ++k)
				{
					EXPECT_EQ(encoded[i + 2 + k], cid_bytes[k]);
				}
				break;
			}
		}
	}
	EXPECT_TRUE(found);
}

TEST_F(TransportParamsEncodeRetrySourceTest, RoundTripsAsServerParameter)
{
	quic::transport_parameters p;
	std::array<uint8_t, 4> cid_bytes{0xDE, 0xAD, 0xBE, 0xEF};
	p.retry_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));

	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	ASSERT_TRUE(decoded.value().retry_source_connection_id.has_value());
	EXPECT_EQ(decoded.value().retry_source_connection_id->length(), cid_bytes.size());
	auto data = decoded.value().retry_source_connection_id->data();
	for (size_t i = 0; i < cid_bytes.size(); ++i)
	{
		EXPECT_EQ(data[i], cid_bytes[i]);
	}
}

TEST_F(TransportParamsEncodeRetrySourceTest, RetrySourceCidAtMaxLength)
{
	// Max connection-id length is 20 bytes; encoded size is id (1) + len (1)
	// + 20 = 22 bytes for this single parameter.
	quic::transport_parameters p;
	std::array<uint8_t, 20> cid_bytes{};
	for (size_t i = 0; i < cid_bytes.size(); ++i)
	{
		cid_bytes[i] = static_cast<uint8_t>(0xA0 + i);
	}
	p.retry_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));

	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	ASSERT_TRUE(decoded.value().retry_source_connection_id.has_value());
	EXPECT_EQ(decoded.value().retry_source_connection_id->length(), 20u);
	// Server-side validate must accept retry_source_connection_id.
	EXPECT_TRUE(decoded.value().validate(true).is_ok());
}

// ============================================================================
// Round 2 (issue #1148): expand branch coverage from 62.6% to ≥75%.
//
// Targets enumerated in the issue body:
//   * Truncated varint length prefixes
//   * Length-field exceeding remaining payload
//   * Duplicate parameter IDs in same extension
//   * Reserved parameter ID values (RFC 9287 greasing)
//   * Parameter-value-too-large for fixed-width fields
//   * Greasing-parameter (RFC 9287) edge cases
//   * Empty / single-byte extension boundary cases
//
// All TEST identifiers carry the "Extra" suffix to avoid colliding with the
// existing Round 1 suites above and with the broader
// quic_transport_params_test.cpp / quic_transport_params_coverage_test.cpp
// suites.
// ============================================================================

namespace {

// Helper: build a varint with a specific prefix-class while clipping the
// trailing bytes. Useful for exercising the varint::decode() failure path
// inside each switch arm.
auto truncated_varint_bytes(uint8_t prefix_class, size_t actual_bytes)
	-> std::vector<uint8_t>
{
	// prefix_class is the 2-bit code 0..3 indicating the encoded length:
	// 0 -> 1 byte, 1 -> 2 bytes, 2 -> 4 bytes, 3 -> 8 bytes.
	std::vector<uint8_t> out;
	out.reserve(actual_bytes);
	if (actual_bytes == 0)
	{
		return out;
	}
	out.push_back(static_cast<uint8_t>(prefix_class << 6));
	for (size_t i = 1; i < actual_bytes; ++i)
	{
		out.push_back(0x00);
	}
	return out;
}

auto make_varint_param(uint64_t id, uint64_t value) -> std::vector<uint8_t>
{
	auto v = quic::varint::encode(value);
	return make_param(id, std::span<const uint8_t>(v.data(), v.size()));
}

}  // namespace

// ----------------------------------------------------------------------------
// Per-case malformed-varint extras: cover the truncation arms for each
// remaining transport_param_id that the Round 1 file does not individually
// test. Each input is a parameter whose declared length matches the byte
// payload, but the bytes inside are a varint prefix that promises more bytes
// than are present.
// ----------------------------------------------------------------------------

class TransportParamsBranchTruncatedVarintExtra
	: public ::testing::TestWithParam<uint64_t>
{
};

TEST_P(TransportParamsBranchTruncatedVarintExtra, EachParamRejectsTruncated2ByteVarint)
{
	const uint64_t param_id = GetParam();
	// Inner varint is class 0b01 (declares 2 bytes) but only 1 byte present.
	auto inner = truncated_varint_bytes(1, 1);
	auto buf = make_param(param_id, std::span<const uint8_t>(inner.data(), inner.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_P(TransportParamsBranchTruncatedVarintExtra, EachParamRejectsTruncated4ByteVarint)
{
	const uint64_t param_id = GetParam();
	// Inner varint is class 0b10 (declares 4 bytes) but only 3 bytes present.
	auto inner = truncated_varint_bytes(2, 3);
	auto buf = make_param(param_id, std::span<const uint8_t>(inner.data(), inner.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_P(TransportParamsBranchTruncatedVarintExtra, EachParamRejectsTruncated8ByteVarint)
{
	const uint64_t param_id = GetParam();
	// Inner varint is class 0b11 (declares 8 bytes) but only 7 bytes present.
	auto inner = truncated_varint_bytes(3, 7);
	auto buf = make_param(param_id, std::span<const uint8_t>(inner.data(), inner.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

INSTANTIATE_TEST_SUITE_P(
	VarintParams,
	TransportParamsBranchTruncatedVarintExtra,
	::testing::Values(
		quic::transport_param_id::max_idle_timeout,
		quic::transport_param_id::max_udp_payload_size,
		quic::transport_param_id::initial_max_data,
		quic::transport_param_id::initial_max_stream_data_bidi_local,
		quic::transport_param_id::initial_max_stream_data_bidi_remote,
		quic::transport_param_id::initial_max_stream_data_uni,
		quic::transport_param_id::initial_max_streams_bidi,
		quic::transport_param_id::initial_max_streams_uni,
		quic::transport_param_id::ack_delay_exponent,
		quic::transport_param_id::max_ack_delay,
		quic::transport_param_id::active_connection_id_limit));

// ----------------------------------------------------------------------------
// Length-field exceeding remaining payload. Each test asserts that the
// param_len > data.size() check at transport_params.cpp:249 fires, regardless
// of which parameter id appears before the overrun length prefix.
// ----------------------------------------------------------------------------

class TransportParamsBranchLengthOverrunExtra
	: public ::testing::TestWithParam<uint64_t>
{
};

TEST_P(TransportParamsBranchLengthOverrunExtra, LengthExceedsRemainingPayloadRejected)
{
	const uint64_t param_id = GetParam();
	// Write the id, then declare a 4-byte payload, then supply only 1 byte.
	auto id_bytes = quic::varint::encode(param_id);
	auto len_bytes = quic::varint::encode(uint64_t{4});
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), id_bytes.begin(), id_bytes.end());
	buf.insert(buf.end(), len_bytes.begin(), len_bytes.end());
	buf.push_back(0x00);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_P(TransportParamsBranchLengthOverrunExtra, ImplausibleLargeLengthRejected)
{
	const uint64_t param_id = GetParam();
	// Declare a length of 16383 (max representable in a 2-byte varint) with no
	// trailing payload bytes; the check at line 249 must short-circuit before
	// any switch-arm runs.
	auto id_bytes = quic::varint::encode(param_id);
	auto len_bytes = quic::varint::encode(uint64_t{16383});
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), id_bytes.begin(), id_bytes.end());
	buf.insert(buf.end(), len_bytes.begin(), len_bytes.end());
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

INSTANTIATE_TEST_SUITE_P(
	AllKnownParamIds,
	TransportParamsBranchLengthOverrunExtra,
	::testing::Values(
		quic::transport_param_id::original_destination_connection_id,
		quic::transport_param_id::max_idle_timeout,
		quic::transport_param_id::stateless_reset_token,
		quic::transport_param_id::max_udp_payload_size,
		quic::transport_param_id::initial_max_data,
		quic::transport_param_id::initial_max_stream_data_bidi_local,
		quic::transport_param_id::initial_max_stream_data_bidi_remote,
		quic::transport_param_id::initial_max_stream_data_uni,
		quic::transport_param_id::initial_max_streams_bidi,
		quic::transport_param_id::initial_max_streams_uni,
		quic::transport_param_id::ack_delay_exponent,
		quic::transport_param_id::max_ack_delay,
		quic::transport_param_id::disable_active_migration,
		quic::transport_param_id::preferred_address,
		quic::transport_param_id::active_connection_id_limit,
		quic::transport_param_id::initial_source_connection_id,
		quic::transport_param_id::retry_source_connection_id));

// ----------------------------------------------------------------------------
// Duplicate parameter IDs. Round 1 covers max_idle_timeout, ack_delay_exponent,
// and preferred_address. This parameterised suite walks the rest so each
// switch arm visits the duplicate-detection branch with seen_params already
// populated.
// ----------------------------------------------------------------------------

class TransportParamsBranchDuplicateExtra
	: public ::testing::TestWithParam<uint64_t>
{
};

TEST_P(TransportParamsBranchDuplicateExtra, DuplicateRejected)
{
	const uint64_t param_id = GetParam();
	auto first = make_varint_param(param_id, 42);
	auto second = make_varint_param(param_id, 99);
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), first.begin(), first.end());
	buf.insert(buf.end(), second.begin(), second.end());
	auto result = quic::transport_parameters::decode(as_span(buf));
	EXPECT_TRUE(result.is_err());
}

INSTANTIATE_TEST_SUITE_P(
	VarintCarryingParams,
	TransportParamsBranchDuplicateExtra,
	::testing::Values(
		quic::transport_param_id::max_udp_payload_size,
		quic::transport_param_id::initial_max_data,
		quic::transport_param_id::initial_max_stream_data_bidi_local,
		quic::transport_param_id::initial_max_stream_data_bidi_remote,
		quic::transport_param_id::initial_max_stream_data_uni,
		quic::transport_param_id::initial_max_streams_bidi,
		quic::transport_param_id::initial_max_streams_uni,
		quic::transport_param_id::max_ack_delay,
		quic::transport_param_id::active_connection_id_limit));

class TransportParamsBranchDuplicateConnectionIdExtra : public ::testing::Test
{
};

TEST_F(TransportParamsBranchDuplicateConnectionIdExtra, DuplicateOriginalDestinationCidRejected)
{
	std::array<uint8_t, 4> cid_a{0x01, 0x02, 0x03, 0x04};
	std::array<uint8_t, 4> cid_b{0x05, 0x06, 0x07, 0x08};
	auto first = make_param(quic::transport_param_id::original_destination_connection_id,
							std::span<const uint8_t>(cid_a.data(), cid_a.size()));
	auto second = make_param(quic::transport_param_id::original_destination_connection_id,
							 std::span<const uint8_t>(cid_b.data(), cid_b.size()));
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), first.begin(), first.end());
	buf.insert(buf.end(), second.begin(), second.end());
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchDuplicateConnectionIdExtra, DuplicateInitialSourceCidRejected)
{
	std::array<uint8_t, 4> cid_a{0xAA, 0xBB, 0xCC, 0xDD};
	std::array<uint8_t, 4> cid_b{0xEE, 0xFF, 0x00, 0x11};
	auto first = make_param(quic::transport_param_id::initial_source_connection_id,
							std::span<const uint8_t>(cid_a.data(), cid_a.size()));
	auto second = make_param(quic::transport_param_id::initial_source_connection_id,
							 std::span<const uint8_t>(cid_b.data(), cid_b.size()));
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), first.begin(), first.end());
	buf.insert(buf.end(), second.begin(), second.end());
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchDuplicateConnectionIdExtra, DuplicateRetrySourceCidRejected)
{
	std::array<uint8_t, 2> cid_a{0x10, 0x20};
	std::array<uint8_t, 2> cid_b{0x30, 0x40};
	auto first = make_param(quic::transport_param_id::retry_source_connection_id,
							std::span<const uint8_t>(cid_a.data(), cid_a.size()));
	auto second = make_param(quic::transport_param_id::retry_source_connection_id,
							 std::span<const uint8_t>(cid_b.data(), cid_b.size()));
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), first.begin(), first.end());
	buf.insert(buf.end(), second.begin(), second.end());
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchDuplicateConnectionIdExtra, DuplicateStatelessResetTokenRejected)
{
	std::array<uint8_t, 16> token_a{};
	std::array<uint8_t, 16> token_b{};
	token_a.fill(0x11);
	token_b.fill(0x22);
	auto first = make_param(quic::transport_param_id::stateless_reset_token,
							std::span<const uint8_t>(token_a.data(), token_a.size()));
	auto second = make_param(quic::transport_param_id::stateless_reset_token,
							 std::span<const uint8_t>(token_b.data(), token_b.size()));
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), first.begin(), first.end());
	buf.insert(buf.end(), second.begin(), second.end());
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchDuplicateConnectionIdExtra, DuplicateDisableActiveMigrationRejected)
{
	// disable_active_migration carries an empty value: length 0.
	std::array<uint8_t, 0> empty{};
	auto first = make_param(quic::transport_param_id::disable_active_migration,
							std::span<const uint8_t>(empty.data(), empty.size()));
	auto second = make_param(quic::transport_param_id::disable_active_migration,
							 std::span<const uint8_t>(empty.data(), empty.size()));
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), first.begin(), first.end());
	buf.insert(buf.end(), second.begin(), second.end());
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

// ----------------------------------------------------------------------------
// RFC 9287 greasing values: ids of the form 31*N+27 (0x1B, 0x3A, 0x59, ...)
// reserved as no-ops. The decoder must ignore them (default branch at
// transport_params.cpp:543) and continue parsing remaining parameters.
// ----------------------------------------------------------------------------

class TransportParamsBranchGreasingExtra
	: public ::testing::TestWithParam<uint64_t>
{
};

TEST_P(TransportParamsBranchGreasingExtra, GreasingValueIgnoredEmpty)
{
	const uint64_t grease_id = GetParam();
	// Empty value.
	auto buf = make_param(grease_id, std::span<const uint8_t>{});
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	// No known parameter mutated.
	auto& p = result.value();
	EXPECT_EQ(p.max_idle_timeout, 0u);
	EXPECT_EQ(p.initial_max_data, 0u);
}

TEST_P(TransportParamsBranchGreasingExtra, GreasingValueIgnoredWithOpaquePayload)
{
	const uint64_t grease_id = GetParam();
	std::vector<uint8_t> opaque{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
	auto buf = make_param(grease_id, std::span<const uint8_t>(opaque.data(), opaque.size()));
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	// Greasing must not poison subsequent state.
	EXPECT_EQ(result.value().max_idle_timeout, 0u);
}

TEST_P(TransportParamsBranchGreasingExtra, GreasingFollowedByKnownParameter)
{
	const uint64_t grease_id = GetParam();
	std::vector<uint8_t> opaque{0x01, 0x02, 0x03};
	auto grease = make_param(grease_id, std::span<const uint8_t>(opaque.data(), opaque.size()));
	auto known = make_varint_param(quic::transport_param_id::initial_max_data, 4096);
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), grease.begin(), grease.end());
	buf.insert(buf.end(), known.begin(), known.end());
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().initial_max_data, 4096u);
}

INSTANTIATE_TEST_SUITE_P(
	ReservedIds,
	TransportParamsBranchGreasingExtra,
	::testing::Values(
		// 31*N+27 for N = 0, 1, 2, 3, 4, ... (RFC 9287)
		uint64_t{27},
		uint64_t{58},
		uint64_t{89},
		uint64_t{120},
		uint64_t{151},
		// A larger reserved id that requires a 2-byte varint encoding.
		uint64_t{27 + 31 * 100},
		// A reserved id requiring a 4-byte varint encoding.
		uint64_t{27 + 31 * 100000}));

// ----------------------------------------------------------------------------
// Empty / single-byte extension boundary cases.
// ----------------------------------------------------------------------------

class TransportParamsBranchExtensionBoundaryExtra : public ::testing::Test
{
};

TEST_F(TransportParamsBranchExtensionBoundaryExtra, EmptyExtensionDecodesToDefault)
{
	std::vector<uint8_t> empty;
	auto result = quic::transport_parameters::decode(as_span(empty));
	ASSERT_TRUE(result.is_ok());
	// All defaults intact.
	EXPECT_EQ(result.value().max_idle_timeout, 0u);
	EXPECT_EQ(result.value().ack_delay_exponent, 3u);
	EXPECT_EQ(result.value().max_ack_delay, 25u);
	EXPECT_EQ(result.value().max_udp_payload_size, 65527u);
	EXPECT_EQ(result.value().active_connection_id_limit, 2u);
	EXPECT_FALSE(result.value().original_destination_connection_id.has_value());
	EXPECT_FALSE(result.value().preferred_address.has_value());
}

TEST_F(TransportParamsBranchExtensionBoundaryExtra, SingleByteExtensionIsTruncatedIdRejected)
{
	// A single byte whose varint prefix declares an 8-byte form. read_varint
	// for the id must fail before any switch arm runs.
	std::vector<uint8_t> buf{0xC0};
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchExtensionBoundaryExtra, SingleByteExtensionIsLoneValidIdNoLength)
{
	// 0x01 is a valid 1-byte varint for max_idle_timeout but the parameter
	// length varint is missing entirely.
	std::vector<uint8_t> buf{0x01};
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchExtensionBoundaryExtra, ZeroLengthDisableActiveMigrationAccepted)
{
	std::array<uint8_t, 0> empty{};
	auto buf = make_param(quic::transport_param_id::disable_active_migration,
						  std::span<const uint8_t>(empty.data(), empty.size()));
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	EXPECT_TRUE(result.value().disable_active_migration);
}

TEST_F(TransportParamsBranchExtensionBoundaryExtra, ZeroLengthVarintParamDecodesAsZero)
{
	// A zero-length payload for max_idle_timeout: inner varint::decode of an
	// empty span should fail. Confirms the per-arm error branch fires.
	std::array<uint8_t, 0> empty{};
	auto buf = make_param(quic::transport_param_id::max_idle_timeout,
						  std::span<const uint8_t>(empty.data(), empty.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchExtensionBoundaryExtra, SingleParamMaxLengthVarintAccepted)
{
	// Encode initial_max_data with a maximum 4-byte-form value. Exercises the
	// upper boundary of the varint decode path without saturating to 8-byte.
	const uint64_t value = quic::varint::max_4byte;
	auto buf = make_varint_param(quic::transport_param_id::initial_max_data, value);
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().initial_max_data, value);
}

TEST_F(TransportParamsBranchExtensionBoundaryExtra, SingleParamMaxLength8ByteVarintAccepted)
{
	// 8-byte form. Confirms the largest representable varint decodes without
	// truncation.
	const uint64_t value = quic::varint_max;
	auto buf = make_varint_param(quic::transport_param_id::initial_max_data, value);
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().initial_max_data, value);
}

// ----------------------------------------------------------------------------
// Bound-check error branches for fixed-range parameters. These mirror the
// happy-path boundary tests already in coverage_test.cpp but use varint forms
// that force the decoder through the larger length classes — the same value
// can be encoded in 1, 2, 4, or 8 bytes, and the bounds check fires after
// varint decode regardless of width.
// ----------------------------------------------------------------------------

class TransportParamsBranchBoundsExtra : public ::testing::Test
{
};

TEST_F(TransportParamsBranchBoundsExtra, AckDelayExponentRejectedWith2ByteForm)
{
	// 21 encoded as a 2-byte varint instead of 1-byte: same value, different
	// width. Forces the post-decode bound check at line 331-336.
	std::vector<uint8_t> value{0x40, 0x15};  // 0x40 prefix + low byte 0x15 = 21
	auto buf = make_param(quic::transport_param_id::ack_delay_exponent,
						  std::span<const uint8_t>(value.data(), value.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchBoundsExtra, AckDelayExponentRejectedWith4ByteForm)
{
	// 21 encoded as a 4-byte varint.
	std::vector<uint8_t> value{0x80, 0x00, 0x00, 0x15};
	auto buf = make_param(quic::transport_param_id::ack_delay_exponent,
						  std::span<const uint8_t>(value.data(), value.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchBoundsExtra, AckDelayExponentRejectedAtMaxRepresentable)
{
	// Max-representable varint value, far above the cap of 20.
	auto buf = make_varint_param(quic::transport_param_id::ack_delay_exponent,
								 quic::varint_max);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchBoundsExtra, MaxAckDelayRejectedAtMaxRepresentable)
{
	auto buf = make_varint_param(quic::transport_param_id::max_ack_delay,
								 quic::varint_max);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchBoundsExtra, MaxUdpPayloadSizeRejectedAtZero)
{
	auto buf = make_varint_param(quic::transport_param_id::max_udp_payload_size, 0);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchBoundsExtra, MaxUdpPayloadSizeAcceptsHugeValue)
{
	// Above 1200 is accepted by decode (any other validator concerns are
	// outside decode's contract).
	auto buf = make_varint_param(quic::transport_param_id::max_udp_payload_size,
								 quic::varint_max);
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().max_udp_payload_size, quic::varint_max);
}

TEST_F(TransportParamsBranchBoundsExtra, ActiveConnectionIdLimitRejectedAtZero)
{
	auto buf = make_varint_param(quic::transport_param_id::active_connection_id_limit, 0);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchBoundsExtra, StatelessResetTokenRejectedAtZeroLength)
{
	std::array<uint8_t, 0> empty{};
	auto buf = make_param(quic::transport_param_id::stateless_reset_token,
						  std::span<const uint8_t>(empty.data(), empty.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchBoundsExtra, StatelessResetTokenRejectedAtOneByte)
{
	std::array<uint8_t, 1> single{0x01};
	auto buf = make_param(quic::transport_param_id::stateless_reset_token,
						  std::span<const uint8_t>(single.data(), single.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchBoundsExtra, DisableActiveMigrationRejectedWithOneBytePayload)
{
	std::array<uint8_t, 1> one{0x00};
	auto buf = make_param(quic::transport_param_id::disable_active_migration,
						  std::span<const uint8_t>(one.data(), one.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

// ----------------------------------------------------------------------------
// Connection-id length rejection branches for each of the three connection-id
// parameters (original_destination, initial_source, retry_source). The
// coverage_test.cpp suite covers initial_source only.
// ----------------------------------------------------------------------------

class TransportParamsBranchConnectionIdLengthExtra : public ::testing::Test
{
};

TEST_F(TransportParamsBranchConnectionIdLengthExtra, OriginalDestinationCidTooLongRejected)
{
	std::vector<uint8_t> too_long(21, 0x42);
	auto buf = make_param(quic::transport_param_id::original_destination_connection_id,
						  std::span<const uint8_t>(too_long.data(), too_long.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchConnectionIdLengthExtra, RetrySourceCidTooLongRejected)
{
	std::vector<uint8_t> too_long(25, 0x37);
	auto buf = make_param(quic::transport_param_id::retry_source_connection_id,
						  std::span<const uint8_t>(too_long.data(), too_long.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchConnectionIdLengthExtra, OriginalDestinationCidAtMaxAccepted)
{
	std::vector<uint8_t> at_max(20, 0xCD);
	auto buf = make_param(quic::transport_param_id::original_destination_connection_id,
						  std::span<const uint8_t>(at_max.data(), at_max.size()));
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	ASSERT_TRUE(result.value().original_destination_connection_id.has_value());
	EXPECT_EQ(result.value().original_destination_connection_id->length(), 20u);
}

TEST_F(TransportParamsBranchConnectionIdLengthExtra, OriginalDestinationCidEmptyAccepted)
{
	std::array<uint8_t, 0> empty{};
	auto buf = make_param(quic::transport_param_id::original_destination_connection_id,
						  std::span<const uint8_t>(empty.data(), empty.size()));
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	ASSERT_TRUE(result.value().original_destination_connection_id.has_value());
	EXPECT_EQ(result.value().original_destination_connection_id->length(), 0u);
}

// ----------------------------------------------------------------------------
// preferred_address: per-field error branches that the coverage_test does
// not exercise. Specifically, the OR of cid_len > 20 and the offset overflow
// check at line 526.
// ----------------------------------------------------------------------------

class TransportParamsBranchPreferredAddressExtra : public ::testing::Test
{
};

TEST_F(TransportParamsBranchPreferredAddressExtra, CidLengthByteAtMaximumBoundary)
{
	// cid_len byte == 20, with exactly 20 payload bytes and a 16-byte token.
	std::vector<uint8_t> payload;
	payload.reserve(4 + 2 + 16 + 2 + 1 + 20 + 16);
	for (int i = 0; i < 4; ++i)
	{
		payload.push_back(static_cast<uint8_t>(192 + i));
	}
	payload.push_back(0x01);
	payload.push_back(0xBB);
	for (int i = 0; i < 16; ++i)
	{
		payload.push_back(static_cast<uint8_t>(0x20 + i));
	}
	payload.push_back(0x80);
	payload.push_back(0x50);
	payload.push_back(20);  // cid length byte
	for (int i = 0; i < 20; ++i)
	{
		payload.push_back(static_cast<uint8_t>(0x40 + i));
	}
	for (int i = 0; i < 16; ++i)
	{
		payload.push_back(static_cast<uint8_t>(0xC0 + i));
	}
	auto buf = make_param(quic::transport_param_id::preferred_address,
						  std::span<const uint8_t>(payload.data(), payload.size()));
	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	ASSERT_TRUE(result.value().preferred_address.has_value());
	EXPECT_EQ(result.value().preferred_address->connection_id.length(), 20u);
}

TEST_F(TransportParamsBranchPreferredAddressExtra, OffsetOverflowWhenCidLengthLies)
{
	// 41 byte minimum is met, but cid_len byte declares 5 bytes when only
	// space for the reset token remains. The offset + cid_len + 16 > param_len
	// check at line 526 must reject this.
	std::vector<uint8_t> payload(41, 0x00);
	// Place the cid_len byte at offset 24 (after 4+2+16+2 = 24).
	payload[24] = 5;
	auto buf = make_param(quic::transport_param_id::preferred_address,
						  std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchPreferredAddressExtra, PreferredAddressJustBelowMinimumRejected)
{
	// 40 bytes — one short of the 41-byte minimum.
	std::vector<uint8_t> payload(40, 0x55);
	auto buf = make_param(quic::transport_param_id::preferred_address,
						  std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsBranchPreferredAddressExtra, PreferredAddressOneByteRejected)
{
	std::vector<uint8_t> payload{0x00};
	auto buf = make_param(quic::transport_param_id::preferred_address,
						  std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

// ----------------------------------------------------------------------------
// encode() emission paths for optional / non-default fields that the
// coverage_test does not individually toggle. Each test sets a single field
// and confirms the resulting buffer contains the matching id.
// ----------------------------------------------------------------------------

class TransportParamsBranchEncodeOptionalExtra : public ::testing::Test
{
protected:
	static auto contains_param_id(const std::vector<uint8_t>& encoded, uint8_t id) -> bool
	{
		// id values <= 0x3F encode as a single byte equal to the id.
		for (size_t i = 0; i < encoded.size(); ++i)
		{
			if (encoded[i] == id)
			{
				return true;
			}
		}
		return false;
	}
};

TEST_F(TransportParamsBranchEncodeOptionalExtra, OriginalDestinationCidEmits)
{
	quic::transport_parameters p;
	std::array<uint8_t, 6> cid{0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
	p.original_destination_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid.data(), cid.size()));
	auto encoded = p.encode();
	EXPECT_TRUE(contains_param_id(
		encoded,
		static_cast<uint8_t>(quic::transport_param_id::original_destination_connection_id)));
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, InitialSourceCidEmits)
{
	quic::transport_parameters p;
	std::array<uint8_t, 4> cid{0xB0, 0xB1, 0xB2, 0xB3};
	p.initial_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid.data(), cid.size()));
	auto encoded = p.encode();
	EXPECT_TRUE(contains_param_id(
		encoded,
		static_cast<uint8_t>(quic::transport_param_id::initial_source_connection_id)));
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, StatelessResetTokenEmits)
{
	quic::transport_parameters p;
	std::array<uint8_t, 16> token{};
	token.fill(0x5A);
	p.stateless_reset_token = token;
	auto encoded = p.encode();
	EXPECT_TRUE(contains_param_id(
		encoded,
		static_cast<uint8_t>(quic::transport_param_id::stateless_reset_token)));
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, MaxIdleTimeoutEmitsWhenNonZero)
{
	quic::transport_parameters p;
	p.max_idle_timeout = 30000;
	auto encoded = p.encode();
	EXPECT_TRUE(contains_param_id(
		encoded, static_cast<uint8_t>(quic::transport_param_id::max_idle_timeout)));
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, AckDelayExponentEmitsWhenNonDefault)
{
	quic::transport_parameters p;
	p.ack_delay_exponent = 7;  // default is 3
	auto encoded = p.encode();
	EXPECT_TRUE(contains_param_id(
		encoded, static_cast<uint8_t>(quic::transport_param_id::ack_delay_exponent)));
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, MaxAckDelayEmitsWhenNonDefault)
{
	quic::transport_parameters p;
	p.max_ack_delay = 50;  // default is 25
	auto encoded = p.encode();
	EXPECT_TRUE(contains_param_id(
		encoded, static_cast<uint8_t>(quic::transport_param_id::max_ack_delay)));
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, InitialMaxStreamDataBidiRemoteEmitsWhenNonZero)
{
	quic::transport_parameters p;
	p.initial_max_stream_data_bidi_remote = 1024;
	auto encoded = p.encode();
	EXPECT_TRUE(contains_param_id(
		encoded,
		static_cast<uint8_t>(quic::transport_param_id::initial_max_stream_data_bidi_remote)));
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, InitialMaxStreamDataUniEmitsWhenNonZero)
{
	quic::transport_parameters p;
	p.initial_max_stream_data_uni = 2048;
	auto encoded = p.encode();
	EXPECT_TRUE(contains_param_id(
		encoded,
		static_cast<uint8_t>(quic::transport_param_id::initial_max_stream_data_uni)));
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, InitialMaxStreamsUniEmitsWhenNonZero)
{
	quic::transport_parameters p;
	p.initial_max_streams_uni = 32;
	auto encoded = p.encode();
	EXPECT_TRUE(contains_param_id(
		encoded, static_cast<uint8_t>(quic::transport_param_id::initial_max_streams_uni)));
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, AllOptionalFieldsRoundTrip)
{
	quic::transport_parameters p;
	std::array<uint8_t, 8> cid{0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
	p.original_destination_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid.data(), cid.size()));
	p.initial_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid.data(), cid.size()));
	p.retry_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid.data(), cid.size()));
	std::array<uint8_t, 16> token{};
	token.fill(0xCC);
	p.stateless_reset_token = token;
	p.max_idle_timeout = 60000;
	p.ack_delay_exponent = 5;
	p.max_ack_delay = 100;
	p.max_udp_payload_size = 1500;
	p.initial_max_data = 65536;
	p.initial_max_stream_data_bidi_local = 32768;
	p.initial_max_stream_data_bidi_remote = 32768;
	p.initial_max_stream_data_uni = 32768;
	p.initial_max_streams_bidi = 50;
	p.initial_max_streams_uni = 50;
	p.disable_active_migration = true;
	p.active_connection_id_limit = 8;

	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(decoded.value().max_idle_timeout, 60000u);
	EXPECT_EQ(decoded.value().ack_delay_exponent, 5u);
	EXPECT_EQ(decoded.value().max_ack_delay, 100u);
	EXPECT_EQ(decoded.value().initial_max_data, 65536u);
	EXPECT_EQ(decoded.value().initial_max_streams_uni, 50u);
	EXPECT_TRUE(decoded.value().disable_active_migration);
	EXPECT_EQ(decoded.value().active_connection_id_limit, 8u);
	ASSERT_TRUE(decoded.value().stateless_reset_token.has_value());
	EXPECT_EQ((*decoded.value().stateless_reset_token)[0], 0xCC);
}

TEST_F(TransportParamsBranchEncodeOptionalExtra, PreferredAddressEmitsLargeAggregate)
{
	quic::transport_parameters p;
	quic::preferred_address_info addr;
	addr.ipv4_address = {10, 0, 0, 1};
	addr.ipv4_port = 0x1234;
	addr.ipv6_address = {0xFE, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x01};
	addr.ipv6_port = 0x5678;
	std::array<uint8_t, 16> cid_bytes{};
	for (size_t i = 0; i < cid_bytes.size(); ++i)
	{
		cid_bytes[i] = static_cast<uint8_t>(0x10 + i);
	}
	addr.connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
	addr.stateless_reset_token.fill(0x77);
	p.preferred_address = addr;

	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	ASSERT_TRUE(decoded.value().preferred_address.has_value());
	EXPECT_EQ(decoded.value().preferred_address->ipv4_port, 0x1234);
	EXPECT_EQ(decoded.value().preferred_address->ipv6_port, 0x5678);
	EXPECT_EQ(decoded.value().preferred_address->connection_id.length(), cid_bytes.size());
	EXPECT_EQ(decoded.value().preferred_address->stateless_reset_token[0], 0x77);
}

// ----------------------------------------------------------------------------
// validate(): server-side variants of the four bound checks. Round 1 / earlier
// tests cover client-side; these confirm the same checks fire for is_server.
// ----------------------------------------------------------------------------

class TransportParamsBranchValidateExtra : public ::testing::Test
{
};

TEST_F(TransportParamsBranchValidateExtra, ServerAlsoRejectsAckDelayExponentOver20)
{
	quic::transport_parameters p;
	p.ack_delay_exponent = 21;
	EXPECT_TRUE(p.validate(true).is_err());
}

TEST_F(TransportParamsBranchValidateExtra, ServerAlsoRejectsMaxAckDelayOverCap)
{
	quic::transport_parameters p;
	p.max_ack_delay = 16384;
	EXPECT_TRUE(p.validate(true).is_err());
}

TEST_F(TransportParamsBranchValidateExtra, ServerAlsoRejectsTinyMaxUdpPayloadSize)
{
	quic::transport_parameters p;
	p.max_udp_payload_size = 1199;
	EXPECT_TRUE(p.validate(true).is_err());
}

TEST_F(TransportParamsBranchValidateExtra, ServerAlsoRejectsActiveConnectionIdLimitBelowTwo)
{
	quic::transport_parameters p;
	p.active_connection_id_limit = 1;
	EXPECT_TRUE(p.validate(true).is_err());
}

TEST_F(TransportParamsBranchValidateExtra, ClientNoServerOnlyAndCleanBoundsIsOk)
{
	// All four bound checks pass and no server-only field set: valid for
	// is_server = false. Confirms the fall-through return ok() branch.
	quic::transport_parameters p;
	p.ack_delay_exponent = 20;
	p.max_ack_delay = 16383;
	p.max_udp_payload_size = 1200;
	p.active_connection_id_limit = 2;
	EXPECT_TRUE(p.validate(false).is_ok());
}

TEST_F(TransportParamsBranchValidateExtra, ServerWithAllOptionalsValidates)
{
	// Server-side validate must not reject any of the server-only fields.
	quic::transport_parameters p;
	std::array<uint8_t, 4> cid{0x01, 0x02, 0x03, 0x04};
	p.original_destination_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid.data(), cid.size()));
	p.retry_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid.data(), cid.size()));
	std::array<uint8_t, 16> token{};
	token.fill(0x99);
	p.stateless_reset_token = token;
	quic::preferred_address_info addr;
	addr.connection_id =
		quic::connection_id(std::span<const uint8_t>(cid.data(), cid.size()));
	p.preferred_address = addr;
	EXPECT_TRUE(p.validate(true).is_ok());
}

// ----------------------------------------------------------------------------
// apply_defaults(): each conditional's false branch is taken when the field
// is already non-zero. Round 1 / earlier tests cover the true branches.
// ----------------------------------------------------------------------------

class TransportParamsBranchApplyDefaultsExtra : public ::testing::Test
{
};

TEST_F(TransportParamsBranchApplyDefaultsExtra, AllNonZeroLeftUnchanged)
{
	quic::transport_parameters p;
	p.max_udp_payload_size = 1500;
	p.ack_delay_exponent = 10;
	p.max_ack_delay = 50;
	p.active_connection_id_limit = 7;
	p.apply_defaults();
	EXPECT_EQ(p.max_udp_payload_size, 1500u);
	EXPECT_EQ(p.ack_delay_exponent, 10u);
	EXPECT_EQ(p.max_ack_delay, 50u);
	EXPECT_EQ(p.active_connection_id_limit, 7u);
}

TEST_F(TransportParamsBranchApplyDefaultsExtra, ZeroedExplicitlyRestoresAllDefaults)
{
	// Force every guarded field to zero so each if-branch's true arm runs.
	quic::transport_parameters p;
	p.max_udp_payload_size = 0;
	p.ack_delay_exponent = 0;
	p.max_ack_delay = 0;
	p.active_connection_id_limit = 0;
	p.apply_defaults();
	EXPECT_EQ(p.max_udp_payload_size, 65527u);
	EXPECT_EQ(p.ack_delay_exponent, 3u);
	EXPECT_EQ(p.max_ack_delay, 25u);
	EXPECT_EQ(p.active_connection_id_limit, 2u);
}

// ----------------------------------------------------------------------------
// Mixed extension: known + greasing + known. Confirms the decoder threads the
// default branch in between switch hits without losing state.
// ----------------------------------------------------------------------------

class TransportParamsBranchMixedExtensionExtra : public ::testing::Test
{
};

TEST_F(TransportParamsBranchMixedExtensionExtra, KnownGreaseKnownChainsCorrectly)
{
	auto first = make_varint_param(quic::transport_param_id::initial_max_data, 1024);
	std::vector<uint8_t> grease_payload{0x01, 0x02, 0x03};
	auto grease = make_param(
		uint64_t{27},  // RFC 9287 reserved
		std::span<const uint8_t>(grease_payload.data(), grease_payload.size()));
	auto last = make_varint_param(quic::transport_param_id::initial_max_streams_bidi, 64);

	std::vector<uint8_t> buf;
	buf.insert(buf.end(), first.begin(), first.end());
	buf.insert(buf.end(), grease.begin(), grease.end());
	buf.insert(buf.end(), last.begin(), last.end());

	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	EXPECT_EQ(result.value().initial_max_data, 1024u);
	EXPECT_EQ(result.value().initial_max_streams_bidi, 64u);
}

TEST_F(TransportParamsBranchMixedExtensionExtra, MultipleGreasingValuesAllIgnored)
{
	std::vector<uint8_t> opaque{0xAA};
	auto g1 = make_param(uint64_t{27},
						 std::span<const uint8_t>(opaque.data(), opaque.size()));
	auto g2 = make_param(uint64_t{58},
						 std::span<const uint8_t>(opaque.data(), opaque.size()));
	auto g3 = make_param(uint64_t{89},
						 std::span<const uint8_t>(opaque.data(), opaque.size()));
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), g1.begin(), g1.end());
	buf.insert(buf.end(), g2.begin(), g2.end());
	buf.insert(buf.end(), g3.begin(), g3.end());

	auto result = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(result.is_ok());
	// All defaults intact: no known param touched.
	EXPECT_EQ(result.value().max_idle_timeout, 0u);
	EXPECT_EQ(result.value().initial_max_data, 0u);
	EXPECT_EQ(result.value().max_ack_delay, 25u);
}

TEST_F(TransportParamsBranchMixedExtensionExtra, DuplicateGreasingIdRejected)
{
	std::vector<uint8_t> opaque{0xAA, 0xBB};
	auto g1 = make_param(uint64_t{27},
						 std::span<const uint8_t>(opaque.data(), opaque.size()));
	auto g2 = make_param(uint64_t{27},
						 std::span<const uint8_t>(opaque.data(), opaque.size()));
	std::vector<uint8_t> buf;
	buf.insert(buf.end(), g1.begin(), g1.end());
	buf.insert(buf.end(), g2.begin(), g2.end());
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}
