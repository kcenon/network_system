/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/connection.h"
#include <gtest/gtest.h>

#include "kcenon/network/detail/protocols/quic/connection_id.h"

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_connection_test.cpp
 * @brief Unit tests for QUIC connection state machine (RFC 9000 Section 5)
 *
 * Tests validate:
 * - Client and server construction with initial state
 * - Connection ID accessors: local_cid(), initial_dcid(), remote_cid()
 * - Statistics: bytes_sent/received, packets_sent/received == 0 initially
 * - Transport parameter accessors
 * - has_pending_data() initially false
 * - close() state transition
 * - PMTUD enable/disable through connection interface
 * - State and handshake state string conversions
 * - CID management: add, retire, duplicates
 * - Subsystem accessors: flow_control(), streams(), crypto(), pmtud()
 */

// ============================================================================
// State String Conversion Tests
// ============================================================================

class ConnectionStateStringTest : public ::testing::Test
{
};

TEST_F(ConnectionStateStringTest, AllConnectionStates)
{
    EXPECT_STREQ(quic::connection_state_to_string(quic::connection_state::idle), "idle");
    EXPECT_STREQ(quic::connection_state_to_string(quic::connection_state::handshaking),
                 "handshaking");
    EXPECT_STREQ(quic::connection_state_to_string(quic::connection_state::connected),
                 "connected");
    EXPECT_STREQ(quic::connection_state_to_string(quic::connection_state::closing), "closing");
    EXPECT_STREQ(quic::connection_state_to_string(quic::connection_state::draining),
                 "draining");
    EXPECT_STREQ(quic::connection_state_to_string(quic::connection_state::closed), "closed");
}

TEST_F(ConnectionStateStringTest, AllHandshakeStates)
{
    EXPECT_STREQ(quic::handshake_state_to_string(quic::handshake_state::initial), "initial");
    EXPECT_STREQ(
        quic::handshake_state_to_string(quic::handshake_state::waiting_server_hello),
        "waiting_server_hello");
    EXPECT_STREQ(quic::handshake_state_to_string(quic::handshake_state::waiting_finished),
                 "waiting_finished");
    EXPECT_STREQ(quic::handshake_state_to_string(quic::handshake_state::complete), "complete");
}

// ============================================================================
// Client Construction Tests
// ============================================================================

class ConnectionClientConstructionTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x01, 0x02, 0x03, 0x04};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionClientConstructionTest, StateIsIdle)
{
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.state(), quic::connection_state::idle);
}

TEST_F(ConnectionClientConstructionTest, HandshakeStateIsInitial)
{
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.handshake_state(), quic::handshake_state::initial);
}

TEST_F(ConnectionClientConstructionTest, IsNotServer)
{
    quic::connection conn(false, dcid);
    EXPECT_FALSE(conn.is_server());
}

TEST_F(ConnectionClientConstructionTest, IsNotEstablished)
{
    quic::connection conn(false, dcid);
    EXPECT_FALSE(conn.is_established());
}

TEST_F(ConnectionClientConstructionTest, IsNotDraining)
{
    quic::connection conn(false, dcid);
    EXPECT_FALSE(conn.is_draining());
}

TEST_F(ConnectionClientConstructionTest, IsNotClosed)
{
    quic::connection conn(false, dcid);
    EXPECT_FALSE(conn.is_closed());
}

TEST_F(ConnectionClientConstructionTest, LocalCidIsNonEmpty)
{
    quic::connection conn(false, dcid);
    EXPECT_FALSE(conn.local_cid().empty());
}

TEST_F(ConnectionClientConstructionTest, InitialDcidMatchesInput)
{
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.initial_dcid(), dcid);
}

TEST_F(ConnectionClientConstructionTest, RemoteCidMatchesDcid)
{
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.remote_cid(), dcid);
}

TEST_F(ConnectionClientConstructionTest, NoCloseErrorCode)
{
    quic::connection conn(false, dcid);
    EXPECT_FALSE(conn.close_error_code().has_value());
}

TEST_F(ConnectionClientConstructionTest, CloseReasonEmpty)
{
    quic::connection conn(false, dcid);
    EXPECT_TRUE(conn.close_reason().empty());
}

// ============================================================================
// Server Construction Tests
// ============================================================================

class ConnectionServerConstructionTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x05, 0x06, 0x07, 0x08};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionServerConstructionTest, StateIsIdle)
{
    quic::connection conn(true, dcid);
    EXPECT_EQ(conn.state(), quic::connection_state::idle);
}

TEST_F(ConnectionServerConstructionTest, IsServer)
{
    quic::connection conn(true, dcid);
    EXPECT_TRUE(conn.is_server());
}

TEST_F(ConnectionServerConstructionTest, IsNotEstablished)
{
    quic::connection conn(true, dcid);
    EXPECT_FALSE(conn.is_established());
}

// ============================================================================
// Statistics Tests
// ============================================================================

class ConnectionStatisticsTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionStatisticsTest, BytesSentInitiallyZero)
{
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.bytes_sent(), 0);
}

TEST_F(ConnectionStatisticsTest, BytesReceivedInitiallyZero)
{
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.bytes_received(), 0);
}

TEST_F(ConnectionStatisticsTest, PacketsSentInitiallyZero)
{
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.packets_sent(), 0);
}

TEST_F(ConnectionStatisticsTest, PacketsReceivedInitiallyZero)
{
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.packets_received(), 0);
}

// ============================================================================
// Transport Parameter Tests
// ============================================================================

class ConnectionTransportParamsTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionTransportParamsTest, SetLocalParams)
{
    quic::connection conn(false, dcid);

    quic::transport_parameters params;
    params.initial_max_data = 1048576;
    params.initial_max_streams_bidi = 100;
    params.initial_max_streams_uni = 50;

    conn.set_local_params(params);

    auto& lp = conn.local_params();
    EXPECT_EQ(lp.initial_max_data, 1048576);
    EXPECT_EQ(lp.initial_max_streams_bidi, 100);
    EXPECT_EQ(lp.initial_max_streams_uni, 50);
}

TEST_F(ConnectionTransportParamsTest, SetRemoteParams)
{
    quic::connection conn(false, dcid);

    quic::transport_parameters params;
    params.initial_max_data = 2097152;
    params.initial_max_streams_bidi = 200;
    params.active_connection_id_limit = 4;
    params.max_idle_timeout = 30000;

    conn.set_remote_params(params);

    auto& rp = conn.remote_params();
    EXPECT_EQ(rp.initial_max_data, 2097152);
    EXPECT_EQ(rp.initial_max_streams_bidi, 200);
    EXPECT_EQ(rp.active_connection_id_limit, 4);
}

// ============================================================================
// Pending Data Tests
// ============================================================================

class ConnectionPendingDataTest : public ::testing::Test
{
};

TEST_F(ConnectionPendingDataTest, InitiallyNoPendingData)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    quic::connection conn(false, quic::connection_id(dcid_bytes));
    EXPECT_FALSE(conn.has_pending_data());
}

TEST_F(ConnectionPendingDataTest, ReceiveEmptyPacketFails)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    quic::connection conn(false, quic::connection_id(dcid_bytes));

    std::vector<uint8_t> empty_packet;
    auto result = conn.receive_packet(empty_packet);
    EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// Close Tests
// ============================================================================

class ConnectionCloseTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionCloseTest, CloseIdleConnection)
{
    quic::connection conn(false, dcid);
    auto result = conn.close(0, "test close");
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(conn.is_draining() || conn.is_closed());
}

TEST_F(ConnectionCloseTest, CloseWithErrorCode)
{
    quic::connection conn(false, dcid);
    auto result = conn.close(0x0A, "flow control error");
    EXPECT_TRUE(result.is_ok());
}

// ============================================================================
// PMTUD through Connection Interface Tests
// ============================================================================

class ConnectionPmtudTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionPmtudTest, PmtudInitiallyDisabled)
{
    quic::connection conn(false, dcid);
    EXPECT_FALSE(conn.pmtud_enabled());
    EXPECT_EQ(conn.path_mtu(), 1200);
}

TEST_F(ConnectionPmtudTest, EnablePmtud)
{
    quic::connection conn(false, dcid);
    conn.enable_pmtud();
    EXPECT_TRUE(conn.pmtud_enabled());
}

TEST_F(ConnectionPmtudTest, DisablePmtud)
{
    quic::connection conn(false, dcid);
    conn.enable_pmtud();
    conn.disable_pmtud();
    EXPECT_FALSE(conn.pmtud_enabled());
    EXPECT_EQ(conn.path_mtu(), 1200);
}

TEST_F(ConnectionPmtudTest, PmtudControllerAccess)
{
    quic::connection conn(false, dcid);
    auto& pmtud = conn.pmtud();
    EXPECT_EQ(pmtud.state(), quic::pmtud_state::disabled);

    const auto& cpmtud = std::as_const(conn).pmtud();
    EXPECT_EQ(cpmtud.state(), quic::pmtud_state::disabled);
}

// ============================================================================
// CID Management Tests
// ============================================================================

class ConnectionCidManagementTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionCidManagementTest, AddLocalCid)
{
    quic::connection conn(false, dcid);
    auto new_cid = quic::connection_id::generate();
    auto result = conn.add_local_cid(new_cid, 1);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(ConnectionCidManagementTest, AddDuplicateSequenceFails)
{
    quic::connection conn(false, dcid);
    auto cid1 = quic::connection_id::generate();
    auto cid2 = quic::connection_id::generate();

    auto r1 = conn.add_local_cid(cid1, 1);
    EXPECT_TRUE(r1.is_ok());

    auto r2 = conn.add_local_cid(cid2, 1);
    EXPECT_FALSE(r2.is_ok());
}

TEST_F(ConnectionCidManagementTest, RetireExistingCid)
{
    quic::connection conn(false, dcid);
    auto new_cid = quic::connection_id::generate();
    (void)conn.add_local_cid(new_cid, 1);

    auto result = conn.retire_cid(1);
    EXPECT_TRUE(result.is_ok());
}

TEST_F(ConnectionCidManagementTest, RetireNonExistentCidFails)
{
    quic::connection conn(false, dcid);
    auto result = conn.retire_cid(99);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionCidManagementTest, RetireLastCidFails)
{
    quic::connection conn(false, dcid);
    auto result = conn.retire_cid(0);
    EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// Subsystem Accessor Tests
// ============================================================================

class ConnectionSubsystemAccessTest : public ::testing::Test
{
};

TEST_F(ConnectionSubsystemAccessTest, FlowControlAccess)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    quic::connection conn(false, quic::connection_id(dcid_bytes));

    auto& fc = conn.flow_control();
    const auto& cfc = std::as_const(conn).flow_control();
    (void)fc;
    (void)cfc;
}

TEST_F(ConnectionSubsystemAccessTest, StreamManagerAccess)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    quic::connection conn(false, quic::connection_id(dcid_bytes));

    auto& sm = conn.streams();
    const auto& csm = std::as_const(conn).streams();
    (void)sm;
    (void)csm;
}

TEST_F(ConnectionSubsystemAccessTest, CryptoAccess)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    quic::connection conn(false, quic::connection_id(dcid_bytes));

    auto& crypto = conn.crypto();
    const auto& ccrypto = std::as_const(conn).crypto();
    EXPECT_FALSE(crypto.is_handshake_complete());
    EXPECT_FALSE(ccrypto.is_handshake_complete());
}

TEST_F(ConnectionSubsystemAccessTest, PeerCidManagerAccess)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    quic::connection conn(false, quic::connection_id(dcid_bytes));

    auto& mgr = conn.peer_cid_manager();
    const auto& cmgr = std::as_const(conn).peer_cid_manager();
    (void)mgr;
    (void)cmgr;
}

// ============================================================================
// Handshake Tests
// ============================================================================

class ConnectionHandshakeTest : public ::testing::Test
{
};

TEST_F(ConnectionHandshakeTest, ServerCannotStartHandshake)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02};
    quic::connection conn(true, quic::connection_id(dcid_bytes));

    auto result = conn.start_handshake("localhost");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionHandshakeTest, ClientStartHandshake)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02, 0x03, 0x04};
    quic::connection conn(false, quic::connection_id(dcid_bytes));

    auto result = conn.start_handshake("localhost");
    ASSERT_TRUE(result.is_ok());

    EXPECT_EQ(conn.state(), quic::connection_state::handshaking);
    EXPECT_EQ(conn.handshake_state(), quic::handshake_state::waiting_server_hello);
    EXPECT_FALSE(conn.is_established());
}

TEST_F(ConnectionHandshakeTest, DoubleStartHandshakeFails)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02, 0x03, 0x04};
    quic::connection conn(false, quic::connection_id(dcid_bytes));

    auto r1 = conn.start_handshake("localhost");
    ASSERT_TRUE(r1.is_ok());

    auto r2 = conn.start_handshake("localhost");
    EXPECT_FALSE(r2.is_ok());
}
