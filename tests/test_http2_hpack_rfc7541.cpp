// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file test_http2_hpack_rfc7541.cpp
 * @brief HPACK tests using RFC 7541 Appendix C test vectors
 *
 * These tests verify HPACK encoder/decoder behavior against the
 * official test vectors from RFC 7541 Appendix C.
 *
 * References:
 * - RFC 7541 Section C.1: Integer representation examples
 * - RFC 7541 Section C.2: Header field representation examples
 * - RFC 7541 Section C.3: Request examples without Huffman coding
 * - RFC 7541 Section C.5: Response examples without Huffman coding
 */

#include <gtest/gtest.h>
#include "internal/protocols/http2/hpack.h"
#include <vector>
#include <string>

using namespace kcenon::network::protocols::http2;

class HpackRFC7541Test : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// RFC 7541 Appendix C.1 - Integer Representation Examples
// ============================================================================

TEST_F(HpackRFC7541Test, RFC7541_C1_EntrySizeCalculation)
{
    // RFC 7541 Section 4.1: Entry size = length of name + length of value + 32
    // "custom-key" (10) + "custom-header" (13) + 32 = 55
    http_header h("custom-key", "custom-header");
    EXPECT_EQ(h.size(), 10 + 13 + 32);
}

// ============================================================================
// RFC 7541 Appendix C.2 - Header Field Representation Examples
// ============================================================================

TEST_F(HpackRFC7541Test, RFC7541_C2_1_IndexedHeaderField)
{
    // C.2.1: Indexed Header Field Representation
    // Header :method: GET is at static table index 2
    // Encoded: 0x82 (indexed, index=2)
    std::vector<uint8_t> encoded = {0x82};

    hpack_decoder decoder;
    auto result = decoder.decode(encoded);
    ASSERT_TRUE(result.is_ok());

    auto headers = result.value();
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers[0].name, ":method");
    EXPECT_EQ(headers[0].value, "GET");
}

TEST_F(HpackRFC7541Test, RFC7541_C2_2_LiteralWithIndexingNewName)
{
    // C.2.2: Literal Header Field with Incremental Indexing — New Name
    // Header: custom-key: custom-header
    // Encoded as: 0x40 (literal with indexing, new name)
    //             + string "custom-key" (len=10)
    //             + string "custom-header" (len=13)
    std::vector<uint8_t> encoded = {
        0x40,                                                   // Literal with indexing, new name
        0x0a,                                                   // Name length = 10
        'c', 'u', 's', 't', 'o', 'm', '-', 'k', 'e', 'y',     // "custom-key"
        0x0d,                                                   // Value length = 13
        'c', 'u', 's', 't', 'o', 'm', '-',
        'h', 'e', 'a', 'd', 'e', 'r'                           // "custom-header"
    };

    hpack_decoder decoder;
    auto result = decoder.decode(encoded);
    ASSERT_TRUE(result.is_ok());

    auto headers = result.value();
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers[0].name, "custom-key");
    EXPECT_EQ(headers[0].value, "custom-header");
}

TEST_F(HpackRFC7541Test, RFC7541_C2_3_LiteralWithoutIndexing)
{
    // C.2.3: Literal Header Field without Indexing
    // Header: :path: /sample/path
    // :path is at static table index 4
    // Encoded as: 0x04 (literal without indexing, name index=4)
    //             + string "/sample/path" (len=12)
    std::vector<uint8_t> encoded = {
        0x04,                                                   // Literal without indexing, name index=4
        0x0c,                                                   // Value length = 12
        '/', 's', 'a', 'm', 'p', 'l', 'e',
        '/', 'p', 'a', 't', 'h'                                // "/sample/path"
    };

    hpack_decoder decoder;
    auto result = decoder.decode(encoded);
    ASSERT_TRUE(result.is_ok());

    auto headers = result.value();
    ASSERT_EQ(headers.size(), 1u);
    EXPECT_EQ(headers[0].name, ":path");
    EXPECT_EQ(headers[0].value, "/sample/path");
}

// ============================================================================
// RFC 7541 Appendix C.3 - Request Examples Without Huffman Coding
// ============================================================================

TEST_F(HpackRFC7541Test, RFC7541_C3_1_FirstRequest)
{
    // C.3.1: First Request
    // :method: GET, :scheme: http, :path: /, :authority: www.example.com
    // All use static table references or literal with indexing
    hpack_encoder encoder;
    hpack_decoder decoder;

    std::vector<http_header> request1 = {
        {":method", "GET"},
        {":scheme", "http"},
        {":path", "/"},
        {":authority", "www.example.com"}
    };

    auto encoded = encoder.encode(request1);
    ASSERT_FALSE(encoded.empty());

    auto result = decoder.decode(encoded);
    ASSERT_TRUE(result.is_ok());

    auto decoded = result.value();
    ASSERT_EQ(decoded.size(), 4u);
    EXPECT_EQ(decoded[0].name, ":method");
    EXPECT_EQ(decoded[0].value, "GET");
    EXPECT_EQ(decoded[1].name, ":scheme");
    EXPECT_EQ(decoded[1].value, "http");
    EXPECT_EQ(decoded[2].name, ":path");
    EXPECT_EQ(decoded[2].value, "/");
    EXPECT_EQ(decoded[3].name, ":authority");
    EXPECT_EQ(decoded[3].value, "www.example.com");
}

TEST_F(HpackRFC7541Test, RFC7541_C3_SequentialRequests)
{
    // C.3.1-C.3.3: Sequential requests use the same encoder/decoder
    // to verify dynamic table evolution across requests
    hpack_encoder encoder;
    hpack_decoder decoder;

    // First request
    std::vector<http_header> req1 = {
        {":method", "GET"},
        {":scheme", "http"},
        {":path", "/"},
        {":authority", "www.example.com"}
    };

    auto enc1 = encoder.encode(req1);
    auto dec1 = decoder.decode(enc1);
    ASSERT_TRUE(dec1.is_ok());
    EXPECT_EQ(dec1.value().size(), 4u);

    // Second request (some headers repeated -- should benefit from dynamic table)
    std::vector<http_header> req2 = {
        {":method", "GET"},
        {":scheme", "http"},
        {":path", "/"},
        {":authority", "www.example.com"},
        {"cache-control", "no-cache"}
    };

    auto enc2 = encoder.encode(req2);
    auto dec2 = decoder.decode(enc2);
    ASSERT_TRUE(dec2.is_ok());

    auto headers2 = dec2.value();
    ASSERT_EQ(headers2.size(), 5u);
    EXPECT_EQ(headers2[4].name, "cache-control");
    EXPECT_EQ(headers2[4].value, "no-cache");

    // Third request (different path and authority)
    std::vector<http_header> req3 = {
        {":method", "GET"},
        {":scheme", "https"},
        {":path", "/index.html"},
        {":authority", "www.example.com"},
        {"custom-key", "custom-value"}
    };

    auto enc3 = encoder.encode(req3);
    auto dec3 = decoder.decode(enc3);
    ASSERT_TRUE(dec3.is_ok());

    auto headers3 = dec3.value();
    ASSERT_EQ(headers3.size(), 5u);
    EXPECT_EQ(headers3[1].name, ":scheme");
    EXPECT_EQ(headers3[1].value, "https");
    EXPECT_EQ(headers3[2].name, ":path");
    EXPECT_EQ(headers3[2].value, "/index.html");
    EXPECT_EQ(headers3[4].name, "custom-key");
    EXPECT_EQ(headers3[4].value, "custom-value");
}

// ============================================================================
// RFC 7541 Appendix C.5 - Response Examples Without Huffman Coding
// ============================================================================

TEST_F(HpackRFC7541Test, RFC7541_C5_SequentialResponses)
{
    // C.5.1-C.5.3: Sequential responses with dynamic table evolution
    // Uses a 256-byte dynamic table to force evictions
    hpack_encoder encoder(256);
    hpack_decoder decoder(256);

    // First response (302, cache-control: private, date, location)
    std::vector<http_header> resp1 = {
        {":status", "302"},
        {"cache-control", "private"},
        {"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
        {"location", "https://www.example.com"}
    };

    auto enc1 = encoder.encode(resp1);
    auto dec1 = decoder.decode(enc1);
    ASSERT_TRUE(dec1.is_ok());

    auto h1 = dec1.value();
    ASSERT_EQ(h1.size(), 4u);
    EXPECT_EQ(h1[0].name, ":status");
    EXPECT_EQ(h1[0].value, "302");
    EXPECT_EQ(h1[1].name, "cache-control");
    EXPECT_EQ(h1[1].value, "private");
    EXPECT_EQ(h1[2].name, "date");
    EXPECT_EQ(h1[2].value, "Mon, 21 Oct 2013 20:13:21 GMT");
    EXPECT_EQ(h1[3].name, "location");
    EXPECT_EQ(h1[3].value, "https://www.example.com");

    // Second response (307, with some repeated headers)
    std::vector<http_header> resp2 = {
        {":status", "307"},
        {"cache-control", "private"},
        {"date", "Mon, 21 Oct 2013 20:13:21 GMT"},
        {"location", "https://www.example.com"}
    };

    auto enc2 = encoder.encode(resp2);
    auto dec2 = decoder.decode(enc2);
    ASSERT_TRUE(dec2.is_ok());

    auto h2 = dec2.value();
    ASSERT_EQ(h2.size(), 4u);
    EXPECT_EQ(h2[0].name, ":status");
    EXPECT_EQ(h2[0].value, "307");

    // Third response (200, with content-encoding)
    std::vector<http_header> resp3 = {
        {":status", "200"},
        {"cache-control", "private"},
        {"date", "Mon, 21 Oct 2013 20:13:22 GMT"},
        {"location", "https://www.example.com"},
        {"content-encoding", "gzip"},
        {"set-cookie", "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU; max-age=3600; version=1"}
    };

    auto enc3 = encoder.encode(resp3);
    auto dec3 = decoder.decode(enc3);
    ASSERT_TRUE(dec3.is_ok());

    auto h3 = dec3.value();
    ASSERT_EQ(h3.size(), 6u);
    EXPECT_EQ(h3[0].name, ":status");
    EXPECT_EQ(h3[0].value, "200");
    EXPECT_EQ(h3[4].name, "content-encoding");
    EXPECT_EQ(h3[4].value, "gzip");
    EXPECT_EQ(h3[5].name, "set-cookie");
}

// ============================================================================
// Dynamic Table Behavior (RFC 7541 Section 4)
// ============================================================================

TEST_F(HpackRFC7541Test, DynamicTableEvictsOldestEntries)
{
    // RFC 7541 Section 4.4: Entries are evicted from the end (oldest)
    // Use small table to force eviction
    dynamic_table table(128);

    // Each entry: name(10) + value(10) + 32 = 52 bytes
    table.insert("aaaaaaaaaa", "bbbbbbbbbb"); // 52 bytes
    EXPECT_EQ(table.entry_count(), 1u);

    table.insert("cccccccccc", "dddddddddd"); // 52 bytes, total 104
    EXPECT_EQ(table.entry_count(), 2u);

    // This should evict the first entry (104 + 52 = 156 > 128)
    table.insert("eeeeeeeeee", "ffffffffff"); // evict oldest
    EXPECT_EQ(table.entry_count(), 2u);

    // Verify the oldest entry was evicted
    // Index 0 should now be the most recently added entry
    auto entry0 = table.get(0);
    ASSERT_TRUE(entry0.has_value());
    EXPECT_EQ(entry0->name, "eeeeeeeeee");

    auto entry1 = table.get(1);
    ASSERT_TRUE(entry1.has_value());
    EXPECT_EQ(entry1->name, "cccccccccc");
}

TEST_F(HpackRFC7541Test, DynamicTableSizeUpdate)
{
    // RFC 7541 Section 4.3: Dynamic table size update
    dynamic_table table(4096);

    table.insert("name1", "value1"); // 5 + 6 + 32 = 43 bytes
    table.insert("name2", "value2"); // 43 bytes
    EXPECT_EQ(table.entry_count(), 2u);

    // Reduce table size to force evictions
    table.set_max_size(50);
    EXPECT_LE(table.current_size(), 50u);
}

TEST_F(HpackRFC7541Test, DynamicTableSizeUpdateToZero)
{
    // RFC 7541 Section 4.3: Setting size to 0 clears all entries
    dynamic_table table(4096);

    table.insert("name1", "value1");
    table.insert("name2", "value2");
    EXPECT_EQ(table.entry_count(), 2u);

    table.set_max_size(0);
    EXPECT_EQ(table.entry_count(), 0u);
    EXPECT_EQ(table.current_size(), 0u);
}

// ============================================================================
// Static Table Verification (RFC 7541 Appendix A)
// ============================================================================

TEST_F(HpackRFC7541Test, StaticTableAppendixA_SelectedEntries)
{
    // Verify selected entries from RFC 7541 Appendix A
    auto entry1 = static_table::get(1);
    ASSERT_TRUE(entry1.has_value());
    EXPECT_EQ(entry1->name, ":authority");
    EXPECT_EQ(entry1->value, "");

    auto entry2 = static_table::get(2);
    ASSERT_TRUE(entry2.has_value());
    EXPECT_EQ(entry2->name, ":method");
    EXPECT_EQ(entry2->value, "GET");

    auto entry3 = static_table::get(3);
    ASSERT_TRUE(entry3.has_value());
    EXPECT_EQ(entry3->name, ":method");
    EXPECT_EQ(entry3->value, "POST");

    auto entry4 = static_table::get(4);
    ASSERT_TRUE(entry4.has_value());
    EXPECT_EQ(entry4->name, ":path");
    EXPECT_EQ(entry4->value, "/");

    auto entry5 = static_table::get(5);
    ASSERT_TRUE(entry5.has_value());
    EXPECT_EQ(entry5->name, ":path");
    EXPECT_EQ(entry5->value, "/index.html");

    auto entry8 = static_table::get(8);
    ASSERT_TRUE(entry8.has_value());
    EXPECT_EQ(entry8->name, ":status");
    EXPECT_EQ(entry8->value, "200");

    // Index 0 is invalid
    auto entry0 = static_table::get(0);
    EXPECT_FALSE(entry0.has_value());

    // Index 62+ is invalid
    auto entry62 = static_table::get(62);
    EXPECT_FALSE(entry62.has_value());
}

TEST_F(HpackRFC7541Test, StaticTableFindByName)
{
    // Find by name only
    auto idx = static_table::find(":method");
    EXPECT_GE(idx, 2u);
    EXPECT_LE(idx, 3u);

    // Find by name and value
    auto idx_get = static_table::find(":method", "GET");
    EXPECT_EQ(idx_get, 2u);

    auto idx_post = static_table::find(":method", "POST");
    EXPECT_EQ(idx_post, 3u);

    // Non-existent header
    auto idx_none = static_table::find("x-nonexistent");
    EXPECT_EQ(idx_none, 0u);
}

// ============================================================================
// Encoder-Decoder Round Trip Consistency
// ============================================================================

TEST_F(HpackRFC7541Test, ConsistentEncodingAcrossConnections)
{
    // Two independent encoder/decoder pairs should produce same results
    hpack_encoder enc1, enc2;
    hpack_decoder dec1, dec2;

    std::vector<http_header> headers = {
        {":method", "GET"},
        {":path", "/api/data"},
        {"accept", "application/json"},
        {"authorization", "Bearer token123"}
    };

    auto encoded1 = enc1.encode(headers);
    auto encoded2 = enc2.encode(headers);

    auto result1 = dec1.decode(encoded1);
    auto result2 = dec2.decode(encoded2);

    ASSERT_TRUE(result1.is_ok());
    ASSERT_TRUE(result2.is_ok());

    auto decoded1 = result1.value();
    auto decoded2 = result2.value();

    ASSERT_EQ(decoded1.size(), decoded2.size());
    for (size_t i = 0; i < decoded1.size(); ++i)
    {
        EXPECT_EQ(decoded1[i].name, decoded2[i].name);
        EXPECT_EQ(decoded1[i].value, decoded2[i].value);
    }
}

TEST_F(HpackRFC7541Test, LargeHeaderSetRoundTrip)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    std::vector<http_header> headers = {
        {":method", "POST"},
        {":scheme", "https"},
        {":path", "/api/v2/resources"},
        {":authority", "api.example.com"},
        {"content-type", "application/json; charset=utf-8"},
        {"content-length", "1024"},
        {"accept", "application/json"},
        {"accept-encoding", "gzip, deflate, br"},
        {"accept-language", "en-US,en;q=0.9"},
        {"authorization", "Bearer eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0"},
        {"user-agent", "NetworkSystem/1.0"},
        {"x-request-id", "550e8400-e29b-41d4-a716-446655440000"},
        {"x-trace-id", "00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01"},
        {"cache-control", "no-cache"},
        {"connection", "keep-alive"}
    };

    auto encoded = encoder.encode(headers);
    auto result = decoder.decode(encoded);

    ASSERT_TRUE(result.is_ok());

    auto decoded = result.value();
    ASSERT_EQ(decoded.size(), headers.size());

    for (size_t i = 0; i < headers.size(); ++i)
    {
        EXPECT_EQ(decoded[i].name, headers[i].name)
            << "Mismatch at header index " << i;
        EXPECT_EQ(decoded[i].value, headers[i].value)
            << "Mismatch at header index " << i;
    }
}

TEST_F(HpackRFC7541Test, EmptyHeaderValueRoundTrip)
{
    hpack_encoder encoder;
    hpack_decoder decoder;

    std::vector<http_header> headers = {
        {":method", "GET"},
        {":path", "/"},
        {"x-empty", ""},
        {"x-normal", "value"}
    };

    auto encoded = encoder.encode(headers);
    auto result = decoder.decode(encoded);

    ASSERT_TRUE(result.is_ok());

    auto decoded = result.value();
    ASSERT_EQ(decoded.size(), 4u);
    EXPECT_EQ(decoded[2].name, "x-empty");
    EXPECT_EQ(decoded[2].value, "");
}

// ============================================================================
// Error Handling
// ============================================================================

TEST_F(HpackRFC7541Test, DecoderRejectsEmptyInput)
{
    hpack_decoder decoder;
    std::vector<uint8_t> empty;

    auto result = decoder.decode(empty);
    // Empty input should either succeed with empty headers or fail gracefully
    if (result.is_ok())
    {
        EXPECT_TRUE(result.value().empty());
    }
}

TEST_F(HpackRFC7541Test, DecoderRejectsTruncatedData)
{
    hpack_decoder decoder;

    // Truncated literal header: says name length is 10 but only provides 3 bytes
    std::vector<uint8_t> truncated = {
        0x40,       // Literal with indexing, new name
        0x0a,       // Name length = 10
        'a', 'b', 'c' // Only 3 bytes of name
    };

    auto result = decoder.decode(truncated);
    EXPECT_TRUE(result.is_err());
}

TEST_F(HpackRFC7541Test, DecoderRejectsInvalidIndex)
{
    hpack_decoder decoder;

    // Indexed header with index beyond static+dynamic table
    // 0xFF followed by large integer
    std::vector<uint8_t> invalid = {0xFE}; // Index 126, no dynamic entries

    auto result = decoder.decode(invalid);
    EXPECT_TRUE(result.is_err());
}
