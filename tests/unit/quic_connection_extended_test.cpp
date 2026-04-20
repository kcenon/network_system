/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.
*****************************************************************************/

#include "internal/protocols/quic/connection.h"
#include "internal/protocols/quic/frame.h"
#include "internal/protocols/quic/packet.h"
#include "internal/protocols/quic/transport_params.h"

#include "kcenon/network/detail/protocols/quic/connection_id.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

namespace quic = kcenon::network::protocols::quic;

/**
 * @file quic_connection_extended_test.cpp
 * @brief Extended unit tests for QUIC connection state machine.
 *
 * Targets the lower-coverage code paths in connection.cpp:
 *   - String conversion default branches (out-of-range enum values)
 *   - Constructor variants (different DCID lengths, server vs client)
 *   - Transport parameter application side effects
 *   - Connection ID add/retire ordering and edge cases
 *   - Receive packet error and ignore paths (long header types,
 *     short header rejection before handshake, draining pass-through)
 *   - generate_packets() in idle, closing, and closed states
 *   - Timer behavior with very short idle timeouts (drives state to closed)
 *   - close()/close_application() interaction and idempotency
 *   - PMTUD enable/disable side effects on path_mtu()
 *   - has_pending_data() across pending crypto, ACK, and close states
 *   - active_peer_cid() fallback paths
 *
 * All tests are deterministic, perform no real network IO, and avoid
 * dependencies on TLS/crypto subsystem state by using only the public API.
 */

namespace
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

quic::connection_id make_dcid(std::initializer_list<uint8_t> bytes)
{
    std::vector<uint8_t> v(bytes);
    return quic::connection_id(v);
}

quic::connection_id make_dcid_of_length(size_t n, uint8_t fill = 0xAB)
{
    std::vector<uint8_t> v(n, fill);
    return quic::connection_id(v);
}

quic::transport_parameters make_params_with_idle(uint64_t idle_ms)
{
    quic::transport_parameters p;
    p.max_idle_timeout = idle_ms;
    p.initial_max_data = 65536;
    p.initial_max_streams_bidi = 16;
    p.initial_max_streams_uni = 8;
    p.active_connection_id_limit = 4;
    return p;
}

} // namespace

// ===========================================================================
// String Conversion Default-Branch Tests
// ===========================================================================

class ConnectionStateStringDefaultTest : public ::testing::Test
{
};

TEST_F(ConnectionStateStringDefaultTest, ConnectionStateUnknownReturnsUnknown)
{
    // Cast an out-of-range value to exercise the default switch branch.
    auto s = quic::connection_state_to_string(
        static_cast<quic::connection_state>(0xFF));
    EXPECT_STREQ(s, "unknown");
}

TEST_F(ConnectionStateStringDefaultTest, HandshakeStateUnknownReturnsUnknown)
{
    auto s = quic::handshake_state_to_string(
        static_cast<quic::handshake_state>(0xEE));
    EXPECT_STREQ(s, "unknown");
}

// ===========================================================================
// Constructor Edge Cases
// ===========================================================================

class ConnectionConstructorEdgeTest : public ::testing::Test
{
};

TEST_F(ConnectionConstructorEdgeTest, ClientWithEmptyDcid)
{
    quic::connection conn(false, quic::connection_id{});
    EXPECT_FALSE(conn.is_server());
    // Local CID is generated regardless of input DCID.
    EXPECT_FALSE(conn.local_cid().empty());
    // Remote CID for client equals input DCID, which is empty.
    EXPECT_TRUE(conn.remote_cid().empty());
    EXPECT_TRUE(conn.initial_dcid().empty());
}

TEST_F(ConnectionConstructorEdgeTest, ServerWithEmptyDcid)
{
    quic::connection conn(true, quic::connection_id{});
    EXPECT_TRUE(conn.is_server());
    EXPECT_FALSE(conn.local_cid().empty());
    // Server does not set remote CID at construction; it remains empty
    // until the first packet from the client.
    EXPECT_TRUE(conn.remote_cid().empty());
}

TEST_F(ConnectionConstructorEdgeTest, ClientWithMaxLengthDcid)
{
    auto dcid = make_dcid_of_length(quic::connection_id::max_length, 0xCD);
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.initial_dcid(), dcid);
    EXPECT_EQ(conn.remote_cid(), dcid);
    EXPECT_EQ(conn.remote_cid().length(), quic::connection_id::max_length);
}

TEST_F(ConnectionConstructorEdgeTest, ClientHasDefaultActiveConnectionIdLimit)
{
    quic::connection conn(false, make_dcid({0x01}));
    // make_default_client_params sets active_connection_id_limit = 8.
    EXPECT_EQ(conn.local_params().active_connection_id_limit, 8u);
}

TEST_F(ConnectionConstructorEdgeTest, ServerHasDefaultActiveConnectionIdLimit)
{
    quic::connection conn(true, make_dcid({0x01}));
    EXPECT_EQ(conn.local_params().active_connection_id_limit, 8u);
}

TEST_F(ConnectionConstructorEdgeTest, InitialSourceConnectionIdMatchesLocal)
{
    quic::connection conn(false, make_dcid({0x33}));
    ASSERT_TRUE(conn.local_params().initial_source_connection_id.has_value());
    EXPECT_EQ(*conn.local_params().initial_source_connection_id, conn.local_cid());
}

TEST_F(ConnectionConstructorEdgeTest, ClientPeerCidManagerHasInitialCid)
{
    auto dcid = make_dcid({0x11, 0x22, 0x33});
    quic::connection conn(false, dcid);
    // Client constructor calls peer_cid_manager_.set_initial_peer_cid(dcid).
    EXPECT_GE(conn.peer_cid_manager().peer_cid_count(), 1u);
}

TEST_F(ConnectionConstructorEdgeTest, ServerPeerCidManagerStartsEmpty)
{
    auto dcid = make_dcid({0xAA});
    quic::connection conn(true, dcid);
    // Server does not seed peer_cid_manager at construction.
    EXPECT_EQ(conn.peer_cid_manager().peer_cid_count(), 0u);
}

// ===========================================================================
// Transport Parameter Application Tests
// ===========================================================================

class ConnectionTransportParamApplyTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0x01, 0x02, 0x03, 0x04});
};

TEST_F(ConnectionTransportParamApplyTest, SetLocalParamsAppliesStreamLimitsToManager)
{
    quic::connection conn(false, dcid_);
    auto params = make_params_with_idle(0);
    params.initial_max_streams_bidi = 77;
    params.initial_max_streams_uni = 33;
    conn.set_local_params(params);

    EXPECT_EQ(conn.streams().local_max_streams_bidi(), 77u);
    EXPECT_EQ(conn.streams().local_max_streams_uni(), 33u);
}

TEST_F(ConnectionTransportParamApplyTest, SetLocalParamsForcesInitialSourceConnectionId)
{
    quic::connection conn(false, dcid_);
    quic::transport_parameters params;
    // Even if the caller did not set this, the connection forces it to local_cid.
    conn.set_local_params(params);
    ASSERT_TRUE(conn.local_params().initial_source_connection_id.has_value());
    EXPECT_EQ(*conn.local_params().initial_source_connection_id, conn.local_cid());
}

TEST_F(ConnectionTransportParamApplyTest, SetRemoteParamsLowerDataKeepsExistingSendLimit)
{
    // flow_controller::update_send_limit is monotonic — it only raises the
    // send limit, never lowers it. The connection initializes with a 1MB
    // window, so setting remote params with a smaller value must not shrink
    // the send limit.
    quic::connection conn(false, dcid_);
    auto initial_limit = conn.flow_control().send_limit();
    ASSERT_GT(initial_limit, 0u);

    quic::transport_parameters params;
    params.initial_max_data = 0;
    conn.set_remote_params(params);
    EXPECT_EQ(conn.flow_control().send_limit(), initial_limit);
}

TEST_F(ConnectionTransportParamApplyTest, SetRemoteParamsLargerDataRaisesSendLimit)
{
    quic::connection conn(false, dcid_);
    auto initial_limit = conn.flow_control().send_limit();
    quic::transport_parameters params;
    params.initial_max_data = initial_limit + 1024 * 1024;
    conn.set_remote_params(params);
    EXPECT_EQ(conn.flow_control().send_limit(), initial_limit + 1024 * 1024);
}

TEST_F(ConnectionTransportParamApplyTest, SetRemoteParamsUpdatesIdleDeadline)
{
    quic::connection conn(false, dcid_);
    auto initial_deadline = conn.idle_deadline();

    quic::transport_parameters params;
    params.max_idle_timeout = 1;  // 1ms — minimum of local (30000) and remote (1) wins.
    conn.set_remote_params(params);

    // Idle deadline should be much earlier now (reset to ~now + 1ms).
    EXPECT_LE(conn.idle_deadline(), initial_deadline);
}

TEST_F(ConnectionTransportParamApplyTest, SetRemoteParamsZeroIdleTimeoutKeepsExistingDeadline)
{
    quic::connection conn(false, dcid_);
    auto before = conn.idle_deadline();

    quic::transport_parameters params;
    params.max_idle_timeout = 0;  // disabled — should not update idle_deadline_.
    conn.set_remote_params(params);

    // The deadline is unchanged because the "if (max_idle_timeout > 0)" guard
    // skips the assignment.
    EXPECT_EQ(conn.idle_deadline(), before);
}

TEST_F(ConnectionTransportParamApplyTest, SetRemoteParamsLowerActiveCidLimit)
{
    quic::connection conn(false, dcid_);
    quic::transport_parameters params;
    params.active_connection_id_limit = 2;
    conn.set_remote_params(params);
    EXPECT_EQ(conn.peer_cid_manager().active_cid_limit(), 2u);
}

TEST_F(ConnectionTransportParamApplyTest, SetRemoteParamsHigherActiveCidLimit)
{
    quic::connection conn(false, dcid_);
    quic::transport_parameters params;
    params.active_connection_id_limit = 100;
    conn.set_remote_params(params);
    EXPECT_EQ(conn.peer_cid_manager().active_cid_limit(), 100u);
}

// ===========================================================================
// Connection ID Add / Retire Coverage
// ===========================================================================

class ConnectionCidExtendedTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0xCC, 0xDD});
};

TEST_F(ConnectionCidExtendedTest, AddManyLocalCidsThenRetireAllButOne)
{
    quic::connection conn(false, dcid_);
    constexpr int N = 20;
    for (int i = 1; i <= N; ++i)
    {
        EXPECT_TRUE(conn.add_local_cid(quic::connection_id::generate(),
                                        static_cast<uint64_t>(i))
                        .is_ok());
    }
    // Sequence 0 is the initial CID. Retire sequences 1..N (but stop before
    // we hit the last remaining CID, which is forbidden).
    for (int i = 1; i <= N; ++i)
    {
        EXPECT_TRUE(conn.retire_cid(static_cast<uint64_t>(i)).is_ok());
    }
    // Now only seq 0 remains; retiring it must fail.
    EXPECT_FALSE(conn.retire_cid(0).is_ok());
}

TEST_F(ConnectionCidExtendedTest, AddDuplicateAfterAddingManyFails)
{
    quic::connection conn(false, dcid_);
    EXPECT_TRUE(conn.add_local_cid(quic::connection_id::generate(), 5).is_ok());
    EXPECT_TRUE(conn.add_local_cid(quic::connection_id::generate(), 6).is_ok());
    // Re-using sequence 5 must fail with protocol_violation.
    EXPECT_FALSE(conn.add_local_cid(quic::connection_id::generate(), 5).is_ok());
}

TEST_F(ConnectionCidExtendedTest, RetireUnknownSequenceFails)
{
    quic::connection conn(false, dcid_);
    EXPECT_FALSE(conn.retire_cid(12345).is_ok());
}

TEST_F(ConnectionCidExtendedTest, RetireSameSequenceTwiceFails)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.add_local_cid(quic::connection_id::generate(), 1).is_ok());
    ASSERT_TRUE(conn.add_local_cid(quic::connection_id::generate(), 2).is_ok());
    EXPECT_TRUE(conn.retire_cid(1).is_ok());
    EXPECT_FALSE(conn.retire_cid(1).is_ok());
}

// ===========================================================================
// Active Peer CID Path Tests
// ===========================================================================

class ConnectionActivePeerCidTest : public ::testing::Test
{
};

TEST_F(ConnectionActivePeerCidTest, ClientFallsBackToRemoteCidWhenManagerEmpty)
{
    // Construct a server with an empty initial DCID so that the client-side
    // seeding path doesn't run, then we observe the fallback branch.
    quic::connection conn(true, make_dcid({0x05}));
    // Server: peer_cid_manager has no entries, so active_peer_cid() should
    // return remote_cid_ (which is empty for a server before first packet).
    const auto& cid = conn.active_peer_cid();
    EXPECT_TRUE(cid.empty());
}

TEST_F(ConnectionActivePeerCidTest, ClientUsesPeerCidManagerWhenSeeded)
{
    auto dcid = make_dcid({0xA0, 0xA1, 0xA2, 0xA3});
    quic::connection conn(false, dcid);
    // Client: peer_cid_manager is seeded with dcid in constructor.
    EXPECT_EQ(conn.active_peer_cid(), dcid);
}

TEST_F(ConnectionActivePeerCidTest, RotateFailsBeforeNewConnectionIdReceived)
{
    quic::connection conn(false, make_dcid({0xB0, 0xB1}));
    EXPECT_FALSE(conn.rotate_peer_cid().is_ok());
}

// ===========================================================================
// Receive Packet Coverage
// ===========================================================================

class ConnectionReceivePacketTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0xDE, 0xAD, 0xBE, 0xEF});
};

TEST_F(ConnectionReceivePacketTest, EmptyPacketFails)
{
    quic::connection conn(false, dcid_);
    auto r = conn.receive_packet(std::vector<uint8_t>{});
    EXPECT_FALSE(r.is_ok());
}

TEST_F(ConnectionReceivePacketTest, EmptyPacketDoesNotIncrementCounters)
{
    quic::connection conn(false, dcid_);
    auto bytes_before = conn.bytes_received();
    auto pkts_before = conn.packets_received();
    (void)conn.receive_packet(std::vector<uint8_t>{});
    EXPECT_EQ(conn.bytes_received(), bytes_before);
    EXPECT_EQ(conn.packets_received(), pkts_before);
}

TEST_F(ConnectionReceivePacketTest, MalformedShortHeaderBeforeHandshakeFails)
{
    quic::connection conn(false, dcid_);
    // Short header (high bit clear) with random bytes, before handshake.
    std::vector<uint8_t> pkt(20, 0x40);
    auto r = conn.receive_packet(pkt);
    EXPECT_FALSE(r.is_ok());
}

TEST_F(ConnectionReceivePacketTest, MalformedLongHeaderFails)
{
    quic::connection conn(false, dcid_);
    // Long header bit set but the bytes do not form a valid header.
    std::vector<uint8_t> pkt = {0xC0, 0x00};  // Truncated.
    auto r = conn.receive_packet(pkt);
    EXPECT_FALSE(r.is_ok());
}

TEST_F(ConnectionReceivePacketTest, ReceiveInDrainingStateAccepted)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "drain me").is_ok());
    ASSERT_TRUE(conn.is_draining());

    auto pkts_before = conn.packets_received();
    auto bytes_before = conn.bytes_received();

    // Even garbage is accepted (and counted) in draining.
    std::vector<uint8_t> pkt(8, 0xFF);
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
    EXPECT_EQ(conn.packets_received(), pkts_before + 1);
    EXPECT_EQ(conn.bytes_received(), bytes_before + pkt.size());
}

TEST_F(ConnectionReceivePacketTest, ReceiveAfterCloseDrivesIntoDrainingPath)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0x10, "x").is_ok());
    // close() puts us in closing/draining; second receive should be a no-op
    // success per the draining branch.
    std::vector<uint8_t> pkt(4, 0xAA);
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
}

TEST_F(ConnectionReceivePacketTest, ResetsIdleTimerOnValidEntry)
{
    quic::connection conn(false, dcid_);
    auto deadline_before = conn.idle_deadline();

    // Sleep briefly, then send a packet that fails parse but still increments
    // the byte/packet counters and resets the idle timer.
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::vector<uint8_t> pkt = {0xC0, 0x00, 0x00, 0x00, 0x01};
    (void)conn.receive_packet(pkt);

    // The reset must push the deadline forward (>=) compared to the original.
    EXPECT_GE(conn.idle_deadline(), deadline_before);
}

// ===========================================================================
// generate_packets() Coverage
// ===========================================================================

class ConnectionGeneratePacketsExtTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0xE0, 0xE1, 0xE2, 0xE3});
};

TEST_F(ConnectionGeneratePacketsExtTest, IdleConnectionProducesEmptyOrDoesNotCrash)
{
    quic::connection conn(false, dcid_);
    auto pkts = conn.generate_packets();
    // Either no packets, or all entries are empty (build_packet returns {}
    // when keys are not yet derived).
    bool ok = pkts.empty() ||
              std::all_of(pkts.begin(), pkts.end(),
                          [](const auto& p) { return p.empty(); });
    EXPECT_TRUE(ok);
}

TEST_F(ConnectionGeneratePacketsExtTest, GenerateAfterCloseReturnsNonCrashing)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "bye").is_ok());
    auto pkts = conn.generate_packets();
    (void)pkts;
    SUCCEED();
}

TEST_F(ConnectionGeneratePacketsExtTest, GenerateAfterCloseApplicationReturnsNonCrashing)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close_application(0x99, "app-bye").is_ok());
    auto pkts = conn.generate_packets();
    (void)pkts;
    SUCCEED();
}

TEST_F(ConnectionGeneratePacketsExtTest, GenerateAfterClosedReturnsEmpty)
{
    quic::connection conn(false, dcid_);
    // Force closed state via short idle timeout.
    auto params = make_params_with_idle(1);
    conn.set_local_params(params);
    conn.set_remote_params(params);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    conn.on_timeout();

    if (conn.is_closed())
    {
        auto pkts = conn.generate_packets();
        EXPECT_TRUE(pkts.empty());
    }
}

TEST_F(ConnectionGeneratePacketsExtTest, ServerInIdleProducesEmpty)
{
    quic::connection conn(true, dcid_);
    auto pkts = conn.generate_packets();
    bool ok = pkts.empty() ||
              std::all_of(pkts.begin(), pkts.end(),
                          [](const auto& p) { return p.empty(); });
    EXPECT_TRUE(ok);
}

// ===========================================================================
// Timer Tests
// ===========================================================================

class ConnectionTimerExtTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0xF0});
};

TEST_F(ConnectionTimerExtTest, IdleTimeoutDrivesConnectionToClosed)
{
    quic::connection conn(false, dcid_);
    auto params = make_params_with_idle(1);
    conn.set_local_params(params);
    conn.set_remote_params(params);

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    conn.on_timeout();

    EXPECT_TRUE(conn.is_closed());
    ASSERT_TRUE(conn.close_error_code().has_value());
    EXPECT_EQ(*conn.close_error_code(), 0u);
    EXPECT_EQ(conn.close_reason(), "Idle timeout");
}

TEST_F(ConnectionTimerExtTest, OnTimeoutInDrainingDoesNotImmediatelyClose)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "x").is_ok());
    ASSERT_TRUE(conn.is_draining());
    // Drain deadline is ~now + 300ms; timeout immediately should not close.
    conn.on_timeout();
    EXPECT_FALSE(conn.is_closed());
}

TEST_F(ConnectionTimerExtTest, DrainTimeoutTransitionsToClosed)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "x").is_ok());
    ASSERT_TRUE(conn.is_draining());
    // Wait past the drain deadline (3 * 100ms = 300ms).
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    conn.on_timeout();
    EXPECT_TRUE(conn.is_closed());
}

TEST_F(ConnectionTimerExtTest, NextTimeoutInDrainingReturnsDrainDeadline)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "").is_ok());
    auto t = conn.next_timeout();
    ASSERT_TRUE(t.has_value());
    EXPECT_GT(*t, std::chrono::steady_clock::now() -
                      std::chrono::seconds(1));
}

TEST_F(ConnectionTimerExtTest, NextTimeoutInClosedReturnsNullopt)
{
    quic::connection conn(false, dcid_);
    auto params = make_params_with_idle(1);
    conn.set_local_params(params);
    conn.set_remote_params(params);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    conn.on_timeout();
    if (conn.is_closed())
    {
        EXPECT_FALSE(conn.next_timeout().has_value());
    }
}

TEST_F(ConnectionTimerExtTest, NextTimeoutInIdleReturnsValue)
{
    quic::connection conn(false, dcid_);
    auto t = conn.next_timeout();
    EXPECT_TRUE(t.has_value());
}

// ===========================================================================
// Close Tests Extended
// ===========================================================================

class ConnectionCloseExtTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0x12, 0x34});
};

TEST_F(ConnectionCloseExtTest, CloseAppFromIdleSetsApplicationFlag)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close_application(0x77, "app").is_ok());
    EXPECT_TRUE(conn.is_draining() || conn.is_closed());
    ASSERT_TRUE(conn.close_error_code().has_value());
    EXPECT_EQ(*conn.close_error_code(), 0x77u);
    EXPECT_EQ(conn.close_reason(), "app");
    EXPECT_TRUE(conn.has_pending_data());
}

TEST_F(ConnectionCloseExtTest, CloseTransportThenAppKeepsTransportError)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0x42, "transport").is_ok());
    auto first_code = *conn.close_error_code();
    ASSERT_TRUE(conn.close_application(0x99, "app").is_ok());
    // Second close must not overwrite the first.
    EXPECT_EQ(*conn.close_error_code(), first_code);
    EXPECT_EQ(conn.close_reason(), "transport");
}

TEST_F(ConnectionCloseExtTest, CloseAppThenTransportKeepsAppError)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close_application(0x33, "app").is_ok());
    auto first_code = *conn.close_error_code();
    ASSERT_TRUE(conn.close(0x44, "transport").is_ok());
    EXPECT_EQ(*conn.close_error_code(), first_code);
    EXPECT_EQ(conn.close_reason(), "app");
}

TEST_F(ConnectionCloseExtTest, CloseFromHandshakingState)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());
    ASSERT_EQ(conn.state(), quic::connection_state::handshaking);
    ASSERT_TRUE(conn.close(7, "abort").is_ok());
    EXPECT_TRUE(conn.is_draining() || conn.is_closed());
    EXPECT_EQ(*conn.close_error_code(), 7u);
}

TEST_F(ConnectionCloseExtTest, CloseWithMaxUint64ErrorCode)
{
    quic::connection conn(false, dcid_);
    constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
    ASSERT_TRUE(conn.close(kMax, "max").is_ok());
    EXPECT_EQ(*conn.close_error_code(), kMax);
}

TEST_F(ConnectionCloseExtTest, ClosedConnectionStaysClosedAcrossMultipleCloseCalls)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "first").is_ok());
    auto state_after_first = conn.state();
    for (int i = 0; i < 5; ++i)
    {
        ASSERT_TRUE(conn.close(static_cast<uint64_t>(i), "n").is_ok());
        ASSERT_TRUE(conn.close_application(static_cast<uint64_t>(i), "n").is_ok());
    }
    EXPECT_EQ(conn.state(), state_after_first);
}

// ===========================================================================
// has_pending_data() Coverage
// ===========================================================================

class ConnectionPendingDataExtTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0xAA, 0xBB});
};

TEST_F(ConnectionPendingDataExtTest, NoPendingInIdle)
{
    quic::connection conn(false, dcid_);
    EXPECT_FALSE(conn.has_pending_data());
}

TEST_F(ConnectionPendingDataExtTest, PendingAfterStartHandshake)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());
    // After start_handshake we have pending crypto data (ClientHello).
    EXPECT_TRUE(conn.has_pending_data());
}

TEST_F(ConnectionPendingDataExtTest, PendingAfterClose)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "").is_ok());
    EXPECT_TRUE(conn.has_pending_data());
}

TEST_F(ConnectionPendingDataExtTest, PendingAfterCloseApplication)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close_application(1, "").is_ok());
    EXPECT_TRUE(conn.has_pending_data());
}

// ===========================================================================
// PMTUD Extended Coverage
// ===========================================================================

class ConnectionPmtudCoverageTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0x9A, 0x9B});
};

TEST_F(ConnectionPmtudCoverageTest, DisableWhileDisabledIsIdempotent)
{
    quic::connection conn(false, dcid_);
    ASSERT_FALSE(conn.pmtud_enabled());
    conn.disable_pmtud();
    EXPECT_FALSE(conn.pmtud_enabled());
    EXPECT_EQ(conn.path_mtu(), 1200u);
}

TEST_F(ConnectionPmtudCoverageTest, EnableThenDisableThenEnable)
{
    quic::connection conn(false, dcid_);
    conn.enable_pmtud();
    conn.disable_pmtud();
    conn.enable_pmtud();
    EXPECT_TRUE(conn.pmtud_enabled());
}

TEST_F(ConnectionPmtudCoverageTest, PathMtuMatchesController)
{
    quic::connection conn(false, dcid_);
    EXPECT_EQ(conn.path_mtu(), conn.pmtud().current_mtu());
    conn.enable_pmtud();
    EXPECT_EQ(conn.path_mtu(), conn.pmtud().current_mtu());
}

// ===========================================================================
// Statistics Coverage
// ===========================================================================

class ConnectionStatsCoverageTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0xC0, 0xDE});
};

TEST_F(ConnectionStatsCoverageTest, EmptyPacketDoesNotCountInDraining)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "").is_ok());
    auto pkts_before = conn.packets_received();
    // Empty packet: the empty-check happens BEFORE the draining branch, so
    // even in draining state, an empty packet returns an error and does not
    // increment the counter.
    auto r = conn.receive_packet(std::vector<uint8_t>{});
    EXPECT_FALSE(r.is_ok());
    EXPECT_EQ(conn.packets_received(), pkts_before);
}

TEST_F(ConnectionStatsCoverageTest, MultipleReceivesAccumulateBytes)
{
    quic::connection conn(false, dcid_);
    constexpr size_t kSize = 5;
    constexpr int kRounds = 3;
    auto bytes_before = conn.bytes_received();
    for (int i = 0; i < kRounds; ++i)
    {
        std::vector<uint8_t> pkt(kSize, 0xC0);
        (void)conn.receive_packet(pkt);
    }
    EXPECT_GE(conn.bytes_received(), bytes_before + kSize * kRounds);
}

TEST_F(ConnectionStatsCoverageTest, BytesSentAndPacketsSentStartZero)
{
    quic::connection conn(false, dcid_);
    EXPECT_EQ(conn.bytes_sent(), 0u);
    EXPECT_EQ(conn.packets_sent(), 0u);
}

// ===========================================================================
// Subsystem Read-Write Access
// ===========================================================================

class ConnectionSubsystemReadWriteTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0x6D, 0x6E});
};

TEST_F(ConnectionSubsystemReadWriteTest, FlowControlIsWritableThroughAccessor)
{
    quic::connection conn(false, dcid_);
    auto& fc = conn.flow_control();
    auto base = fc.send_limit();
    // update_send_limit is monotonic: raising above current works.
    fc.update_send_limit(base + 100000);
    EXPECT_EQ(conn.flow_control().send_limit(), base + 100000);
}

TEST_F(ConnectionSubsystemReadWriteTest, StreamManagerIsWritableThroughAccessor)
{
    quic::connection conn(false, dcid_);
    conn.streams().set_local_max_streams_bidi(42);
    EXPECT_EQ(conn.streams().local_max_streams_bidi(), 42u);
}

TEST_F(ConnectionSubsystemReadWriteTest, PeerCidManagerIsWritableThroughAccessor)
{
    quic::connection conn(false, dcid_);
    conn.peer_cid_manager().set_active_cid_limit(11);
    EXPECT_EQ(conn.peer_cid_manager().active_cid_limit(), 11u);
}

TEST_F(ConnectionSubsystemReadWriteTest, ConstAccessorsAreUsable)
{
    quic::connection conn(false, dcid_);
    const auto& c = conn;
    (void)c.flow_control();
    (void)c.streams();
    (void)c.crypto();
    (void)c.pmtud();
    (void)c.peer_cid_manager();
    SUCCEED();
}

// ===========================================================================
// Server-Specific Path Coverage
// ===========================================================================

class ConnectionServerPathTest : public ::testing::Test
{
};

TEST_F(ConnectionServerPathTest, ServerStartHandshakeFails)
{
    quic::connection conn(true, make_dcid({0x77}));
    auto r = conn.start_handshake("name");
    EXPECT_FALSE(r.is_ok());
    EXPECT_EQ(conn.state(), quic::connection_state::idle);
}

TEST_F(ConnectionServerPathTest, ServerInitServerHandshakeWithMissingCertFails)
{
    quic::connection conn(true, make_dcid({0x77}));
    auto r = conn.init_server_handshake("/no/such/cert.pem", "/no/such/key.pem");
    EXPECT_FALSE(r.is_ok());
    // State must remain idle on failure.
    EXPECT_EQ(conn.state(), quic::connection_state::idle);
}

TEST_F(ConnectionServerPathTest, ClientInitServerHandshakeFails)
{
    quic::connection conn(false, make_dcid({0x88}));
    auto r = conn.init_server_handshake("/x", "/y");
    EXPECT_FALSE(r.is_ok());
}

// ===========================================================================
// State Predicates
// ===========================================================================

class ConnectionStatePredicateTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0xEE, 0xFF});
};

TEST_F(ConnectionStatePredicateTest, IsDrainingTrueAfterClose)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "").is_ok());
    EXPECT_TRUE(conn.is_draining());
}

TEST_F(ConnectionStatePredicateTest, IsClosedTrueAfterIdleTimeout)
{
    quic::connection conn(false, dcid_);
    auto params = make_params_with_idle(1);
    conn.set_local_params(params);
    conn.set_remote_params(params);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    conn.on_timeout();
    EXPECT_TRUE(conn.is_closed());
}

TEST_F(ConnectionStatePredicateTest, IsEstablishedFalseInIdle)
{
    quic::connection conn(false, dcid_);
    EXPECT_FALSE(conn.is_established());
}

TEST_F(ConnectionStatePredicateTest, IsEstablishedFalseInHandshaking)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("h").is_ok());
    EXPECT_FALSE(conn.is_established());
}

TEST_F(ConnectionStatePredicateTest, IsEstablishedFalseInDraining)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.close(0, "").is_ok());
    EXPECT_FALSE(conn.is_established());
}

// ===========================================================================
// Parameterized Receive Packet Tests (table-driven)
// ===========================================================================

struct ReceiveCase
{
    const char* name;
    std::vector<uint8_t> data;
    bool expect_ok;
};

class ConnectionReceiveParamTest
    : public ::testing::TestWithParam<ReceiveCase>
{
};

TEST_P(ConnectionReceiveParamTest, MatchesExpectation)
{
    auto& c = GetParam();
    quic::connection conn(false, make_dcid({0x01, 0x02}));
    auto r = conn.receive_packet(c.data);
    EXPECT_EQ(r.is_ok(), c.expect_ok) << "case=" << c.name;
}

INSTANTIATE_TEST_SUITE_P(
    BadInputs,
    ConnectionReceiveParamTest,
    ::testing::Values(
        ReceiveCase{"empty", {}, false},
        ReceiveCase{"single_long_header_byte", {0xC0}, false},
        ReceiveCase{"truncated_long_header", {0xC0, 0x00}, false},
        ReceiveCase{"truncated_version", {0xC0, 0x00, 0x00, 0x00}, false},
        ReceiveCase{"short_header_before_handshake_short", {0x40}, false},
        ReceiveCase{"short_header_with_partial_dcid",
                    {0x40, 0x01}, false}),
    [](const ::testing::TestParamInfo<ReceiveCase>& info)
    {
        return std::string(info.param.name);
    });

// ===========================================================================
// Parameterized Close-Code Tests
// ===========================================================================

struct CloseCase
{
    const char* name;
    uint64_t error_code;
    std::string reason;
    bool application;
};

class ConnectionCloseParamTest
    : public ::testing::TestWithParam<CloseCase>
{
};

TEST_P(ConnectionCloseParamTest, RecordsErrorAndReason)
{
    auto& c = GetParam();
    quic::connection conn(false, make_dcid({0x01, 0x02}));

    auto r = c.application
        ? conn.close_application(c.error_code, c.reason)
        : conn.close(c.error_code, c.reason);
    ASSERT_TRUE(r.is_ok()) << "case=" << c.name;

    ASSERT_TRUE(conn.close_error_code().has_value());
    EXPECT_EQ(*conn.close_error_code(), c.error_code) << "case=" << c.name;
    EXPECT_EQ(conn.close_reason(), c.reason) << "case=" << c.name;
    EXPECT_TRUE(conn.is_draining() || conn.is_closed()) << "case=" << c.name;
}

INSTANTIATE_TEST_SUITE_P(
    Variations,
    ConnectionCloseParamTest,
    ::testing::Values(
        CloseCase{"transport_zero", 0, "", false},
        CloseCase{"transport_with_reason", 0x1A, "flow", false},
        CloseCase{"transport_large_code", 0xDEADBEEFu, "deadbeef", false},
        CloseCase{"app_zero", 0, "", true},
        CloseCase{"app_simple", 0x10, "app err", true},
        CloseCase{"app_long_reason", 0xFF, std::string(200, 'X'), true}),
    [](const ::testing::TestParamInfo<CloseCase>& info)
    {
        return std::string(info.param.name);
    });

// ===========================================================================
// Parameterized Param-Application Tests
// ===========================================================================

struct ParamApplyCase
{
    const char* name;
    uint64_t initial_max_data;
    uint64_t initial_max_streams_bidi;
    uint64_t initial_max_streams_uni;
    uint64_t active_cid_limit;
};

class ConnectionRemoteParamsParamTest
    : public ::testing::TestWithParam<ParamApplyCase>
{
};

TEST_P(ConnectionRemoteParamsParamTest, AppliesAllFields)
{
    auto& c = GetParam();
    quic::connection conn(false, make_dcid({0x77}));
    auto base_send_limit = conn.flow_control().send_limit();

    quic::transport_parameters p;
    p.initial_max_data = c.initial_max_data;
    p.initial_max_streams_bidi = c.initial_max_streams_bidi;
    p.initial_max_streams_uni = c.initial_max_streams_uni;
    p.active_connection_id_limit = c.active_cid_limit;
    conn.set_remote_params(p);

    // flow_controller::update_send_limit only raises the limit. So the
    // observed send_limit equals max(base, initial_max_data).
    auto expected_send_limit = std::max(base_send_limit, c.initial_max_data);
    EXPECT_EQ(conn.flow_control().send_limit(), expected_send_limit)
        << "case=" << c.name;
    EXPECT_EQ(conn.streams().peer_max_streams_bidi(), c.initial_max_streams_bidi)
        << "case=" << c.name;
    EXPECT_EQ(conn.streams().peer_max_streams_uni(), c.initial_max_streams_uni)
        << "case=" << c.name;
    EXPECT_EQ(conn.peer_cid_manager().active_cid_limit(), c.active_cid_limit)
        << "case=" << c.name;
}

INSTANTIATE_TEST_SUITE_P(
    Combinations,
    ConnectionRemoteParamsParamTest,
    ::testing::Values(
        ParamApplyCase{"all_zero", 0, 0, 0, 0},
        ParamApplyCase{"min_nonzero", 1, 1, 1, 1},
        ParamApplyCase{"typical", 1048576, 100, 100, 8},
        ParamApplyCase{"large_data", 64ull * 1024 * 1024, 50, 50, 16},
        ParamApplyCase{"many_streams", 1024, 10000, 5000, 4}),
    [](const ::testing::TestParamInfo<ParamApplyCase>& info)
    {
        return std::string(info.param.name);
    });

// ===========================================================================
// Receive Packet After Handshake Started — exercises process_long_header_packet
// ===========================================================================
//
// After start_handshake(), the client has derived initial keys, so
// process_long_header_packet can run beyond the early "keys not available"
// short-circuit. Frames built by frame_builder are then fed in raw (no
// AEAD), which is fine because the connection's process_frames operates
// directly on the payload span.

class ConnectionPostHandshakeReceiveTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0xAB, 0xCD, 0xEF, 0x10, 0x20, 0x30});

    static auto build_long_header_packet(quic::packet_type type,
                                         const quic::connection_id& dcid,
                                         const quic::connection_id& scid,
                                         uint64_t pn,
                                         std::span<const uint8_t> payload)
        -> std::vector<uint8_t>
    {
        std::vector<uint8_t> hdr;
        switch (type)
        {
        case quic::packet_type::initial:
            hdr = quic::packet_builder::build_initial(dcid, scid, {}, pn);
            break;
        case quic::packet_type::handshake:
            hdr = quic::packet_builder::build_handshake(dcid, scid, pn);
            break;
        case quic::packet_type::zero_rtt:
            hdr = quic::packet_builder::build_zero_rtt(dcid, scid, pn);
            break;
        case quic::packet_type::retry:
            hdr = quic::packet_builder::build_retry(dcid, scid, {}, {}, 0);
            break;
        default:
            break;
        }
        std::vector<uint8_t> pkt;
        pkt.reserve(hdr.size() + payload.size());
        pkt.insert(pkt.end(), hdr.begin(), hdr.end());
        pkt.insert(pkt.end(), payload.begin(), payload.end());
        return pkt;
    }
};

TEST_F(ConnectionPostHandshakeReceiveTest, InitialPacketAfterStartHandshakeIsAccepted)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    auto scid = quic::connection_id::generate();
    // PADDING frame (single 0x00 byte) is the simplest valid frame.
    std::vector<uint8_t> payload = {0x00};
    auto pkt = build_long_header_packet(quic::packet_type::initial,
                                         dcid_, scid, 0, payload);
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
    // Bytes/packets received must have grown.
    EXPECT_GT(conn.bytes_received(), 0u);
    EXPECT_GT(conn.packets_received(), 0u);
}

TEST_F(ConnectionPostHandshakeReceiveTest, HandshakePacketBeforeKeysJustReturnsOk)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    auto scid = quic::connection_id::generate();
    std::vector<uint8_t> payload = {0x00};  // PADDING
    auto pkt = build_long_header_packet(quic::packet_type::handshake,
                                         dcid_, scid, 0, payload);
    // Handshake keys are not yet available; the function exits early with ok().
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
}

TEST_F(ConnectionPostHandshakeReceiveTest, ZeroRttPacketKeysNotAvailableReturnsOk)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    auto scid = quic::connection_id::generate();
    std::vector<uint8_t> payload = {0x00};
    auto pkt = build_long_header_packet(quic::packet_type::zero_rtt,
                                         dcid_, scid, 0, payload);
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
}

TEST_F(ConnectionPostHandshakeReceiveTest, RetryPacketIsAcceptedSilently)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    auto scid = quic::connection_id::generate();
    auto pkt = build_long_header_packet(quic::packet_type::retry,
                                         dcid_, scid, 0,
                                         std::vector<uint8_t>{});
    // Retry packets are processed via the dedicated retry branch and return
    // ok() without further work in this implementation.
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
}

TEST_F(ConnectionPostHandshakeReceiveTest, InitialPacketWithPingFrame)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    auto scid = quic::connection_id::generate();
    // PING frame (0x01)
    std::vector<uint8_t> payload = {0x01};
    auto pkt = build_long_header_packet(quic::packet_type::initial,
                                         dcid_, scid, 1, payload);
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
}

TEST_F(ConnectionPostHandshakeReceiveTest, InitialPacketWithMultiplePaddingFrames)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    auto scid = quic::connection_id::generate();
    std::vector<uint8_t> payload(32, 0x00);  // 32 PADDING frames
    auto pkt = build_long_header_packet(quic::packet_type::initial,
                                         dcid_, scid, 2, payload);
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
}

TEST_F(ConnectionPostHandshakeReceiveTest, InitialPacketTracksLargestPacketNumber)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    auto scid = quic::connection_id::generate();
    std::vector<uint8_t> payload = {0x00};

    auto pkt0 = build_long_header_packet(quic::packet_type::initial,
                                         dcid_, scid, 5, payload);
    EXPECT_TRUE(conn.receive_packet(pkt0).is_ok());

    auto pkt1 = build_long_header_packet(quic::packet_type::initial,
                                         dcid_, scid, 10, payload);
    EXPECT_TRUE(conn.receive_packet(pkt1).is_ok());
    // Internal largest_received tracking is private; we just verify both
    // packets are accepted without errors.
}

// ===========================================================================
// Server-Side First-Packet Path — exercises is_server_ branches
// ===========================================================================
//
// The server constructor leaves remote_cid_ empty. The first long header
// packet should set both remote_cid_ (via the "remote_cid_.length() == 0"
// branch) and original_destination_connection_id (via the is_server_ branch).
// Additionally, the server needs init_server_handshake to have keys, but
// the early "keys not available" short-circuit lets us still exercise the
// pre-key code path and observe the effect.

class ConnectionServerReceiveTest : public ::testing::Test
{
};

TEST_F(ConnectionServerReceiveTest, ServerInitialPacketWithoutKeysStillUpdatesPacketsReceived)
{
    quic::connection conn(true, make_dcid({0x77, 0x88}));
    auto pkts_before = conn.packets_received();

    auto dcid = make_dcid({0x77, 0x88});
    auto scid = quic::connection_id::generate();
    std::vector<uint8_t> hdr =
        quic::packet_builder::build_initial(dcid, scid, {}, 0);
    std::vector<uint8_t> pkt = hdr;
    pkt.push_back(0x00);  // PADDING

    // Server does not have keys; the function returns ok() but the byte
    // counters and packet counters increase.
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
    EXPECT_EQ(conn.packets_received(), pkts_before + 1);
    EXPECT_GE(conn.bytes_received(), pkt.size());
}

// ===========================================================================
// generate_packets() — Walk Through Closing Path Branches
// ===========================================================================

class ConnectionGenerateClosingTest : public ::testing::Test
{
};

TEST_F(ConnectionGenerateClosingTest, GenerateAfterCloseFromHandshaking)
{
    // start_handshake → close → generate must not crash and must return a
    // (possibly empty) vector.
    quic::connection conn(false, make_dcid({0x42, 0x43}));
    ASSERT_TRUE(conn.start_handshake("h").is_ok());
    ASSERT_TRUE(conn.close(1, "abort").is_ok());
    auto pkts = conn.generate_packets();
    (void)pkts;
    SUCCEED();
}

TEST_F(ConnectionGenerateClosingTest, GenerateAfterIdleTimeoutClosed)
{
    quic::connection conn(false, make_dcid({0xA1}));
    auto p = make_params_with_idle(1);
    conn.set_local_params(p);
    conn.set_remote_params(p);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    conn.on_timeout();
    ASSERT_TRUE(conn.is_closed());
    auto pkts = conn.generate_packets();
    EXPECT_TRUE(pkts.empty());
}

// ===========================================================================
// Long DCID Handling
// ===========================================================================
//
// Tests targeting connection_id length variations. The connection accepts
// any DCID length between 0 and 20 (max_length).

class ConnectionDcidLengthTest
    : public ::testing::TestWithParam<size_t>
{
};

TEST_P(ConnectionDcidLengthTest, AcceptsDcidOfGivenLength)
{
    size_t len = GetParam();
    auto dcid = make_dcid_of_length(len, 0x55);
    quic::connection conn(false, dcid);
    EXPECT_EQ(conn.initial_dcid().length(), len);
    EXPECT_EQ(conn.remote_cid().length(), len);
    // Local CID is generated to default 8-byte length (see connection_id::generate).
    EXPECT_GT(conn.local_cid().length(), 0u);
}

INSTANTIATE_TEST_SUITE_P(
    Lengths,
    ConnectionDcidLengthTest,
    ::testing::Values(0u, 1u, 4u, 8u, 12u, 16u, 20u),
    [](const ::testing::TestParamInfo<size_t>& info)
    {
        return "len_" + std::to_string(info.param);
    });

// ===========================================================================
// Frame Dispatch Coverage — exercises handle_frame() variant branches
// ===========================================================================
//
// After start_handshake, the client has Initial-level read keys, so a
// crafted Initial packet whose payload is a serialized frame will pass
// process_long_header_packet → process_frames → handle_frame, exercising
// the various variant branches per frame type.

class ConnectionFrameDispatchTest : public ::testing::Test
{
protected:
    quic::connection_id dcid_ = make_dcid({0xF1, 0xF2, 0xF3, 0xF4});

    static auto make_initial_packet_with_frame_bytes(
        const quic::connection_id& dcid,
        const quic::connection_id& scid,
        uint64_t pn,
        std::span<const uint8_t> frame_bytes)
        -> std::vector<uint8_t>
    {
        auto hdr = quic::packet_builder::build_initial(dcid, scid, {}, pn);
        std::vector<uint8_t> pkt;
        pkt.reserve(hdr.size() + frame_bytes.size());
        pkt.insert(pkt.end(), hdr.begin(), hdr.end());
        pkt.insert(pkt.end(), frame_bytes.begin(), frame_bytes.end());
        return pkt;
    }
};

TEST_F(ConnectionFrameDispatchTest, AckFrameDispatchedSuccessfully)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::ack_frame af;
    af.largest_acknowledged = 0;
    af.ack_delay = 0;
    auto bytes = quic::frame_builder::build_ack(af);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    auto r = conn.receive_packet(pkt);
    EXPECT_TRUE(r.is_ok());
}

TEST_F(ConnectionFrameDispatchTest, MaxDataFrameUpdatesFlowControl)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());
    auto base = conn.flow_control().send_limit();

    quic::max_data_frame mdf;
    mdf.maximum_data = base + 4096;
    auto bytes = quic::frame_builder::build_max_data(mdf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
    EXPECT_EQ(conn.flow_control().send_limit(), base + 4096);
}

TEST_F(ConnectionFrameDispatchTest, MaxStreamsBidiFrameUpdatesPeerLimit)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::max_streams_frame msf;
    msf.maximum_streams = 99;
    msf.bidirectional = true;
    auto bytes = quic::frame_builder::build_max_streams(msf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
    EXPECT_EQ(conn.streams().peer_max_streams_bidi(), 99u);
}

TEST_F(ConnectionFrameDispatchTest, MaxStreamsUniFrameUpdatesPeerLimit)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::max_streams_frame msf;
    msf.maximum_streams = 77;
    msf.bidirectional = false;
    auto bytes = quic::frame_builder::build_max_streams(msf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
    EXPECT_EQ(conn.streams().peer_max_streams_uni(), 77u);
}

TEST_F(ConnectionFrameDispatchTest, ConnectionCloseFrameDrivesIntoDraining)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());
    ASSERT_FALSE(conn.is_draining());

    quic::connection_close_frame ccf;
    ccf.error_code = 0xAB;
    ccf.frame_type = 0;
    ccf.reason_phrase = "peer-initiated";
    ccf.is_application_error = false;
    auto bytes = quic::frame_builder::build_connection_close(ccf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());

    EXPECT_TRUE(conn.is_draining());
    ASSERT_TRUE(conn.close_error_code().has_value());
    EXPECT_EQ(*conn.close_error_code(), 0xABu);
    EXPECT_EQ(conn.close_reason(), "peer-initiated");
}

TEST_F(ConnectionFrameDispatchTest, ApplicationConnectionCloseFrameDrivesDraining)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::connection_close_frame ccf;
    ccf.error_code = 0x4242;
    ccf.reason_phrase = "app-bye";
    ccf.is_application_error = true;
    auto bytes = quic::frame_builder::build_connection_close(ccf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());

    EXPECT_TRUE(conn.is_draining());
    EXPECT_EQ(*conn.close_error_code(), 0x4242u);
}

TEST_F(ConnectionFrameDispatchTest, HandshakeDoneFrameOnClientFlipsToConnected)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());
    ASSERT_EQ(conn.state(), quic::connection_state::handshaking);

    quic::handshake_done_frame hdf;
    auto bytes = quic::frame_builder::build_handshake_done();

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());

    // handle_frame for handshake_done flips client state to connected.
    EXPECT_EQ(conn.state(), quic::connection_state::connected);
    EXPECT_EQ(conn.handshake_state(), quic::handshake_state::complete);
    EXPECT_TRUE(conn.is_established());
}

TEST_F(ConnectionFrameDispatchTest, HandshakeDoneFrameOnServerIgnored)
{
    quic::connection conn(true, dcid_);
    // Server cannot start_handshake but can init_server_handshake. We
    // skip the cert step since we just want is_server_ branch coverage —
    // handshake_done on server is a no-op.
    auto scid = quic::connection_id::generate();
    auto bytes = quic::frame_builder::build_handshake_done();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    // Without keys, the receive returns ok() before reaching handle_frame
    // anyway. We just check no crash and state remains idle.
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
    EXPECT_NE(conn.state(), quic::connection_state::connected);
}

TEST_F(ConnectionFrameDispatchTest, RetireConnectionIdFrameRetiresUnknownFails)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::retire_connection_id_frame rcf;
    rcf.sequence_number = 1234;  // never added
    auto bytes = quic::frame_builder::build_retire_connection_id(rcf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    // The handler returns the retire_cid result. Unknown sequence is an
    // error, so receive_packet propagates it as not-ok.
    auto r = conn.receive_packet(pkt);
    EXPECT_FALSE(r.is_ok());
}

TEST_F(ConnectionFrameDispatchTest, RetireConnectionIdFrameRetiresKnownSequence)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());
    // Seed an additional CID first so retire is allowed (cannot retire last).
    ASSERT_TRUE(conn.add_local_cid(quic::connection_id::generate(), 1).is_ok());

    quic::retire_connection_id_frame rcf;
    rcf.sequence_number = 1;
    auto bytes = quic::frame_builder::build_retire_connection_id(rcf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
}

TEST_F(ConnectionFrameDispatchTest, NewTokenFrameIsAccepted)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::new_token_frame ntf;
    ntf.token = {0x01, 0x02, 0x03, 0x04};
    auto bytes = quic::frame_builder::build_new_token(ntf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
}

TEST_F(ConnectionFrameDispatchTest, DataBlockedFrameIsAccepted)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::data_blocked_frame dbf;
    dbf.maximum_data = 99999;
    auto bytes = quic::frame_builder::build_data_blocked(dbf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
}

TEST_F(ConnectionFrameDispatchTest, StreamsBlockedFrameIsAccepted)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::streams_blocked_frame sbf;
    sbf.maximum_streams = 7;
    sbf.bidirectional = true;
    auto bytes = quic::frame_builder::build_streams_blocked(sbf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
}

TEST_F(ConnectionFrameDispatchTest, PathChallengeFrameIsAccepted)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::path_challenge_frame pcf;
    pcf.data = {{1, 2, 3, 4, 5, 6, 7, 8}};
    auto bytes = quic::frame_builder::build_path_challenge(pcf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
}

TEST_F(ConnectionFrameDispatchTest, PathResponseFrameIsAccepted)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::path_response_frame prf;
    prf.data = {{8, 7, 6, 5, 4, 3, 2, 1}};
    auto bytes = quic::frame_builder::build_path_response(prf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
}

TEST_F(ConnectionFrameDispatchTest, MaxStreamDataFrameOnUnknownStreamIsNoOp)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::max_stream_data_frame msdf;
    msdf.stream_id = 0;  // No such stream exists yet
    msdf.maximum_stream_data = 1024;
    auto bytes = quic::frame_builder::build_max_stream_data(msdf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
}

TEST_F(ConnectionFrameDispatchTest, ResetStreamOnUnknownStreamIsNoOp)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::reset_stream_frame rsf;
    rsf.stream_id = 1234;  // Doesn't exist
    rsf.application_error_code = 9;
    rsf.final_size = 0;
    auto bytes = quic::frame_builder::build_reset_stream(rsf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
}

TEST_F(ConnectionFrameDispatchTest, StopSendingOnUnknownStreamIsNoOp)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    quic::stop_sending_frame ssf;
    ssf.stream_id = 5555;
    ssf.application_error_code = 1;
    auto bytes = quic::frame_builder::build_stop_sending(ssf);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
}

TEST_F(ConnectionFrameDispatchTest, NewConnectionIdFrameStoresPeerCid)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());
    auto count_before = conn.peer_cid_manager().peer_cid_count();

    quic::new_connection_id_frame ncif;
    ncif.sequence_number = 1;
    ncif.retire_prior_to = 0;
    ncif.connection_id = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
    ncif.stateless_reset_token = {{0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}};
    auto bytes = quic::frame_builder::build_new_connection_id(ncif);

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, bytes);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
    EXPECT_GE(conn.peer_cid_manager().peer_cid_count(), count_before + 1);
}

// ===========================================================================
// Multi-Frame Payload Coverage
// ===========================================================================
//
// Exercises process_frames' loop over multiple frames in a single payload.

TEST_F(ConnectionFrameDispatchTest, MultipleFramesInOnePacket)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    // Build payload: PADDING + PING + MAX_DATA(base+1024) + PADDING
    auto base = conn.flow_control().send_limit();

    quic::max_data_frame mdf;
    mdf.maximum_data = base + 1024;
    auto md_bytes = quic::frame_builder::build_max_data(mdf);

    std::vector<uint8_t> payload;
    payload.push_back(0x00);              // PADDING
    payload.push_back(0x01);              // PING
    payload.insert(payload.end(), md_bytes.begin(), md_bytes.end());
    payload.push_back(0x00);              // PADDING

    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, payload);
    EXPECT_TRUE(conn.receive_packet(pkt).is_ok());
    EXPECT_EQ(conn.flow_control().send_limit(), base + 1024);
}

TEST_F(ConnectionFrameDispatchTest, UnparsableFramePayloadFails)
{
    quic::connection conn(false, dcid_);
    ASSERT_TRUE(conn.start_handshake("server").is_ok());

    // 0x30 is not a valid QUIC frame type; frame_parser::parse_all should error.
    std::vector<uint8_t> payload = {0x30, 0x00, 0x00};
    auto scid = quic::connection_id::generate();
    auto pkt = make_initial_packet_with_frame_bytes(dcid_, scid, 0, payload);
    auto r = conn.receive_packet(pkt);
    EXPECT_FALSE(r.is_ok());
}

// ===========================================================================
// next_timeout combined with loss detector
// ===========================================================================

TEST(ConnectionLossDetectorIntegration, NextTimeoutUsesEarliestOfIdleAndLoss)
{
    // Without sending packets, loss_detector has no timer. next_timeout
    // should return idle_deadline_ unchanged.
    quic::connection conn(false, make_dcid({0xCC}));
    auto t = conn.next_timeout();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(*t, conn.idle_deadline());
}
