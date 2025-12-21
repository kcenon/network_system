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
#include "kcenon/network/protocols/http2/frame.h"
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
