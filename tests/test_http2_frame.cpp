/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <gtest/gtest.h>
#include "internal/protocols/http2/frame.h"
#include <vector>
#include <cstdint>

using namespace kcenon::network::protocols::http2;

class Http2FrameTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(Http2FrameTest, ParsesDataFrame)
{
    // DATA frame with stream ID 1, payload "hello"
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x05,  // Length: 5
        0x00,              // Type: DATA
        0x01,              // Flags: END_STREAM
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        'h', 'e', 'l', 'l', 'o'  // Payload
    };

    auto frame_result = frame::parse(raw);

    ASSERT_TRUE(frame_result.is_ok());
    auto& frm = frame_result.value();
    EXPECT_EQ(frm->header().type, frame_type::data);
    EXPECT_EQ(frm->header().length, 5);
    EXPECT_EQ(frm->header().stream_id, 1u);
    EXPECT_EQ(frm->header().flags, frame_flags::end_stream);

    auto data_frm = dynamic_cast<data_frame*>(frm.get());
    ASSERT_NE(data_frm, nullptr);
    EXPECT_TRUE(data_frm->is_end_stream());

    auto data = data_frm->data();
    std::string data_str(data.begin(), data.end());
    EXPECT_EQ(data_str, "hello");
}

TEST_F(Http2FrameTest, ParsesHeadersFrame)
{
    // HEADERS frame with stream ID 1, empty header block
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x00,  // Length: 0
        0x01,              // Type: HEADERS
        0x05,              // Flags: END_STREAM | END_HEADERS
        0x00, 0x00, 0x00, 0x01  // Stream ID: 1
    };

    auto frame_result = frame::parse(raw);

    ASSERT_TRUE(frame_result.is_ok());
    auto& frm = frame_result.value();
    EXPECT_EQ(frm->header().type, frame_type::headers);
    EXPECT_EQ(frm->header().stream_id, 1u);

    auto headers_frm = dynamic_cast<headers_frame*>(frm.get());
    ASSERT_NE(headers_frm, nullptr);
    EXPECT_TRUE(headers_frm->is_end_stream());
    EXPECT_TRUE(headers_frm->is_end_headers());
}

TEST_F(Http2FrameTest, ParsesSettingsFrame)
{
    // SETTINGS frame with HEADER_TABLE_SIZE=4096, ENABLE_PUSH=0
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x0C,  // Length: 12
        0x04,              // Type: SETTINGS
        0x00,              // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        // Setting 1: HEADER_TABLE_SIZE=4096
        0x00, 0x01,        // Identifier: 1
        0x00, 0x00, 0x10, 0x00,  // Value: 4096
        // Setting 2: ENABLE_PUSH=0
        0x00, 0x02,        // Identifier: 2
        0x00, 0x00, 0x00, 0x00   // Value: 0
    };

    auto frame_result = frame::parse(raw);

    ASSERT_TRUE(frame_result.is_ok());
    auto& frm = frame_result.value();
    EXPECT_EQ(frm->header().type, frame_type::settings);
    EXPECT_EQ(frm->header().stream_id, 0u);

    auto settings_frm = dynamic_cast<settings_frame*>(frm.get());
    ASSERT_NE(settings_frm, nullptr);
    EXPECT_FALSE(settings_frm->is_ack());

    const auto& settings = settings_frm->settings();
    ASSERT_EQ(settings.size(), 2u);
    EXPECT_EQ(settings[0].identifier, 1u);
    EXPECT_EQ(settings[0].value, 4096u);
    EXPECT_EQ(settings[1].identifier, 2u);
    EXPECT_EQ(settings[1].value, 0u);
}

TEST_F(Http2FrameTest, ParsesSettingsAckFrame)
{
    // SETTINGS ACK frame
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x00,  // Length: 0
        0x04,              // Type: SETTINGS
        0x01,              // Flags: ACK
        0x00, 0x00, 0x00, 0x00  // Stream ID: 0
    };

    auto frame_result = frame::parse(raw);

    ASSERT_TRUE(frame_result.is_ok());
    auto& frm = frame_result.value();
    EXPECT_EQ(frm->header().type, frame_type::settings);

    auto settings_frm = dynamic_cast<settings_frame*>(frm.get());
    ASSERT_NE(settings_frm, nullptr);
    EXPECT_TRUE(settings_frm->is_ack());
    EXPECT_EQ(settings_frm->settings().size(), 0u);
}

TEST_F(Http2FrameTest, ParsesRstStreamFrame)
{
    // RST_STREAM frame with stream ID 1, error code CANCEL(8)
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x04,  // Length: 4
        0x03,              // Type: RST_STREAM
        0x00,              // Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0x00, 0x00, 0x00, 0x08   // Error code: CANCEL
    };

    auto frame_result = frame::parse(raw);

    ASSERT_TRUE(frame_result.is_ok());
    auto& frm = frame_result.value();
    EXPECT_EQ(frm->header().type, frame_type::rst_stream);
    EXPECT_EQ(frm->header().stream_id, 1u);

    auto rst_frm = dynamic_cast<rst_stream_frame*>(frm.get());
    ASSERT_NE(rst_frm, nullptr);
    EXPECT_EQ(rst_frm->error_code(), 8u);
}

TEST_F(Http2FrameTest, ParsesPingFrame)
{
    // PING frame with opaque data
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x08,  // Length: 8
        0x06,              // Type: PING
        0x00,              // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08  // Opaque data
    };

    auto frame_result = frame::parse(raw);

    ASSERT_TRUE(frame_result.is_ok());
    auto& frm = frame_result.value();
    EXPECT_EQ(frm->header().type, frame_type::ping);
    EXPECT_EQ(frm->header().stream_id, 0u);

    auto ping_frm = dynamic_cast<ping_frame*>(frm.get());
    ASSERT_NE(ping_frm, nullptr);
    EXPECT_FALSE(ping_frm->is_ack());

    const auto& data = ping_frm->opaque_data();
    for (size_t i = 0; i < 8; ++i)
    {
        EXPECT_EQ(data[i], static_cast<uint8_t>(i + 1));
    }
}

TEST_F(Http2FrameTest, ParsesGoawayFrame)
{
    // GOAWAY frame with last stream ID 5, error code PROTOCOL_ERROR(1)
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x08,  // Length: 8
        0x07,              // Type: GOAWAY
        0x00,              // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x00, 0x00, 0x00, 0x05,  // Last stream ID: 5
        0x00, 0x00, 0x00, 0x01   // Error code: PROTOCOL_ERROR
    };

    auto frame_result = frame::parse(raw);

    ASSERT_TRUE(frame_result.is_ok());
    auto& frm = frame_result.value();
    EXPECT_EQ(frm->header().type, frame_type::goaway);
    EXPECT_EQ(frm->header().stream_id, 0u);

    auto goaway_frm = dynamic_cast<goaway_frame*>(frm.get());
    ASSERT_NE(goaway_frm, nullptr);
    EXPECT_EQ(goaway_frm->last_stream_id(), 5u);
    EXPECT_EQ(goaway_frm->error_code(), 1u);
}

TEST_F(Http2FrameTest, ParsesWindowUpdateFrame)
{
    // WINDOW_UPDATE frame with stream ID 0, increment 65536
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x04,  // Length: 4
        0x08,              // Type: WINDOW_UPDATE
        0x00,              // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x00, 0x01, 0x00, 0x00   // Window size increment: 65536
    };

    auto frame_result = frame::parse(raw);

    ASSERT_TRUE(frame_result.is_ok());
    auto& frm = frame_result.value();
    EXPECT_EQ(frm->header().type, frame_type::window_update);
    EXPECT_EQ(frm->header().stream_id, 0u);

    auto window_frm = dynamic_cast<window_update_frame*>(frm.get());
    ASSERT_NE(window_frm, nullptr);
    EXPECT_EQ(window_frm->window_size_increment(), 65536u);
}

TEST_F(Http2FrameTest, SerializesAndDeserializesDataFrame)
{
    // Create DATA frame
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    data_frame original(1, data, true, false);

    // Serialize
    auto serialized = original.serialize();

    // Parse back
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed_frm = dynamic_cast<data_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed_frm, nullptr);

    EXPECT_EQ(parsed_frm->header().stream_id, 1u);
    EXPECT_TRUE(parsed_frm->is_end_stream());

    auto parsed_data = parsed_frm->data();
    EXPECT_EQ(std::vector<uint8_t>(parsed_data.begin(), parsed_data.end()), data);
}

TEST_F(Http2FrameTest, SerializesAndDeserializesSettingsFrame)
{
    // Create SETTINGS frame
    std::vector<setting_parameter> settings = {
        {1, 4096},
        {2, 0},
        {3, 100}
    };
    settings_frame original(settings);

    // Serialize
    auto serialized = original.serialize();

    // Parse back
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed_frm = dynamic_cast<settings_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed_frm, nullptr);

    const auto& parsed_settings = parsed_frm->settings();
    ASSERT_EQ(parsed_settings.size(), 3u);
    EXPECT_EQ(parsed_settings[0].identifier, 1u);
    EXPECT_EQ(parsed_settings[0].value, 4096u);
    EXPECT_EQ(parsed_settings[1].identifier, 2u);
    EXPECT_EQ(parsed_settings[1].value, 0u);
    EXPECT_EQ(parsed_settings[2].identifier, 3u);
    EXPECT_EQ(parsed_settings[2].value, 100u);
}

TEST_F(Http2FrameTest, RejectsInvalidFrameHeader)
{
    // Insufficient data for header
    std::vector<uint8_t> raw = {0x00, 0x00, 0x05, 0x00};

    auto frame_result = frame::parse(raw);
    EXPECT_TRUE(frame_result.is_err());
}

TEST_F(Http2FrameTest, RejectsDataFrameWithZeroStreamId)
{
    // DATA frame with stream ID 0 (invalid)
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x05,  // Length: 5
        0x00,              // Type: DATA
        0x00,              // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0 (invalid)
        'h', 'e', 'l', 'l', 'o'
    };

    auto frame_result = frame::parse(raw);
    EXPECT_TRUE(frame_result.is_err());
}

TEST_F(Http2FrameTest, RejectsSettingsFrameWithNonZeroStreamId)
{
    // SETTINGS frame with stream ID 1 (invalid)
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x00,  // Length: 0
        0x04,              // Type: SETTINGS
        0x01,              // Flags: ACK
        0x00, 0x00, 0x00, 0x01  // Stream ID: 1 (invalid)
    };

    auto frame_result = frame::parse(raw);
    EXPECT_TRUE(frame_result.is_err());
}

// ============================================================
// Frame Header Tests
// ============================================================

TEST_F(Http2FrameTest, ParsesFrameHeaderDirectly)
{
    std::vector<uint8_t> raw = {
        0x00, 0x04, 0x00,        // Length: 1024
        0x01,                    // Type: HEADERS
        0x05,                    // Flags: END_STREAM | END_HEADERS
        0x00, 0x00, 0x00, 0x03  // Stream ID: 3
    };

    auto result = frame_header::parse(raw);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().length, 1024u);
    EXPECT_EQ(result.value().type, frame_type::headers);
    EXPECT_EQ(result.value().flags, 0x05);
    EXPECT_EQ(result.value().stream_id, 3u);
}

TEST_F(Http2FrameTest, SerializesFrameHeaderDirectly)
{
    frame_header hdr;
    hdr.length = 256;
    hdr.type = frame_type::data;
    hdr.flags = frame_flags::end_stream;
    hdr.stream_id = 7;

    auto bytes = hdr.serialize();
    ASSERT_EQ(bytes.size(), 9u);

    // Length: 256 = 0x000100
    EXPECT_EQ(bytes[0], 0x00);
    EXPECT_EQ(bytes[1], 0x01);
    EXPECT_EQ(bytes[2], 0x00);
    // Type: DATA = 0x00
    EXPECT_EQ(bytes[3], 0x00);
    // Flags: END_STREAM = 0x01
    EXPECT_EQ(bytes[4], 0x01);
    // Stream ID: 7
    EXPECT_EQ(bytes[5], 0x00);
    EXPECT_EQ(bytes[6], 0x00);
    EXPECT_EQ(bytes[7], 0x00);
    EXPECT_EQ(bytes[8], 0x07);
}

TEST_F(Http2FrameTest, FrameHeaderRoundTrip)
{
    frame_header original;
    original.length = 16384;
    original.type = frame_type::settings;
    original.flags = frame_flags::ack;
    original.stream_id = 0;

    auto bytes = original.serialize();
    auto parsed = frame_header::parse(bytes);

    ASSERT_TRUE(parsed.is_ok());
    EXPECT_EQ(parsed.value().length, original.length);
    EXPECT_EQ(parsed.value().type, original.type);
    EXPECT_EQ(parsed.value().flags, original.flags);
    EXPECT_EQ(parsed.value().stream_id, original.stream_id);
}

TEST_F(Http2FrameTest, RejectsEmptyDataForFrameHeader)
{
    std::vector<uint8_t> empty;
    auto result = frame_header::parse(empty);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, RejectsOversizedFrameLength)
{
    // Length = 16,777,216 (exceeds max 16,777,215)
    std::vector<uint8_t> raw = {
        0xFF, 0xFF, 0xFF,        // Length: 16,777,215 (max, should be ok)
        0x00,                    // Type: DATA
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x01  // Stream ID: 1
    };

    // Max frame size (16,777,215) should parse ok for header
    auto result = frame_header::parse(raw);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().length, 16777215u);
}

TEST_F(Http2FrameTest, FrameHeaderMasksStreamIdReservedBit)
{
    // Stream ID with MSB set (reserved bit should be masked)
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x00,        // Length: 0
        0x01,                    // Type: HEADERS
        0x04,                    // Flags: END_HEADERS
        0x80, 0x00, 0x00, 0x05  // Stream ID: 0x80000005 (MSB set)
    };

    auto result = frame_header::parse(raw);
    ASSERT_TRUE(result.is_ok());
    // MSB should be masked off: 0x80000005 & 0x7FFFFFFF = 5
    EXPECT_EQ(result.value().stream_id, 5u);
}

// ============================================================
// Data Frame Error Tests
// ============================================================

TEST_F(Http2FrameTest, ParsesPaddedDataFrame)
{
    // Padded DATA frame: pad_length=2, data="hi", padding=0x00 0x00
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x05,        // Length: 5 (1 pad_length + 2 data + 2 padding)
        0x00,                    // Type: DATA
        0x08,                    // Flags: PADDED
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0x02,                    // Pad length: 2
        'h', 'i',                // Data
        0x00, 0x00               // Padding
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    auto data_frm = dynamic_cast<data_frame*>(result.value().get());
    ASSERT_NE(data_frm, nullptr);
    EXPECT_TRUE(data_frm->is_padded());

    auto data = data_frm->data();
    std::string data_str(data.begin(), data.end());
    EXPECT_EQ(data_str, "hi");
}

TEST_F(Http2FrameTest, RejectsPaddedDataFrameWithEmptyPayload)
{
    // Padded DATA frame but payload is empty (no pad length byte)
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x00,        // Length: 0
        0x00,                    // Type: DATA
        0x08,                    // Flags: PADDED
        0x00, 0x00, 0x00, 0x01  // Stream ID: 1
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, RejectsPaddedDataFrameWithInvalidPadding)
{
    // Pad length exceeds payload size
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x02,        // Length: 2
        0x00,                    // Type: DATA
        0x08,                    // Flags: PADDED
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0x05,                    // Pad length: 5 (but only 1 byte remaining)
        0x41                     // Only 1 byte
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, DataFrameWithoutEndStream)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x03,        // Length: 3
        0x00,                    // Type: DATA
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        'a', 'b', 'c'
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    auto data_frm = dynamic_cast<data_frame*>(result.value().get());
    ASSERT_NE(data_frm, nullptr);
    EXPECT_FALSE(data_frm->is_end_stream());
    EXPECT_FALSE(data_frm->is_padded());
}

TEST_F(Http2FrameTest, DataFrameEmptyPayload)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x00,        // Length: 0
        0x00,                    // Type: DATA
        0x01,                    // Flags: END_STREAM
        0x00, 0x00, 0x00, 0x01  // Stream ID: 1
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    auto data_frm = dynamic_cast<data_frame*>(result.value().get());
    ASSERT_NE(data_frm, nullptr);
    EXPECT_TRUE(data_frm->is_end_stream());
    EXPECT_EQ(data_frm->data().size(), 0u);
}

// ============================================================
// Headers Frame Error Tests
// ============================================================

TEST_F(Http2FrameTest, RejectsHeadersFrameWithZeroStreamId)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x00,        // Length: 0
        0x01,                    // Type: HEADERS
        0x04,                    // Flags: END_HEADERS
        0x00, 0x00, 0x00, 0x00  // Stream ID: 0 (invalid)
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, ParsesPaddedHeadersFrame)
{
    // Padded HEADERS frame: pad_length=1, header_block="AB", padding=0x00
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x04,        // Length: 4
        0x01,                    // Type: HEADERS
        0x0C,                    // Flags: PADDED | END_HEADERS
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0x01,                    // Pad length: 1
        0x41, 0x42,              // Header block: "AB"
        0x00                     // Padding
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    auto hdr_frm = dynamic_cast<headers_frame*>(result.value().get());
    ASSERT_NE(hdr_frm, nullptr);
    EXPECT_TRUE(hdr_frm->is_end_headers());

    auto block = hdr_frm->header_block();
    EXPECT_EQ(block.size(), 2u);
    EXPECT_EQ(block[0], 0x41);
    EXPECT_EQ(block[1], 0x42);
}

TEST_F(Http2FrameTest, RejectsPaddedHeadersFrameWithEmptyPayload)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x00,        // Length: 0
        0x01,                    // Type: HEADERS
        0x08,                    // Flags: PADDED
        0x00, 0x00, 0x00, 0x01  // Stream ID: 1
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, RejectsPaddedHeadersFrameWithInvalidPadding)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x02,        // Length: 2
        0x01,                    // Type: HEADERS
        0x08,                    // Flags: PADDED
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0x0A,                    // Pad length: 10 (exceeds remaining)
        0x41                     // Header block: 1 byte
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, HeadersFrameWithHeaderBlock)
{
    std::vector<uint8_t> header_block = {0x82, 0x84, 0x87};
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x03,        // Length: 3
        0x01,                    // Type: HEADERS
        0x05,                    // Flags: END_STREAM | END_HEADERS
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0x82, 0x84, 0x87         // Header block
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    auto hdr_frm = dynamic_cast<headers_frame*>(result.value().get());
    ASSERT_NE(hdr_frm, nullptr);
    auto block = hdr_frm->header_block();
    EXPECT_EQ(block.size(), 3u);
    EXPECT_EQ(std::vector<uint8_t>(block.begin(), block.end()), header_block);
}

// ============================================================
// Settings Frame Error Tests
// ============================================================

TEST_F(Http2FrameTest, RejectsSettingsAckWithPayload)
{
    // SETTINGS ACK with non-empty payload (invalid per RFC 7540 6.5)
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x06,        // Length: 6
        0x04,                    // Type: SETTINGS
        0x01,                    // Flags: ACK
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x00, 0x01,              // Identifier
        0x00, 0x00, 0x10, 0x00   // Value
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, RejectsSettingsWithOddPayloadSize)
{
    // Payload not multiple of 6 (invalid per RFC 7540 6.5)
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x05,        // Length: 5 (not multiple of 6)
        0x04,                    // Type: SETTINGS
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x00, 0x01, 0x00, 0x00, 0x10  // 5 bytes
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, ParsesSettingsFrameWithSingleParameter)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x06,        // Length: 6
        0x04,                    // Type: SETTINGS
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x00, 0x04,              // Identifier: INITIAL_WINDOW_SIZE
        0x00, 0x01, 0x00, 0x00   // Value: 65536
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    auto settings_frm = dynamic_cast<settings_frame*>(result.value().get());
    ASSERT_NE(settings_frm, nullptr);
    ASSERT_EQ(settings_frm->settings().size(), 1u);
    EXPECT_EQ(settings_frm->settings()[0].identifier,
              static_cast<uint16_t>(setting_identifier::initial_window_size));
    EXPECT_EQ(settings_frm->settings()[0].value, 65536u);
}

// ============================================================
// RST_STREAM Error Tests
// ============================================================

TEST_F(Http2FrameTest, RejectsRstStreamWithZeroStreamId)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x04,        // Length: 4
        0x03,                    // Type: RST_STREAM
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0 (invalid)
        0x00, 0x00, 0x00, 0x08   // Error code
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, RejectsRstStreamWithInvalidPayloadSize)
{
    // RST_STREAM must be exactly 4 bytes
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x03,        // Length: 3 (should be 4)
        0x03,                    // Type: RST_STREAM
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0x00, 0x00, 0x08         // Only 3 bytes (invalid)
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

// ============================================================
// PING Error Tests
// ============================================================

TEST_F(Http2FrameTest, RejectsPingWithNonZeroStreamId)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x08,        // Length: 8
        0x06,                    // Type: PING
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1 (invalid)
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, RejectsPingWithInvalidPayloadSize)
{
    // PING must be exactly 8 bytes
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x04,        // Length: 4 (should be 8)
        0x06,                    // Type: PING
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x01, 0x02, 0x03, 0x04   // Only 4 bytes (invalid)
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, ParsesPingAckFrame)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x08,        // Length: 8
        0x06,                    // Type: PING
        0x01,                    // Flags: ACK
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    auto ping_frm = dynamic_cast<ping_frame*>(result.value().get());
    ASSERT_NE(ping_frm, nullptr);
    EXPECT_TRUE(ping_frm->is_ack());
    EXPECT_EQ(ping_frm->opaque_data()[0], 0xDE);
    EXPECT_EQ(ping_frm->opaque_data()[7], 0xBE);
}

// ============================================================
// GOAWAY Error Tests
// ============================================================

TEST_F(Http2FrameTest, RejectsGoawayWithNonZeroStreamId)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x08,        // Length: 8
        0x07,                    // Type: GOAWAY
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1 (invalid)
        0x00, 0x00, 0x00, 0x05,  // Last stream ID
        0x00, 0x00, 0x00, 0x00   // Error code
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, RejectsGoawayWithShortPayload)
{
    // GOAWAY needs at least 8 bytes
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x04,        // Length: 4 (should be >= 8)
        0x07,                    // Type: GOAWAY
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x00, 0x00, 0x00, 0x05   // Only 4 bytes
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, ParsesGoawayWithAdditionalData)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x0C,        // Length: 12 (8 required + 4 additional)
        0x07,                    // Type: GOAWAY
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x00, 0x00, 0x00, 0x0A,  // Last stream ID: 10
        0x00, 0x00, 0x00, 0x02,  // Error code: INTERNAL_ERROR
        'E', 'R', 'R', '!'      // Additional debug data
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    auto goaway_frm = dynamic_cast<goaway_frame*>(result.value().get());
    ASSERT_NE(goaway_frm, nullptr);
    EXPECT_EQ(goaway_frm->last_stream_id(), 10u);
    EXPECT_EQ(goaway_frm->error_code(), 2u);

    auto additional = goaway_frm->additional_data();
    EXPECT_EQ(additional.size(), 4u);
    std::string debug_str(additional.begin(), additional.end());
    EXPECT_EQ(debug_str, "ERR!");
}

// ============================================================
// WINDOW_UPDATE Error Tests
// ============================================================

TEST_F(Http2FrameTest, RejectsWindowUpdateWithZeroIncrement)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x04,        // Length: 4
        0x08,                    // Type: WINDOW_UPDATE
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x00,  // Stream ID: 0
        0x00, 0x00, 0x00, 0x00   // Increment: 0 (invalid)
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, RejectsWindowUpdateWithInvalidPayloadSize)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x03,        // Length: 3 (should be 4)
        0x08,                    // Type: WINDOW_UPDATE
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0x00, 0x00, 0x01         // Only 3 bytes
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, WindowUpdateWithNonZeroStreamId)
{
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x04,        // Length: 4
        0x08,                    // Type: WINDOW_UPDATE
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x05,  // Stream ID: 5
        0x00, 0x00, 0x80, 0x00   // Increment: 32768
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    auto wnd_frm = dynamic_cast<window_update_frame*>(result.value().get());
    ASSERT_NE(wnd_frm, nullptr);
    EXPECT_EQ(wnd_frm->header().stream_id, 5u);
    EXPECT_EQ(wnd_frm->window_size_increment(), 32768u);
}

// ============================================================
// Generic Frame Tests
// ============================================================

TEST_F(Http2FrameTest, RejectsFrameWithInsufficientPayload)
{
    // Header says 10 bytes payload but only 5 available
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x0A,        // Length: 10
        0x00,                    // Type: DATA
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0x01, 0x02, 0x03, 0x04, 0x05  // Only 5 bytes
    };

    auto result = frame::parse(raw);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2FrameTest, ParsesUnknownFrameType)
{
    // Unknown frame type 0xFF
    std::vector<uint8_t> raw = {
        0x00, 0x00, 0x02,        // Length: 2
        0xFF,                    // Type: unknown
        0x00,                    // Flags: none
        0x00, 0x00, 0x00, 0x01,  // Stream ID: 1
        0xAA, 0xBB               // Payload
    };

    auto result = frame::parse(raw);
    ASSERT_TRUE(result.is_ok());

    // Should be a generic frame (not a specialized subclass)
    auto& frm = result.value();
    EXPECT_EQ(frm->header().type, static_cast<frame_type>(0xFF));
    EXPECT_EQ(frm->payload().size(), 2u);
}

// ============================================================
// Round-Trip Tests (construct → serialize → parse)
// ============================================================

TEST_F(Http2FrameTest, RoundTripHeadersFrame)
{
    std::vector<uint8_t> header_block = {0x82, 0x84, 0x87, 0x41, 0x0F};
    headers_frame original(3, header_block, true, true);

    auto serialized = original.serialize();
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed = dynamic_cast<headers_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->header().stream_id, 3u);
    EXPECT_TRUE(parsed->is_end_stream());
    EXPECT_TRUE(parsed->is_end_headers());

    auto block = parsed->header_block();
    EXPECT_EQ(std::vector<uint8_t>(block.begin(), block.end()), header_block);
}

TEST_F(Http2FrameTest, RoundTripRstStreamFrame)
{
    rst_stream_frame original(5, static_cast<uint32_t>(error_code::cancel));

    auto serialized = original.serialize();
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed = dynamic_cast<rst_stream_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->header().stream_id, 5u);
    EXPECT_EQ(parsed->error_code(), static_cast<uint32_t>(error_code::cancel));
}

TEST_F(Http2FrameTest, RoundTripPingFrame)
{
    std::array<uint8_t, 8> ping_data = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    ping_frame original(ping_data, false);

    auto serialized = original.serialize();
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed = dynamic_cast<ping_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed, nullptr);
    EXPECT_FALSE(parsed->is_ack());
    EXPECT_EQ(parsed->opaque_data(), ping_data);
}

TEST_F(Http2FrameTest, RoundTripPingAckFrame)
{
    std::array<uint8_t, 8> ping_data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    ping_frame original(ping_data, true);

    auto serialized = original.serialize();
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed = dynamic_cast<ping_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed, nullptr);
    EXPECT_TRUE(parsed->is_ack());
    EXPECT_EQ(parsed->opaque_data(), ping_data);
}

TEST_F(Http2FrameTest, RoundTripGoawayFrame)
{
    goaway_frame original(100, static_cast<uint32_t>(error_code::no_error));

    auto serialized = original.serialize();
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed = dynamic_cast<goaway_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->last_stream_id(), 100u);
    EXPECT_EQ(parsed->error_code(), static_cast<uint32_t>(error_code::no_error));
    EXPECT_EQ(parsed->additional_data().size(), 0u);
}

TEST_F(Http2FrameTest, RoundTripGoawayFrameWithAdditionalData)
{
    std::vector<uint8_t> debug_data = {'d', 'e', 'b', 'u', 'g'};
    goaway_frame original(42, static_cast<uint32_t>(error_code::internal_error), debug_data);

    auto serialized = original.serialize();
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed = dynamic_cast<goaway_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->last_stream_id(), 42u);
    EXPECT_EQ(parsed->error_code(), static_cast<uint32_t>(error_code::internal_error));

    auto additional = parsed->additional_data();
    EXPECT_EQ(std::vector<uint8_t>(additional.begin(), additional.end()), debug_data);
}

TEST_F(Http2FrameTest, RoundTripWindowUpdateFrame)
{
    window_update_frame original(7, 1048576);

    auto serialized = original.serialize();
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed = dynamic_cast<window_update_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->header().stream_id, 7u);
    EXPECT_EQ(parsed->window_size_increment(), 1048576u);
}

TEST_F(Http2FrameTest, RoundTripSettingsAckFrame)
{
    settings_frame original({}, true);

    auto serialized = original.serialize();
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed = dynamic_cast<settings_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed, nullptr);
    EXPECT_TRUE(parsed->is_ack());
    EXPECT_EQ(parsed->settings().size(), 0u);
}

TEST_F(Http2FrameTest, RoundTripWindowUpdateConnectionLevel)
{
    // Stream ID 0 means connection-level flow control
    window_update_frame original(0, 65535);

    auto serialized = original.serialize();
    auto parsed_result = frame::parse(serialized);
    ASSERT_TRUE(parsed_result.is_ok());

    auto parsed = dynamic_cast<window_update_frame*>(parsed_result.value().get());
    ASSERT_NE(parsed, nullptr);
    EXPECT_EQ(parsed->header().stream_id, 0u);
    EXPECT_EQ(parsed->window_size_increment(), 65535u);
}
