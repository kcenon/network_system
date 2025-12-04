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

#include "kcenon/network/protocols/quic/connection.h"
#include "kcenon/network/protocols/quic/transport_params.h"

using namespace network_system::protocols::quic;

// ============================================================================
// Transport Parameters Tests
// ============================================================================

class TransportParametersTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TransportParametersTest, DefaultClientParams)
{
    auto params = make_default_client_params();

    EXPECT_EQ(params.max_idle_timeout, 30000);
    EXPECT_EQ(params.max_udp_payload_size, 65527);
    EXPECT_EQ(params.initial_max_data, 1048576);
    EXPECT_EQ(params.initial_max_stream_data_bidi_local, 262144);
    EXPECT_EQ(params.initial_max_stream_data_bidi_remote, 262144);
    EXPECT_EQ(params.initial_max_stream_data_uni, 262144);
    EXPECT_EQ(params.initial_max_streams_bidi, 100);
    EXPECT_EQ(params.initial_max_streams_uni, 100);
    EXPECT_EQ(params.ack_delay_exponent, 3);
    EXPECT_EQ(params.max_ack_delay, 25);
    EXPECT_EQ(params.active_connection_id_limit, 8);
    EXPECT_FALSE(params.disable_active_migration);
}

TEST_F(TransportParametersTest, DefaultServerParams)
{
    auto params = make_default_server_params();

    EXPECT_EQ(params.max_idle_timeout, 30000);
    EXPECT_EQ(params.initial_max_data, 1048576);
    EXPECT_EQ(params.active_connection_id_limit, 8);
}

TEST_F(TransportParametersTest, EncodeDecodeRoundtrip)
{
    transport_parameters params;
    params.max_idle_timeout = 60000;
    params.max_udp_payload_size = 1350;
    params.initial_max_data = 2097152;
    params.initial_max_stream_data_bidi_local = 131072;
    params.initial_max_stream_data_bidi_remote = 131072;
    params.initial_max_stream_data_uni = 65536;
    params.initial_max_streams_bidi = 50;
    params.initial_max_streams_uni = 25;
    params.ack_delay_exponent = 4;
    params.max_ack_delay = 30;
    params.disable_active_migration = true;
    params.active_connection_id_limit = 4;

    auto encoded = params.encode();
    EXPECT_FALSE(encoded.empty());

    auto decode_result = transport_parameters::decode(encoded);
    ASSERT_TRUE(decode_result.is_ok());

    auto& decoded = decode_result.value();
    EXPECT_EQ(decoded.max_idle_timeout, params.max_idle_timeout);
    EXPECT_EQ(decoded.max_udp_payload_size, params.max_udp_payload_size);
    EXPECT_EQ(decoded.initial_max_data, params.initial_max_data);
    EXPECT_EQ(decoded.initial_max_stream_data_bidi_local,
              params.initial_max_stream_data_bidi_local);
    EXPECT_EQ(decoded.initial_max_stream_data_bidi_remote,
              params.initial_max_stream_data_bidi_remote);
    EXPECT_EQ(decoded.initial_max_stream_data_uni, params.initial_max_stream_data_uni);
    EXPECT_EQ(decoded.initial_max_streams_bidi, params.initial_max_streams_bidi);
    EXPECT_EQ(decoded.initial_max_streams_uni, params.initial_max_streams_uni);
    EXPECT_EQ(decoded.ack_delay_exponent, params.ack_delay_exponent);
    EXPECT_EQ(decoded.max_ack_delay, params.max_ack_delay);
    EXPECT_EQ(decoded.disable_active_migration, params.disable_active_migration);
    EXPECT_EQ(decoded.active_connection_id_limit, params.active_connection_id_limit);
}

TEST_F(TransportParametersTest, EncodeDecodeWithConnectionIds)
{
    transport_parameters params;
    params.initial_source_connection_id = connection_id::generate();
    params.original_destination_connection_id = connection_id::generate();
    params.max_idle_timeout = 30000;

    auto encoded = params.encode();
    auto decode_result = transport_parameters::decode(encoded);
    ASSERT_TRUE(decode_result.is_ok());

    auto& decoded = decode_result.value();
    EXPECT_TRUE(decoded.initial_source_connection_id.has_value());
    EXPECT_TRUE(decoded.original_destination_connection_id.has_value());
    EXPECT_EQ(decoded.initial_source_connection_id->data().size(),
              params.initial_source_connection_id->data().size());
}

TEST_F(TransportParametersTest, ValidateClientParams)
{
    auto params = make_default_client_params();
    auto result = params.validate(false);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(TransportParametersTest, ValidateServerParams)
{
    auto params = make_default_server_params();
    params.original_destination_connection_id = connection_id::generate();
    auto result = params.validate(true);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(TransportParametersTest, ValidateRejectsClientWithServerOnlyParams)
{
    auto params = make_default_client_params();
    params.original_destination_connection_id = connection_id::generate();

    auto result = params.validate(false);
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParametersTest, ValidateRejectsInvalidAckDelayExponent)
{
    transport_parameters params;
    params.ack_delay_exponent = 21;  // Max is 20

    auto result = params.validate(false);
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParametersTest, ValidateRejectsInvalidMaxAckDelay)
{
    transport_parameters params;
    params.max_ack_delay = 20000;  // Max is 16383

    auto result = params.validate(false);
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParametersTest, ValidateRejectsInvalidMaxUdpPayloadSize)
{
    transport_parameters params;
    params.max_udp_payload_size = 1000;  // Min is 1200

    auto result = params.validate(false);
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParametersTest, ValidateRejectsInvalidActiveConnectionIdLimit)
{
    transport_parameters params;
    params.active_connection_id_limit = 1;  // Min is 2

    auto result = params.validate(false);
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParametersTest, DecodeRejectsDuplicateParameters)
{
    // Manually create encoded data with duplicate parameter
    std::vector<uint8_t> data;

    // Add max_idle_timeout twice
    data.push_back(0x01);  // param id
    data.push_back(0x01);  // length
    data.push_back(0x00);  // value

    data.push_back(0x01);  // param id (duplicate)
    data.push_back(0x01);  // length
    data.push_back(0x00);  // value

    auto result = transport_parameters::decode(data);
    EXPECT_TRUE(result.is_err());
}

TEST_F(TransportParametersTest, DecodeRejectsInvalidStatelessResetToken)
{
    std::vector<uint8_t> data;

    // stateless_reset_token with wrong length
    data.push_back(0x02);  // param id
    data.push_back(0x08);  // length (should be 16)
    for (int i = 0; i < 8; i++)
    {
        data.push_back(0x00);
    }

    auto result = transport_parameters::decode(data);
    EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Connection State Tests
// ============================================================================

class ConnectionStateTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ConnectionStateTest, StateToStringConversions)
{
    EXPECT_STREQ(connection_state_to_string(connection_state::idle), "idle");
    EXPECT_STREQ(connection_state_to_string(connection_state::handshaking), "handshaking");
    EXPECT_STREQ(connection_state_to_string(connection_state::connected), "connected");
    EXPECT_STREQ(connection_state_to_string(connection_state::closing), "closing");
    EXPECT_STREQ(connection_state_to_string(connection_state::draining), "draining");
    EXPECT_STREQ(connection_state_to_string(connection_state::closed), "closed");
}

TEST_F(ConnectionStateTest, HandshakeStateToStringConversions)
{
    EXPECT_STREQ(handshake_state_to_string(handshake_state::initial), "initial");
    EXPECT_STREQ(handshake_state_to_string(handshake_state::waiting_server_hello),
                 "waiting_server_hello");
    EXPECT_STREQ(handshake_state_to_string(handshake_state::waiting_finished),
                 "waiting_finished");
    EXPECT_STREQ(handshake_state_to_string(handshake_state::complete), "complete");
}

// ============================================================================
// Connection Tests
// ============================================================================

class ConnectionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        initial_dcid_ = connection_id::generate();
    }

    void TearDown() override {}

    connection_id initial_dcid_;
};

TEST_F(ConnectionTest, ClientConnectionConstruction)
{
    connection conn(false, initial_dcid_);

    EXPECT_FALSE(conn.is_server());
    EXPECT_EQ(conn.state(), connection_state::idle);
    EXPECT_EQ(conn.handshake_state(), handshake_state::initial);
    EXPECT_FALSE(conn.is_established());
    EXPECT_FALSE(conn.is_draining());
    EXPECT_FALSE(conn.is_closed());
}

TEST_F(ConnectionTest, ServerConnectionConstruction)
{
    connection conn(true, initial_dcid_);

    EXPECT_TRUE(conn.is_server());
    EXPECT_EQ(conn.state(), connection_state::idle);
    EXPECT_EQ(conn.handshake_state(), handshake_state::initial);
}

TEST_F(ConnectionTest, ConnectionHasLocalCid)
{
    connection conn(false, initial_dcid_);

    EXPECT_GT(conn.local_cid().length(), 0);
}

TEST_F(ConnectionTest, ClientConnectionRemoteCidIsInitialDcid)
{
    connection conn(false, initial_dcid_);

    EXPECT_EQ(conn.remote_cid(), initial_dcid_);
    EXPECT_EQ(conn.initial_dcid(), initial_dcid_);
}

TEST_F(ConnectionTest, SetLocalTransportParams)
{
    connection conn(false, initial_dcid_);

    transport_parameters params;
    params.max_idle_timeout = 60000;
    params.initial_max_data = 2097152;

    conn.set_local_params(params);

    EXPECT_EQ(conn.local_params().max_idle_timeout, 60000);
    EXPECT_EQ(conn.local_params().initial_max_data, 2097152);
    // Initial source CID should be set automatically
    EXPECT_TRUE(conn.local_params().initial_source_connection_id.has_value());
}

TEST_F(ConnectionTest, SetRemoteTransportParams)
{
    connection conn(false, initial_dcid_);

    transport_parameters params;
    params.max_idle_timeout = 45000;
    params.initial_max_data = 1048576;
    params.initial_max_streams_bidi = 50;

    conn.set_remote_params(params);

    EXPECT_EQ(conn.remote_params().max_idle_timeout, 45000);
    EXPECT_EQ(conn.remote_params().initial_max_data, 1048576);

    // Stream limits should be applied
    EXPECT_EQ(conn.streams().peer_max_streams_bidi(), 50);
}

TEST_F(ConnectionTest, AddLocalConnectionId)
{
    connection conn(false, initial_dcid_);

    auto new_cid = connection_id::generate();
    auto result = conn.add_local_cid(new_cid, 1);

    EXPECT_TRUE(result.is_ok());
}

TEST_F(ConnectionTest, AddDuplicateSequenceConnectionIdFails)
{
    connection conn(false, initial_dcid_);

    auto cid1 = connection_id::generate();
    auto cid2 = connection_id::generate();

    (void)conn.add_local_cid(cid1, 1);
    auto result = conn.add_local_cid(cid2, 1);

    EXPECT_TRUE(result.is_err());
}

TEST_F(ConnectionTest, RetireConnectionId)
{
    connection conn(false, initial_dcid_);

    auto new_cid = connection_id::generate();
    (void)conn.add_local_cid(new_cid, 1);

    // Retire sequence 0 (the initial CID)
    auto result = conn.retire_cid(0);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(ConnectionTest, RetireNonexistentConnectionIdFails)
{
    connection conn(false, initial_dcid_);

    auto result = conn.retire_cid(99);
    EXPECT_TRUE(result.is_err());
}

TEST_F(ConnectionTest, RetireLastConnectionIdFails)
{
    connection conn(false, initial_dcid_);

    // Only one CID exists (sequence 0)
    auto result = conn.retire_cid(0);
    EXPECT_TRUE(result.is_err());
}

TEST_F(ConnectionTest, CloseConnection)
{
    connection conn(false, initial_dcid_);

    auto result = conn.close(0, "Normal close");

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(conn.state(), connection_state::closing);
    EXPECT_TRUE(conn.is_draining());
    EXPECT_EQ(conn.close_error_code(), 0);
    EXPECT_EQ(conn.close_reason(), "Normal close");
}

TEST_F(ConnectionTest, CloseApplicationConnection)
{
    connection conn(false, initial_dcid_);

    auto result = conn.close_application(100, "App error");

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(conn.state(), connection_state::closing);
    EXPECT_EQ(conn.close_error_code(), 100);
    EXPECT_EQ(conn.close_reason(), "App error");
}

TEST_F(ConnectionTest, DoubleCloseIsNoOp)
{
    connection conn(false, initial_dcid_);

    (void)conn.close(0, "First close");
    auto result = conn.close(1, "Second close");

    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(conn.close_error_code(), 0);  // Still first error code
}

TEST_F(ConnectionTest, StreamManagerAccess)
{
    connection conn(false, initial_dcid_);

    auto& streams = conn.streams();
    EXPECT_EQ(streams.stream_count(), 0);
}

TEST_F(ConnectionTest, FlowControllerAccess)
{
    connection conn(false, initial_dcid_);

    auto& fc = conn.flow_control();
    EXPECT_EQ(fc.bytes_sent(), 0);
    EXPECT_EQ(fc.bytes_received(), 0);
}

TEST_F(ConnectionTest, CryptoAccess)
{
    connection conn(false, initial_dcid_);

    auto& crypto = conn.crypto();
    EXPECT_FALSE(crypto.is_handshake_complete());
}

TEST_F(ConnectionTest, InitialStatistics)
{
    connection conn(false, initial_dcid_);

    EXPECT_EQ(conn.bytes_sent(), 0);
    EXPECT_EQ(conn.bytes_received(), 0);
    EXPECT_EQ(conn.packets_sent(), 0);
    EXPECT_EQ(conn.packets_received(), 0);
}

TEST_F(ConnectionTest, NextTimeoutExists)
{
    connection conn(false, initial_dcid_);

    auto timeout = conn.next_timeout();
    EXPECT_TRUE(timeout.has_value());
}

TEST_F(ConnectionTest, IdleDeadlineSet)
{
    connection conn(false, initial_dcid_);

    auto deadline = conn.idle_deadline();
    auto now = std::chrono::steady_clock::now();

    EXPECT_GT(deadline, now);
}

TEST_F(ConnectionTest, HasNoPendingDataInitially)
{
    connection conn(false, initial_dcid_);

    EXPECT_FALSE(conn.has_pending_data());
}

TEST_F(ConnectionTest, GeneratePacketsEmptyInitially)
{
    connection conn(false, initial_dcid_);

    auto packets = conn.generate_packets();
    EXPECT_TRUE(packets.empty());
}

TEST_F(ConnectionTest, ServerCannotStartHandshake)
{
    connection conn(true, initial_dcid_);

    auto result = conn.start_handshake("example.com");
    EXPECT_TRUE(result.is_err());
}

TEST_F(ConnectionTest, EmptyPacketIsRejected)
{
    connection conn(false, initial_dcid_);

    auto result = conn.receive_packet(std::span<const uint8_t>{});
    EXPECT_TRUE(result.is_err());
}

// Note: Move operations are deleted because internal members are not movable
// TEST_F(ConnectionTest, MoveConstruction)
// TEST_F(ConnectionTest, MoveAssignment)

// ============================================================================
// Preferred Address Tests
// ============================================================================

class PreferredAddressTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(PreferredAddressTest, EncodeDecodePreferredAddress)
{
    transport_parameters params;

    preferred_address_info addr;
    addr.ipv4_address = {192, 168, 1, 1};
    addr.ipv4_port = 443;
    addr.ipv6_address = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    addr.ipv6_port = 8443;
    addr.connection_id = connection_id::generate();
    addr.stateless_reset_token.fill(0xAB);

    params.preferred_address = addr;

    auto encoded = params.encode();
    auto decode_result = transport_parameters::decode(encoded);

    ASSERT_TRUE(decode_result.is_ok());
    ASSERT_TRUE(decode_result.value().preferred_address.has_value());

    auto& decoded_addr = decode_result.value().preferred_address.value();
    EXPECT_EQ(decoded_addr.ipv4_address, addr.ipv4_address);
    EXPECT_EQ(decoded_addr.ipv4_port, addr.ipv4_port);
    EXPECT_EQ(decoded_addr.ipv6_address, addr.ipv6_address);
    EXPECT_EQ(decoded_addr.ipv6_port, addr.ipv6_port);
    EXPECT_EQ(decoded_addr.stateless_reset_token, addr.stateless_reset_token);
}

