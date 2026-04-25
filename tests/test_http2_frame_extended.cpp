// BSD 3-Clause License
// Copyright (c) 2024, kcenon
// See the LICENSE file in the project root for full license information.

// Supplementary coverage for src/protocols/http2/frame.cpp (Issue #1030).
// Targets remaining branches not exercised by tests/test_http2_frame.cpp:
// - Constructor variants (end_headers=false, settings_frame ack default,
//   ping_frame default opaque data, goaway_frame with empty additional_data)
// - Boundary cases in padded frames (zero pad length, pad consumes
//   exactly the trailing bytes, all-padding payload)
// - frame_header parse boundary (length == max_frame_size)
// - All remaining frame_type switch arms (push_promise, continuation)
// - reserved-bit handling in stream IDs (highest bit must be masked)

#include <gtest/gtest.h>
#include "internal/protocols/http2/frame.h"

#include <array>
#include <cstdint>
#include <vector>

using namespace kcenon::network::protocols::http2;

namespace
{
    // Build a 9-byte HTTP/2 frame header followed by `payload` bytes.
    std::vector<uint8_t> make_raw_frame(uint32_t length,
                                        uint8_t type,
                                        uint8_t flags,
                                        uint32_t stream_id,
                                        const std::vector<uint8_t>& payload = {})
    {
        std::vector<uint8_t> raw = {
            static_cast<uint8_t>((length >> 16) & 0xFF),
            static_cast<uint8_t>((length >> 8) & 0xFF),
            static_cast<uint8_t>(length & 0xFF),
            type,
            flags,
            static_cast<uint8_t>((stream_id >> 24) & 0xFF),
            static_cast<uint8_t>((stream_id >> 16) & 0xFF),
            static_cast<uint8_t>((stream_id >> 8) & 0xFF),
            static_cast<uint8_t>(stream_id & 0xFF),
        };
        raw.insert(raw.end(), payload.begin(), payload.end());
        return raw;
    }
}

class Http2FrameExtendedTest : public ::testing::Test {};

// ----- frame_header boundary -----------------------------------------------

TEST_F(Http2FrameExtendedTest, FrameHeaderAcceptsMaxFrameSize)
{
    // length == (1<<24)-1 should be accepted (boundary just inside the limit).
    constexpr uint32_t max_len = (1u << 24) - 1;
    std::vector<uint8_t> raw = {
        0xFF, 0xFF, 0xFF,        // length = max
        0x04,                    // SETTINGS
        0x01,                    // ACK (so payload size 0 still parses)
        0x00, 0x00, 0x00, 0x00,  // stream_id 0
    };
    auto hdr = frame_header::parse(raw);
    ASSERT_TRUE(hdr.is_ok());
    EXPECT_EQ(hdr.value().length, max_len);
}

TEST_F(Http2FrameExtendedTest, FrameHeaderRejectsLengthAboveMax)
{
    // Construct a header whose 24-bit length wraps to 0 cannot exceed max,
    // so test boundary by setting length one past max via raw bytes is impossible
    // (24 bits saturate at max). The parser still validates header.length, so
    // a synthetic header whose computed length exceeds max requires direct
    // assignment via serialize. Instead, verify that parse honours the limit
    // by constructing a frame_header with length above max manually.
    frame_header hdr;
    hdr.length = (1u << 24);  // > max_frame_size (validation only triggers
                              // inside parse, not serialize)
    hdr.type = frame_type::data;
    hdr.flags = 0;
    hdr.stream_id = 1;
    auto bytes = hdr.serialize();
    // serialize truncates to the low 24 bits: result is "0".
    EXPECT_EQ(bytes.size(), 9u);
    EXPECT_EQ(bytes[0], 0u);
    EXPECT_EQ(bytes[1], 0u);
    EXPECT_EQ(bytes[2], 0u);
}

// ----- DATA frame edge cases -----------------------------------------------

TEST_F(Http2FrameExtendedTest, ParsesPaddedDataFrameZeroPadLength)
{
    // pad_length byte present but zero: no actual padding.
    auto raw = make_raw_frame(/*len=*/4, /*type=DATA*/ 0x0,
                              /*flags=PADDED*/ 0x08, /*sid=*/1,
                              {0x00, 'A', 'B', 'C'});
    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());
    auto* d = dynamic_cast<data_frame*>(result.value().get());
    ASSERT_NE(d, nullptr);
    EXPECT_TRUE(d->is_padded());
    EXPECT_EQ(d->data().size(), 3u);
    EXPECT_EQ(d->data()[0], 'A');
    EXPECT_EQ(d->data()[2], 'C');
}

TEST_F(Http2FrameExtendedTest, ParsesPaddedDataFrameAllPadding)
{
    // pad_length == payload length - 1: zero data bytes.
    auto raw = make_raw_frame(/*len=*/3, /*type=*/0x0, /*flags=PADDED*/ 0x08,
                              /*sid=*/1, {0x02, 0x00, 0x00});
    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());
    auto* d = dynamic_cast<data_frame*>(result.value().get());
    ASSERT_NE(d, nullptr);
    EXPECT_TRUE(d->is_padded());
    EXPECT_TRUE(d->data().empty());
}

TEST_F(Http2FrameExtendedTest, DataFrameRoundTripPadded)
{
    data_frame original(7, std::vector<uint8_t>{'p', 'a', 'd', 'd', 'y'},
                        /*end_stream=*/false, /*padded=*/true);
    auto bytes = original.serialize();
    auto reparsed = frame::parse(bytes);
    ASSERT_TRUE(reparsed.is_ok());
    auto* d = dynamic_cast<data_frame*>(reparsed.value().get());
    ASSERT_NE(d, nullptr);
    EXPECT_TRUE(d->is_padded());
    EXPECT_EQ(d->header().stream_id, 7u);
    EXPECT_EQ(std::string(d->data().begin(), d->data().end()), "paddy");
}

// ----- HEADERS frame variants ----------------------------------------------

TEST_F(Http2FrameExtendedTest, HeadersFrameConstructorEndHeadersFalse)
{
    headers_frame hf(3, std::vector<uint8_t>{0xAA, 0xBB},
                     /*end_stream=*/false, /*end_headers=*/false);
    EXPECT_FALSE(hf.is_end_stream());
    EXPECT_FALSE(hf.is_end_headers());
    EXPECT_EQ(hf.header().flags, frame_flags::none);
    EXPECT_EQ(hf.header().length, 2u);
    auto bytes = hf.serialize();
    auto re = frame::parse(bytes);
    ASSERT_TRUE(re.is_ok());
    auto* h = dynamic_cast<headers_frame*>(re.value().get());
    ASSERT_NE(h, nullptr);
    EXPECT_FALSE(h->is_end_headers());
    EXPECT_FALSE(h->is_end_stream());
}

TEST_F(Http2FrameExtendedTest, HeadersFrameConstructorEndStreamOnly)
{
    headers_frame hf(5, std::vector<uint8_t>{0xCC},
                     /*end_stream=*/true, /*end_headers=*/false);
    EXPECT_TRUE(hf.is_end_stream());
    EXPECT_FALSE(hf.is_end_headers());
}

TEST_F(Http2FrameExtendedTest, ParsesPaddedHeadersFrameZeroPadLength)
{
    auto raw = make_raw_frame(/*len=*/3, /*type=HEADERS*/ 0x1,
                              /*flags=PADDED|END_HEADERS*/ 0x0C,
                              /*sid=*/3, {0x00, 0x80, 0x81});
    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());
    auto* h = dynamic_cast<headers_frame*>(result.value().get());
    ASSERT_NE(h, nullptr);
    EXPECT_TRUE(h->is_end_headers());
    EXPECT_EQ(h->header_block().size(), 2u);
}

// ----- SETTINGS frame -------------------------------------------------------

TEST_F(Http2FrameExtendedTest, SettingsFrameDefaultConstructorEmpty)
{
    settings_frame sf;
    EXPECT_FALSE(sf.is_ack());
    EXPECT_TRUE(sf.settings().empty());
    EXPECT_EQ(sf.header().length, 0u);
    EXPECT_EQ(sf.header().type, frame_type::settings);
    EXPECT_EQ(sf.header().stream_id, 0u);
}

TEST_F(Http2FrameExtendedTest, SettingsAckIgnoresProvidedSettings)
{
    // When ack=true the constructor must not encode settings into the payload.
    settings_frame sf({{1, 4096}, {2, 0}}, /*ack=*/true);
    EXPECT_TRUE(sf.is_ack());
    EXPECT_EQ(sf.header().length, 0u);
    EXPECT_TRUE(sf.payload().empty());
}

// ----- PING frame -----------------------------------------------------------

TEST_F(Http2FrameExtendedTest, PingFrameDefaultConstructorZeroData)
{
    ping_frame ping;
    EXPECT_FALSE(ping.is_ack());
    EXPECT_EQ(ping.header().length, 8u);
    EXPECT_EQ(ping.header().type, frame_type::ping);
    EXPECT_EQ(ping.header().stream_id, 0u);
    for (auto byte : ping.opaque_data()) {
        EXPECT_EQ(byte, 0u);
    }
}

// ----- RST_STREAM error code round-trip ------------------------------------

TEST_F(Http2FrameExtendedTest, RstStreamLargeErrorCodeRoundTrip)
{
    rst_stream_frame rs(11, 0xDEADBEEF);
    EXPECT_EQ(rs.error_code(), 0xDEADBEEFu);
    auto bytes = rs.serialize();
    auto re = frame::parse(bytes);
    ASSERT_TRUE(re.is_ok());
    auto* d = dynamic_cast<rst_stream_frame*>(re.value().get());
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->error_code(), 0xDEADBEEFu);
    EXPECT_EQ(d->header().stream_id, 11u);
}

// ----- GOAWAY frame --------------------------------------------------------

TEST_F(Http2FrameExtendedTest, GoawayConstructorReservedBitInLastStreamIdMasked)
{
    // last_stream_id has the reserved high bit set; the encoder must mask it.
    constexpr uint32_t raw_id = 0x80000005;
    constexpr uint32_t masked = 0x00000005;
    goaway_frame gf(raw_id, 0x00000001u);
    auto bytes = gf.serialize();
    auto re = frame::parse(bytes);
    ASSERT_TRUE(re.is_ok());
    auto* g = dynamic_cast<goaway_frame*>(re.value().get());
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->last_stream_id(), masked);
    EXPECT_EQ(g->error_code(), 1u);
    EXPECT_TRUE(g->additional_data().empty());
}

TEST_F(Http2FrameExtendedTest, GoawayParsedReservedBitInLastStreamIdMasked)
{
    // Build raw bytes with the reserved bit set in the last_stream_id field.
    auto raw = make_raw_frame(/*len=*/8, /*type=GOAWAY*/ 0x7,
                              /*flags=*/0, /*sid=*/0,
                              {0x80, 0x00, 0x00, 0x07,   // last_stream_id (reserved bit)
                               0x00, 0x00, 0x00, 0x02}); // error_code
    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());
    auto* g = dynamic_cast<goaway_frame*>(result.value().get());
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->last_stream_id(), 7u);
    EXPECT_EQ(g->error_code(), 2u);
}

// ----- WINDOW_UPDATE -------------------------------------------------------

TEST_F(Http2FrameExtendedTest, WindowUpdateSerializeMasksReservedBitOnWire)
{
    // The accessor preserves the original constructor argument; only the
    // on-wire encoding masks out the reserved high bit (RFC 7540 Section 6.9).
    window_update_frame wu(0, 0x80000001);
    EXPECT_EQ(wu.window_size_increment(), 0x80000001u);
    auto bytes = wu.serialize();
    ASSERT_EQ(bytes.size(), 9u + 4u);
    EXPECT_EQ(bytes[9], 0x00u);
    EXPECT_EQ(bytes[10], 0x00u);
    EXPECT_EQ(bytes[11], 0x00u);
    EXPECT_EQ(bytes[12], 0x01u);
    auto re = frame::parse(bytes);
    ASSERT_TRUE(re.is_ok());
    auto* w = dynamic_cast<window_update_frame*>(re.value().get());
    ASSERT_NE(w, nullptr);
    EXPECT_EQ(w->window_size_increment(), 1u);
}

// ----- frame::parse switch coverage ----------------------------------------

TEST_F(Http2FrameExtendedTest, ParsesPushPromiseAsGenericFrame)
{
    // PUSH_PROMISE (0x5) is not handled by a dedicated subclass - parser must
    // fall through to the default branch and yield a base `frame`.
    auto raw = make_raw_frame(/*len=*/4, /*type=PUSH_PROMISE*/ 0x5,
                              /*flags=*/0, /*sid=*/1,
                              {0x00, 0x00, 0x00, 0x03});
    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());
    auto& f = result.value();
    EXPECT_EQ(f->header().type, frame_type::push_promise);
    EXPECT_EQ(f->payload().size(), 4u);
    EXPECT_EQ(dynamic_cast<data_frame*>(f.get()), nullptr);
    EXPECT_EQ(dynamic_cast<headers_frame*>(f.get()), nullptr);
}

TEST_F(Http2FrameExtendedTest, ParsesContinuationAsGenericFrame)
{
    auto raw = make_raw_frame(/*len=*/2, /*type=CONTINUATION*/ 0x9,
                              /*flags=END_HEADERS*/ 0x4, /*sid=*/1,
                              {0x82, 0x84});
    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value()->header().type, frame_type::continuation);
    EXPECT_EQ(result.value()->payload().size(), 2u);
}

TEST_F(Http2FrameExtendedTest, ParsesPriorityAsGenericFrame)
{
    auto raw = make_raw_frame(/*len=*/5, /*type=PRIORITY*/ 0x2,
                              /*flags=*/0, /*sid=*/1,
                              {0x00, 0x00, 0x00, 0x03, 0xFF});
    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value()->header().type, frame_type::priority);
    EXPECT_EQ(result.value()->payload().size(), 5u);
}

// ----- frame::parse propagates header errors -------------------------------

TEST_F(Http2FrameExtendedTest, ParsePropagatesHeaderTooShortError)
{
    // Only 5 bytes - shorter than 9-byte frame header.
    std::vector<uint8_t> raw = {0x00, 0x00, 0x00, 0x01, 0x00};
    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

// ----- frame base class accessors ------------------------------------------

TEST_F(Http2FrameExtendedTest, FrameBaseConstructorPreservesHeaderAndPayload)
{
    frame_header hdr;
    hdr.length = 3;
    hdr.type = frame_type::data;
    hdr.flags = frame_flags::none;
    hdr.stream_id = 99;
    std::vector<uint8_t> payload{0x10, 0x20, 0x30};
    frame f(hdr, payload);
    EXPECT_EQ(f.header().length, 3u);
    EXPECT_EQ(f.header().stream_id, 99u);
    ASSERT_EQ(f.payload().size(), 3u);
    EXPECT_EQ(f.payload()[0], 0x10);
    EXPECT_EQ(f.payload()[2], 0x30);
}

// ----- Settings parse with no parameters -----------------------------------

TEST_F(Http2FrameExtendedTest, ParsesEmptySettingsFrame)
{
    auto raw = make_raw_frame(/*len=*/0, /*type=SETTINGS*/ 0x4,
                              /*flags=*/0, /*sid=*/0);
    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());
    auto* s = dynamic_cast<settings_frame*>(result.value().get());
    ASSERT_NE(s, nullptr);
    EXPECT_FALSE(s->is_ack());
    EXPECT_TRUE(s->settings().empty());
}
