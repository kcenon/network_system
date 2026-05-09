// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_socket_dispatcher_branch_test.cpp
 * @brief Direct-dispatch branch coverage for src/internal/quic_socket.cpp
 *        (Issue #1122, Round 1 of #953).
 *
 * Mirrors the strategy of @ref http2_server_dispatcher_branch_test.cpp
 * Round 3 / #1121: invokes the private @c quic_socket::process_frame and
 * its per-handler private members directly via the
 * @c quic_socket_test_access friend, sidestepping the wire path that
 * would otherwise require a live UDP peer plus a complete TLS 1.3
 * handshake to reach.
 *
 * Coverage targets per handler:
 *  - process_frame: every alternative arm of the protocols::quic::frame
 *    variant that the dispatcher inspects (crypto, stream, ack,
 *    connection_close, handshake_done, ping, padding) plus the implicit
 *    fall-through arm covering the 13 unhandled variants.
 *  - process_stream_frame: callback-set + callback-empty branches.
 *  - process_connection_close_frame: callback-set + callback-empty
 *    branches; resulting state transition to draining.
 *  - process_handshake_done_frame: client + server role branches and the
 *    handshake-already-complete short-circuit.
 *  - process_ack_frame: placeholder no-op invocation for coverage.
 *  - send_pending_packets: closed/idle early-return branch (state guard)
 *    and the populated-state path that flushes queued data even when no
 *    write keys are available (graceful failure of send_packet).
 *  - queue_crypto_data: append path with single + multiple + larger
 *    payloads, current_level()-driven indexing.
 *  - determine_encryption_level: every long-header packet_type branch
 *    (initial, zero_rtt, handshake, default fall-through) and the
 *    short-header (application) branch.
 *  - transition_state: drives the state machine through every reachable
 *    transition without a peer, exercising the atomic store and ensuring
 *    state() observes the new value.
 *
 * State preconditions: the socket is constructed on a UDP socket bound to
 * an ephemeral port with no peer. Most tests assert callback observations
 * rather than network state — the wire path is intentionally unreachable
 * here.
 */

#include "internal/quic_socket.h"
#include "internal/protocols/quic/frame.h"
#include "internal/protocols/quic/frame_types.h"
#include "internal/protocols/quic/keys.h"
#include "internal/protocols/quic/packet.h"

#include "quic_socket_test_access.h"

#include <gtest/gtest.h>

#include <asio.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace internal_ns = kcenon::network::internal;
namespace quic_proto = kcenon::network::protocols::quic;
namespace test_support = kcenon::network::tests::support;

using test_access = test_support::quic_socket_test_access;

namespace
{

// Helper to construct a fresh quic_socket bound to v4 ephemeral.
std::shared_ptr<internal_ns::quic_socket> make_quic(
    asio::io_context& io,
    internal_ns::quic_role role)
{
    asio::ip::udp::socket sock(io, asio::ip::udp::v4());
    return std::make_shared<internal_ns::quic_socket>(
        std::move(sock), role);
}

} // namespace

class QuicSocketDispatcherBranchTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        io_ = std::make_unique<asio::io_context>();
    }

    void TearDown() override
    {
        if (io_)
        {
            io_->stop();
            io_.reset();
        }
    }

    std::unique_ptr<asio::io_context> io_;
};

// ============================================================================
// process_frame: variant arm coverage
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, ProcessFrameDispatchesPaddingArm)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::frame f = quic_proto::padding_frame{};
    test_access::process_frame(*q, f);

    // No observable side effect for padding — coverage of the dispatch
    // arm is the assertion.
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::idle);
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessFrameDispatchesPingArm)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::frame f = quic_proto::ping_frame{};
    test_access::process_frame(*q, f);

    // PING frames don't transition state by themselves — they will be
    // ACKed with the next outgoing packet which is not driven here.
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::idle);
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessFrameDispatchesAckArm)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::ack_frame ack{};
    ack.largest_acknowledged = 5;
    ack.ack_delay = 100;

    quic_proto::frame f = ack;
    test_access::process_frame(*q, f);

    // process_ack_frame is a placeholder no-op; coverage of the dispatch
    // arm is the assertion.
    SUCCEED();
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessFrameDispatchesStreamArm)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    std::atomic<bool> got_callback{false};
    std::atomic<std::uint64_t> received_stream_id{0};
    std::atomic<bool> received_fin{false};
    q->set_stream_data_callback(
        [&](std::uint64_t sid, std::span<const std::uint8_t>, bool fin) {
            received_stream_id.store(sid);
            received_fin.store(fin);
            got_callback.store(true);
        });

    quic_proto::stream_frame sf{};
    sf.stream_id = 4;
    sf.data = {0xde, 0xad, 0xbe, 0xef};
    sf.fin = true;

    quic_proto::frame f = sf;
    test_access::process_frame(*q, f);

    EXPECT_TRUE(got_callback.load());
    EXPECT_EQ(received_stream_id.load(), 4u);
    EXPECT_TRUE(received_fin.load());
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessFrameDispatchesConnectionCloseArm)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    std::atomic<bool> got_close{false};
    std::atomic<std::uint64_t> received_code{0};
    q->set_close_callback(
        [&](std::uint64_t code, const std::string&) {
            received_code.store(code);
            got_close.store(true);
        });

    quic_proto::connection_close_frame cc{};
    cc.error_code = 0x7;
    cc.reason_phrase = "test";
    cc.is_application_error = false;

    quic_proto::frame f = cc;
    test_access::process_frame(*q, f);

    EXPECT_TRUE(got_close.load());
    EXPECT_EQ(received_code.load(), 0x7u);
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::draining);
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessFrameDispatchesHandshakeDoneArm)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::frame f = quic_proto::handshake_done_frame{};
    test_access::process_frame(*q, f);

    // Client-side HANDSHAKE_DONE flips handshake_complete_ true and
    // transitions state to connected.
    EXPECT_TRUE(test_access::handshake_complete(*q));
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::connected);
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessFrameDefaultFallthroughForUnhandledVariant)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // max_streams_frame is one of the 13 variants the dispatcher does
    // not inspect — falls through the if/else-if chain with no side
    // effect. Drives the implicit fall-through arm.
    quic_proto::max_streams_frame ms{};
    ms.bidirectional = true;
    ms.maximum_streams = 100;

    quic_proto::frame f = ms;
    test_access::process_frame(*q, f);

    // No observable side effect — coverage of the fall-through is the
    // assertion.
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::idle);
}

// ============================================================================
// process_stream_frame: callback-set + callback-empty branches
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, ProcessStreamFrameWithEmptyCallback)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // No callback set — exercises the empty-function branch.
    quic_proto::stream_frame sf{};
    sf.stream_id = 1;
    sf.data = {1, 2, 3};
    sf.fin = false;

    test_access::process_stream_frame(*q, sf);

    SUCCEED();
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessStreamFrameWithCallbackSet)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    std::atomic<int> invocation_count{0};
    std::atomic<std::uint64_t> last_stream_id{0};
    q->set_stream_data_callback(
        [&](std::uint64_t sid, std::span<const std::uint8_t>, bool) {
            invocation_count.fetch_add(1);
            last_stream_id.store(sid);
        });

    for (std::uint64_t sid : {std::uint64_t{1}, std::uint64_t{5},
                               std::uint64_t{9}})
    {
        quic_proto::stream_frame sf{};
        sf.stream_id = sid;
        sf.data = {0xa, 0xb};
        sf.fin = false;
        test_access::process_stream_frame(*q, sf);
    }

    EXPECT_EQ(invocation_count.load(), 3);
    EXPECT_EQ(last_stream_id.load(), 9u);
}

// ============================================================================
// process_ack_frame: placeholder no-op invocation
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, ProcessAckFramePlaceholderNoOp)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::ack_frame ack{};
    ack.largest_acknowledged = 42;
    ack.ack_delay = 250;
    ack.ranges.push_back({0, 3});

    test_access::process_ack_frame(*q, ack);

    // Placeholder — no side effect.
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::idle);
}

// ============================================================================
// process_connection_close_frame: callback-set + callback-empty branches
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, ProcessConnectionCloseFrameWithEmptyCallback)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // No callback — exercises the empty-function branch in the close
    // handler. The state still transitions to draining and the idle
    // timer is started.
    quic_proto::connection_close_frame cc{};
    cc.error_code = 0;
    cc.reason_phrase = "";

    test_access::process_connection_close_frame(*q, cc);

    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::draining);
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessConnectionCloseFrameWithApplicationError)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    std::atomic<std::uint64_t> received_code{0};
    std::atomic<bool> reason_seen{false};
    q->set_close_callback(
        [&](std::uint64_t code, const std::string& r) {
            received_code.store(code);
            reason_seen.store(!r.empty());
        });

    quic_proto::connection_close_frame cc{};
    cc.error_code = 0x100;
    cc.reason_phrase = "application error";
    cc.is_application_error = true;

    test_access::process_connection_close_frame(*q, cc);

    EXPECT_EQ(received_code.load(), 0x100u);
    EXPECT_TRUE(reason_seen.load());
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::draining);
}

// ============================================================================
// process_handshake_done_frame: client + server role branches and the
// handshake-already-complete short-circuit
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, ProcessHandshakeDoneFrameOnClientWhenIncomplete)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    std::atomic<int> connected_calls{0};
    q->set_connected_callback([&]() { connected_calls.fetch_add(1); });

    EXPECT_FALSE(test_access::handshake_complete(*q));

    test_access::process_handshake_done_frame(*q);

    EXPECT_TRUE(test_access::handshake_complete(*q));
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::connected);
    EXPECT_EQ(connected_calls.load(), 1);
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessHandshakeDoneFrameOnClientWhenAlreadyComplete)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // Pre-set handshake_complete_ to true — drives the
    // !handshake_complete_.load() short-circuit branch.
    test_access::set_handshake_complete(*q, true);

    std::atomic<int> connected_calls{0};
    q->set_connected_callback([&]() { connected_calls.fetch_add(1); });

    test_access::process_handshake_done_frame(*q);

    // No transition / no callback when handshake already complete.
    EXPECT_EQ(connected_calls.load(), 0);
    // State should still be idle since we never transitioned earlier.
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::idle);
}

TEST_F(QuicSocketDispatcherBranchTest, ProcessHandshakeDoneFrameOnServerIsIgnored)
{
    auto q = make_quic(*io_, internal_ns::quic_role::server);

    std::atomic<int> connected_calls{0};
    q->set_connected_callback([&]() { connected_calls.fetch_add(1); });

    test_access::process_handshake_done_frame(*q);

    // Server-side is the !=client branch — no transition / no callback.
    EXPECT_FALSE(test_access::handshake_complete(*q));
    EXPECT_EQ(connected_calls.load(), 0);
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::idle);
}

// ============================================================================
// queue_crypto_data: append path
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, QueueCryptoDataSinglePayload)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // current_level() defaults to initial when crypto is uninitialized.
    EXPECT_EQ(test_access::pending_crypto_data_size(
                  *q, quic_proto::encryption_level::initial),
              0u);

    test_access::queue_crypto_data(*q, std::vector<std::uint8_t>{1, 2, 3});

    EXPECT_EQ(test_access::pending_crypto_data_size(
                  *q, quic_proto::encryption_level::initial),
              1u);
}

TEST_F(QuicSocketDispatcherBranchTest, QueueCryptoDataMultiplePayloads)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    for (int i = 0; i < 5; ++i)
    {
        test_access::queue_crypto_data(
            *q, std::vector<std::uint8_t>(static_cast<std::size_t>(i + 1),
                                           static_cast<std::uint8_t>(i)));
    }

    EXPECT_EQ(test_access::pending_crypto_data_size(
                  *q, quic_proto::encryption_level::initial),
              5u);
}

TEST_F(QuicSocketDispatcherBranchTest, QueueCryptoDataLargePayload)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    constexpr std::size_t kSize = 4096;
    test_access::queue_crypto_data(
        *q, std::vector<std::uint8_t>(kSize, 0xAB));

    EXPECT_EQ(test_access::pending_crypto_data_size(
                  *q, quic_proto::encryption_level::initial),
              1u);
}

// ============================================================================
// send_pending_packets: state-guard branches
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, SendPendingPacketsEarlyReturnOnIdleState)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // Default state == idle — drives the early-return branch.
    test_access::send_pending_packets(*q);

    SUCCEED();
}

TEST_F(QuicSocketDispatcherBranchTest, SendPendingPacketsEarlyReturnOnClosedState)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // Force state to closed — drives the second early-return condition.
    test_access::transition_state(
        *q, internal_ns::quic_connection_state::closed);

    test_access::send_pending_packets(*q);

    SUCCEED();
}

TEST_F(QuicSocketDispatcherBranchTest, SendPendingPacketsWithQueuedCryptoDataNonIdle)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // Queue some crypto data so the populated-state path has work to do.
    test_access::queue_crypto_data(
        *q, std::vector<std::uint8_t>{0x10, 0x20});
    EXPECT_EQ(test_access::pending_crypto_data_size(
                  *q, quic_proto::encryption_level::initial),
              1u);

    // Move out of idle so the state-guard does not short-circuit.
    test_access::transition_state(
        *q, internal_ns::quic_connection_state::handshake);

    // Without write keys, send_packet returns an error and the work is
    // dropped from the queue — but the dispatch path through
    // send_pending_packets is exercised regardless.
    test_access::send_pending_packets(*q);

    EXPECT_EQ(test_access::pending_crypto_data_size(
                  *q, quic_proto::encryption_level::initial),
              0u);
}

// ============================================================================
// determine_encryption_level: every long-header packet_type branch and
// the short-header (application) branch
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, DetermineEncryptionLevelLongHeaderInitial)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::long_header lh{};
    // first_byte: long-form (bit7=1) + fixed-bit (bit6=1) + type Initial (bits 4-5 = 00)
    lh.first_byte = 0xC0;
    quic_proto::packet_header header = lh;

    EXPECT_EQ(test_access::determine_encryption_level(*q, header),
              quic_proto::encryption_level::initial);
}

TEST_F(QuicSocketDispatcherBranchTest, DetermineEncryptionLevelLongHeaderZeroRtt)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::long_header lh{};
    // first_byte: long-form + fixed-bit + type 0-RTT (bits 4-5 = 01)
    lh.first_byte = 0xD0;
    quic_proto::packet_header header = lh;

    EXPECT_EQ(test_access::determine_encryption_level(*q, header),
              quic_proto::encryption_level::zero_rtt);
}

TEST_F(QuicSocketDispatcherBranchTest, DetermineEncryptionLevelLongHeaderHandshake)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::long_header lh{};
    // first_byte: long-form + fixed-bit + type Handshake (bits 4-5 = 10)
    lh.first_byte = 0xE0;
    quic_proto::packet_header header = lh;

    EXPECT_EQ(test_access::determine_encryption_level(*q, header),
              quic_proto::encryption_level::handshake);
}

TEST_F(QuicSocketDispatcherBranchTest, DetermineEncryptionLevelLongHeaderRetryFallsThroughToInitial)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::long_header lh{};
    // first_byte: long-form + fixed-bit + type Retry (bits 4-5 = 11)
    lh.first_byte = 0xF0;
    quic_proto::packet_header header = lh;

    // Retry is not initial / 0-RTT / handshake — falls through the
    // default arm to encryption_level::initial.
    EXPECT_EQ(test_access::determine_encryption_level(*q, header),
              quic_proto::encryption_level::initial);
}

TEST_F(QuicSocketDispatcherBranchTest, DetermineEncryptionLevelShortHeaderIsApplication)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    quic_proto::short_header sh{};
    sh.first_byte = 0x40;  // short-form (bit7=0), fixed-bit (bit6=1)
    quic_proto::packet_header header = sh;

    EXPECT_EQ(test_access::determine_encryption_level(*q, header),
              quic_proto::encryption_level::application);
}

// ============================================================================
// transition_state: drives the state machine through every reachable
// transition without a peer
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, TransitionStateWalksAllReachableStates)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    using state = internal_ns::quic_connection_state;
    const std::array<state, 7> walk = {
        state::idle,
        state::handshake_start,
        state::handshake,
        state::connected,
        state::closing,
        state::draining,
        state::closed,
    };

    for (auto s : walk)
    {
        test_access::transition_state(*q, s);
        EXPECT_EQ(test_access::state(*q), s);
    }
}

TEST_F(QuicSocketDispatcherBranchTest, TransitionStateRoundTripIsIdempotent)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    using state = internal_ns::quic_connection_state;

    // Apply the same state twice — the atomic store happens regardless
    // and is observable via state().
    test_access::transition_state(*q, state::handshake);
    EXPECT_EQ(test_access::state(*q), state::handshake);

    test_access::transition_state(*q, state::handshake);
    EXPECT_EQ(test_access::state(*q), state::handshake);

    // And go backwards (e.g., for testing).
    test_access::transition_state(*q, state::idle);
    EXPECT_EQ(test_access::state(*q), state::idle);
}

// ============================================================================
// on_retransmit_timeout: drives the retransmit handler
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, OnRetransmitTimeoutOnIdleSocketIsSafe)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // on_retransmit_timeout calls send_pending_packets which short-
    // circuits on idle — exercises the handler entry path.
    test_access::on_retransmit_timeout(*q);

    SUCCEED();
}

TEST_F(QuicSocketDispatcherBranchTest, OnRetransmitTimeoutOnHandshakeSocketDrivesSendPending)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    test_access::queue_crypto_data(
        *q, std::vector<std::uint8_t>{0xAA, 0xBB});
    test_access::transition_state(
        *q, internal_ns::quic_connection_state::handshake);

    test_access::on_retransmit_timeout(*q);

    // send_pending_packets ran and (without keys) drained the queue.
    EXPECT_EQ(test_access::pending_crypto_data_size(
                  *q, quic_proto::encryption_level::initial),
              0u);
}

// ============================================================================
// process_crypto_frame: TLS-uninitialized error-path coverage
// ============================================================================

TEST_F(QuicSocketDispatcherBranchTest, ProcessCryptoFrameOnUninitializedTlsReturnsEarly)
{
    auto q = make_quic(*io_, internal_ns::quic_role::client);

    // Without init_client(), crypto_.process_crypto_data() returns an
    // error — drives the response_result.is_err() early-return branch.
    quic_proto::crypto_frame cf{};
    cf.offset = 0;
    cf.data = {0x16, 0x03, 0x03};

    test_access::process_crypto_frame(*q, cf);

    EXPECT_FALSE(test_access::handshake_complete(*q));
    EXPECT_EQ(test_access::state(*q),
              internal_ns::quic_connection_state::idle);
}
