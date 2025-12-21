/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "kcenon/network/protocols/quic/frame.h"
#include "kcenon/network/protocols/quic/frame_types.h"

namespace kcenon::network::protocols::quic
{
namespace
{

class FrameTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ============================================================================
// Frame Type Helpers Tests
// ============================================================================

TEST_F(FrameTest, IsStreamFrame)
{
    EXPECT_FALSE(is_stream_frame(0x00));  // PADDING
    EXPECT_FALSE(is_stream_frame(0x07));  // NEW_TOKEN
    EXPECT_TRUE(is_stream_frame(0x08));   // STREAM base
    EXPECT_TRUE(is_stream_frame(0x09));   // STREAM + FIN
    EXPECT_TRUE(is_stream_frame(0x0a));   // STREAM + LEN
    EXPECT_TRUE(is_stream_frame(0x0f));   // STREAM + all flags
    EXPECT_FALSE(is_stream_frame(0x10));  // MAX_DATA
}

TEST_F(FrameTest, GetStreamFlags)
{
    EXPECT_EQ(get_stream_flags(0x08), 0x00);
    EXPECT_EQ(get_stream_flags(0x09), stream_flags::fin);
    EXPECT_EQ(get_stream_flags(0x0a), stream_flags::len);
    EXPECT_EQ(get_stream_flags(0x0c), stream_flags::off);
    EXPECT_EQ(get_stream_flags(0x0f), 0x07);
}

TEST_F(FrameTest, MakeStreamType)
{
    EXPECT_EQ(make_stream_type(false, false, false), 0x08);
    EXPECT_EQ(make_stream_type(true, false, false), 0x09);
    EXPECT_EQ(make_stream_type(false, true, false), 0x0a);
    EXPECT_EQ(make_stream_type(false, false, true), 0x0c);
    EXPECT_EQ(make_stream_type(true, true, true), 0x0f);
}

TEST_F(FrameTest, FrameTypeToString)
{
    EXPECT_EQ(frame_type_to_string(frame_type::padding), "PADDING");
    EXPECT_EQ(frame_type_to_string(frame_type::ping), "PING");
    EXPECT_EQ(frame_type_to_string(frame_type::ack), "ACK");
    EXPECT_EQ(frame_type_to_string(frame_type::crypto), "CRYPTO");
    EXPECT_EQ(frame_type_to_string(frame_type::stream_base), "STREAM");
    EXPECT_EQ(frame_type_to_string(frame_type::connection_close), "CONNECTION_CLOSE");
}

// ============================================================================
// PADDING Frame Tests
// ============================================================================

TEST_F(FrameTest, PaddingBuildAndParse)
{
    auto built = frame_builder::build_padding(5);
    ASSERT_EQ(built.size(), 5);
    for (auto byte : built)
    {
        EXPECT_EQ(byte, 0x00);
    }

    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());
    auto& [f, consumed] = result.value();
    EXPECT_EQ(consumed, 5);

    auto* padding = std::get_if<padding_frame>(&f);
    ASSERT_NE(padding, nullptr);
    EXPECT_EQ(padding->count, 5);
}

// ============================================================================
// PING Frame Tests
// ============================================================================

TEST_F(FrameTest, PingBuildAndParse)
{
    auto built = frame_builder::build_ping();
    ASSERT_EQ(built.size(), 1);
    EXPECT_EQ(built[0], 0x01);

    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());
    auto& [f, consumed] = result.value();
    EXPECT_EQ(consumed, 1);

    auto* ping = std::get_if<ping_frame>(&f);
    ASSERT_NE(ping, nullptr);
}

// ============================================================================
// CRYPTO Frame Tests
// ============================================================================

TEST_F(FrameTest, CryptoBuildAndParse)
{
    crypto_frame original;
    original.offset = 100;
    original.data = {0x01, 0x02, 0x03, 0x04, 0x05};

    auto built = frame_builder::build_crypto(original);
    ASSERT_GT(built.size(), 0);

    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());
    auto& [f, consumed] = result.value();
    EXPECT_EQ(consumed, built.size());

    auto* crypto = std::get_if<crypto_frame>(&f);
    ASSERT_NE(crypto, nullptr);
    EXPECT_EQ(crypto->offset, 100);
    EXPECT_EQ(crypto->data, original.data);
}

TEST_F(FrameTest, CryptoEmptyData)
{
    crypto_frame original;
    original.offset = 0;
    original.data = {};

    auto built = frame_builder::build_crypto(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* crypto = std::get_if<crypto_frame>(&result.value().first);
    ASSERT_NE(crypto, nullptr);
    EXPECT_EQ(crypto->offset, 0);
    EXPECT_TRUE(crypto->data.empty());
}

// ============================================================================
// STREAM Frame Tests
// ============================================================================

TEST_F(FrameTest, StreamBasicBuildAndParse)
{
    stream_frame original;
    original.stream_id = 4;
    original.offset = 0;
    original.data = {'H', 'e', 'l', 'l', 'o'};
    original.fin = false;

    auto built = frame_builder::build_stream(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* stream = std::get_if<stream_frame>(&result.value().first);
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->stream_id, 4);
    EXPECT_EQ(stream->offset, 0);
    EXPECT_EQ(stream->data, original.data);
    EXPECT_FALSE(stream->fin);
}

TEST_F(FrameTest, StreamWithOffset)
{
    stream_frame original;
    original.stream_id = 8;
    original.offset = 1000;
    original.data = {0xAB, 0xCD};
    original.fin = false;

    auto built = frame_builder::build_stream(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* stream = std::get_if<stream_frame>(&result.value().first);
    ASSERT_NE(stream, nullptr);
    EXPECT_EQ(stream->stream_id, 8);
    EXPECT_EQ(stream->offset, 1000);
    EXPECT_EQ(stream->data, original.data);
}

TEST_F(FrameTest, StreamWithFin)
{
    stream_frame original;
    original.stream_id = 0;
    original.offset = 0;
    original.data = {};
    original.fin = true;

    auto built = frame_builder::build_stream(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* stream = std::get_if<stream_frame>(&result.value().first);
    ASSERT_NE(stream, nullptr);
    EXPECT_TRUE(stream->fin);
}

// ============================================================================
// ACK Frame Tests
// ============================================================================

TEST_F(FrameTest, AckBasicBuildAndParse)
{
    ack_frame original;
    original.largest_acknowledged = 100;
    original.ack_delay = 50;

    auto built = frame_builder::build_ack(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* ack = std::get_if<ack_frame>(&result.value().first);
    ASSERT_NE(ack, nullptr);
    EXPECT_EQ(ack->largest_acknowledged, 100);
    EXPECT_EQ(ack->ack_delay, 50);
    EXPECT_FALSE(ack->ecn.has_value());
}

TEST_F(FrameTest, AckWithEcn)
{
    ack_frame original;
    original.largest_acknowledged = 200;
    original.ack_delay = 25;
    original.ecn = ecn_counts{10, 20, 5};

    auto built = frame_builder::build_ack(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* ack = std::get_if<ack_frame>(&result.value().first);
    ASSERT_NE(ack, nullptr);
    EXPECT_EQ(ack->largest_acknowledged, 200);
    ASSERT_TRUE(ack->ecn.has_value());
    EXPECT_EQ(ack->ecn->ect0, 10);
    EXPECT_EQ(ack->ecn->ect1, 20);
    EXPECT_EQ(ack->ecn->ecn_ce, 5);
}

// ============================================================================
// CONNECTION_CLOSE Frame Tests
// ============================================================================

TEST_F(FrameTest, ConnectionCloseTransport)
{
    connection_close_frame original;
    original.error_code = 0x0A;  // PROTOCOL_VIOLATION
    original.frame_type = 0x06; // CRYPTO frame
    original.reason_phrase = "Invalid crypto data";
    original.is_application_error = false;

    auto built = frame_builder::build_connection_close(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* close = std::get_if<connection_close_frame>(&result.value().first);
    ASSERT_NE(close, nullptr);
    EXPECT_EQ(close->error_code, 0x0A);
    EXPECT_EQ(close->frame_type, 0x06);
    EXPECT_EQ(close->reason_phrase, "Invalid crypto data");
    EXPECT_FALSE(close->is_application_error);
}

TEST_F(FrameTest, ConnectionCloseApplication)
{
    connection_close_frame original;
    original.error_code = 1001;
    original.reason_phrase = "User disconnected";
    original.is_application_error = true;

    auto built = frame_builder::build_connection_close(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* close = std::get_if<connection_close_frame>(&result.value().first);
    ASSERT_NE(close, nullptr);
    EXPECT_EQ(close->error_code, 1001);
    EXPECT_EQ(close->reason_phrase, "User disconnected");
    EXPECT_TRUE(close->is_application_error);
}

// ============================================================================
// Flow Control Frame Tests
// ============================================================================

TEST_F(FrameTest, MaxDataBuildAndParse)
{
    max_data_frame original;
    original.maximum_data = 1048576;  // 1 MB

    auto built = frame_builder::build_max_data(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* max_data = std::get_if<max_data_frame>(&result.value().first);
    ASSERT_NE(max_data, nullptr);
    EXPECT_EQ(max_data->maximum_data, 1048576);
}

TEST_F(FrameTest, MaxStreamDataBuildAndParse)
{
    max_stream_data_frame original;
    original.stream_id = 4;
    original.maximum_stream_data = 65536;

    auto built = frame_builder::build_max_stream_data(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* max_stream_data = std::get_if<max_stream_data_frame>(&result.value().first);
    ASSERT_NE(max_stream_data, nullptr);
    EXPECT_EQ(max_stream_data->stream_id, 4);
    EXPECT_EQ(max_stream_data->maximum_stream_data, 65536);
}

TEST_F(FrameTest, MaxStreamsBidiBuildAndParse)
{
    max_streams_frame original;
    original.maximum_streams = 100;
    original.bidirectional = true;

    auto built = frame_builder::build_max_streams(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* max_streams = std::get_if<max_streams_frame>(&result.value().first);
    ASSERT_NE(max_streams, nullptr);
    EXPECT_EQ(max_streams->maximum_streams, 100);
    EXPECT_TRUE(max_streams->bidirectional);
}

TEST_F(FrameTest, MaxStreamsUniBuildAndParse)
{
    max_streams_frame original;
    original.maximum_streams = 50;
    original.bidirectional = false;

    auto built = frame_builder::build_max_streams(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* max_streams = std::get_if<max_streams_frame>(&result.value().first);
    ASSERT_NE(max_streams, nullptr);
    EXPECT_EQ(max_streams->maximum_streams, 50);
    EXPECT_FALSE(max_streams->bidirectional);
}

// ============================================================================
// Path Validation Frame Tests
// ============================================================================

TEST_F(FrameTest, PathChallengeBuildAndParse)
{
    path_challenge_frame original;
    original.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    auto built = frame_builder::build_path_challenge(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* challenge = std::get_if<path_challenge_frame>(&result.value().first);
    ASSERT_NE(challenge, nullptr);
    EXPECT_EQ(challenge->data, original.data);
}

TEST_F(FrameTest, PathResponseBuildAndParse)
{
    path_response_frame original;
    original.data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};

    auto built = frame_builder::build_path_response(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* response = std::get_if<path_response_frame>(&result.value().first);
    ASSERT_NE(response, nullptr);
    EXPECT_EQ(response->data, original.data);
}

// ============================================================================
// Connection ID Frame Tests
// ============================================================================

TEST_F(FrameTest, NewConnectionIdBuildAndParse)
{
    new_connection_id_frame original;
    original.sequence_number = 1;
    original.retire_prior_to = 0;
    original.connection_id = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    original.stateless_reset_token = {
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
    };

    auto built = frame_builder::build_new_connection_id(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* new_cid = std::get_if<new_connection_id_frame>(&result.value().first);
    ASSERT_NE(new_cid, nullptr);
    EXPECT_EQ(new_cid->sequence_number, 1);
    EXPECT_EQ(new_cid->retire_prior_to, 0);
    EXPECT_EQ(new_cid->connection_id, original.connection_id);
    EXPECT_EQ(new_cid->stateless_reset_token, original.stateless_reset_token);
}

TEST_F(FrameTest, RetireConnectionIdBuildAndParse)
{
    retire_connection_id_frame original;
    original.sequence_number = 5;

    auto built = frame_builder::build_retire_connection_id(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* retire = std::get_if<retire_connection_id_frame>(&result.value().first);
    ASSERT_NE(retire, nullptr);
    EXPECT_EQ(retire->sequence_number, 5);
}

// ============================================================================
// Stream Control Frame Tests
// ============================================================================

TEST_F(FrameTest, ResetStreamBuildAndParse)
{
    reset_stream_frame original;
    original.stream_id = 8;
    original.application_error_code = 0x100;
    original.final_size = 1024;

    auto built = frame_builder::build_reset_stream(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* reset = std::get_if<reset_stream_frame>(&result.value().first);
    ASSERT_NE(reset, nullptr);
    EXPECT_EQ(reset->stream_id, 8);
    EXPECT_EQ(reset->application_error_code, 0x100);
    EXPECT_EQ(reset->final_size, 1024);
}

TEST_F(FrameTest, StopSendingBuildAndParse)
{
    stop_sending_frame original;
    original.stream_id = 12;
    original.application_error_code = 0x200;

    auto built = frame_builder::build_stop_sending(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* stop = std::get_if<stop_sending_frame>(&result.value().first);
    ASSERT_NE(stop, nullptr);
    EXPECT_EQ(stop->stream_id, 12);
    EXPECT_EQ(stop->application_error_code, 0x200);
}

// ============================================================================
// HANDSHAKE_DONE Frame Tests
// ============================================================================

TEST_F(FrameTest, HandshakeDoneBuildAndParse)
{
    auto built = frame_builder::build_handshake_done();
    ASSERT_EQ(built.size(), 1);
    EXPECT_EQ(built[0], static_cast<uint8_t>(frame_type::handshake_done));

    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* done = std::get_if<handshake_done_frame>(&result.value().first);
    ASSERT_NE(done, nullptr);
}

// ============================================================================
// NEW_TOKEN Frame Tests
// ============================================================================

TEST_F(FrameTest, NewTokenBuildAndParse)
{
    new_token_frame original;
    original.token = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};

    auto built = frame_builder::build_new_token(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* token = std::get_if<new_token_frame>(&result.value().first);
    ASSERT_NE(token, nullptr);
    EXPECT_EQ(token->token, original.token);
}

// ============================================================================
// Blocked Frame Tests
// ============================================================================

TEST_F(FrameTest, DataBlockedBuildAndParse)
{
    data_blocked_frame original;
    original.maximum_data = 500000;

    auto built = frame_builder::build_data_blocked(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* blocked = std::get_if<data_blocked_frame>(&result.value().first);
    ASSERT_NE(blocked, nullptr);
    EXPECT_EQ(blocked->maximum_data, 500000);
}

TEST_F(FrameTest, StreamDataBlockedBuildAndParse)
{
    stream_data_blocked_frame original;
    original.stream_id = 4;
    original.maximum_stream_data = 32768;

    auto built = frame_builder::build_stream_data_blocked(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* blocked = std::get_if<stream_data_blocked_frame>(&result.value().first);
    ASSERT_NE(blocked, nullptr);
    EXPECT_EQ(blocked->stream_id, 4);
    EXPECT_EQ(blocked->maximum_stream_data, 32768);
}

TEST_F(FrameTest, StreamsBlockedBidiBuildAndParse)
{
    streams_blocked_frame original;
    original.maximum_streams = 100;
    original.bidirectional = true;

    auto built = frame_builder::build_streams_blocked(original);
    auto result = frame_parser::parse(built);
    ASSERT_TRUE(result.is_ok());

    auto* blocked = std::get_if<streams_blocked_frame>(&result.value().first);
    ASSERT_NE(blocked, nullptr);
    EXPECT_EQ(blocked->maximum_streams, 100);
    EXPECT_TRUE(blocked->bidirectional);
}

// ============================================================================
// Generic Frame Builder Tests
// ============================================================================

TEST_F(FrameTest, BuildFromVariant)
{
    // Test build() with variant
    frame f = ping_frame{};
    auto built = frame_builder::build(f);
    EXPECT_EQ(built.size(), 1);
    EXPECT_EQ(built[0], 0x01);
}

TEST_F(FrameTest, GetFrameType)
{
    EXPECT_EQ(get_frame_type(frame{padding_frame{}}), frame_type::padding);
    EXPECT_EQ(get_frame_type(frame{ping_frame{}}), frame_type::ping);
    EXPECT_EQ(get_frame_type(frame{crypto_frame{}}), frame_type::crypto);
    EXPECT_EQ(get_frame_type(frame{stream_frame{}}), frame_type::stream_base);
    EXPECT_EQ(get_frame_type(frame{handshake_done_frame{}}), frame_type::handshake_done);
}

// ============================================================================
// Parse All Frames Tests
// ============================================================================

TEST_F(FrameTest, ParseAllMultipleFrames)
{
    // Build multiple frames
    std::vector<uint8_t> buffer;

    auto ping = frame_builder::build_ping();
    buffer.insert(buffer.end(), ping.begin(), ping.end());

    max_data_frame md;
    md.maximum_data = 1024;
    auto max_data = frame_builder::build_max_data(md);
    buffer.insert(buffer.end(), max_data.begin(), max_data.end());

    auto done = frame_builder::build_handshake_done();
    buffer.insert(buffer.end(), done.begin(), done.end());

    // Parse all
    auto result = frame_parser::parse_all(buffer);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().size(), 3);

    EXPECT_NE(std::get_if<ping_frame>(&result.value()[0]), nullptr);
    EXPECT_NE(std::get_if<max_data_frame>(&result.value()[1]), nullptr);
    EXPECT_NE(std::get_if<handshake_done_frame>(&result.value()[2]), nullptr);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(FrameTest, ParseEmptyBuffer)
{
    std::vector<uint8_t> empty;
    auto result = frame_parser::parse(empty);
    EXPECT_TRUE(result.is_err());
}

TEST_F(FrameTest, ParseInvalidFrameType)
{
    // Frame type 0xFF is not defined in RFC 9000
    std::vector<uint8_t> data = {0xFF};
    auto result = frame_parser::parse(data);
    EXPECT_TRUE(result.is_err());
}

TEST_F(FrameTest, ParseInsufficientData)
{
    // CRYPTO frame needs more data after type
    std::vector<uint8_t> data = {0x06};  // Just the type
    auto result = frame_parser::parse(data);
    EXPECT_TRUE(result.is_err());
}

TEST_F(FrameTest, PeekType)
{
    auto ping = frame_builder::build_ping();
    auto result = frame_parser::peek_type(ping);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value().first, static_cast<uint64_t>(frame_type::ping));
    EXPECT_EQ(result.value().second, 1);
}

}  // namespace
}  // namespace kcenon::network::protocols::quic
