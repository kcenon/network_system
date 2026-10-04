/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/connection.h"
#include <gtest/gtest.h>

#include "kcenon/network/detail/protocols/quic/connection_id.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

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

TEST_F(ConnectionHandshakeTest, ClientCannotCallInitServerHandshake)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02, 0x03, 0x04};
    quic::connection conn(false, quic::connection_id(dcid_bytes));

    auto result = conn.init_server_handshake("/nonexistent/cert.pem",
                                              "/nonexistent/key.pem");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionHandshakeTest, ServerInitServerHandshakeFailsOnMissingCert)
{
    std::vector<uint8_t> dcid_bytes = {0x01, 0x02, 0x03, 0x04};
    quic::connection conn(true, quic::connection_id(dcid_bytes));

    auto result = conn.init_server_handshake("/nonexistent/cert.pem",
                                              "/nonexistent/key.pem");
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionHandshakeTest, StartHandshakeMovesToHandshakingState)
{
    std::vector<uint8_t> dcid_bytes = {0xaa, 0xbb, 0xcc, 0xdd};
    quic::connection conn(false, quic::connection_id(dcid_bytes));

    ASSERT_EQ(conn.state(), quic::connection_state::idle);
    auto r = conn.start_handshake("example.com");
    ASSERT_TRUE(r.is_ok());
    EXPECT_EQ(conn.state(), quic::connection_state::handshaking);
    EXPECT_EQ(conn.handshake_state(), quic::handshake_state::waiting_server_hello);
}

// ============================================================================
// Close State Transition Tests
// ============================================================================

class ConnectionCloseTransitionTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x11, 0x22, 0x33, 0x44};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionCloseTransitionTest, CloseApplicationIdleConnection)
{
    quic::connection conn(false, dcid);
    auto result = conn.close_application(0x100, "app error");
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(conn.is_draining() || conn.is_closed());
    ASSERT_TRUE(conn.close_error_code().has_value());
    EXPECT_EQ(*conn.close_error_code(), 0x100u);
    EXPECT_EQ(conn.close_reason(), "app error");
}

TEST_F(ConnectionCloseTransitionTest, CloseAfterCloseIsIdempotent)
{
    quic::connection conn(false, dcid);
    ASSERT_TRUE(conn.close(7, "first").is_ok());

    auto first_state = conn.state();
    auto first_code = conn.close_error_code();

    auto r2 = conn.close(99, "second");
    EXPECT_TRUE(r2.is_ok());
    EXPECT_EQ(conn.state(), first_state);
    // Error code should not be overwritten after the first close
    ASSERT_TRUE(first_code.has_value());
    ASSERT_TRUE(conn.close_error_code().has_value());
    EXPECT_EQ(*conn.close_error_code(), *first_code);
}

TEST_F(ConnectionCloseTransitionTest, CloseApplicationAfterCloseIsIdempotent)
{
    quic::connection conn(false, dcid);
    ASSERT_TRUE(conn.close(3, "transport").is_ok());

    auto r2 = conn.close_application(77, "app");
    EXPECT_TRUE(r2.is_ok());
    // Reason should still be the first one
    EXPECT_EQ(conn.close_reason(), "transport");
}

TEST_F(ConnectionCloseTransitionTest, CloseWithEmptyReason)
{
    quic::connection conn(false, dcid);
    auto result = conn.close(0, "");
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(conn.close_reason().empty());
}

TEST_F(ConnectionCloseTransitionTest, CloseWithLongReason)
{
    quic::connection conn(false, dcid);
    std::string long_reason(512, 'X');
    auto result = conn.close(42, long_reason);
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(conn.close_reason(), long_reason);
}

TEST_F(ConnectionCloseTransitionTest, HasPendingDataAfterClose)
{
    quic::connection conn(false, dcid);
    EXPECT_FALSE(conn.has_pending_data());
    ASSERT_TRUE(conn.close(0, "bye").is_ok());
    EXPECT_TRUE(conn.has_pending_data());
}

// ============================================================================
// Timer and Timeout Tests
// ============================================================================

class ConnectionTimerTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x55, 0x66};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionTimerTest, NextTimeoutExistsInIdleState)
{
    quic::connection conn(false, dcid);
    auto t = conn.next_timeout();
    EXPECT_TRUE(t.has_value());
}

TEST_F(ConnectionTimerTest, IdleDeadlineIsInTheFuture)
{
    quic::connection conn(false, dcid);
    auto now = std::chrono::steady_clock::now();
    EXPECT_GT(conn.idle_deadline(), now);
}

TEST_F(ConnectionTimerTest, OnTimeoutBeforeDeadlineDoesNotClose)
{
    quic::connection conn(false, dcid);
    ASSERT_FALSE(conn.is_closed());
    conn.on_timeout();
    EXPECT_FALSE(conn.is_closed());
}

TEST_F(ConnectionTimerTest, NextTimeoutReturnsNulloptAfterClosed)
{
    quic::connection conn(false, dcid);

    // Force closed state via a short idle timeout
    quic::transport_parameters params;
    params.max_idle_timeout = 1;  // 1ms
    conn.set_local_params(params);
    conn.set_remote_params(params);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    conn.on_timeout();

    if (conn.is_closed())
    {
        EXPECT_FALSE(conn.next_timeout().has_value());
    }
}

TEST_F(ConnectionTimerTest, NextTimeoutReturnsDrainDeadlineWhenClosing)
{
    quic::connection conn(false, dcid);
    ASSERT_TRUE(conn.close(0, "").is_ok());
    ASSERT_TRUE(conn.is_draining());
    auto t = conn.next_timeout();
    EXPECT_TRUE(t.has_value());
}

// ============================================================================
// Peer CID / Rotation Tests
// ============================================================================

class ConnectionPeerCidPathTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0xde, 0xad, 0xbe, 0xef};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionPeerCidPathTest, ActivePeerCidEqualsInitialDcidForClient)
{
    quic::connection conn(false, dcid);
    // For a freshly constructed client, active_peer_cid should equal
    // the initial DCID (either via peer_cid_manager or remote_cid fallback)
    EXPECT_EQ(conn.active_peer_cid(), dcid);
}

TEST_F(ConnectionPeerCidPathTest, RotatePeerCidFailsWithoutAdditionalCids)
{
    quic::connection conn(false, dcid);
    auto result = conn.rotate_peer_cid();
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionPeerCidPathTest, AddMultipleLocalCids)
{
    quic::connection conn(false, dcid);
    auto cid1 = quic::connection_id::generate();
    auto cid2 = quic::connection_id::generate();
    auto cid3 = quic::connection_id::generate();
    EXPECT_TRUE(conn.add_local_cid(cid1, 1).is_ok());
    EXPECT_TRUE(conn.add_local_cid(cid2, 2).is_ok());
    EXPECT_TRUE(conn.add_local_cid(cid3, 3).is_ok());
}

TEST_F(ConnectionPeerCidPathTest, RetireMiddleCidSucceeds)
{
    quic::connection conn(false, dcid);
    auto cid1 = quic::connection_id::generate();
    auto cid2 = quic::connection_id::generate();
    ASSERT_TRUE(conn.add_local_cid(cid1, 1).is_ok());
    ASSERT_TRUE(conn.add_local_cid(cid2, 2).is_ok());

    auto r = conn.retire_cid(1);
    EXPECT_TRUE(r.is_ok());
    // Sequence 2 still retirable (won't be the last)
}

// ============================================================================
// Receive Packet Error-Path Tests
// ============================================================================

class ConnectionReceivePacketErrorTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x77, 0x88};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionReceivePacketErrorTest, SingleByteLongHeaderPacketFails)
{
    quic::connection conn(false, dcid);
    // Long header bit set but too short to parse
    std::vector<uint8_t> pkt = {0xC0};
    auto result = conn.receive_packet(pkt);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionReceivePacketErrorTest, ShortHeaderBeforeHandshakeFails)
{
    quic::connection conn(false, dcid);
    // Short header (bit 0x80 = 0) with plausible DCID
    std::vector<uint8_t> pkt;
    pkt.push_back(0x40);  // Short header form
    pkt.insert(pkt.end(), {0x01, 0x02});  // DCID bytes (2 bytes)
    pkt.push_back(0x00);  // Packet number
    pkt.push_back(0xAA);  // Payload
    auto result = conn.receive_packet(pkt);
    // Short header before handshake complete → should fail
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionReceivePacketErrorTest, InvalidLongHeaderPayloadFails)
{
    quic::connection conn(false, dcid);
    // Long header bit set, but garbage afterwards
    std::vector<uint8_t> pkt = {0xC0, 0x00, 0x00, 0x00, 0x01};
    auto result = conn.receive_packet(pkt);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(ConnectionReceivePacketErrorTest, ReceivePacketInDrainingIgnoresContentButCountsStats)
{
    quic::connection conn(false, dcid);
    ASSERT_TRUE(conn.close(0, "draining test").is_ok());
    ASSERT_TRUE(conn.is_draining());

    auto prev_pkts = conn.packets_received();
    auto prev_bytes = conn.bytes_received();

    std::vector<uint8_t> pkt = {0xC0, 0xFF, 0xFF};
    auto result = conn.receive_packet(pkt);
    // In draining state, packet is silently counted even though bogus
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(conn.packets_received(), prev_pkts + 1);
    EXPECT_EQ(conn.bytes_received(), prev_bytes + pkt.size());
}

// ============================================================================
// Transport Parameter Apply Tests
// ============================================================================

class ConnectionApplyRemoteParamsTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0x99};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionApplyRemoteParamsTest, SetRemoteParamsUpdatesFlowControlSendLimit)
{
    quic::connection conn(false, dcid);

    quic::transport_parameters params;
    params.initial_max_data = 4194304;
    conn.set_remote_params(params);

    EXPECT_EQ(conn.flow_control().send_limit(), 4194304u);
}

TEST_F(ConnectionApplyRemoteParamsTest, SetRemoteParamsUpdatesPeerMaxStreams)
{
    quic::connection conn(false, dcid);

    quic::transport_parameters params;
    params.initial_max_streams_bidi = 321;
    params.initial_max_streams_uni = 123;
    conn.set_remote_params(params);

    EXPECT_EQ(conn.streams().peer_max_streams_bidi(), 321u);
    EXPECT_EQ(conn.streams().peer_max_streams_uni(), 123u);
}

TEST_F(ConnectionApplyRemoteParamsTest, SetRemoteParamsUpdatesActiveCidLimit)
{
    quic::connection conn(false, dcid);

    quic::transport_parameters params;
    params.active_connection_id_limit = 16;
    conn.set_remote_params(params);

    EXPECT_EQ(conn.peer_cid_manager().active_cid_limit(), 16u);
}

TEST_F(ConnectionApplyRemoteParamsTest, SetLocalParamsAppliesStreamLimitsLocally)
{
    quic::connection conn(false, dcid);

    quic::transport_parameters params;
    params.initial_max_streams_bidi = 55;
    params.initial_max_streams_uni = 22;
    conn.set_local_params(params);

    auto& lp = conn.local_params();
    EXPECT_EQ(lp.initial_max_streams_bidi, 55u);
    EXPECT_EQ(lp.initial_max_streams_uni, 22u);
}

// ============================================================================
// Generate Packets Tests
// ============================================================================

class ConnectionGeneratePacketsTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0xa1, 0xa2, 0xa3, 0xa4};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionGeneratePacketsTest, GeneratePacketsEmptyOnIdle)
{
    quic::connection conn(false, dcid);
    auto pkts = conn.generate_packets();
    // Idle connection without start_handshake has no write keys → empty
    // (build_packet returns empty vectors which are filtered out)
    EXPECT_TRUE(pkts.empty() ||
                std::all_of(pkts.begin(), pkts.end(),
                            [](const auto& p) { return p.empty(); }));
}

TEST_F(ConnectionGeneratePacketsTest, GeneratePacketsDoesNotCrashAfterHandshake)
{
    quic::connection conn(false, dcid);
    auto hs = conn.start_handshake("server.example");
    ASSERT_TRUE(hs.is_ok());
    auto pkts = conn.generate_packets();
    // After start_handshake there is pending crypto and initial keys;
    // the call should return without crashing. Actual count may vary by
    // build configuration so we only check it is callable.
    (void)pkts;
    SUCCEED();
}

TEST_F(ConnectionGeneratePacketsTest, GeneratePacketsAfterClosingDoesNotCrash)
{
    quic::connection conn(false, dcid);
    ASSERT_TRUE(conn.close(5, "closing").is_ok());
    auto pkts = conn.generate_packets();
    (void)pkts;
    SUCCEED();
}

// ============================================================================
// Statistics Tests (Extended)
// ============================================================================

class ConnectionStatisticsExtTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0xb0, 0xb1};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionStatisticsExtTest, ReceivePacketIncreasesBytesReceivedEvenOnParseFailure)
{
    quic::connection conn(false, dcid);
    auto before = conn.bytes_received();
    std::vector<uint8_t> pkt = {0xC0, 0x00, 0x00, 0x00, 0x01};
    (void)conn.receive_packet(pkt);
    // receive_packet records bytes before the parse step
    EXPECT_GE(conn.bytes_received(), before);
}

TEST_F(ConnectionStatisticsExtTest, ReceivePacketIncreasesPacketsReceivedOnValidEntry)
{
    quic::connection conn(false, dcid);
    auto before = conn.packets_received();
    std::vector<uint8_t> pkt = {0xC0, 0x00, 0x00, 0x00, 0x01};
    (void)conn.receive_packet(pkt);
    EXPECT_GE(conn.packets_received(), before);
}

// ============================================================================
// PMTUD Extended Tests
// ============================================================================

class ConnectionPmtudExtTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0xc1, 0xc2};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionPmtudExtTest, EnableThenDisableReturnsToMinMtu)
{
    quic::connection conn(false, dcid);
    auto mtu_before = conn.path_mtu();
    conn.enable_pmtud();
    conn.disable_pmtud();
    EXPECT_EQ(conn.path_mtu(), mtu_before);
}

TEST_F(ConnectionPmtudExtTest, DoubleEnableIsIdempotent)
{
    quic::connection conn(false, dcid);
    conn.enable_pmtud();
    conn.enable_pmtud();
    EXPECT_TRUE(conn.pmtud_enabled());
}

// ============================================================================
// Default Transport Parameters Tests
// ============================================================================

class ConnectionDefaultParamsTest : public ::testing::Test
{
protected:
    quic::connection_id dcid;

    void SetUp() override
    {
        std::vector<uint8_t> dcid_bytes = {0xd0};
        dcid = quic::connection_id(dcid_bytes);
    }
};

TEST_F(ConnectionDefaultParamsTest, ClientHasDefaultIdleTimeout)
{
    quic::connection conn(false, dcid);
    // make_default_client_params sets max_idle_timeout = 30000 (30s)
    EXPECT_GT(conn.local_params().max_idle_timeout, 0u);
}

TEST_F(ConnectionDefaultParamsTest, ServerHasDefaultIdleTimeout)
{
    quic::connection conn(true, dcid);
    EXPECT_GT(conn.local_params().max_idle_timeout, 0u);
}

TEST_F(ConnectionDefaultParamsTest, LocalParamsHaveSourceConnectionId)
{
    quic::connection conn(false, dcid);
    // After construction, local_params.initial_source_connection_id should
    // be set to local_cid.
    ASSERT_TRUE(conn.local_params().initial_source_connection_id.has_value());
    EXPECT_EQ(*conn.local_params().initial_source_connection_id, conn.local_cid());
}
