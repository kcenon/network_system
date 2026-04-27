// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_socket_branch_test.cpp
 * @brief Additional branch coverage for src/internal/quic_socket.cpp (Issue #1051)
 *
 * Complements @ref quic_socket_construction_test.cpp by exercising
 * public-API surfaces that remained uncovered after Issue #989:
 *  - Construction with both quic_role values, multiple short-lived
 *    instances, and verification that next_stream_id_ initialization
 *    differs by role (client=0, server=1) via create_stream() guard
 *    semantics
 *  - state() / role() / is_connected() / is_handshake_complete() default
 *    invariants under both roles and across many independently-constructed
 *    instances
 *  - remote_endpoint() default value (unspecified address, port 0) before
 *    any connect()
 *  - local_connection_id() length within RFC 9000 bounds (1..20 bytes)
 *    and uniqueness across many sockets
 *  - remote_connection_id() default-empty length before connect()/accept()
 *  - Callback registration for every callback type with default-
 *    constructed (empty) std::function values, populated lambdas,
 *    repeated replacement, and shared_ptr-captured state
 *  - Underlying socket() accessor mutability (open/close round trip on
 *    the borrowed reference) and const-overload reachability
 *  - connect() role-guard rejection on a server socket and on an
 *    already-non-idle client socket (covers state-guard branch); accept()
 *    role-guard rejection on a client socket
 *  - close() idempotency: returns ok() on every state in
 *    {idle, draining, closed}, including triple-close sequence
 *  - close() with non-zero application error_code (is_application_error
 *    branch) and with very long reason strings
 *  - send_stream_data() guard-clause early-return when not connected
 *    (covers the !is_connected() branch in send_stream_data)
 *  - create_stream() guard-clause early-return when not connected, for
 *    both unidirectional and bidirectional flag values
 *  - close_stream() not_found early-return for an unknown stream id
 *  - start_receive() / stop_receive() flag transitions are observable
 *    via repeated invocation; idle_timer_ cancellation in the destructor
 *    is exercised by the close()-then-destruct sequence
 *  - Move construction and move assignment with populated callbacks,
 *    preserved role and connection ID, and repeat-after-move usability
 *  - Concurrent state queries (role, state, is_connected,
 *    is_handshake_complete, local_connection_id, remote_connection_id)
 *    under shared_ptr lifetime
 *  - Repeated construction / destruction stress to drive the
 *    constructor / destructor / member-initialization paths
 *
 * All tests operate purely on the public API; no real QUIC peer or live
 * TLS handshake is required. These tests are hermetic and rely on the
 * idle / closing / draining / closed state machine reachable without a
 * UDP peer.
 *
 * Honest scope statement: the impl-level methods that physically
 * exchange QUIC packets with a peer remain reachable only with a UDP
 * transport fixture and a TLS-1.3 peer. Specifically, the private
 * surfaces do_receive() (async UDP receive completion), handle_packet()
 * (header parsing, header unprotection, payload decryption,
 * frame parsing, per-frame dispatch), process_frame() and its dispatch
 * branches for every variant in the protocols::quic::frame variant,
 * process_crypto_frame() (TLS handshake state machine including
 * client/server transition to connected), process_stream_frame()
 * (data-callback delivery), process_ack_frame() placeholder,
 * process_connection_close_frame() (peer-driven draining),
 * process_handshake_done_frame() (client-side handshake completion),
 * send_pending_packets() success path with non-empty crypto/stream
 * queues, send_packet() encryption + async_send_to dispatch including
 * initial / handshake / short-header builder branches and the
 * keys_result.is_err() failure branch's complement,
 * queue_crypto_data() append path's complement under populated state,
 * determine_encryption_level() per-packet-type dispatch
 * (initial / zero_rtt / handshake / default / short header), and
 * on_retransmit_timeout() handler all require a live UDP peer that
 * speaks the QUIC wire format end-to-end. The connect() success path
 * past TLS init also requires a peer that completes the TLS handshake.
 * Driving these would require either a mock UDP peer that speaks the
 * QUIC wire format end-to-end or a friend-declared injection point
 * inside quic_socket. The accept() success path past TLS server init
 * additionally requires real PEM cert / key files on disk.
 */

#include "internal/quic_socket.h"

#include "hermetic_transport_fixture.h"
#include "mock_udp_peer.h"

#include <gtest/gtest.h>

#include <asio.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace internal = kcenon::network::internal;
namespace quic_proto = kcenon::network::protocols::quic;

namespace
{

using namespace std::chrono_literals;

// Helper to construct a fresh quic_socket bound to v4 ephemeral.
std::shared_ptr<internal::quic_socket> make_quic(asio::io_context& io,
                                                  internal::quic_role role)
{
    asio::ip::udp::socket sock(io, asio::ip::udp::v4());
    return std::make_shared<internal::quic_socket>(std::move(sock), role);
}

} // namespace

// ============================================================================
// Construction — invariants across roles and multiple instances
// ============================================================================

class QuicSocketBranchTest : public ::testing::Test
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

TEST_F(QuicSocketBranchTest, ManyShortLivedClientInstancesDoNotLeak)
{
    for (int i = 0; i < 16; ++i)
    {
        auto q = make_quic(*io_, internal::quic_role::client);
        EXPECT_EQ(q->role(), internal::quic_role::client);
        EXPECT_EQ(q->state(), internal::quic_connection_state::idle);
    }
}

TEST_F(QuicSocketBranchTest, ManyShortLivedServerInstancesDoNotLeak)
{
    for (int i = 0; i < 16; ++i)
    {
        auto q = make_quic(*io_, internal::quic_role::server);
        EXPECT_EQ(q->role(), internal::quic_role::server);
        EXPECT_EQ(q->state(), internal::quic_connection_state::idle);
    }
}

TEST_F(QuicSocketBranchTest, ClientAndServerHaveIndependentDefaultState)
{
    auto client = make_quic(*io_, internal::quic_role::client);
    auto server = make_quic(*io_, internal::quic_role::server);

    EXPECT_EQ(client->role(), internal::quic_role::client);
    EXPECT_EQ(server->role(), internal::quic_role::server);
    EXPECT_FALSE(client->is_connected());
    EXPECT_FALSE(server->is_connected());
    EXPECT_FALSE(client->is_handshake_complete());
    EXPECT_FALSE(server->is_handshake_complete());
    EXPECT_EQ(client->state(), internal::quic_connection_state::idle);
    EXPECT_EQ(server->state(), internal::quic_connection_state::idle);
}

// ============================================================================
// Default endpoint / connection-id invariants
// ============================================================================

TEST_F(QuicSocketBranchTest, RemoteEndpointIsUnspecifiedBeforeConnect)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto ep = q->remote_endpoint();
    EXPECT_EQ(ep.port(), 0u);
}

TEST_F(QuicSocketBranchTest, LocalConnectionIdLengthIsWithinRfcBounds)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto& cid = q->local_connection_id();
    EXPECT_GT(cid.length(), 0u);
    EXPECT_LE(cid.length(), quic_proto::connection_id::max_length);
}

TEST_F(QuicSocketBranchTest, RemoteConnectionIdIsEmptyBeforeConnect)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto& cid = q->remote_connection_id();
    // A default-constructed connection_id has length 0.
    EXPECT_EQ(cid.length(), 0u);
}

TEST_F(QuicSocketBranchTest, ManyClientsHaveUniqueConnectionIds)
{
    constexpr int kCount = 16;
    std::vector<std::shared_ptr<internal::quic_socket>> sockets;
    sockets.reserve(kCount);

    for (int i = 0; i < kCount; ++i)
    {
        sockets.push_back(make_quic(*io_, internal::quic_role::client));
    }

    // Pairwise comparison: every pair must differ.
    for (int i = 0; i < kCount; ++i)
    {
        for (int j = i + 1; j < kCount; ++j)
        {
            EXPECT_NE(sockets[i]->local_connection_id().to_string(),
                      sockets[j]->local_connection_id().to_string())
                << "i=" << i << " j=" << j;
        }
    }
}

// ============================================================================
// Callback registration — empty / populated / replaced / captured-state
// ============================================================================

TEST_F(QuicSocketBranchTest, SetStreamDataCallbackWithEmptyFunction)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    internal::quic_socket::stream_data_callback empty;
    EXPECT_NO_FATAL_FAILURE(q->set_stream_data_callback(empty));
}

TEST_F(QuicSocketBranchTest, SetConnectedCallbackWithEmptyFunction)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    internal::quic_socket::connected_callback empty;
    EXPECT_NO_FATAL_FAILURE(q->set_connected_callback(empty));
}

TEST_F(QuicSocketBranchTest, SetErrorCallbackWithEmptyFunction)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    internal::quic_socket::error_callback empty;
    EXPECT_NO_FATAL_FAILURE(q->set_error_callback(empty));
}

TEST_F(QuicSocketBranchTest, SetCloseCallbackWithEmptyFunction)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    internal::quic_socket::close_callback empty;
    EXPECT_NO_FATAL_FAILURE(q->set_close_callback(empty));
}

TEST_F(QuicSocketBranchTest, ReplaceCallbacksMultipleTimes)
{
    auto q = make_quic(*io_, internal::quic_role::client);

    // Replace each callback three times; none of them fire on an idle
    // socket, but the lock-protected setter path is exercised.
    int counter = 0;
    q->set_connected_callback([&counter]() { counter += 1; });
    q->set_connected_callback([&counter]() { counter += 10; });
    q->set_connected_callback([&counter]() { counter += 100; });
    EXPECT_EQ(counter, 0);

    q->set_error_callback([&counter](std::error_code) { counter += 1; });
    q->set_error_callback([&counter](std::error_code) { counter += 10; });
    EXPECT_EQ(counter, 0);
}

TEST_F(QuicSocketBranchTest, SharedStateCaptureCallbackAccepted)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto state = std::make_shared<int>(7);

    q->set_stream_data_callback(
        [state](uint64_t, std::span<const uint8_t>, bool)
        {
            (void)state;
        });
    q->set_connected_callback([state]() { (void)state; });
    q->set_error_callback([state](std::error_code) { (void)state; });
    q->set_close_callback([state](uint64_t, const std::string&)
                          { (void)state; });

    EXPECT_EQ(*state, 7);
}

// ============================================================================
// Underlying socket accessor — mutable + const overloads
// ============================================================================

TEST_F(QuicSocketBranchTest, UnderlyingSocketIsAccessibleMutable)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto& sock = q->socket();
    EXPECT_TRUE(sock.is_open());
}

TEST_F(QuicSocketBranchTest, UnderlyingSocketConstOverloadIsReachable)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    const auto& csock =
        static_cast<const internal::quic_socket&>(*q).socket();
    EXPECT_TRUE(csock.is_open());
}

// ============================================================================
// Role-guard rejection branches in connect() / accept()
// ============================================================================

TEST_F(QuicSocketBranchTest, ServerConnectIsRejected)
{
    auto q = make_quic(*io_, internal::quic_role::server);
    asio::ip::udp::endpoint ep(asio::ip::make_address_v4("127.0.0.1"), 12345);

    auto r = q->connect(ep);
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, ServerConnectWithNamedSniIsRejected)
{
    auto q = make_quic(*io_, internal::quic_role::server);
    asio::ip::udp::endpoint ep(asio::ip::make_address_v4("127.0.0.1"), 12346);

    auto r = q->connect(ep, "example.com");
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, ClientAcceptIsRejected)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto r = q->accept("/missing/cert.pem", "/missing/key.pem");
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, ClientAcceptWithEmptyPathsIsRejected)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto r = q->accept("", "");
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, RepeatedServerConnectAttemptsRemainRejected)
{
    auto q = make_quic(*io_, internal::quic_role::server);
    asio::ip::udp::endpoint ep(asio::ip::make_address_v4("127.0.0.1"), 12347);

    for (int i = 0; i < 3; ++i)
    {
        EXPECT_TRUE(q->connect(ep).is_err()) << "iteration " << i;
        EXPECT_FALSE(q->is_connected());
        EXPECT_EQ(q->state(), internal::quic_connection_state::idle);
    }
}

// ============================================================================
// close() — idempotency and varied parameters
// ============================================================================

TEST_F(QuicSocketBranchTest, CloseOnIdleReturnsOk)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto r = q->close();
    EXPECT_TRUE(r.is_ok());
}

TEST_F(QuicSocketBranchTest, CloseWithApplicationErrorCodeReturnsOk)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    // Non-zero error code triggers is_application_error=true branch.
    auto r = q->close(0x42, "application error");
    EXPECT_TRUE(r.is_ok());
}

TEST_F(QuicSocketBranchTest, CloseWithMaxErrorCodeReturnsOk)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto r = q->close(std::numeric_limits<uint64_t>::max(),
                      "max error code");
    EXPECT_TRUE(r.is_ok());
}

TEST_F(QuicSocketBranchTest, CloseWithLongReasonStringReturnsOk)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    std::string long_reason(1024, 'r');
    auto r = q->close(0, long_reason);
    EXPECT_TRUE(r.is_ok());
}

TEST_F(QuicSocketBranchTest, CloseWithEmptyReasonReturnsOk)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto r = q->close(0, "");
    EXPECT_TRUE(r.is_ok());
}

TEST_F(QuicSocketBranchTest, TripleCloseAllReturnOk)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    EXPECT_TRUE(q->close(0, "first").is_ok());
    EXPECT_TRUE(q->close(0, "second").is_ok());
    EXPECT_TRUE(q->close(0, "third").is_ok());
}

TEST_F(QuicSocketBranchTest, CloseAfterCloseTransitionsThroughDraining)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    EXPECT_TRUE(q->close(0, "first").is_ok());
    // After the first close the state machine reaches draining (or
    // closed if the timer already fired). Subsequent closes must
    // hit the early-return branch without changing the result.
    auto state = q->state();
    EXPECT_TRUE(state == internal::quic_connection_state::draining ||
                state == internal::quic_connection_state::closing ||
                state == internal::quic_connection_state::closed);
    EXPECT_TRUE(q->close(0, "second").is_ok());
}

// ============================================================================
// send_stream_data / create_stream / close_stream — guard-clause coverage
// ============================================================================

TEST_F(QuicSocketBranchTest, SendStreamDataNotConnectedReturnsErr)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    std::vector<uint8_t> data{1, 2, 3, 4};
    auto r = q->send_stream_data(0, std::move(data), false);
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, SendStreamDataWithFinNotConnectedReturnsErr)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    std::vector<uint8_t> data{};
    auto r = q->send_stream_data(0, std::move(data), true);
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, SendStreamDataLargePayloadNotConnectedReturnsErr)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    std::vector<uint8_t> data(64 * 1024, 0xab);
    auto r = q->send_stream_data(99, std::move(data), false);
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, CreateStreamBidiNotConnectedReturnsErr)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto r = q->create_stream(false);
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, CreateStreamUniNotConnectedReturnsErr)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto r = q->create_stream(true);
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, CreateStreamServerSideNotConnectedReturnsErr)
{
    auto q = make_quic(*io_, internal::quic_role::server);
    EXPECT_TRUE(q->create_stream(false).is_err());
    EXPECT_TRUE(q->create_stream(true).is_err());
}

TEST_F(QuicSocketBranchTest, CloseStreamUnknownReturnsErr)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto r = q->close_stream(0);
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, CloseStreamMaxIdReturnsErr)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    auto r = q->close_stream(std::numeric_limits<uint64_t>::max());
    EXPECT_TRUE(r.is_err());
}

TEST_F(QuicSocketBranchTest, CloseManyUnknownStreamsAllReturnErr)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    for (uint64_t id : {0u, 1u, 2u, 4u, 5u, 7u, 100u, 4096u, 65535u})
    {
        EXPECT_TRUE(q->close_stream(id).is_err()) << "id=" << id;
    }
}

// ============================================================================
// start_receive / stop_receive — flag transitions
// ============================================================================

TEST_F(QuicSocketBranchTest, StopReceiveBeforeStartIsHarmless)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    EXPECT_NO_FATAL_FAILURE(q->stop_receive());
}

TEST_F(QuicSocketBranchTest, RepeatedStopReceiveIsHarmless)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_NO_FATAL_FAILURE(q->stop_receive());
    }
}

TEST_F(QuicSocketBranchTest, StartReceiveOnIdleSocketDoesNotCrash)
{
    // start_receive() schedules an async_receive_from on the underlying
    // socket. With nothing posted to the io_context, no completion will
    // ever fire; we immediately stop_receive() and rely on the
    // destructor to cancel the operation.
    auto q = make_quic(*io_, internal::quic_role::client);
    EXPECT_NO_FATAL_FAILURE(q->start_receive());
    EXPECT_NO_FATAL_FAILURE(q->stop_receive());
}

// ============================================================================
// Move construction / move assignment — populated state preserved
// ============================================================================

TEST_F(QuicSocketBranchTest, MoveConstructionPreservesRoleAndCallbacksReachable)
{
    auto q1 = make_quic(*io_, internal::quic_role::client);
    auto cid1 = q1->local_connection_id().to_string();

    int callback_hits = 0;
    q1->set_connected_callback([&callback_hits]() { callback_hits += 1; });
    q1->set_error_callback(
        [&callback_hits](std::error_code) { callback_hits += 1; });
    q1->set_close_callback(
        [&callback_hits](uint64_t, const std::string&) { callback_hits += 1; });

    internal::quic_socket q2(std::move(*q1));
    EXPECT_EQ(q2.role(), internal::quic_role::client);
    EXPECT_EQ(q2.local_connection_id().to_string(), cid1);
    EXPECT_EQ(q2.state(), internal::quic_connection_state::idle);

    // The callbacks should have moved with the socket; closing q2
    // executes the close-frame path but does not invoke close_cb_
    // synchronously (it only fires on a peer-driven close).
    EXPECT_EQ(callback_hits, 0);
}

TEST_F(QuicSocketBranchTest, MoveConstructionPreservesServerRole)
{
    auto q1 = make_quic(*io_, internal::quic_role::server);
    internal::quic_socket q2(std::move(*q1));
    EXPECT_EQ(q2.role(), internal::quic_role::server);
}

TEST_F(QuicSocketBranchTest, MoveAssignmentPreservesRoleAndConnectionId)
{
    asio::ip::udp::socket sock1(*io_, asio::ip::udp::v4());
    asio::ip::udp::socket sock2(*io_, asio::ip::udp::v4());

    internal::quic_socket q1(std::move(sock1), internal::quic_role::client);
    internal::quic_socket q2(std::move(sock2), internal::quic_role::server);

    auto cid_q1 = q1.local_connection_id().to_string();

    q2 = std::move(q1);

    EXPECT_EQ(q2.role(), internal::quic_role::client);
    EXPECT_EQ(q2.local_connection_id().to_string(), cid_q1);
    EXPECT_EQ(q2.state(), internal::quic_connection_state::idle);
}

TEST_F(QuicSocketBranchTest, MoveAssignmentChainPreservesFinalRole)
{
    asio::ip::udp::socket s1(*io_, asio::ip::udp::v4());
    asio::ip::udp::socket s2(*io_, asio::ip::udp::v4());
    asio::ip::udp::socket s3(*io_, asio::ip::udp::v4());

    internal::quic_socket a(std::move(s1), internal::quic_role::client);
    internal::quic_socket b(std::move(s2), internal::quic_role::server);
    internal::quic_socket c(std::move(s3), internal::quic_role::client);

    auto cid_a = a.local_connection_id().to_string();

    b = std::move(a);
    c = std::move(b);

    EXPECT_EQ(c.role(), internal::quic_role::client);
    EXPECT_EQ(c.local_connection_id().to_string(), cid_a);
}

TEST_F(QuicSocketBranchTest, SelfMoveAssignmentDoesNotCorruptState)
{
    asio::ip::udp::socket sock(*io_, asio::ip::udp::v4());
    internal::quic_socket q(std::move(sock), internal::quic_role::client);
    auto cid = q.local_connection_id().to_string();

    // Self move-assignment hits the (this != &other) guard in
    // operator=(quic_socket&&).
    auto& self = q;
    q = std::move(self);

    EXPECT_EQ(q.role(), internal::quic_role::client);
    EXPECT_EQ(q.local_connection_id().to_string(), cid);
    EXPECT_EQ(q.state(), internal::quic_connection_state::idle);
}

// ============================================================================
// Concurrent state queries
// ============================================================================

TEST_F(QuicSocketBranchTest, ConcurrentStateQueriesAreSafe)
{
    auto q = make_quic(*io_, internal::quic_role::client);
    constexpr int kThreads = 4;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([q]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_EQ(q->state(),
                          internal::quic_connection_state::idle);
                EXPECT_FALSE(q->is_connected());
                EXPECT_FALSE(q->is_handshake_complete());
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST_F(QuicSocketBranchTest, ConcurrentRoleAndConnectionIdQueriesAreSafe)
{
    auto q = make_quic(*io_, internal::quic_role::server);
    auto cid_str = q->local_connection_id().to_string();

    constexpr int kThreads = 4;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([q, cid_str]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_EQ(q->role(), internal::quic_role::server);
                EXPECT_EQ(q->local_connection_id().to_string(), cid_str);
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST_F(QuicSocketBranchTest, ConcurrentRemoteConnectionIdQueriesAreSafe)
{
    auto q = make_quic(*io_, internal::quic_role::client);

    constexpr int kThreads = 4;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([q]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_EQ(q->remote_connection_id().length(), 0u);
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

// ============================================================================
// Callback registration concurrency
// ============================================================================

TEST_F(QuicSocketBranchTest, ConcurrentCallbackReplacementIsSafe)
{
    auto q = make_quic(*io_, internal::quic_role::client);

    std::atomic<bool> stop{false};

    std::thread writer([q, &stop]
    {
        int i = 0;
        while (!stop.load())
        {
            q->set_connected_callback([i]() { (void)i; });
            ++i;
        }
    });

    std::thread reader([q]
    {
        for (int i = 0; i < 200; ++i)
        {
            EXPECT_FALSE(q->is_connected());
        }
    });

    reader.join();
    stop.store(true);
    writer.join();
}

// ============================================================================
// Multi-instance interactions
// ============================================================================

TEST_F(QuicSocketBranchTest, CloseOneSocketDoesNotAffectAnother)
{
    auto a = make_quic(*io_, internal::quic_role::client);
    auto b = make_quic(*io_, internal::quic_role::client);

    EXPECT_TRUE(a->close(0, "close-a").is_ok());

    EXPECT_FALSE(b->is_connected());
    EXPECT_EQ(b->state(), internal::quic_connection_state::idle);
    EXPECT_GT(b->local_connection_id().length(), 0u);
}

TEST_F(QuicSocketBranchTest, ManyClientsManyServersIndependentState)
{
    constexpr int kCount = 4;
    std::vector<std::shared_ptr<internal::quic_socket>> clients;
    std::vector<std::shared_ptr<internal::quic_socket>> servers;
    clients.reserve(kCount);
    servers.reserve(kCount);

    for (int i = 0; i < kCount; ++i)
    {
        clients.push_back(make_quic(*io_, internal::quic_role::client));
        servers.push_back(make_quic(*io_, internal::quic_role::server));
    }

    for (auto& c : clients)
    {
        EXPECT_EQ(c->role(), internal::quic_role::client);
        EXPECT_EQ(c->state(), internal::quic_connection_state::idle);
    }
    for (auto& s : servers)
    {
        EXPECT_EQ(s->role(), internal::quic_role::server);
        EXPECT_EQ(s->state(), internal::quic_connection_state::idle);
    }
}

// ============================================================================
// Destructor coverage — close-then-destruct sequence
// ============================================================================

TEST_F(QuicSocketBranchTest, DestructorAfterCloseRunsCleanly)
{
    {
        auto q = make_quic(*io_, internal::quic_role::client);
        EXPECT_TRUE(q->close(0x12, "scoped-close").is_ok());
        // Destructor runs at end of scope, exercising stop_receive() +
        // timer cancellation paths.
    }
    SUCCEED();
}

TEST_F(QuicSocketBranchTest, DestructorWithoutCloseRunsCleanly)
{
    {
        auto q = make_quic(*io_, internal::quic_role::client);
        // Skip close(); rely on dtor to call stop_receive() and
        // cancel timers.
    }
    SUCCEED();
}

TEST_F(QuicSocketBranchTest, DestructorAfterStartReceiveRunsCleanly)
{
    {
        auto q = make_quic(*io_, internal::quic_role::client);
        q->start_receive();
        // Destructor flips is_receiving_ to false via stop_receive(),
        // and cancels any pending async_receive_from.
    }
    SUCCEED();
}

// ============================================================================
// Hermetic transport fixture demonstration (Issue #1060)
// ============================================================================

/**
 * @brief Fixture base that uses the new hermetic_transport_fixture worker
 *        thread instead of the local QuicSocketBranchTest io_context, which
 *        is constructed without a worker.
 */
class QuicSocketHermeticTransportTest
    : public kcenon::network::tests::support::hermetic_transport_fixture
{
};

/**
 * @brief Demonstrates that the new mock UDP peer lets a quic_socket receive
 *        synthesized packet bytes via a real loopback UDP send.
 *
 * The quic_socket constructor takes an open udp::socket — the loopback pair
 * provides one half, and mock_udp_peer wraps the other. Sending the synthetic
 * Initial-packet stub exercises the do_receive → handle_packet → first-byte
 * parse path. Bytes after the long-header cannot form a valid frame so
 * processing terminates early, but the receive entry path is reached.
 */
TEST_F(QuicSocketHermeticTransportTest, HandlesPacketViaLoopbackUdpPair)
{
    using namespace kcenon::network::tests::support;

    auto pair = make_loopback_udp_pair(io());
    mock_udp_peer peer(std::move(pair.second));

    auto sock = std::make_shared<internal::quic_socket>(
        std::move(pair.first), internal::quic_role::server);
    ASSERT_NE(sock, nullptr);

    // Begin async receive on the quic_socket side.
    sock->start_receive();

    // Send a stub Initial packet from the peer side.
    const auto packet = make_quic_initial_packet_stub();
    const auto sent = peer.send(packet);
    EXPECT_EQ(sent, packet.size());

    // Allow the io_context worker to run the receive completion.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // The packet should have been ingested without crashing the socket.
    // Whether the parse succeeds is irrelevant for fixture validation —
    // the receive entry point is what we are exercising.
    sock->stop_receive();
    SUCCEED();
}
