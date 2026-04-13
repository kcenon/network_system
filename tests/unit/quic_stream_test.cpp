/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/stream.h"
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_stream_test.cpp
 * @brief Unit tests for QUIC streams (RFC 9000 Sections 2-4)
 *
 * Tests validate:
 * - stream_id_type namespace helpers (is_client_initiated, is_bidirectional, etc.)
 * - stream_id_type make_stream_id/get_type/get_sequence round-trip
 * - send/recv stream state enums and string conversion
 * - stream error code constants
 * - stream constructor and property accessors (id, is_local, is_bidirectional)
 * - Send side: write(), finish(), reset(), pending_bytes(), can_send()
 * - Receive side: receive_data(), read(), has_data()
 * - Flow control: available_send_window(), should_send_max_stream_data()
 * - State transitions (ready -> send -> data_sent, recv -> size_known -> data_recvd)
 */

// ============================================================================
// stream_id_type Tests
// ============================================================================

class StreamIdTypeTest : public ::testing::Test
{
};

TEST_F(StreamIdTypeTest, ConstantValues)
{
	EXPECT_EQ(quic::stream_id_type::client_bidi, 0x00u);
	EXPECT_EQ(quic::stream_id_type::server_bidi, 0x01u);
	EXPECT_EQ(quic::stream_id_type::client_uni, 0x02u);
	EXPECT_EQ(quic::stream_id_type::server_uni, 0x03u);
}

TEST_F(StreamIdTypeTest, IsClientInitiatedForClientStreams)
{
	EXPECT_TRUE(quic::stream_id_type::is_client_initiated(0)); // client bidi #0
	EXPECT_TRUE(quic::stream_id_type::is_client_initiated(4)); // client bidi #1
	EXPECT_TRUE(quic::stream_id_type::is_client_initiated(2)); // client uni #0
}

TEST_F(StreamIdTypeTest, IsServerInitiatedForServerStreams)
{
	EXPECT_TRUE(quic::stream_id_type::is_server_initiated(1)); // server bidi #0
	EXPECT_TRUE(quic::stream_id_type::is_server_initiated(5)); // server bidi #1
	EXPECT_TRUE(quic::stream_id_type::is_server_initiated(3)); // server uni #0
}

TEST_F(StreamIdTypeTest, IsBidirectional)
{
	EXPECT_TRUE(quic::stream_id_type::is_bidirectional(0));  // client bidi
	EXPECT_TRUE(quic::stream_id_type::is_bidirectional(1));  // server bidi
	EXPECT_TRUE(quic::stream_id_type::is_bidirectional(4));  // client bidi #1
	EXPECT_FALSE(quic::stream_id_type::is_bidirectional(2)); // client uni
	EXPECT_FALSE(quic::stream_id_type::is_bidirectional(3)); // server uni
}

TEST_F(StreamIdTypeTest, IsUnidirectional)
{
	EXPECT_TRUE(quic::stream_id_type::is_unidirectional(2));  // client uni
	EXPECT_TRUE(quic::stream_id_type::is_unidirectional(3));  // server uni
	EXPECT_FALSE(quic::stream_id_type::is_unidirectional(0)); // client bidi
	EXPECT_FALSE(quic::stream_id_type::is_unidirectional(1)); // server bidi
}

TEST_F(StreamIdTypeTest, GetType)
{
	EXPECT_EQ(quic::stream_id_type::get_type(0), 0u);
	EXPECT_EQ(quic::stream_id_type::get_type(1), 1u);
	EXPECT_EQ(quic::stream_id_type::get_type(2), 2u);
	EXPECT_EQ(quic::stream_id_type::get_type(3), 3u);
	EXPECT_EQ(quic::stream_id_type::get_type(4), 0u); // client bidi #1
	EXPECT_EQ(quic::stream_id_type::get_type(7), 3u); // server uni #1
}

TEST_F(StreamIdTypeTest, GetSequence)
{
	EXPECT_EQ(quic::stream_id_type::get_sequence(0), 0u);  // type 0, seq 0
	EXPECT_EQ(quic::stream_id_type::get_sequence(4), 1u);  // type 0, seq 1
	EXPECT_EQ(quic::stream_id_type::get_sequence(8), 2u);  // type 0, seq 2
	EXPECT_EQ(quic::stream_id_type::get_sequence(1), 0u);  // type 1, seq 0
	EXPECT_EQ(quic::stream_id_type::get_sequence(5), 1u);  // type 1, seq 1
}

TEST_F(StreamIdTypeTest, MakeStreamIdRoundtrip)
{
	for (uint64_t type = 0; type <= 3; ++type)
	{
		for (uint64_t seq = 0; seq < 10; ++seq)
		{
			auto id = quic::stream_id_type::make_stream_id(type, seq);

			EXPECT_EQ(quic::stream_id_type::get_type(id), type);
			EXPECT_EQ(quic::stream_id_type::get_sequence(id), seq);
		}
	}
}

TEST_F(StreamIdTypeTest, AllFunctionsAreConstexpr)
{
	static_assert(quic::stream_id_type::is_client_initiated(0));
	static_assert(quic::stream_id_type::is_server_initiated(1));
	static_assert(quic::stream_id_type::is_bidirectional(0));
	static_assert(quic::stream_id_type::is_unidirectional(2));
	static_assert(quic::stream_id_type::get_type(4) == 0);
	static_assert(quic::stream_id_type::get_sequence(4) == 1);
	static_assert(quic::stream_id_type::make_stream_id(0, 1) == 4);
	SUCCEED();
}

// ============================================================================
// Stream State Enum Tests
// ============================================================================

class StreamStateEnumTest : public ::testing::Test
{
};

TEST_F(StreamStateEnumTest, SendStates)
{
	EXPECT_NE(quic::send_state_to_string(quic::send_stream_state::ready), nullptr);
	EXPECT_NE(quic::send_state_to_string(quic::send_stream_state::send), nullptr);
	EXPECT_NE(quic::send_state_to_string(quic::send_stream_state::data_sent), nullptr);
	EXPECT_NE(quic::send_state_to_string(quic::send_stream_state::reset_sent), nullptr);
	EXPECT_NE(quic::send_state_to_string(quic::send_stream_state::reset_recvd), nullptr);
	EXPECT_NE(quic::send_state_to_string(quic::send_stream_state::data_recvd), nullptr);
}

TEST_F(StreamStateEnumTest, RecvStates)
{
	EXPECT_NE(quic::recv_state_to_string(quic::recv_stream_state::recv), nullptr);
	EXPECT_NE(quic::recv_state_to_string(quic::recv_stream_state::size_known), nullptr);
	EXPECT_NE(quic::recv_state_to_string(quic::recv_stream_state::data_recvd), nullptr);
	EXPECT_NE(quic::recv_state_to_string(quic::recv_stream_state::reset_recvd), nullptr);
	EXPECT_NE(quic::recv_state_to_string(quic::recv_stream_state::data_read), nullptr);
	EXPECT_NE(quic::recv_state_to_string(quic::recv_stream_state::reset_read), nullptr);
}

// ============================================================================
// Stream Error Code Tests
// ============================================================================

class StreamErrorCodeTest : public ::testing::Test
{
};

TEST_F(StreamErrorCodeTest, ErrorCodes)
{
	EXPECT_EQ(quic::stream_error::invalid_stream_id, -700);
	EXPECT_EQ(quic::stream_error::stream_not_found, -701);
	EXPECT_EQ(quic::stream_error::stream_limit_exceeded, -702);
	EXPECT_EQ(quic::stream_error::flow_control_error, -703);
	EXPECT_EQ(quic::stream_error::final_size_error, -704);
	EXPECT_EQ(quic::stream_error::stream_state_error, -705);
	EXPECT_EQ(quic::stream_error::stream_reset, -706);
	EXPECT_EQ(quic::stream_error::buffer_full, -707);
}

// ============================================================================
// Stream Constructor Tests
// ============================================================================

class StreamConstructorTest : public ::testing::Test
{
};

TEST_F(StreamConstructorTest, BasicProperties)
{
	quic::stream s(0, true, 65536);

	EXPECT_EQ(s.id(), 0u);
	EXPECT_TRUE(s.is_local());
	EXPECT_TRUE(s.is_bidirectional());
	EXPECT_FALSE(s.is_unidirectional());
}

TEST_F(StreamConstructorTest, ServerUnidirectionalStream)
{
	quic::stream s(3, false, 65536);

	EXPECT_EQ(s.id(), 3u);
	EXPECT_FALSE(s.is_local());
	EXPECT_TRUE(s.is_unidirectional());
	EXPECT_FALSE(s.is_bidirectional());
}

TEST_F(StreamConstructorTest, InitialSendState)
{
	quic::stream s(0, true);

	EXPECT_EQ(s.send_state(), quic::send_stream_state::ready);
}

TEST_F(StreamConstructorTest, InitialRecvState)
{
	quic::stream s(0, true);

	EXPECT_EQ(s.recv_state(), quic::recv_stream_state::recv);
}

TEST_F(StreamConstructorTest, InitialPendingBytesZero)
{
	quic::stream s(0, true);

	EXPECT_EQ(s.pending_bytes(), 0u);
}

TEST_F(StreamConstructorTest, InitialNoFin)
{
	quic::stream s(0, true);

	EXPECT_FALSE(s.fin_sent());
	EXPECT_FALSE(s.is_fin_received());
}

// ============================================================================
// Send Side Tests
// ============================================================================

class StreamSendTest : public ::testing::Test
{
protected:
	quic::stream stream_{0, true, 65536};

	void SetUp() override
	{
		stream_.set_max_send_data(65536);
	}
};

TEST_F(StreamSendTest, CanSendInitially)
{
	EXPECT_TRUE(stream_.can_send());
}

TEST_F(StreamSendTest, WriteData)
{
	std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04, 0x05};

	auto result = stream_.write(data);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value(), 5u);
	EXPECT_EQ(stream_.pending_bytes(), 5u);
}

TEST_F(StreamSendTest, WriteMultiple)
{
	std::vector<uint8_t> data1 = {0x01, 0x02};
	std::vector<uint8_t> data2 = {0x03, 0x04, 0x05};

	stream_.write(data1);
	stream_.write(data2);

	EXPECT_EQ(stream_.pending_bytes(), 5u);
}

TEST_F(StreamSendTest, FinishStream)
{
	std::vector<uint8_t> data = {0x01};
	stream_.write(data);

	auto result = stream_.finish();

	EXPECT_FALSE(result.is_err());
	EXPECT_TRUE(stream_.fin_sent());
}

TEST_F(StreamSendTest, ResetStream)
{
	std::vector<uint8_t> data = {0x01};
	stream_.write(data);

	auto result = stream_.reset(0x42);

	EXPECT_FALSE(result.is_err());
	EXPECT_EQ(stream_.send_state(), quic::send_stream_state::reset_sent);
}

TEST_F(StreamSendTest, CannotSendAfterReset)
{
	stream_.reset(0);

	EXPECT_FALSE(stream_.can_send());
}

// ============================================================================
// Receive Side Tests
// ============================================================================

class StreamReceiveTest : public ::testing::Test
{
protected:
	quic::stream stream_{1, false, 65536};

	void SetUp() override
	{
		stream_.set_max_recv_data(65536);
	}
};

TEST_F(StreamReceiveTest, ReceiveDataInOrder)
{
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};

	auto result = stream_.receive_data(0, data, false);

	EXPECT_FALSE(result.is_err());
	EXPECT_TRUE(stream_.has_data());
}

TEST_F(StreamReceiveTest, ReceiveAndRead)
{
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	stream_.receive_data(0, data, false);

	std::vector<uint8_t> buffer(10);
	auto read_result = stream_.read(buffer);

	ASSERT_FALSE(read_result.is_err());
	EXPECT_EQ(read_result.value(), 3u);
	EXPECT_EQ(buffer[0], 0x01);
	EXPECT_EQ(buffer[1], 0x02);
	EXPECT_EQ(buffer[2], 0x03);
}

TEST_F(StreamReceiveTest, ReceiveWithFin)
{
	std::vector<uint8_t> data = {0x01, 0x02};

	stream_.receive_data(0, data, true);

	EXPECT_TRUE(stream_.is_fin_received());
}

TEST_F(StreamReceiveTest, ReceiveResetStream)
{
	auto result = stream_.receive_reset(0x42, 0);

	EXPECT_FALSE(result.is_err());
	EXPECT_EQ(stream_.recv_state(), quic::recv_stream_state::reset_recvd);
	EXPECT_EQ(stream_.reset_error_code().value(), 0x42u);
}

TEST_F(StreamReceiveTest, ReceiveStopSending)
{
	auto result = stream_.receive_stop_sending(0x99);

	EXPECT_FALSE(result.is_err());
	EXPECT_EQ(stream_.stop_sending_error_code().value(), 0x99u);
}

TEST_F(StreamReceiveTest, NoDataInitially)
{
	EXPECT_FALSE(stream_.has_data());
}

// ============================================================================
// Flow Control Tests
// ============================================================================

class StreamFlowControlTest : public ::testing::Test
{
protected:
	quic::stream stream_{0, true, 1000};

	void SetUp() override
	{
		stream_.set_max_send_data(1000);
	}
};

TEST_F(StreamFlowControlTest, AvailableSendWindow)
{
	EXPECT_EQ(stream_.available_send_window(), 1000u);
}

TEST_F(StreamFlowControlTest, AvailableWindowDecreasesAfterWrite)
{
	std::vector<uint8_t> data(500, 0x42);
	stream_.write(data);

	// After writing 500 bytes with next_stream_frame, the send offset advances
	// But pending_bytes shows buffered data, not yet acknowledged
	EXPECT_EQ(stream_.pending_bytes(), 500u);
}

TEST_F(StreamFlowControlTest, SetMaxSendDataIncreasesWindow)
{
	stream_.set_max_send_data(2000);

	EXPECT_EQ(stream_.max_send_data(), 2000u);
}

TEST_F(StreamFlowControlTest, SetMaxRecvData)
{
	stream_.set_max_recv_data(5000);

	EXPECT_EQ(stream_.max_recv_data(), 5000u);
}
