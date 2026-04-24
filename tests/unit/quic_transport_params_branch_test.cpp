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
