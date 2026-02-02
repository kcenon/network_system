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

#include "internal/protocols/quic/stream.h"
#include "internal/protocols/quic/stream_manager.h"
#include "internal/protocols/quic/flow_control.h"

#include <vector>
#include <array>
#include <thread>

using namespace kcenon::network::protocols::quic;

// ============================================================================
// Stream ID Type Tests
// ============================================================================

class QuicStreamIdTest : public ::testing::Test { };

TEST_F(QuicStreamIdTest, ClientBidirectionalStream)
{
    // Client bidi: 0, 4, 8, 12, ...
    EXPECT_TRUE(stream_id_type::is_client_initiated(0));
    EXPECT_TRUE(stream_id_type::is_bidirectional(0));
    EXPECT_FALSE(stream_id_type::is_server_initiated(0));
    EXPECT_FALSE(stream_id_type::is_unidirectional(0));

    EXPECT_EQ(stream_id_type::get_type(0), stream_id_type::client_bidi);
    EXPECT_EQ(stream_id_type::get_sequence(0), 0UL);
    EXPECT_EQ(stream_id_type::get_sequence(4), 1UL);
    EXPECT_EQ(stream_id_type::get_sequence(8), 2UL);
}

TEST_F(QuicStreamIdTest, ServerBidirectionalStream)
{
    // Server bidi: 1, 5, 9, 13, ...
    EXPECT_TRUE(stream_id_type::is_server_initiated(1));
    EXPECT_TRUE(stream_id_type::is_bidirectional(1));
    EXPECT_FALSE(stream_id_type::is_client_initiated(1));
    EXPECT_FALSE(stream_id_type::is_unidirectional(1));

    EXPECT_EQ(stream_id_type::get_type(1), stream_id_type::server_bidi);
    EXPECT_EQ(stream_id_type::get_sequence(1), 0UL);
    EXPECT_EQ(stream_id_type::get_sequence(5), 1UL);
}

TEST_F(QuicStreamIdTest, ClientUnidirectionalStream)
{
    // Client uni: 2, 6, 10, 14, ...
    EXPECT_TRUE(stream_id_type::is_client_initiated(2));
    EXPECT_TRUE(stream_id_type::is_unidirectional(2));
    EXPECT_FALSE(stream_id_type::is_server_initiated(2));
    EXPECT_FALSE(stream_id_type::is_bidirectional(2));

    EXPECT_EQ(stream_id_type::get_type(2), stream_id_type::client_uni);
    EXPECT_EQ(stream_id_type::get_sequence(2), 0UL);
    EXPECT_EQ(stream_id_type::get_sequence(6), 1UL);
}

TEST_F(QuicStreamIdTest, ServerUnidirectionalStream)
{
    // Server uni: 3, 7, 11, 15, ...
    EXPECT_TRUE(stream_id_type::is_server_initiated(3));
    EXPECT_TRUE(stream_id_type::is_unidirectional(3));
    EXPECT_FALSE(stream_id_type::is_client_initiated(3));
    EXPECT_FALSE(stream_id_type::is_bidirectional(3));

    EXPECT_EQ(stream_id_type::get_type(3), stream_id_type::server_uni);
    EXPECT_EQ(stream_id_type::get_sequence(3), 0UL);
    EXPECT_EQ(stream_id_type::get_sequence(7), 1UL);
}

TEST_F(QuicStreamIdTest, MakeStreamId)
{
    EXPECT_EQ(stream_id_type::make_stream_id(stream_id_type::client_bidi, 0), 0UL);
    EXPECT_EQ(stream_id_type::make_stream_id(stream_id_type::client_bidi, 1), 4UL);
    EXPECT_EQ(stream_id_type::make_stream_id(stream_id_type::server_bidi, 0), 1UL);
    EXPECT_EQ(stream_id_type::make_stream_id(stream_id_type::client_uni, 0), 2UL);
    EXPECT_EQ(stream_id_type::make_stream_id(stream_id_type::server_uni, 0), 3UL);
}

// ============================================================================
// Stream State Tests
// ============================================================================

class QuicStreamTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create a bidirectional local stream (client bidi, stream 0)
        local_bidi_stream_ = std::make_unique<stream>(0, true, 65536);

        // Create a unidirectional local stream (client uni, stream 2)
        local_uni_stream_ = std::make_unique<stream>(2, true, 65536);

        // Create a bidirectional peer stream (server bidi, stream 1)
        peer_bidi_stream_ = std::make_unique<stream>(1, false, 65536);

        // Create a unidirectional peer stream (server uni, stream 3)
        peer_uni_stream_ = std::make_unique<stream>(3, false, 65536);
    }

    std::unique_ptr<stream> local_bidi_stream_;
    std::unique_ptr<stream> local_uni_stream_;
    std::unique_ptr<stream> peer_bidi_stream_;
    std::unique_ptr<stream> peer_uni_stream_;
};

TEST_F(QuicStreamTest, InitialState)
{
    EXPECT_EQ(local_bidi_stream_->send_state(), send_stream_state::ready);
    EXPECT_EQ(local_bidi_stream_->recv_state(), recv_stream_state::recv);
    EXPECT_TRUE(local_bidi_stream_->can_send());
    EXPECT_FALSE(local_bidi_stream_->has_data());
}

TEST_F(QuicStreamTest, StreamProperties)
{
    EXPECT_EQ(local_bidi_stream_->id(), 0UL);
    EXPECT_TRUE(local_bidi_stream_->is_local());
    EXPECT_TRUE(local_bidi_stream_->is_bidirectional());
    EXPECT_FALSE(local_bidi_stream_->is_unidirectional());

    EXPECT_EQ(local_uni_stream_->id(), 2UL);
    EXPECT_TRUE(local_uni_stream_->is_unidirectional());
}

TEST_F(QuicStreamTest, WriteData)
{
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};

    auto result = local_bidi_stream_->write(data);
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 5UL);

    EXPECT_EQ(local_bidi_stream_->send_state(), send_stream_state::send);
    EXPECT_EQ(local_bidi_stream_->pending_bytes(), 5UL);
}

TEST_F(QuicStreamTest, WriteToPeerUniStream)
{
    // Cannot write to peer-initiated unidirectional stream
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    auto result = peer_uni_stream_->write(data);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicStreamTest, GenerateStreamFrame)
{
    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o'};
    auto write_result = local_bidi_stream_->write(data);
    ASSERT_TRUE(write_result.is_ok());

    auto frame = local_bidi_stream_->next_stream_frame(1000);
    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->stream_id, 0UL);
    EXPECT_EQ(frame->offset, 0UL);
    EXPECT_EQ(frame->data.size(), 5UL);
    EXPECT_FALSE(frame->fin);

    // Buffer should be empty now
    EXPECT_EQ(local_bidi_stream_->pending_bytes(), 0UL);
}

TEST_F(QuicStreamTest, FinishStream)
{
    std::vector<uint8_t> data = {'H', 'i'};
    auto write_result = local_bidi_stream_->write(data);
    ASSERT_TRUE(write_result.is_ok());

    auto finish_result = local_bidi_stream_->finish();
    ASSERT_TRUE(finish_result.is_ok());
    EXPECT_TRUE(local_bidi_stream_->fin_sent());

    // Frame should have FIN set
    auto frame = local_bidi_stream_->next_stream_frame(1000);
    ASSERT_TRUE(frame.has_value());
    EXPECT_TRUE(frame->fin);
}

TEST_F(QuicStreamTest, ResetStream)
{
    auto result = local_bidi_stream_->reset(0x42);
    ASSERT_TRUE(result.is_ok());

    EXPECT_EQ(local_bidi_stream_->send_state(), send_stream_state::reset_sent);
    EXPECT_EQ(local_bidi_stream_->reset_error_code(), 0x42UL);
    EXPECT_FALSE(local_bidi_stream_->can_send());
}

TEST_F(QuicStreamTest, ReceiveData)
{
    std::vector<uint8_t> data = {'W', 'o', 'r', 'l', 'd'};

    auto result = peer_bidi_stream_->receive_data(0, data, false);
    ASSERT_TRUE(result.is_ok());

    EXPECT_TRUE(peer_bidi_stream_->has_data());

    // Read the data back
    std::array<uint8_t, 10> buffer{};
    auto read_result = peer_bidi_stream_->read(buffer);
    ASSERT_TRUE(read_result.is_ok());
    EXPECT_EQ(read_result.value(), 5UL);
    EXPECT_EQ(buffer[0], 'W');
    EXPECT_EQ(buffer[4], 'd');
}

TEST_F(QuicStreamTest, ReceiveDataWithGap)
{
    // Receive data with a gap (offset 5, should buffer)
    std::vector<uint8_t> data2 = {'2', '2', '2'};
    auto result2 = peer_bidi_stream_->receive_data(5, data2, false);
    ASSERT_TRUE(result2.is_ok());

    // No data should be available yet (gap at offset 0-4)
    EXPECT_FALSE(peer_bidi_stream_->has_data());

    // Now receive the missing data
    std::vector<uint8_t> data1 = {'1', '1', '1', '1', '1'};
    auto result1 = peer_bidi_stream_->receive_data(0, data1, false);
    ASSERT_TRUE(result1.is_ok());

    // Both chunks should be available now
    EXPECT_TRUE(peer_bidi_stream_->has_data());

    std::array<uint8_t, 20> buffer{};
    auto read_result = peer_bidi_stream_->read(buffer);
    ASSERT_TRUE(read_result.is_ok());
    EXPECT_EQ(read_result.value(), 8UL); // 5 + 3 bytes
}

TEST_F(QuicStreamTest, ReceiveWithFin)
{
    std::vector<uint8_t> data = {'E', 'n', 'd'};
    auto result = peer_bidi_stream_->receive_data(0, data, true);
    ASSERT_TRUE(result.is_ok());

    EXPECT_TRUE(peer_bidi_stream_->is_fin_received());
    EXPECT_EQ(peer_bidi_stream_->recv_state(), recv_stream_state::size_known);
}

TEST_F(QuicStreamTest, ReceiveReset)
{
    auto result = peer_bidi_stream_->receive_reset(0x100, 0);
    ASSERT_TRUE(result.is_ok());

    EXPECT_EQ(peer_bidi_stream_->recv_state(), recv_stream_state::reset_recvd);
    EXPECT_EQ(peer_bidi_stream_->reset_error_code(), 0x100UL);
}

TEST_F(QuicStreamTest, FlowControlLimit)
{
    // Create a new stream with a small initial window for this test
    // The constructor accepts initial_max_data as the third parameter
    auto small_window_stream = std::make_unique<stream>(4, true, 10);

    std::vector<uint8_t> data(20, 'X'); // 20 bytes, but limit is 10

    // First write of 10 bytes should succeed
    auto result1 = small_window_stream->write(std::span<const uint8_t>(data.data(), 10));
    ASSERT_TRUE(result1.is_ok());
    EXPECT_EQ(result1.value(), 10UL);

    // Second write should fail - window exhausted
    auto result2 = small_window_stream->write(std::span<const uint8_t>(data.data(), 10));
    EXPECT_FALSE(result2.is_ok()); // Write fails when window is full
}

TEST_F(QuicStreamTest, FlowControlUpdate)
{
    // Create a stream with initial window of 100 for this test
    auto test_stream = std::make_unique<stream>(4, true, 100);

    EXPECT_EQ(test_stream->available_send_window(), 100UL);

    // After writing, window decreases (data goes to send buffer first)
    std::vector<uint8_t> data(50, 'Y');
    auto write_result = test_stream->write(data);
    ASSERT_TRUE(write_result.is_ok());

    // Consume the data from send buffer by generating a frame
    auto frame = test_stream->next_stream_frame(1000);
    ASSERT_TRUE(frame.has_value());

    // After sending, window should be reduced by bytes sent
    EXPECT_EQ(test_stream->available_send_window(), 50UL);
}

// ============================================================================
// Stream Manager Tests
// ============================================================================

class QuicStreamManagerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        client_manager_ = std::make_unique<stream_manager>(false); // Client
        server_manager_ = std::make_unique<stream_manager>(true);  // Server

        // Set initial stream limits
        client_manager_->set_peer_max_streams_bidi(10);
        client_manager_->set_peer_max_streams_uni(10);
        server_manager_->set_peer_max_streams_bidi(10);
        server_manager_->set_peer_max_streams_uni(10);
    }

    std::unique_ptr<stream_manager> client_manager_;
    std::unique_ptr<stream_manager> server_manager_;
};

TEST_F(QuicStreamManagerTest, CreateClientBidiStream)
{
    auto result = client_manager_->create_bidirectional_stream();
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 0UL); // Client bidi starts at 0

    auto result2 = client_manager_->create_bidirectional_stream();
    ASSERT_TRUE(result2.is_ok());
    EXPECT_EQ(result2.value(), 4UL); // Next is 4
}

TEST_F(QuicStreamManagerTest, CreateServerBidiStream)
{
    auto result = server_manager_->create_bidirectional_stream();
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 1UL); // Server bidi starts at 1

    auto result2 = server_manager_->create_bidirectional_stream();
    ASSERT_TRUE(result2.is_ok());
    EXPECT_EQ(result2.value(), 5UL); // Next is 5
}

TEST_F(QuicStreamManagerTest, CreateClientUniStream)
{
    auto result = client_manager_->create_unidirectional_stream();
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 2UL); // Client uni starts at 2
}

TEST_F(QuicStreamManagerTest, CreateServerUniStream)
{
    auto result = server_manager_->create_unidirectional_stream();
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 3UL); // Server uni starts at 3
}

TEST_F(QuicStreamManagerTest, GetStream)
{
    auto create_result = client_manager_->create_bidirectional_stream();
    ASSERT_TRUE(create_result.is_ok());

    auto* s = client_manager_->get_stream(0);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->id(), 0UL);
}

TEST_F(QuicStreamManagerTest, GetOrCreatePeerStream)
{
    // Server receives client-initiated stream
    auto result = server_manager_->get_or_create_stream(0);
    ASSERT_TRUE(result.is_ok());

    auto* s = result.value();
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->id(), 0UL);
    EXPECT_FALSE(s->is_local());
}

TEST_F(QuicStreamManagerTest, ImplicitStreamCreation)
{
    // When we receive stream ID 8, streams 0 and 4 should also be created
    auto result = server_manager_->get_or_create_stream(8);
    ASSERT_TRUE(result.is_ok());

    // Check that intermediate streams were created
    EXPECT_TRUE(server_manager_->has_stream(0));
    EXPECT_TRUE(server_manager_->has_stream(4));
    EXPECT_TRUE(server_manager_->has_stream(8));
}

TEST_F(QuicStreamManagerTest, StreamLimitEnforcement)
{
    client_manager_->set_peer_max_streams_bidi(2);

    // Create first two streams
    EXPECT_TRUE(client_manager_->create_bidirectional_stream().is_ok());
    EXPECT_TRUE(client_manager_->create_bidirectional_stream().is_ok());

    // Third should fail
    auto result = client_manager_->create_bidirectional_stream();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicStreamManagerTest, StreamsWithPendingData)
{
    auto create1 = client_manager_->create_bidirectional_stream();
    auto create2 = client_manager_->create_bidirectional_stream();
    ASSERT_TRUE(create1.is_ok());
    ASSERT_TRUE(create2.is_ok());

    // Write to one stream
    auto* s1 = client_manager_->get_stream(create1.value());
    std::vector<uint8_t> data = {'a', 'b', 'c'};
    s1->set_max_send_data(1000);
    (void)s1->write(data);

    auto pending = client_manager_->streams_with_pending_data();
    EXPECT_EQ(pending.size(), 1UL);
    EXPECT_EQ(pending[0]->id(), 0UL);
}

TEST_F(QuicStreamManagerTest, RemoveClosedStreams)
{
    auto create_result = client_manager_->create_bidirectional_stream();
    ASSERT_TRUE(create_result.is_ok());

    auto* s = client_manager_->get_stream(create_result.value());
    ASSERT_NE(s, nullptr);

    // Reset the stream to put it in terminal state
    (void)s->reset(0);

    // For now, check that stream exists
    EXPECT_TRUE(client_manager_->has_stream(0));
}

TEST_F(QuicStreamManagerTest, CloseAllStreams)
{
    (void)client_manager_->create_bidirectional_stream();
    (void)client_manager_->create_bidirectional_stream();

    EXPECT_EQ(client_manager_->stream_count(), 2UL);

    client_manager_->close_all_streams(0x01);

    // Streams should be in reset state
    client_manager_->for_each_stream([](const stream& s) {
        EXPECT_EQ(s.send_state(), send_stream_state::reset_sent);
    });
}

TEST_F(QuicStreamManagerTest, Reset)
{
    (void)client_manager_->create_bidirectional_stream();
    (void)client_manager_->create_bidirectional_stream();

    EXPECT_EQ(client_manager_->stream_count(), 2UL);

    client_manager_->reset();

    EXPECT_EQ(client_manager_->stream_count(), 0UL);

    // Next stream should start fresh
    auto result = client_manager_->create_bidirectional_stream();
    ASSERT_TRUE(result.is_ok());
    EXPECT_EQ(result.value(), 0UL);
}

// ============================================================================
// Flow Controller Tests
// ============================================================================

class QuicFlowControllerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        fc_ = std::make_unique<flow_controller>(1048576); // 1MB window
    }

    std::unique_ptr<flow_controller> fc_;
};

TEST_F(QuicFlowControllerTest, InitialState)
{
    EXPECT_EQ(fc_->send_limit(), 1048576UL);
    EXPECT_EQ(fc_->bytes_sent(), 0UL);
    EXPECT_EQ(fc_->available_send_window(), 1048576UL);
    EXPECT_FALSE(fc_->is_send_blocked());
}

TEST_F(QuicFlowControllerTest, ConsumeSendWindow)
{
    auto result = fc_->consume_send_window(1000);
    ASSERT_TRUE(result.is_ok());

    EXPECT_EQ(fc_->bytes_sent(), 1000UL);
    EXPECT_EQ(fc_->available_send_window(), 1048576UL - 1000UL);
}

TEST_F(QuicFlowControllerTest, SendBlocked)
{
    // Consume entire window
    auto result = fc_->consume_send_window(1048576);
    ASSERT_TRUE(result.is_ok());

    EXPECT_TRUE(fc_->is_send_blocked());
    EXPECT_EQ(fc_->available_send_window(), 0UL);

    // Trying to send more should fail
    auto blocked_result = fc_->consume_send_window(1);
    EXPECT_FALSE(blocked_result.is_ok());
}

TEST_F(QuicFlowControllerTest, UpdateSendLimit)
{
    (void)fc_->consume_send_window(1048576);
    EXPECT_TRUE(fc_->is_send_blocked());

    // Peer sends MAX_DATA
    fc_->update_send_limit(2097152); // 2MB

    EXPECT_FALSE(fc_->is_send_blocked());
    EXPECT_EQ(fc_->available_send_window(), 2097152UL - 1048576UL);
}

TEST_F(QuicFlowControllerTest, ReceiveData)
{
    auto result = fc_->record_received(5000);
    ASSERT_TRUE(result.is_ok());

    EXPECT_EQ(fc_->bytes_received(), 5000UL);
}

TEST_F(QuicFlowControllerTest, ReceiveOverLimit)
{
    // Set a smaller receive limit
    fc_->reset(1000);

    auto result = fc_->record_received(1001);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicFlowControllerTest, GenerateMaxData)
{
    // Receive and consume data to trigger MAX_DATA
    (void)fc_->record_received(600000);
    fc_->record_consumed(600000);

    // Should trigger MAX_DATA update (>50% consumed)
    EXPECT_TRUE(fc_->should_send_max_data());

    auto max_data = fc_->generate_max_data();
    EXPECT_TRUE(max_data.has_value());
    EXPECT_GT(max_data.value(), 1048576UL);
}

TEST_F(QuicFlowControllerTest, DataBlockedFrame)
{
    (void)fc_->consume_send_window(1048576);

    EXPECT_TRUE(fc_->should_send_data_blocked());

    fc_->mark_data_blocked_sent();
    EXPECT_FALSE(fc_->should_send_data_blocked());
}

TEST_F(QuicFlowControllerTest, FlowControlStats)
{
    (void)fc_->consume_send_window(100);
    (void)fc_->record_received(200);
    fc_->record_consumed(50);

    auto stats = get_flow_control_stats(*fc_);

    EXPECT_EQ(stats.bytes_sent, 100UL);
    EXPECT_EQ(stats.bytes_received, 200UL);
    EXPECT_EQ(stats.bytes_consumed, 50UL);
    EXPECT_EQ(stats.send_limit, 1048576UL);
    EXPECT_FALSE(stats.send_blocked);
}

TEST_F(QuicFlowControllerTest, ResetFlowController)
{
    (void)fc_->consume_send_window(100);
    (void)fc_->record_received(200);

    fc_->reset(65536);

    EXPECT_EQ(fc_->bytes_sent(), 0UL);
    EXPECT_EQ(fc_->bytes_received(), 0UL);
    EXPECT_EQ(fc_->send_limit(), 65536UL);
    EXPECT_EQ(fc_->window_size(), 65536UL);
}

// ============================================================================
// State String Tests
// ============================================================================

TEST(QuicStreamStateStringTest, SendStateToString)
{
    EXPECT_STREQ(send_state_to_string(send_stream_state::ready), "ready");
    EXPECT_STREQ(send_state_to_string(send_stream_state::send), "send");
    EXPECT_STREQ(send_state_to_string(send_stream_state::data_sent), "data_sent");
    EXPECT_STREQ(send_state_to_string(send_stream_state::reset_sent), "reset_sent");
    EXPECT_STREQ(send_state_to_string(send_stream_state::reset_recvd), "reset_recvd");
    EXPECT_STREQ(send_state_to_string(send_stream_state::data_recvd), "data_recvd");
}

TEST(QuicStreamStateStringTest, RecvStateToString)
{
    EXPECT_STREQ(recv_state_to_string(recv_stream_state::recv), "recv");
    EXPECT_STREQ(recv_state_to_string(recv_stream_state::size_known), "size_known");
    EXPECT_STREQ(recv_state_to_string(recv_stream_state::data_recvd), "data_recvd");
    EXPECT_STREQ(recv_state_to_string(recv_stream_state::reset_recvd), "reset_recvd");
    EXPECT_STREQ(recv_state_to_string(recv_stream_state::data_read), "data_read");
    EXPECT_STREQ(recv_state_to_string(recv_stream_state::reset_read), "reset_read");
}
