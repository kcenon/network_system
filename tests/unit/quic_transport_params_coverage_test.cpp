/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

// Coverage-expansion tests for src/protocols/quic/transport_params.cpp
// targeting branches not reached by tests/unit/quic_transport_params_test.cpp:
// per-field encode isolation, preferred_address byte-layout verification,
// exact-boundary accept/reject for every documented limit, inner varint
// decode failures, preferred_address edge cases, unknown parameter placement,
// apply_defaults() partial-zero selection, factory validation symmetry, and
// encode parameter ordering.
//
// Part of epic #953 (expand unit test coverage from 40% to 80%). Single-file
// sub-issue #1015.

#include "internal/protocols/quic/transport_params.h"
#include "internal/protocols/quic/varint.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

namespace {

// --- Encoding helpers --------------------------------------------------------

// Build a single-parameter encoded blob: varint(id) || varint(len) || value.
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

// Parameter whose value is a single varint.
auto make_varint_param(uint64_t id, uint64_t value) -> std::vector<uint8_t>
{
	auto value_bytes = quic::varint::encode(value);
	return make_param(id,
		std::span<const uint8_t>(value_bytes.data(), value_bytes.size()));
}

// Parameter whose value is zero bytes long.
auto make_empty_param(uint64_t id) -> std::vector<uint8_t>
{
	return make_param(id, std::span<const uint8_t>{});
}

// Concatenate encoded blobs.
auto concat(std::initializer_list<std::vector<uint8_t>> parts) -> std::vector<uint8_t>
{
	std::vector<uint8_t> out;
	for (const auto& p : parts)
	{
		out.insert(out.end(), p.begin(), p.end());
	}
	return out;
}

// Extract the decode-wise span view.
auto as_span(const std::vector<uint8_t>& v) -> std::span<const uint8_t>
{
	return std::span<const uint8_t>(v.data(), v.size());
}

// Look for the byte sequence of a parameter ID as a varint, returning the
// position in buffer or std::string::npos. For IDs < 64 the encoding is one
// byte equal to the ID value.
auto find_param_id(const std::vector<uint8_t>& buffer, uint64_t id) -> size_t
{
	auto id_bytes = quic::varint::encode(id);
	for (size_t i = 0; i + id_bytes.size() <= buffer.size(); ++i)
	{
		if (std::equal(id_bytes.begin(), id_bytes.end(), buffer.begin() + i))
		{
			return i;
		}
	}
	return static_cast<size_t>(-1);
}

// Encode a preferred_address payload by hand so tests can inject malformed
// variants. Layout (total 41+cid_len bytes):
//  4 IPv4 | 2 IPv4 port | 16 IPv6 | 2 IPv6 port | 1 cid_len | N cid | 16 token.
auto build_preferred_address_payload(uint8_t cid_len,
	const std::vector<uint8_t>& cid_bytes) -> std::vector<uint8_t>
{
	std::vector<uint8_t> data;
	data.resize(4 + 2, 0);  // IPv4 + port
	data.resize(data.size() + 16 + 2, 0);  // IPv6 + port
	data.push_back(cid_len);
	data.insert(data.end(), cid_bytes.begin(), cid_bytes.end());
	data.resize(data.size() + 16, 0);  // stateless reset token
	return data;
}

}  // namespace

// ============================================================================
// Encode bytewise — per-field single-param emission (only the field set)
// ============================================================================

class TransportParamsEncodeBytewiseTest : public ::testing::Test
{
};

TEST_F(TransportParamsEncodeBytewiseTest, OnlyMaxIdleTimeoutEmitted)
{
	quic::transport_parameters p;
	p.max_idle_timeout = 30000;
	auto encoded = p.encode();
	EXPECT_NE(find_param_id(encoded, quic::transport_param_id::max_idle_timeout),
		static_cast<size_t>(-1));
	EXPECT_EQ(find_param_id(encoded, quic::transport_param_id::max_ack_delay),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeBytewiseTest, OnlyInitialMaxDataEmitted)
{
	quic::transport_parameters p;
	p.initial_max_data = 1024;
	auto encoded = p.encode();
	EXPECT_NE(find_param_id(encoded, quic::transport_param_id::initial_max_data),
		static_cast<size_t>(-1));
	EXPECT_EQ(find_param_id(encoded,
		quic::transport_param_id::initial_max_stream_data_bidi_local),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeBytewiseTest, OnlyInitialMaxStreamDataBidiLocalEmitted)
{
	quic::transport_parameters p;
	p.initial_max_stream_data_bidi_local = 512;
	auto encoded = p.encode();
	EXPECT_NE(find_param_id(encoded,
		quic::transport_param_id::initial_max_stream_data_bidi_local),
		static_cast<size_t>(-1));
	EXPECT_EQ(find_param_id(encoded,
		quic::transport_param_id::initial_max_stream_data_bidi_remote),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeBytewiseTest, OnlyInitialMaxStreamsBidiEmitted)
{
	quic::transport_parameters p;
	p.initial_max_streams_bidi = 100;
	auto encoded = p.encode();
	EXPECT_NE(find_param_id(encoded,
		quic::transport_param_id::initial_max_streams_bidi),
		static_cast<size_t>(-1));
	EXPECT_EQ(find_param_id(encoded,
		quic::transport_param_id::initial_max_streams_uni),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeBytewiseTest, OnlyAckDelayExponentEmitted)
{
	quic::transport_parameters p;
	p.ack_delay_exponent = 5;  // default is 3
	auto encoded = p.encode();
	EXPECT_NE(find_param_id(encoded, quic::transport_param_id::ack_delay_exponent),
		static_cast<size_t>(-1));
	EXPECT_EQ(find_param_id(encoded, quic::transport_param_id::max_ack_delay),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeBytewiseTest, OnlyActiveConnectionIdLimitEmitted)
{
	quic::transport_parameters p;
	p.active_connection_id_limit = 8;  // default is 2
	auto encoded = p.encode();
	EXPECT_NE(find_param_id(encoded,
		quic::transport_param_id::active_connection_id_limit),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeBytewiseTest, DisableActiveMigrationEmittedAsEmpty)
{
	quic::transport_parameters p;
	p.disable_active_migration = true;
	auto encoded = p.encode();
	size_t pos = find_param_id(encoded,
		quic::transport_param_id::disable_active_migration);
	ASSERT_NE(pos, static_cast<size_t>(-1));
	// After the id byte, the length varint is the single byte 0x00.
	ASSERT_LT(pos + 1, encoded.size());
	EXPECT_EQ(encoded[pos + 1], 0u);
}

TEST_F(TransportParamsEncodeBytewiseTest, StatelessResetTokenEmittedAs16Bytes)
{
	quic::transport_parameters p;
	std::array<uint8_t, 16> token{};
	for (size_t i = 0; i < 16; ++i) token[i] = static_cast<uint8_t>(i + 1);
	p.stateless_reset_token = token;
	auto encoded = p.encode();
	size_t pos = find_param_id(encoded,
		quic::transport_param_id::stateless_reset_token);
	ASSERT_NE(pos, static_cast<size_t>(-1));
	// ID=0x02 -> 1 byte; length varint for 16 is 0x10 -> 1 byte; then 16 value bytes.
	ASSERT_EQ(encoded[pos + 1], 16u);
	for (size_t i = 0; i < 16; ++i)
	{
		EXPECT_EQ(encoded[pos + 2 + i], token[i]);
	}
}

// ============================================================================
// Encode skip-default — default-valued fields produce no bytes
// ============================================================================

class TransportParamsEncodeSkipDefaultTest : public ::testing::Test
{
};

TEST_F(TransportParamsEncodeSkipDefaultTest, DefaultMaxIdleTimeoutIsSkipped)
{
	quic::transport_parameters p;
	p.max_idle_timeout = 0;  // default
	p.max_ack_delay = 100;  // forces non-empty output
	auto encoded = p.encode();
	EXPECT_EQ(find_param_id(encoded, quic::transport_param_id::max_idle_timeout),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeSkipDefaultTest, DefaultAckDelayExponentIsSkipped)
{
	quic::transport_parameters p;
	p.ack_delay_exponent = 3;  // default
	p.max_idle_timeout = 1;
	auto encoded = p.encode();
	EXPECT_EQ(find_param_id(encoded, quic::transport_param_id::ack_delay_exponent),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeSkipDefaultTest, DefaultMaxAckDelayIsSkipped)
{
	quic::transport_parameters p;
	p.max_ack_delay = 25;  // default
	p.max_idle_timeout = 1;
	auto encoded = p.encode();
	EXPECT_EQ(find_param_id(encoded, quic::transport_param_id::max_ack_delay),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeSkipDefaultTest, DefaultMaxUdpPayloadSizeIsSkipped)
{
	quic::transport_parameters p;
	p.max_udp_payload_size = 65527;  // default
	p.max_idle_timeout = 1;
	auto encoded = p.encode();
	EXPECT_EQ(find_param_id(encoded,
		quic::transport_param_id::max_udp_payload_size),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeSkipDefaultTest, DefaultActiveConnectionIdLimitIsSkipped)
{
	quic::transport_parameters p;
	p.active_connection_id_limit = 2;  // default
	p.max_idle_timeout = 1;
	auto encoded = p.encode();
	EXPECT_EQ(find_param_id(encoded,
		quic::transport_param_id::active_connection_id_limit),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeSkipDefaultTest, DisableActiveMigrationFalseIsSkipped)
{
	quic::transport_parameters p;
	p.disable_active_migration = false;  // default
	p.max_idle_timeout = 1;
	auto encoded = p.encode();
	EXPECT_EQ(find_param_id(encoded,
		quic::transport_param_id::disable_active_migration),
		static_cast<size_t>(-1));
}

TEST_F(TransportParamsEncodeSkipDefaultTest, AllDefaultsEncodeToEmpty)
{
	quic::transport_parameters p;  // all defaults
	auto encoded = p.encode();
	EXPECT_TRUE(encoded.empty());
}

// ============================================================================
// Decode boundary — exact accept/reject at each documented limit
// ============================================================================

class TransportParamsDecodeBoundaryTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeBoundaryTest, AckDelayExponentAccept20)
{
	auto buf = make_varint_param(quic::transport_param_id::ack_delay_exponent, 20);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_ok());
}

TEST_F(TransportParamsDecodeBoundaryTest, AckDelayExponentReject21)
{
	auto buf = make_varint_param(quic::transport_param_id::ack_delay_exponent, 21);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, MaxAckDelayAccept16383)
{
	auto buf = make_varint_param(quic::transport_param_id::max_ack_delay, 16383);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_ok());
}

TEST_F(TransportParamsDecodeBoundaryTest, MaxAckDelayReject16384)
{
	auto buf = make_varint_param(quic::transport_param_id::max_ack_delay, 16384);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, MaxUdpPayloadSizeAccept1200)
{
	auto buf = make_varint_param(quic::transport_param_id::max_udp_payload_size, 1200);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_ok());
}

TEST_F(TransportParamsDecodeBoundaryTest, MaxUdpPayloadSizeReject1199)
{
	auto buf = make_varint_param(quic::transport_param_id::max_udp_payload_size, 1199);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, ActiveConnectionIdLimitAccept2)
{
	auto buf = make_varint_param(
		quic::transport_param_id::active_connection_id_limit, 2);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_ok());
}

TEST_F(TransportParamsDecodeBoundaryTest, ActiveConnectionIdLimitReject1)
{
	auto buf = make_varint_param(
		quic::transport_param_id::active_connection_id_limit, 1);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, ActiveConnectionIdLimitReject0)
{
	auto buf = make_varint_param(
		quic::transport_param_id::active_connection_id_limit, 0);
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, StatelessResetTokenAccept16Bytes)
{
	std::vector<uint8_t> token(16, 0xAB);
	auto buf = make_param(quic::transport_param_id::stateless_reset_token,
		std::span<const uint8_t>(token.data(), token.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_ok());
}

TEST_F(TransportParamsDecodeBoundaryTest, StatelessResetTokenReject15Bytes)
{
	std::vector<uint8_t> token(15, 0xAB);
	auto buf = make_param(quic::transport_param_id::stateless_reset_token,
		std::span<const uint8_t>(token.data(), token.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, StatelessResetTokenReject17Bytes)
{
	std::vector<uint8_t> token(17, 0xAB);
	auto buf = make_param(quic::transport_param_id::stateless_reset_token,
		std::span<const uint8_t>(token.data(), token.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeBoundaryTest, ConnectionIdAccept20Bytes)
{
	std::vector<uint8_t> cid(20, 0xCD);
	auto buf = make_param(quic::transport_param_id::initial_source_connection_id,
		std::span<const uint8_t>(cid.data(), cid.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_ok());
}

TEST_F(TransportParamsDecodeBoundaryTest, ConnectionIdReject21Bytes)
{
	std::vector<uint8_t> cid(21, 0xCD);
	auto buf = make_param(quic::transport_param_id::initial_source_connection_id,
		std::span<const uint8_t>(cid.data(), cid.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

// ============================================================================
// Decode malformed varint — inner value-varint decode failures
// ============================================================================

class TransportParamsDecodeMalformedVarintTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeMalformedVarintTest, MaxIdleTimeoutTruncatedVarint)
{
	// Length says 2 bytes; value is a 2-byte-prefix varint (0x40) but only
	// the first byte is present -> inner varint decode fails.
	std::vector<uint8_t> truncated{0x40};  // 2-byte varint, 1 byte delivered
	auto buf = make_param(quic::transport_param_id::max_idle_timeout,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, AckDelayExponentTruncatedVarint)
{
	std::vector<uint8_t> truncated{0x80};  // 4-byte varint prefix
	auto buf = make_param(quic::transport_param_id::ack_delay_exponent,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, InitialMaxDataTruncated4ByteVarint)
{
	std::vector<uint8_t> truncated{0x80, 0x00, 0x00};
	auto buf = make_param(quic::transport_param_id::initial_max_data,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest,
	InitialMaxStreamsBidiTruncated8ByteVarint)
{
	std::vector<uint8_t> truncated{0xC0, 0, 0, 0, 0, 0, 0};  // needs 8 bytes
	auto buf = make_param(quic::transport_param_id::initial_max_streams_bidi,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, MaxAckDelayTruncatedVarint)
{
	std::vector<uint8_t> truncated{0x40};
	auto buf = make_param(quic::transport_param_id::max_ack_delay,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, ActiveConnectionIdLimitTruncatedVarint)
{
	std::vector<uint8_t> truncated{0x80, 0x00};
	auto buf = make_param(quic::transport_param_id::active_connection_id_limit,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, MaxUdpPayloadSizeTruncatedVarint)
{
	std::vector<uint8_t> truncated{0xC0, 0, 0};
	auto buf = make_param(quic::transport_param_id::max_udp_payload_size,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeMalformedVarintTest, InitialMaxStreamDataTruncatedVarint)
{
	std::vector<uint8_t> truncated{0x40};
	auto buf = make_param(
		quic::transport_param_id::initial_max_stream_data_bidi_local,
		std::span<const uint8_t>(truncated.data(), truncated.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

// ============================================================================
// Decode preferred_address — edge cases
// ============================================================================

class TransportParamsDecodePreferredAddressTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodePreferredAddressTest, MinimumSizeEmptyCid)
{
	// 41 bytes = 4 + 2 + 16 + 2 + 1(cid_len=0) + 0 + 16.
	auto payload = build_preferred_address_payload(0, {});
	ASSERT_EQ(payload.size(), 41u);
	auto buf = make_param(quic::transport_param_id::preferred_address,
		std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_ok());
}

TEST_F(TransportParamsDecodePreferredAddressTest, MaxCidLength20)
{
	std::vector<uint8_t> cid(20, 0xEE);
	auto payload = build_preferred_address_payload(20, cid);
	ASSERT_EQ(payload.size(), 61u);
	auto buf = make_param(quic::transport_param_id::preferred_address,
		std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_ok());
}

TEST_F(TransportParamsDecodePreferredAddressTest, CidLengthExceeds20Rejected)
{
	std::vector<uint8_t> cid(21, 0xEE);
	auto payload = build_preferred_address_payload(21, cid);
	auto buf = make_param(quic::transport_param_id::preferred_address,
		std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodePreferredAddressTest, PayloadTooSmallRejected)
{
	// 40 bytes — one byte short of the minimum (41 with empty cid).
	std::vector<uint8_t> payload(40, 0);
	auto buf = make_param(quic::transport_param_id::preferred_address,
		std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodePreferredAddressTest, TruncatedCidBytesRejected)
{
	// cid_len claims 10 but only 5 cid bytes + 16 reset-token bytes present.
	std::vector<uint8_t> payload;
	payload.resize(4 + 2 + 16 + 2, 0);
	payload.push_back(10);  // cid_len
	payload.insert(payload.end(), 5, 0xAA);
	payload.insert(payload.end(), 16, 0);  // token
	auto buf = make_param(quic::transport_param_id::preferred_address,
		std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodePreferredAddressTest, MissingResetTokenRejected)
{
	// cid_len=0 but fewer than 16 trailing bytes for reset token.
	std::vector<uint8_t> payload;
	payload.resize(4 + 2 + 16 + 2, 0);
	payload.push_back(0);  // cid_len
	payload.insert(payload.end(), 8, 0);  // only 8 bytes where 16 expected
	auto buf = make_param(quic::transport_param_id::preferred_address,
		std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

// ============================================================================
// Decode unknown parameter — placement
// ============================================================================

class TransportParamsDecodeUnknownParamTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeUnknownParamTest, UnknownAtStartIgnored)
{
	auto unknown = make_varint_param(0x9999, 42);
	auto known = make_varint_param(quic::transport_param_id::max_idle_timeout, 5000);
	auto buf = concat({unknown, known});
	auto r = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(r.is_ok());
	EXPECT_EQ(r.value().max_idle_timeout, 5000u);
}

TEST_F(TransportParamsDecodeUnknownParamTest, UnknownAtEndIgnored)
{
	auto known = make_varint_param(quic::transport_param_id::max_idle_timeout, 5000);
	auto unknown = make_varint_param(0x9999, 42);
	auto buf = concat({known, unknown});
	auto r = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(r.is_ok());
	EXPECT_EQ(r.value().max_idle_timeout, 5000u);
}

TEST_F(TransportParamsDecodeUnknownParamTest, UnknownBetweenKnownIgnored)
{
	auto first = make_varint_param(
		quic::transport_param_id::max_idle_timeout, 5000);
	auto unknown = make_varint_param(0x9999, 42);
	auto second = make_varint_param(quic::transport_param_id::max_ack_delay, 100);
	auto buf = concat({first, unknown, second});
	auto r = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(r.is_ok());
	EXPECT_EQ(r.value().max_idle_timeout, 5000u);
	EXPECT_EQ(r.value().max_ack_delay, 100u);
}

TEST_F(TransportParamsDecodeUnknownParamTest, MultipleUnknownsIgnored)
{
	auto u1 = make_varint_param(0x9999, 1);
	auto u2 = make_varint_param(0xAAAA, 2);
	auto u3 = make_varint_param(0xBBBB, 3);
	auto known = make_varint_param(quic::transport_param_id::initial_max_data, 1024);
	auto buf = concat({u1, u2, u3, known});
	auto r = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(r.is_ok());
	EXPECT_EQ(r.value().initial_max_data, 1024u);
}

TEST_F(TransportParamsDecodeUnknownParamTest, UnknownWithNonEmptyOpaqueLengthIgnored)
{
	std::vector<uint8_t> opaque(4, 0xFF);
	auto unknown = make_param(0x9999,
		std::span<const uint8_t>(opaque.data(), opaque.size()));
	auto known = make_varint_param(
		quic::transport_param_id::max_idle_timeout, 5000);
	auto buf = concat({unknown, known});
	auto r = quic::transport_parameters::decode(as_span(buf));
	ASSERT_TRUE(r.is_ok());
	EXPECT_EQ(r.value().max_idle_timeout, 5000u);
}

// ============================================================================
// Validate boundary — server-only rejection and range checks
// ============================================================================

class TransportParamsValidateBoundaryTest : public ::testing::Test
{
};

TEST_F(TransportParamsValidateBoundaryTest, ClientRejectsOriginalDestinationConnectionId)
{
	quic::transport_parameters p;
	std::array<uint8_t, 8> cid_bytes{1, 2, 3, 4, 5, 6, 7, 8};
	p.original_destination_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
	EXPECT_TRUE(p.validate(false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, ClientRejectsRetrySourceConnectionId)
{
	quic::transport_parameters p;
	std::array<uint8_t, 8> cid_bytes{1, 2, 3, 4, 5, 6, 7, 8};
	p.retry_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
	EXPECT_TRUE(p.validate(false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, ClientRejectsStatelessResetToken)
{
	quic::transport_parameters p;
	std::array<uint8_t, 16> token{};
	p.stateless_reset_token = token;
	EXPECT_TRUE(p.validate(false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, ClientRejectsPreferredAddress)
{
	quic::transport_parameters p;
	p.preferred_address = quic::preferred_address_info{};
	EXPECT_TRUE(p.validate(false).is_err());
}

TEST_F(TransportParamsValidateBoundaryTest, ServerAcceptsServerOnlyFields)
{
	quic::transport_parameters p;
	std::array<uint8_t, 8> cid_bytes{1, 2, 3, 4, 5, 6, 7, 8};
	p.original_destination_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
	p.preferred_address = quic::preferred_address_info{};
	EXPECT_TRUE(p.validate(true).is_ok());
}

// ============================================================================
// apply_defaults() — partial-zero selective override
// ============================================================================

class TransportParamsApplyDefaultsPartialTest : public ::testing::Test
{
};

TEST_F(TransportParamsApplyDefaultsPartialTest, OnlyAckDelayExponentOverridden)
{
	quic::transport_parameters p;
	p.ack_delay_exponent = 0;
	p.max_ack_delay = 99;
	p.max_udp_payload_size = 12345;
	p.active_connection_id_limit = 5;
	p.apply_defaults();
	EXPECT_EQ(p.ack_delay_exponent, 3u);
	EXPECT_EQ(p.max_ack_delay, 99u);
	EXPECT_EQ(p.max_udp_payload_size, 12345u);
	EXPECT_EQ(p.active_connection_id_limit, 5u);
}

TEST_F(TransportParamsApplyDefaultsPartialTest, OnlyMaxAckDelayOverridden)
{
	quic::transport_parameters p;
	p.max_ack_delay = 0;
	p.ack_delay_exponent = 7;
	p.max_udp_payload_size = 12345;
	p.active_connection_id_limit = 5;
	p.apply_defaults();
	EXPECT_EQ(p.ack_delay_exponent, 7u);
	EXPECT_EQ(p.max_ack_delay, 25u);
	EXPECT_EQ(p.max_udp_payload_size, 12345u);
	EXPECT_EQ(p.active_connection_id_limit, 5u);
}

TEST_F(TransportParamsApplyDefaultsPartialTest, OnlyMaxUdpPayloadSizeOverridden)
{
	quic::transport_parameters p;
	p.max_udp_payload_size = 0;
	p.ack_delay_exponent = 7;
	p.max_ack_delay = 99;
	p.active_connection_id_limit = 5;
	p.apply_defaults();
	EXPECT_EQ(p.ack_delay_exponent, 7u);
	EXPECT_EQ(p.max_ack_delay, 99u);
	EXPECT_EQ(p.max_udp_payload_size, 65527u);
	EXPECT_EQ(p.active_connection_id_limit, 5u);
}

TEST_F(TransportParamsApplyDefaultsPartialTest, OnlyActiveConnectionIdLimitOverridden)
{
	quic::transport_parameters p;
	p.active_connection_id_limit = 0;
	p.ack_delay_exponent = 7;
	p.max_ack_delay = 99;
	p.max_udp_payload_size = 12345;
	p.apply_defaults();
	EXPECT_EQ(p.ack_delay_exponent, 7u);
	EXPECT_EQ(p.max_ack_delay, 99u);
	EXPECT_EQ(p.max_udp_payload_size, 12345u);
	EXPECT_EQ(p.active_connection_id_limit, 2u);
}

// ============================================================================
// Factory validation — cross-validation symmetry
// ============================================================================

class TransportParamsFactoryValidationTest : public ::testing::Test
{
};

TEST_F(TransportParamsFactoryValidationTest, ClientParamsPassClientValidate)
{
	auto p = quic::make_default_client_params();
	EXPECT_TRUE(p.validate(false).is_ok());
}

TEST_F(TransportParamsFactoryValidationTest, ServerParamsPassServerValidate)
{
	auto p = quic::make_default_server_params();
	EXPECT_TRUE(p.validate(true).is_ok());
}

TEST_F(TransportParamsFactoryValidationTest, ClientParamsPassServerValidate)
{
	// Client defaults contain no server-only fields, so server-side validate
	// should also accept them — asymmetry matters only if a side sneaks in
	// a forbidden field.
	auto p = quic::make_default_client_params();
	EXPECT_TRUE(p.validate(true).is_ok());
}

TEST_F(TransportParamsFactoryValidationTest, ClientFactoryRoundTrips)
{
	auto p = quic::make_default_client_params();
	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(decoded.value(), p);
}

// ============================================================================
// RoundTrip exotic — preferred_address with varied cid lengths
// ============================================================================

class TransportParamsRoundTripExoticTest : public ::testing::Test
{
};

TEST_F(TransportParamsRoundTripExoticTest, PreferredAddressEmptyConnectionId)
{
	quic::transport_parameters p;
	p.preferred_address = quic::preferred_address_info{};
	p.preferred_address->ipv4_port = 443;
	p.preferred_address->ipv6_port = 443;
	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	ASSERT_TRUE(decoded.value().preferred_address.has_value());
	EXPECT_TRUE(decoded.value().preferred_address->connection_id.empty());
	EXPECT_EQ(decoded.value().preferred_address->ipv4_port, 443u);
	EXPECT_EQ(decoded.value().preferred_address->ipv6_port, 443u);
}

TEST_F(TransportParamsRoundTripExoticTest, PreferredAddressMaxConnectionId)
{
	quic::transport_parameters p;
	std::array<uint8_t, 20> cid_bytes{};
	for (size_t i = 0; i < 20; ++i) cid_bytes[i] = static_cast<uint8_t>(0x10 + i);
	p.preferred_address = quic::preferred_address_info{};
	p.preferred_address->connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	ASSERT_TRUE(decoded.value().preferred_address.has_value());
	EXPECT_EQ(decoded.value().preferred_address->connection_id.length(), 20u);
}

TEST_F(TransportParamsRoundTripExoticTest, ConnectionIdAtMaxLength)
{
	quic::transport_parameters p;
	std::array<uint8_t, 20> cid_bytes{};
	for (size_t i = 0; i < 20; ++i) cid_bytes[i] = static_cast<uint8_t>(i * 3 + 1);
	p.initial_source_connection_id =
		quic::connection_id(std::span<const uint8_t>(cid_bytes.data(), cid_bytes.size()));
	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	ASSERT_TRUE(decoded.value().initial_source_connection_id.has_value());
	EXPECT_EQ(decoded.value().initial_source_connection_id->length(), 20u);
}

TEST_F(TransportParamsRoundTripExoticTest, MultipleVarintsAtMax4ByteBoundary)
{
	quic::transport_parameters p;
	p.initial_max_data = 1073741823;  // max 4-byte varint
	p.initial_max_streams_bidi = 1073741823;
	p.initial_max_streams_uni = 1073741823;
	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(decoded.value().initial_max_data, 1073741823u);
	EXPECT_EQ(decoded.value().initial_max_streams_bidi, 1073741823u);
	EXPECT_EQ(decoded.value().initial_max_streams_uni, 1073741823u);
}

TEST_F(TransportParamsRoundTripExoticTest, CombinedFieldsRoundTrip)
{
	quic::transport_parameters p;
	p.max_idle_timeout = 60000;
	p.ack_delay_exponent = 10;
	p.max_ack_delay = 200;
	p.max_udp_payload_size = 1452;
	p.initial_max_data = 1048576;
	p.initial_max_stream_data_bidi_local = 65535;
	p.initial_max_stream_data_bidi_remote = 65535;
	p.initial_max_stream_data_uni = 65535;
	p.initial_max_streams_bidi = 128;
	p.initial_max_streams_uni = 64;
	p.disable_active_migration = true;
	p.active_connection_id_limit = 4;
	auto encoded = p.encode();
	auto decoded = quic::transport_parameters::decode(as_span(encoded));
	ASSERT_TRUE(decoded.is_ok());
	EXPECT_EQ(decoded.value(), p);
}

// ============================================================================
// Decode duplicate parameter — second occurrence rejected
// ============================================================================

class TransportParamsDecodeDuplicateTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeDuplicateTest, DuplicateMaxIdleTimeoutRejected)
{
	auto first = make_varint_param(
		quic::transport_param_id::max_idle_timeout, 5000);
	auto second = make_varint_param(
		quic::transport_param_id::max_idle_timeout, 10000);
	auto buf = concat({first, second});
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeDuplicateTest, DuplicateAckDelayExponentRejected)
{
	auto first = make_varint_param(
		quic::transport_param_id::ack_delay_exponent, 3);
	auto second = make_varint_param(
		quic::transport_param_id::ack_delay_exponent, 5);
	auto buf = concat({first, second});
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeDuplicateTest, DuplicatePreferredAddressRejected)
{
	auto payload = build_preferred_address_payload(0, {});
	auto p1 = make_param(quic::transport_param_id::preferred_address,
		std::span<const uint8_t>(payload.data(), payload.size()));
	auto p2 = make_param(quic::transport_param_id::preferred_address,
		std::span<const uint8_t>(payload.data(), payload.size()));
	auto buf = concat({p1, p2});
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

// ============================================================================
// Decode structural — outer length / id malformation
// ============================================================================

class TransportParamsDecodeStructuralTest : public ::testing::Test
{
};

TEST_F(TransportParamsDecodeStructuralTest, EmptyBufferDecodesToDefault)
{
	std::vector<uint8_t> empty;
	auto r = quic::transport_parameters::decode(as_span(empty));
	ASSERT_TRUE(r.is_ok());
	EXPECT_EQ(r.value(), quic::transport_parameters{});
}

TEST_F(TransportParamsDecodeStructuralTest, TruncatedAfterIdRejected)
{
	// Only the parameter ID byte, no length varint.
	std::vector<uint8_t> buf{
		static_cast<uint8_t>(quic::transport_param_id::max_idle_timeout)};
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeStructuralTest, LengthExceedsRemainingRejected)
{
	// id=max_idle_timeout, length=100, but no value bytes supplied.
	std::vector<uint8_t> buf{
		static_cast<uint8_t>(quic::transport_param_id::max_idle_timeout),
		static_cast<uint8_t>(100)};
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}

TEST_F(TransportParamsDecodeStructuralTest, DisableActiveMigrationNonZeroLengthRejected)
{
	// disable_active_migration must be length 0; supplying 1-byte value is invalid.
	std::vector<uint8_t> payload{0x00};
	auto buf = make_param(quic::transport_param_id::disable_active_migration,
		std::span<const uint8_t>(payload.data(), payload.size()));
	EXPECT_TRUE(quic::transport_parameters::decode(as_span(buf)).is_err());
}
