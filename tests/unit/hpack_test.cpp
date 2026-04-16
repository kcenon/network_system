/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/http2/hpack.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace http2 = kcenon::network::protocols::http2;

/**
 * @file hpack_test.cpp
 * @brief Unit tests for HPACK header compression (RFC 7541)
 *
 * Tests validate:
 * - http_header struct construction and size calculation
 * - static_table lookup by index and name
 * - dynamic_table insertion, retrieval, eviction, and size tracking
 * - hpack_encoder construction and table size management
 * - hpack_decoder construction and table size management
 * - Encode/decode round-trip for simple headers
 * - huffman encode/decode round-trip and encoded_size
 * - Error paths (invalid indices, empty data)
 */

// ============================================================================
// http_header Tests
// ============================================================================

class HttpHeaderTest : public ::testing::Test
{
};

TEST_F(HttpHeaderTest, DefaultConstruction)
{
	http2::http_header h;
	EXPECT_TRUE(h.name.empty());
	EXPECT_TRUE(h.value.empty());
}

TEST_F(HttpHeaderTest, ParameterizedConstruction)
{
	http2::http_header h("content-type", "application/json");
	EXPECT_EQ(h.name, "content-type");
	EXPECT_EQ(h.value, "application/json");
}

TEST_F(HttpHeaderTest, SizeCalculation)
{
	// RFC 7541: size = name.size() + value.size() + 32
	http2::http_header h("content-type", "text/html");
	EXPECT_EQ(h.size(), std::string("content-type").size()
							 + std::string("text/html").size() + 32);
}

TEST_F(HttpHeaderTest, EmptyHeaderSize)
{
	http2::http_header h;
	EXPECT_EQ(h.size(), 32u);
}

TEST_F(HttpHeaderTest, Equality)
{
	http2::http_header a("name", "value");
	http2::http_header b("name", "value");
	http2::http_header c("name", "other");
	EXPECT_EQ(a, b);
	EXPECT_NE(a, c);
}

// ============================================================================
// static_table Tests
// ============================================================================

class StaticTableTest : public ::testing::Test
{
};

TEST_F(StaticTableTest, Size)
{
	EXPECT_EQ(http2::static_table::size(), 61u);
}

TEST_F(StaticTableTest, GetValidIndex)
{
	// Index 1 is :authority (RFC 7541 Appendix A)
	auto entry = http2::static_table::get(1);
	ASSERT_TRUE(entry.has_value());
	EXPECT_EQ(entry->name, ":authority");
}

TEST_F(StaticTableTest, GetIndex2)
{
	// Index 2 is :method GET
	auto entry = http2::static_table::get(2);
	ASSERT_TRUE(entry.has_value());
	EXPECT_EQ(entry->name, ":method");
	EXPECT_EQ(entry->value, "GET");
}

TEST_F(StaticTableTest, GetIndex3)
{
	// Index 3 is :method POST
	auto entry = http2::static_table::get(3);
	ASSERT_TRUE(entry.has_value());
	EXPECT_EQ(entry->name, ":method");
	EXPECT_EQ(entry->value, "POST");
}

TEST_F(StaticTableTest, GetIndexZero)
{
	// Index 0 is invalid
	auto entry = http2::static_table::get(0);
	EXPECT_FALSE(entry.has_value());
}

TEST_F(StaticTableTest, GetIndexOutOfRange)
{
	auto entry = http2::static_table::get(62);
	EXPECT_FALSE(entry.has_value());
}

TEST_F(StaticTableTest, GetIndexMax)
{
	// Index 61 should be valid (last entry)
	auto entry = http2::static_table::get(61);
	EXPECT_TRUE(entry.has_value());
}

TEST_F(StaticTableTest, FindByNameAndValue)
{
	// :method GET is at index 2
	auto index = http2::static_table::find(":method", "GET");
	EXPECT_EQ(index, 2u);
}

TEST_F(StaticTableTest, FindByNameOnly)
{
	auto index = http2::static_table::find(":authority");
	EXPECT_GT(index, 0u);
}

TEST_F(StaticTableTest, FindNotFound)
{
	auto index = http2::static_table::find("x-nonexistent-header", "value");
	EXPECT_EQ(index, 0u);
}

// ============================================================================
// dynamic_table Tests
// ============================================================================

class DynamicTableTest : public ::testing::Test
{
protected:
	http2::dynamic_table table_{4096};
};

TEST_F(DynamicTableTest, InitialState)
{
	EXPECT_EQ(table_.current_size(), 0u);
	EXPECT_EQ(table_.max_size(), 4096u);
	EXPECT_EQ(table_.entry_count(), 0u);
}

TEST_F(DynamicTableTest, InsertAndGet)
{
	table_.insert("custom-header", "custom-value");
	EXPECT_EQ(table_.entry_count(), 1u);
	EXPECT_GT(table_.current_size(), 0u);

	auto entry = table_.get(0);
	ASSERT_TRUE(entry.has_value());
	EXPECT_EQ(entry->name, "custom-header");
	EXPECT_EQ(entry->value, "custom-value");
}

TEST_F(DynamicTableTest, InsertMultiple)
{
	table_.insert("header-a", "value-a");
	table_.insert("header-b", "value-b");
	EXPECT_EQ(table_.entry_count(), 2u);

	// Most recently inserted should be at index 0
	auto entry0 = table_.get(0);
	ASSERT_TRUE(entry0.has_value());
	EXPECT_EQ(entry0->name, "header-b");

	auto entry1 = table_.get(1);
	ASSERT_TRUE(entry1.has_value());
	EXPECT_EQ(entry1->name, "header-a");
}

TEST_F(DynamicTableTest, GetInvalidIndex)
{
	auto entry = table_.get(0);
	EXPECT_FALSE(entry.has_value());
}

TEST_F(DynamicTableTest, FindByNameAndValue)
{
	table_.insert("x-custom", "hello");
	auto index = table_.find("x-custom", "hello");
	ASSERT_TRUE(index.has_value());
	EXPECT_EQ(*index, 0u);
}

TEST_F(DynamicTableTest, FindByNameOnly)
{
	table_.insert("x-custom", "hello");
	auto index = table_.find("x-custom");
	ASSERT_TRUE(index.has_value());
	EXPECT_EQ(*index, 0u);
}

TEST_F(DynamicTableTest, FindNotFound)
{
	auto index = table_.find("nonexistent");
	EXPECT_FALSE(index.has_value());
}

TEST_F(DynamicTableTest, SetMaxSize)
{
	table_.set_max_size(2048);
	EXPECT_EQ(table_.max_size(), 2048u);
}

TEST_F(DynamicTableTest, SetMaxSizeZero)
{
	table_.insert("header", "value");
	EXPECT_GT(table_.entry_count(), 0u);

	table_.set_max_size(0);
	EXPECT_EQ(table_.entry_count(), 0u);
	EXPECT_EQ(table_.current_size(), 0u);
}

TEST_F(DynamicTableTest, EvictionOnInsert)
{
	// Use a very small table to force eviction
	http2::dynamic_table small_table(64);
	// Each entry overhead is 32 bytes + name + value size
	small_table.insert("a", "b");  // 32 + 1 + 1 = 34 bytes
	EXPECT_EQ(small_table.entry_count(), 1u);

	small_table.insert("c", "d");  // 34 + 34 = 68 > 64, should evict first
	EXPECT_EQ(small_table.entry_count(), 1u);

	auto entry = small_table.get(0);
	ASSERT_TRUE(entry.has_value());
	EXPECT_EQ(entry->name, "c");
}

TEST_F(DynamicTableTest, Clear)
{
	table_.insert("header", "value");
	EXPECT_GT(table_.entry_count(), 0u);

	table_.clear();
	EXPECT_EQ(table_.entry_count(), 0u);
	EXPECT_EQ(table_.current_size(), 0u);
}

TEST_F(DynamicTableTest, CustomMaxSize)
{
	http2::dynamic_table custom(8192);
	EXPECT_EQ(custom.max_size(), 8192u);
}

TEST_F(DynamicTableTest, DefaultMaxSize)
{
	http2::dynamic_table default_table;
	EXPECT_EQ(default_table.max_size(), 4096u);
}

// ============================================================================
// hpack_encoder Tests
// ============================================================================

class HpackEncoderTest : public ::testing::Test
{
protected:
	http2::hpack_encoder encoder_;
};

TEST_F(HpackEncoderTest, DefaultConstruction)
{
	EXPECT_EQ(encoder_.table_size(), 0u);
}

TEST_F(HpackEncoderTest, CustomTableSize)
{
	http2::hpack_encoder custom_encoder(8192);
	EXPECT_EQ(custom_encoder.table_size(), 0u);
}

TEST_F(HpackEncoderTest, EncodeEmptyHeaders)
{
	std::vector<http2::http_header> headers;
	auto encoded = encoder_.encode(headers);
	EXPECT_TRUE(encoded.empty());
}

TEST_F(HpackEncoderTest, EncodeSingleHeader)
{
	std::vector<http2::http_header> headers = {
		{":method", "GET"}};
	auto encoded = encoder_.encode(headers);
	EXPECT_FALSE(encoded.empty());
}

TEST_F(HpackEncoderTest, EncodeMultipleHeaders)
{
	std::vector<http2::http_header> headers = {
		{":method", "GET"},
		{":path", "/"},
		{":scheme", "https"},
		{"content-type", "text/html"}};
	auto encoded = encoder_.encode(headers);
	EXPECT_FALSE(encoded.empty());
}

TEST_F(HpackEncoderTest, SetMaxTableSize)
{
	encoder_.set_max_table_size(2048);
	// After encoding, the table size limit should be applied
	std::vector<http2::http_header> headers = {
		{"x-custom", "value"}};
	auto encoded = encoder_.encode(headers);
	EXPECT_FALSE(encoded.empty());
}

// ============================================================================
// hpack_decoder Tests
// ============================================================================

class HpackDecoderTest : public ::testing::Test
{
protected:
	http2::hpack_decoder decoder_;
};

TEST_F(HpackDecoderTest, DefaultConstruction)
{
	EXPECT_EQ(decoder_.table_size(), 0u);
}

TEST_F(HpackDecoderTest, CustomTableSize)
{
	http2::hpack_decoder custom_decoder(8192);
	EXPECT_EQ(custom_decoder.table_size(), 0u);
}

TEST_F(HpackDecoderTest, DecodeEmptyData)
{
	std::vector<uint8_t> empty_data;
	std::span<const uint8_t> span(empty_data);
	auto result = decoder_.decode(span);
	ASSERT_TRUE(result);
	EXPECT_TRUE(result.value().empty());
}

TEST_F(HpackDecoderTest, SetMaxTableSize)
{
	decoder_.set_max_table_size(2048);
	// Should not fail
	EXPECT_EQ(decoder_.table_size(), 0u);
}

// ============================================================================
// Encoder-Decoder Round-Trip Tests
// ============================================================================

class HpackRoundTripTest : public ::testing::Test
{
protected:
	http2::hpack_encoder encoder_;
	http2::hpack_decoder decoder_;
};

TEST_F(HpackRoundTripTest, StaticTableHeader)
{
	std::vector<http2::http_header> original = {
		{":method", "GET"}};

	auto encoded = encoder_.encode(original);
	ASSERT_FALSE(encoded.empty());

	std::span<const uint8_t> span(encoded);
	auto result = decoder_.decode(span);
	ASSERT_TRUE(result);
	ASSERT_EQ(result.value().size(), 1u);
	EXPECT_EQ(result.value()[0].name, ":method");
	EXPECT_EQ(result.value()[0].value, "GET");
}

TEST_F(HpackRoundTripTest, CustomHeader)
{
	std::vector<http2::http_header> original = {
		{"x-custom-header", "custom-value"}};

	auto encoded = encoder_.encode(original);
	ASSERT_FALSE(encoded.empty());

	std::span<const uint8_t> span(encoded);
	auto result = decoder_.decode(span);
	ASSERT_TRUE(result);
	ASSERT_EQ(result.value().size(), 1u);
	EXPECT_EQ(result.value()[0].name, "x-custom-header");
	EXPECT_EQ(result.value()[0].value, "custom-value");
}

TEST_F(HpackRoundTripTest, MultipleHeaders)
{
	std::vector<http2::http_header> original = {
		{":method", "POST"},
		{":path", "/api/data"},
		{":scheme", "https"},
		{"content-type", "application/json"},
		{"x-request-id", "abc-123"}};

	auto encoded = encoder_.encode(original);
	ASSERT_FALSE(encoded.empty());

	std::span<const uint8_t> span(encoded);
	auto result = decoder_.decode(span);
	ASSERT_TRUE(result);
	ASSERT_EQ(result.value().size(), original.size());

	for (size_t i = 0; i < original.size(); ++i)
	{
		EXPECT_EQ(result.value()[i].name, original[i].name);
		EXPECT_EQ(result.value()[i].value, original[i].value);
	}
}

// ============================================================================
// Huffman Encoding Tests
// ============================================================================

class HuffmanTest : public ::testing::Test
{
};

TEST_F(HuffmanTest, EncodeEmptyString)
{
	auto encoded = http2::huffman::encode("");
	EXPECT_TRUE(encoded.empty());
}

TEST_F(HuffmanTest, EncodeNonEmpty)
{
	auto encoded = http2::huffman::encode("www.example.com");
	EXPECT_FALSE(encoded.empty());
}

TEST_F(HuffmanTest, EncodedSizeEmpty)
{
	EXPECT_EQ(http2::huffman::encoded_size(""), 0u);
}

TEST_F(HuffmanTest, EncodedSizeNonEmpty)
{
	auto size = http2::huffman::encoded_size("www.example.com");
	EXPECT_GT(size, 0u);
	// Huffman encoding should typically be smaller than or equal to raw
	EXPECT_LE(size, std::string("www.example.com").size());
}

TEST_F(HuffmanTest, DecodeEmptyData)
{
	std::vector<uint8_t> empty_data;
	std::span<const uint8_t> span(empty_data);
	auto result = http2::huffman::decode(span);
	ASSERT_TRUE(result);
	EXPECT_TRUE(result.value().empty());
}

TEST_F(HuffmanTest, RoundTrip)
{
	std::string original = "www.example.com";
	auto encoded = http2::huffman::encode(original);
	ASSERT_FALSE(encoded.empty());

	std::span<const uint8_t> span(encoded);
	auto result = http2::huffman::decode(span);
	ASSERT_TRUE(result);
	EXPECT_EQ(result.value(), original);
}

TEST_F(HuffmanTest, RoundTripVariousStrings)
{
	std::vector<std::string> test_strings = {
		"Hello",
		"GET",
		"content-type",
		"application/json",
		"/api/v1/users",
		"Mozilla/5.0"};

	for (const auto& str : test_strings)
	{
		auto encoded = http2::huffman::encode(str);
		std::span<const uint8_t> span(encoded);
		auto result = http2::huffman::decode(span);
		ASSERT_TRUE(result) << "Failed for: " << str;
		EXPECT_EQ(result.value(), str) << "Mismatch for: " << str;
	}
}
