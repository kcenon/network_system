/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file trace_context_test.cpp
 * @brief Unit tests for trace_context class
 *
 * Tests validate:
 * - Default construction (invalid context)
 * - Construction with valid trace/span IDs
 * - W3C traceparent header serialization and parsing
 * - Header propagation (to_headers / from_headers)
 * - Hex conversion utilities (bytes_to_hex, hex_to_bytes)
 * - ID generation (generate_trace_id, generate_span_id)
 * - Equality/inequality operators
 * - Thread-local context management
 * - Edge cases: zero IDs, malformed traceparent strings
 */

#include "kcenon/network/detail/tracing/trace_context.h"
#include "kcenon/network/detail/tracing/span.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::tracing;

// ============================================================================
// Construction Tests
// ============================================================================

class TraceContextConstructionTest : public ::testing::Test
{
};

TEST_F(TraceContextConstructionTest, DefaultConstructionIsInvalid)
{
	trace_context ctx;
	EXPECT_FALSE(ctx.is_valid());
	EXPECT_FALSE(ctx.is_sampled());
}

TEST_F(TraceContextConstructionTest, ConstructWithValidIds)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0xAB);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0xCD);

	trace_context ctx(tid, sid, trace_flags::sampled);
	EXPECT_TRUE(ctx.is_valid());
	EXPECT_TRUE(ctx.is_sampled());
	EXPECT_EQ(ctx.trace_id(), tid);
	EXPECT_EQ(ctx.span_id(), sid);
	EXPECT_FALSE(ctx.parent_span_id().has_value());
}

TEST_F(TraceContextConstructionTest, ConstructWithParentSpanId)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0x11);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0x22);
	span_id_t parent_sid{};
	std::fill(parent_sid.begin(), parent_sid.end(), 0x33);

	trace_context ctx(tid, sid, trace_flags::sampled, parent_sid);
	EXPECT_TRUE(ctx.is_valid());
	ASSERT_TRUE(ctx.parent_span_id().has_value());
	EXPECT_EQ(ctx.parent_span_id().value(), parent_sid);
}

TEST_F(TraceContextConstructionTest, ZeroTraceIdIsInvalid)
{
	trace_id_t tid{};
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0xAB);

	trace_context ctx(tid, sid, trace_flags::sampled);
	EXPECT_FALSE(ctx.is_valid());
}

TEST_F(TraceContextConstructionTest, ZeroSpanIdIsInvalid)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0xAB);
	span_id_t sid{};

	trace_context ctx(tid, sid, trace_flags::sampled);
	EXPECT_FALSE(ctx.is_valid());
}

TEST_F(TraceContextConstructionTest, NotSampledFlag)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0x01);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0x02);

	trace_context ctx(tid, sid, trace_flags::none);
	EXPECT_TRUE(ctx.is_valid());
	EXPECT_FALSE(ctx.is_sampled());
	EXPECT_EQ(ctx.flags(), trace_flags::none);
}

// ============================================================================
// Traceparent Serialization Tests
// ============================================================================

class TraceContextTraceparentTest : public ::testing::Test
{
};

TEST_F(TraceContextTraceparentTest, ToTraceparentFormat)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0xAB);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0xCD);

	trace_context ctx(tid, sid, trace_flags::sampled);
	auto traceparent = ctx.to_traceparent();

	// Format: 00-{32 hex}-{16 hex}-{2 hex}
	EXPECT_EQ(traceparent.size(), 55u);
	EXPECT_EQ(traceparent.substr(0, 3), "00-");
	EXPECT_EQ(traceparent.substr(3, 32), "abababababababababababababababab");
	EXPECT_EQ(traceparent[35], '-');
	EXPECT_EQ(traceparent.substr(36, 16), "cdcdcdcdcdcdcdcd");
	EXPECT_EQ(traceparent[52], '-');
	EXPECT_EQ(traceparent.substr(53, 2), "01");
}

TEST_F(TraceContextTraceparentTest, ToTraceparentNotSampled)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0x01);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0x02);

	trace_context ctx(tid, sid, trace_flags::none);
	auto traceparent = ctx.to_traceparent();

	EXPECT_EQ(traceparent.substr(53, 2), "00");
}

TEST_F(TraceContextTraceparentTest, InvalidContextReturnsEmpty)
{
	trace_context ctx;
	EXPECT_TRUE(ctx.to_traceparent().empty());
}

TEST_F(TraceContextTraceparentTest, RoundTripParsing)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0xAB);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0xCD);

	trace_context original(tid, sid, trace_flags::sampled);
	auto traceparent = original.to_traceparent();

	auto parsed = trace_context::from_traceparent(traceparent);
	EXPECT_TRUE(parsed.is_valid());
	EXPECT_EQ(parsed.trace_id(), original.trace_id());
	EXPECT_EQ(parsed.span_id(), original.span_id());
	EXPECT_EQ(parsed.is_sampled(), original.is_sampled());
}

TEST_F(TraceContextTraceparentTest, ParseValidTraceparent)
{
	auto ctx = trace_context::from_traceparent(
		"00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01");

	EXPECT_TRUE(ctx.is_valid());
	EXPECT_TRUE(ctx.is_sampled());
	EXPECT_EQ(ctx.trace_id_hex(), "0af7651916cd43dd8448eb211c80319c");
	EXPECT_EQ(ctx.span_id_hex(), "b7ad6b7169203331");
}

TEST_F(TraceContextTraceparentTest, ParseNotSampledTraceparent)
{
	auto ctx = trace_context::from_traceparent(
		"00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-00");

	EXPECT_TRUE(ctx.is_valid());
	EXPECT_FALSE(ctx.is_sampled());
}

// ============================================================================
// Traceparent Parsing Edge Cases
// ============================================================================

class TraceContextParsingEdgeCaseTest : public ::testing::Test
{
};

TEST_F(TraceContextParsingEdgeCaseTest, TooShortString)
{
	auto ctx = trace_context::from_traceparent("00-abc-def-01");
	EXPECT_FALSE(ctx.is_valid());
}

TEST_F(TraceContextParsingEdgeCaseTest, EmptyString)
{
	auto ctx = trace_context::from_traceparent("");
	EXPECT_FALSE(ctx.is_valid());
}

TEST_F(TraceContextParsingEdgeCaseTest, WrongVersion)
{
	auto ctx = trace_context::from_traceparent(
		"01-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01");
	EXPECT_FALSE(ctx.is_valid());
}

TEST_F(TraceContextParsingEdgeCaseTest, InvalidHexCharacters)
{
	auto ctx = trace_context::from_traceparent(
		"00-ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ-b7ad6b7169203331-01");
	EXPECT_FALSE(ctx.is_valid());
}

TEST_F(TraceContextParsingEdgeCaseTest, MissingSeparator)
{
	auto ctx = trace_context::from_traceparent(
		"00-0af7651916cd43dd8448eb211c80319cb7ad6b7169203331-01");
	EXPECT_FALSE(ctx.is_valid());
}

// ============================================================================
// Header Propagation Tests
// ============================================================================

class TraceContextHeaderTest : public ::testing::Test
{
};

TEST_F(TraceContextHeaderTest, ToHeaders)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0x01);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0x02);

	trace_context ctx(tid, sid, trace_flags::sampled);
	auto headers = ctx.to_headers();

	ASSERT_EQ(headers.size(), 1u);
	EXPECT_EQ(headers[0].first, "traceparent");
	EXPECT_FALSE(headers[0].second.empty());
}

TEST_F(TraceContextHeaderTest, InvalidContextEmptyHeaders)
{
	trace_context ctx;
	auto headers = ctx.to_headers();
	EXPECT_TRUE(headers.empty());
}

TEST_F(TraceContextHeaderTest, FromHeadersRoundTrip)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0xAA);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0xBB);

	trace_context original(tid, sid, trace_flags::sampled);
	auto headers = original.to_headers();

	auto parsed = trace_context::from_headers(headers);
	EXPECT_TRUE(parsed.is_valid());
	EXPECT_EQ(parsed.trace_id(), original.trace_id());
	EXPECT_EQ(parsed.span_id(), original.span_id());
}

TEST_F(TraceContextHeaderTest, FromHeadersCaseInsensitive)
{
	std::vector<std::pair<std::string, std::string>> headers = {
		{"Traceparent",
		 "00-0af7651916cd43dd8448eb211c80319c-b7ad6b7169203331-01"}};

	// The implementation lowercases the header name, so this should not match
	// because it compares against "traceparent" (all lowercase)
	// Actually looking at the code, it does lowercase comparison
	auto ctx = trace_context::from_headers(headers);
	// The implementation converts to lowercase, so "Traceparent" -> "traceparent" matches
	EXPECT_TRUE(ctx.is_valid());
}

TEST_F(TraceContextHeaderTest, FromHeadersNoTraceparent)
{
	std::vector<std::pair<std::string, std::string>> headers = {
		{"content-type", "application/json"},
		{"authorization", "Bearer token"}};

	auto ctx = trace_context::from_headers(headers);
	EXPECT_FALSE(ctx.is_valid());
}

// ============================================================================
// Hex Conversion Tests
// ============================================================================

class HexConversionTest : public ::testing::Test
{
};

TEST_F(HexConversionTest, BytesToHex)
{
	uint8_t data[] = {0x0A, 0xF7, 0x65, 0x19};
	auto hex = bytes_to_hex(data, 4);
	EXPECT_EQ(hex, "0af76519");
}

TEST_F(HexConversionTest, BytesToHexEmpty)
{
	auto hex = bytes_to_hex(nullptr, 0);
	EXPECT_TRUE(hex.empty());
}

TEST_F(HexConversionTest, BytesToHexAllZeros)
{
	uint8_t data[] = {0x00, 0x00, 0x00};
	auto hex = bytes_to_hex(data, 3);
	EXPECT_EQ(hex, "000000");
}

TEST_F(HexConversionTest, BytesToHexAllFF)
{
	uint8_t data[] = {0xFF, 0xFF};
	auto hex = bytes_to_hex(data, 2);
	EXPECT_EQ(hex, "ffff");
}

TEST_F(HexConversionTest, HexToBytesValid)
{
	uint8_t out[4] = {};
	bool result = hex_to_bytes("0af76519", out, 4);
	EXPECT_TRUE(result);
	EXPECT_EQ(out[0], 0x0A);
	EXPECT_EQ(out[1], 0xF7);
	EXPECT_EQ(out[2], 0x65);
	EXPECT_EQ(out[3], 0x19);
}

TEST_F(HexConversionTest, HexToBytesUpperCase)
{
	uint8_t out[2] = {};
	bool result = hex_to_bytes("ABCD", out, 2);
	EXPECT_TRUE(result);
	EXPECT_EQ(out[0], 0xAB);
	EXPECT_EQ(out[1], 0xCD);
}

TEST_F(HexConversionTest, HexToBytesWrongLength)
{
	uint8_t out[4] = {};
	bool result = hex_to_bytes("0af7", out, 4);
	EXPECT_FALSE(result);
}

TEST_F(HexConversionTest, HexToBytesInvalidChars)
{
	uint8_t out[2] = {};
	bool result = hex_to_bytes("ZZZZ", out, 2);
	EXPECT_FALSE(result);
}

// ============================================================================
// ID Generation Tests
// ============================================================================

class TraceIdGenerationTest : public ::testing::Test
{
};

TEST_F(TraceIdGenerationTest, GenerateTraceIdNonZero)
{
	auto id = generate_trace_id();
	bool all_zero = std::all_of(id.begin(), id.end(),
								[](uint8_t b) { return b == 0; });
	// Statistically negligible chance of all zeros from random generation
	EXPECT_FALSE(all_zero);
}

TEST_F(TraceIdGenerationTest, GenerateSpanIdNonZero)
{
	auto id = generate_span_id();
	bool all_zero = std::all_of(id.begin(), id.end(),
								[](uint8_t b) { return b == 0; });
	EXPECT_FALSE(all_zero);
}

TEST_F(TraceIdGenerationTest, GenerateUniqueTraceIds)
{
	std::set<std::string> ids;
	for (int i = 0; i < 100; ++i)
	{
		auto id = generate_trace_id();
		ids.insert(bytes_to_hex(id.data(), id.size()));
	}
	// All 100 IDs should be unique
	EXPECT_EQ(ids.size(), 100u);
}

TEST_F(TraceIdGenerationTest, GenerateUniqueSpanIds)
{
	std::set<std::string> ids;
	for (int i = 0; i < 100; ++i)
	{
		auto id = generate_span_id();
		ids.insert(bytes_to_hex(id.data(), id.size()));
	}
	EXPECT_EQ(ids.size(), 100u);
}

// ============================================================================
// Equality Tests
// ============================================================================

class TraceContextEqualityTest : public ::testing::Test
{
};

TEST_F(TraceContextEqualityTest, EqualContexts)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0x01);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0x02);

	trace_context ctx1(tid, sid, trace_flags::sampled);
	trace_context ctx2(tid, sid, trace_flags::sampled);

	EXPECT_EQ(ctx1, ctx2);
	EXPECT_FALSE(ctx1 != ctx2);
}

TEST_F(TraceContextEqualityTest, DifferentTraceId)
{
	trace_id_t tid1{};
	std::fill(tid1.begin(), tid1.end(), 0x01);
	trace_id_t tid2{};
	std::fill(tid2.begin(), tid2.end(), 0x02);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0x03);

	trace_context ctx1(tid1, sid, trace_flags::sampled);
	trace_context ctx2(tid2, sid, trace_flags::sampled);

	EXPECT_NE(ctx1, ctx2);
}

TEST_F(TraceContextEqualityTest, DifferentFlags)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0x01);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0x02);

	trace_context ctx1(tid, sid, trace_flags::sampled);
	trace_context ctx2(tid, sid, trace_flags::none);

	EXPECT_NE(ctx1, ctx2);
}

TEST_F(TraceContextEqualityTest, InvalidContextsAreEqual)
{
	trace_context ctx1;
	trace_context ctx2;
	EXPECT_EQ(ctx1, ctx2);
}

// ============================================================================
// Thread-Local Context Tests
// ============================================================================

class TraceContextThreadLocalTest : public ::testing::Test
{
protected:
	void TearDown() override
	{
		// Ensure clean thread-local state after each test
		shutdown_tracing();
	}
};

TEST_F(TraceContextThreadLocalTest, CurrentDefaultIsInvalid)
{
	auto ctx = trace_context::current();
	EXPECT_FALSE(ctx.is_valid());
}

TEST_F(TraceContextThreadLocalTest, CreateSpanSetsCurrentContext)
{
	auto s = trace_context::create_span("test_span");
	auto ctx = trace_context::current();

	EXPECT_TRUE(ctx.is_valid());
	EXPECT_EQ(ctx.trace_id(), s.context().trace_id());
}

TEST_F(TraceContextThreadLocalTest, SpanDestructionRestoresContext)
{
	{
		auto s = trace_context::create_span("outer");
		auto outer_ctx = trace_context::current();
		EXPECT_TRUE(outer_ctx.is_valid());
	}
	// After span destruction, context should be cleared
	auto ctx = trace_context::current();
	EXPECT_FALSE(ctx.is_valid());
}

TEST_F(TraceContextThreadLocalTest, ChildSpanSharesTraceId)
{
	auto parent = trace_context::create_span("parent");
	auto parent_ctx = parent.context();

	auto child = parent_ctx.create_child_span("child");
	auto child_ctx = child.context();

	EXPECT_EQ(parent_ctx.trace_id(), child_ctx.trace_id());
	EXPECT_NE(parent_ctx.span_id(), child_ctx.span_id());
	ASSERT_TRUE(child_ctx.parent_span_id().has_value());
	EXPECT_EQ(child_ctx.parent_span_id().value(), parent_ctx.span_id());
}

TEST_F(TraceContextThreadLocalTest, ThreadIsolation)
{
	auto main_span = trace_context::create_span("main_span");
	auto main_trace_id = main_span.context().trace_id();

	trace_id_t thread_trace_id{};
	bool thread_had_valid_ctx = false;

	std::thread t([&]() {
		// Thread should start with invalid context (thread-local)
		auto ctx = trace_context::current();
		thread_had_valid_ctx = ctx.is_valid();

		auto thread_span = trace_context::create_span("thread_span");
		thread_trace_id = thread_span.context().trace_id();
	});
	t.join();

	EXPECT_FALSE(thread_had_valid_ctx);
	// Different thread should have a different trace ID
	EXPECT_NE(main_trace_id, thread_trace_id);
}

// ============================================================================
// Hex String Tests
// ============================================================================

class TraceContextHexStringTest : public ::testing::Test
{
};

TEST_F(TraceContextHexStringTest, TraceIdHex)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0xAB);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0xCD);

	trace_context ctx(tid, sid, trace_flags::sampled);
	EXPECT_EQ(ctx.trace_id_hex(), "abababababababababababababababab");
	EXPECT_EQ(ctx.trace_id_hex().size(), 32u);
}

TEST_F(TraceContextHexStringTest, SpanIdHex)
{
	trace_id_t tid{};
	std::fill(tid.begin(), tid.end(), 0x01);
	span_id_t sid{};
	std::fill(sid.begin(), sid.end(), 0xEF);

	trace_context ctx(tid, sid, trace_flags::none);
	EXPECT_EQ(ctx.span_id_hex(), "efefefefefefefef");
	EXPECT_EQ(ctx.span_id_hex().size(), 16u);
}
