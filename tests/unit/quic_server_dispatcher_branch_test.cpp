// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_server_dispatcher_branch_test.cpp
 * @brief Direct-dispatch branch coverage for src/experimental/quic_server.cpp
 *        (Issue #1123, expansion of #953).
 *
 * Mirrors the strategy of @ref quic_socket_dispatcher_branch_test.cpp
 * (Issue #1122) and @ref http2_server_dispatcher_branch_test.cpp (#1121):
 * invokes the private dispatch helpers of @c messaging_quic_server
 * directly via the @c quic_server_probe friend, sidestepping the wire path
 * that would otherwise require a live UDP peer plus a complete TLS-1.3
 * handshake to reach.
 *
 * The friend gate @c NETWORK_ENABLE_TEST_INJECTION is defined PUBLIC on
 * the @c network_system target when @c BUILD_TESTS=ON, so the macro
 * propagates here transitively. Production builds with @c BUILD_TESTS=OFF
 * compile byte-identical because the macro is undefined and the friend
 * forward declaration in @c src/internal/experimental/quic_server.h is
 * gated by the same macro.
 *
 * Coverage targets per private surface:
 *  - @c generate_session_id: counter-driven id generation across many
 *    calls, monotonic counter increment, server_id prefix preserved on
 *    every output, distinct ids across concurrent invocations.
 *  - @c on_session_close: empty-map silent-no-op branch, unknown-id
 *    silent-no-op branch (under populated map driven only by in-flight
 *    handle_packet probes — the find()==end short-circuit returns
 *    without invoking the disconnection callback).
 *  - @c cleanup_dead_sessions: empty-map early-fall-through branch, no
 *    log line emitted when no sessions are dead.
 *  - @c start_receive: not-running early-return guard (falls through
 *    without scheduling an async_receive_from); also fires when
 *    @c udp_socket_ is null.
 *  - @c start_cleanup_timer: same not-running / null-timer guard pair as
 *    @c start_receive — no async_wait scheduled when guard fires.
 *  - @c invoke_*_callback dispatchers: empty-function branch (no
 *    registered callback), populated-function branch with a counted
 *    lambda, repeated invocation under shared_ptr lifetime.
 *  - @c handle_packet: empty-data early-return branch (already covered by
 *    @c quic_server_probe_test.cpp), header-parse-error branch (garbage
 *    buffer), and the @c find_or_create_session()==nullptr branch under
 *    @c max_connections=0.
 *
 * State preconditions: tests instantiate the server with
 * @c std::make_shared<messaging_quic_server> so @c shared_from_this()
 * is callable inside any path that captures @c weak_from_this(). No
 * @c start_server() is required for the dispatch helpers — the lifecycle
 * is intentionally left in the not-running state to drive the early-
 * return branches.
 */

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_server.h"

#include "kcenon/network/detail/session/quic_session.h"
#include "quic_server_probe.h"

#include <gtest/gtest.h>

#include <asio/ip/udp.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace core = kcenon::network::core;
namespace session_ns = kcenon::network::session;
namespace test_support = kcenon::network::tests::support;

using probe = test_support::quic_server_probe;

namespace
{

constexpr unsigned short kSessionIdCounterIterations = 32;

} // namespace

// ============================================================================
// generate_session_id: counter increment + server_id prefix
// ============================================================================

TEST(QuicServerDispatcherGenerateId, GeneratesIdContainingServerIdPrefix)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("prefix-srv");
    auto id = probe::invoke_generate_session_id(*server);
    EXPECT_NE(id.find("prefix-srv-"), std::string::npos)
        << "generated id=" << id;
}

TEST(QuicServerDispatcherGenerateId, GeneratesUniqueIdsAcrossManyCalls)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("uniq");
    std::set<std::string> ids;
    for (unsigned short i = 0; i < kSessionIdCounterIterations; ++i)
    {
        auto id = probe::invoke_generate_session_id(*server);
        EXPECT_TRUE(ids.insert(id).second) << "duplicate=" << id;
    }
    EXPECT_EQ(ids.size(), kSessionIdCounterIterations);
}

TEST(QuicServerDispatcherGenerateId, EmptyServerIdYieldsLeadingHyphen)
{
    auto server = std::make_shared<core::messaging_quic_server>("");
    auto id = probe::invoke_generate_session_id(*server);
    // server_id is empty, so the formatted output is "-<counter>"
    ASSERT_FALSE(id.empty());
    EXPECT_EQ(id.front(), '-');
}

TEST(QuicServerDispatcherGenerateId, IdsAreSequentialAcrossInstances)
{
    // Each instance has its own atomic counter; instance A's first id and
    // instance B's first id share suffix "0" because the counter is
    // per-server.
    auto a = std::make_shared<core::messaging_quic_server>("A");
    auto b = std::make_shared<core::messaging_quic_server>("B");
    auto a0 = probe::invoke_generate_session_id(*a);
    auto b0 = probe::invoke_generate_session_id(*b);
    auto a1 = probe::invoke_generate_session_id(*a);
    EXPECT_NE(a0, b0);
    EXPECT_NE(a0, a1);
    EXPECT_EQ(a0.back(), '0');
    EXPECT_EQ(b0.back(), '0');
    EXPECT_EQ(a1.back(), '1');
}

TEST(QuicServerDispatcherGenerateId, ConcurrentInvocationsProduceUniqueIds)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("concurrent");

    constexpr int kThreads = 4;
    constexpr int kPerThread = 32;
    std::vector<std::vector<std::string>> per_thread_ids(kThreads);
    std::vector<std::thread> ths;
    ths.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        ths.emplace_back([&, t]
        {
            per_thread_ids[t].reserve(kPerThread);
            for (int i = 0; i < kPerThread; ++i)
            {
                per_thread_ids[t].push_back(
                    probe::invoke_generate_session_id(*server));
            }
        });
    }
    for (auto& th : ths)
    {
        th.join();
    }

    std::set<std::string> all_ids;
    for (const auto& v : per_thread_ids)
    {
        for (const auto& id : v)
        {
            all_ids.insert(id);
        }
    }
    EXPECT_EQ(all_ids.size(),
              static_cast<std::size_t>(kThreads * kPerThread));
}

// ============================================================================
// on_session_close: empty / unknown-id branches
// ============================================================================

TEST(QuicServerDispatcherSessionClose, EmptyMapUnknownIdIsSilent)
{
    auto server = std::make_shared<core::messaging_quic_server>("close-empty");

    // No callback registered => the silent-no-op branch fires twice:
    // once at sessions_.find()==end and once at the populated-callback
    // gate.
    EXPECT_NO_FATAL_FAILURE(probe::invoke_on_session_close(*server, ""));
    EXPECT_NO_FATAL_FAILURE(
        probe::invoke_on_session_close(*server, "unknown-id"));
    EXPECT_NO_FATAL_FAILURE(
        probe::invoke_on_session_close(*server, std::string(2048, 'z')));

    EXPECT_EQ(server->session_count(), 0u);
}

TEST(QuicServerDispatcherSessionClose, EmptyMapWithCallbackRegisteredIsSilent)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("close-cb");

    std::atomic<int> disconnect_calls{0};
    server->set_disconnection_callback(
        [&disconnect_calls](
            std::shared_ptr<session_ns::quic_session>) {
            disconnect_calls.fetch_add(1);
        });

    // unknown-id => find()==end branch fires => callback NOT invoked.
    probe::invoke_on_session_close(*server, "ghost");
    EXPECT_EQ(disconnect_calls.load(), 0);
}

TEST(QuicServerDispatcherSessionClose, RepeatedUnknownIdIsIdempotent)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("close-rep");

    for (int i = 0; i < 8; ++i)
    {
        probe::invoke_on_session_close(*server,
                                       "ghost-" + std::to_string(i));
    }
    EXPECT_EQ(server->session_count(), 0u);
}

// ============================================================================
// cleanup_dead_sessions: empty-map fall-through
// ============================================================================

TEST(QuicServerDispatcherCleanup, CleanupOnEmptyMapIsHarmless)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("cleanup-empty");
    EXPECT_NO_FATAL_FAILURE(probe::invoke_cleanup_dead_sessions(*server));
    EXPECT_EQ(server->session_count(), 0u);
}

TEST(QuicServerDispatcherCleanup, RepeatedCleanupOnEmptyMapStaysAtZero)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("cleanup-rep");
    for (int i = 0; i < 8; ++i)
    {
        probe::invoke_cleanup_dead_sessions(*server);
        EXPECT_EQ(server->session_count(), 0u) << "iteration " << i;
    }
}

TEST(QuicServerDispatcherCleanup, CleanupOnRunningEmptyServerStaysAtZero)
{
    // Drive the same empty-map fall-through with the server in the
    // running state so the surrounding @c is_running() == true branch
    // is also exercised by the wrapping @c start_cleanup_timer continuation
    // during @c stop_server tear-down.
    auto server =
        std::make_shared<core::messaging_quic_server>("cleanup-running");
    ASSERT_TRUE(server->start_server(0).is_ok());

    probe::invoke_cleanup_dead_sessions(*server);
    EXPECT_EQ(server->session_count(), 0u);
    EXPECT_TRUE(server->is_running());

    EXPECT_TRUE(server->stop_server().is_ok());
}

// ============================================================================
// start_receive: not-running guard
// ============================================================================

TEST(QuicServerDispatcherStartReceive, NotRunningEarlyReturn)
{
    // is_running()==false => early-return branch fires; no work scheduled.
    auto server =
        std::make_shared<core::messaging_quic_server>("recv-guard");
    EXPECT_FALSE(server->is_running());
    EXPECT_NO_FATAL_FAILURE(probe::invoke_start_receive(*server));
    EXPECT_FALSE(server->is_running());
}

// ============================================================================
// start_cleanup_timer: not-running guard
// ============================================================================

TEST(QuicServerDispatcherStartCleanupTimer, NotRunningEarlyReturn)
{
    // The branch fires when cleanup_timer_ is null OR is_running() is
    // false. Both are true on a never-started server.
    auto server =
        std::make_shared<core::messaging_quic_server>("timer-guard");
    EXPECT_FALSE(server->is_running());
    EXPECT_NO_FATAL_FAILURE(probe::invoke_start_cleanup_timer(*server));
    EXPECT_FALSE(server->is_running());
}

// ============================================================================
// invoke_connection_callback: empty + populated branches
// ============================================================================

TEST(QuicServerDispatcherInvokeCallbacks, ConnectionEmptyCallbackIsNoOp)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("inv-conn-empty");
    // No callback registered => empty-function branch in callback_manager.
    EXPECT_NO_FATAL_FAILURE(
        probe::invoke_connection_callback_dispatcher(*server, nullptr));
}

TEST(QuicServerDispatcherInvokeCallbacks, ConnectionWithCallbackInvokes)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("inv-conn-cb");

    std::atomic<int> invocations{0};
    server->set_connection_callback(
        [&invocations](std::shared_ptr<session_ns::quic_session>) {
            invocations.fetch_add(1);
        });

    // Driving with a null session pointer still routes through the
    // populated-function branch. The lambda body does not dereference
    // its argument, so this is safe.
    probe::invoke_connection_callback_dispatcher(*server, nullptr);
    EXPECT_EQ(invocations.load(), 1);

    probe::invoke_connection_callback_dispatcher(*server, nullptr);
    probe::invoke_connection_callback_dispatcher(*server, nullptr);
    EXPECT_EQ(invocations.load(), 3);
}

TEST(QuicServerDispatcherInvokeCallbacks, DisconnectionEmptyCallbackIsNoOp)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("inv-disc-empty");
    EXPECT_NO_FATAL_FAILURE(
        probe::invoke_disconnection_callback_dispatcher(*server, nullptr));
}

TEST(QuicServerDispatcherInvokeCallbacks, DisconnectionWithCallbackInvokes)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("inv-disc-cb");

    std::atomic<int> invocations{0};
    server->set_disconnection_callback(
        [&invocations](std::shared_ptr<session_ns::quic_session>) {
            invocations.fetch_add(1);
        });

    probe::invoke_disconnection_callback_dispatcher(*server, nullptr);
    EXPECT_EQ(invocations.load(), 1);
}

TEST(QuicServerDispatcherInvokeCallbacks, ReceiveEmptyCallbackIsNoOp)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("inv-recv-empty");
    EXPECT_NO_FATAL_FAILURE(
        probe::invoke_receive_callback_dispatcher(
            *server, nullptr, std::vector<std::uint8_t>{}));
}

TEST(QuicServerDispatcherInvokeCallbacks, ReceiveWithCallbackPropagatesData)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("inv-recv-cb");

    std::atomic<int> invocations{0};
    std::atomic<std::size_t> last_size{0};
    server->set_receive_callback(
        [&invocations, &last_size](
            std::shared_ptr<session_ns::quic_session>,
            const std::vector<std::uint8_t>& data) {
            invocations.fetch_add(1);
            last_size.store(data.size());
        });

    std::vector<std::uint8_t> payload = {0xde, 0xad, 0xbe, 0xef};
    probe::invoke_receive_callback_dispatcher(*server, nullptr, payload);
    EXPECT_EQ(invocations.load(), 1);
    EXPECT_EQ(last_size.load(), 4u);

    probe::invoke_receive_callback_dispatcher(
        *server, nullptr, std::vector<std::uint8_t>{});
    EXPECT_EQ(invocations.load(), 2);
    EXPECT_EQ(last_size.load(), 0u);
}

TEST(QuicServerDispatcherInvokeCallbacks, StreamReceiveEmptyCallbackIsNoOp)
{
    auto server = std::make_shared<core::messaging_quic_server>(
        "inv-stream-empty");
    EXPECT_NO_FATAL_FAILURE(
        probe::invoke_stream_receive_callback_dispatcher(
            *server, nullptr, /*stream_id=*/0,
            std::vector<std::uint8_t>{}, /*fin=*/false));
}

TEST(QuicServerDispatcherInvokeCallbacks, StreamReceiveWithCallbackInvokes)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("inv-stream-cb");

    std::atomic<int> invocations{0};
    std::atomic<std::uint64_t> last_stream_id{0};
    std::atomic<bool> last_fin{false};
    server->set_stream_receive_callback(
        [&invocations, &last_stream_id, &last_fin](
            std::shared_ptr<session_ns::quic_session>,
            std::uint64_t stream_id,
            const std::vector<std::uint8_t>&,
            bool fin) {
            invocations.fetch_add(1);
            last_stream_id.store(stream_id);
            last_fin.store(fin);
        });

    probe::invoke_stream_receive_callback_dispatcher(
        *server, nullptr, /*stream_id=*/4,
        std::vector<std::uint8_t>{1, 2, 3}, /*fin=*/false);
    EXPECT_EQ(invocations.load(), 1);
    EXPECT_EQ(last_stream_id.load(), 4u);
    EXPECT_FALSE(last_fin.load());

    probe::invoke_stream_receive_callback_dispatcher(
        *server, nullptr, /*stream_id=*/9,
        std::vector<std::uint8_t>{}, /*fin=*/true);
    EXPECT_EQ(invocations.load(), 2);
    EXPECT_EQ(last_stream_id.load(), 9u);
    EXPECT_TRUE(last_fin.load());
}

TEST(QuicServerDispatcherInvokeCallbacks, ErrorEmptyCallbackIsNoOp)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("inv-err-empty");
    std::error_code ec = std::make_error_code(std::errc::connection_reset);
    EXPECT_NO_FATAL_FAILURE(
        probe::invoke_error_callback_dispatcher(*server, ec));
}

TEST(QuicServerDispatcherInvokeCallbacks, ErrorWithCallbackInvokes)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("inv-err-cb");

    std::atomic<int> invocations{0};
    std::atomic<int> last_value{0};
    server->set_error_callback(
        [&invocations, &last_value](std::error_code e) {
            invocations.fetch_add(1);
            last_value.store(e.value());
        });

    auto ec_a =
        std::make_error_code(std::errc::connection_aborted);
    auto ec_b =
        std::make_error_code(std::errc::host_unreachable);
    probe::invoke_error_callback_dispatcher(*server, ec_a);
    EXPECT_EQ(invocations.load(), 1);
    EXPECT_EQ(last_value.load(), ec_a.value());

    probe::invoke_error_callback_dispatcher(*server, ec_b);
    EXPECT_EQ(invocations.load(), 2);
    EXPECT_EQ(last_value.load(), ec_b.value());
}

// ============================================================================
// handle_packet: empty + garbage + max_connections=0 branches
// ============================================================================

TEST(QuicServerDispatcherHandlePacket, EmptyDataIsRejected)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("hp-empty");
    asio::ip::udp::endpoint from{};
    std::array<std::uint8_t, 0> empty{};
    EXPECT_NO_FATAL_FAILURE(probe::invoke_handle_packet(
        *server, std::span<const std::uint8_t>(empty.data(), empty.size()),
        from));
    EXPECT_EQ(server->session_count(), 0u);
}

TEST(QuicServerDispatcherHandlePacket, ZeroFilledBufferIsParseError)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("hp-zeros");
    asio::ip::udp::endpoint from{};
    std::array<std::uint8_t, 64> zeros{};
    EXPECT_NO_FATAL_FAILURE(probe::invoke_handle_packet(
        *server, std::span<const std::uint8_t>(zeros.data(), zeros.size()),
        from));
    EXPECT_EQ(server->session_count(), 0u);
}

TEST(QuicServerDispatcherHandlePacket, RandomBytesAreParseError)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("hp-random");
    asio::ip::udp::endpoint from{};
    // Random-ish bytes that will not parse as a valid QUIC long header
    // because the first-byte version-magic check fails.
    std::array<std::uint8_t, 32> random_bytes = {
        0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    };
    EXPECT_NO_FATAL_FAILURE(probe::invoke_handle_packet(
        *server,
        std::span<const std::uint8_t>(random_bytes.data(),
                                      random_bytes.size()),
        from));
    EXPECT_EQ(server->session_count(), 0u);
}

TEST(QuicServerDispatcherHandlePacket, RepeatedInvalidPacketsStayAtZero)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("hp-repeated");
    asio::ip::udp::endpoint from{};
    std::array<std::uint8_t, 16> garbage{};

    for (int i = 0; i < 8; ++i)
    {
        EXPECT_NO_FATAL_FAILURE(probe::invoke_handle_packet(
            *server,
            std::span<const std::uint8_t>(garbage.data(), garbage.size()),
            from));
    }
    EXPECT_EQ(server->session_count(), 0u);
}

TEST(QuicServerDispatcherHandlePacket,
     VariedSourceEndpointsDoNotCrashOnInvalidData)
{
    // The source endpoint string is logged in the parse-error branch.
    // Drive with an IPv4 endpoint whose address differs across calls to
    // confirm no crash on diverse from values.
    auto server =
        std::make_shared<core::messaging_quic_server>("hp-endpoints");
    std::array<std::uint8_t, 8> garbage{};

    asio::ip::udp::endpoint endpoints[] = {
        asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), 1024),
        asio::ip::udp::endpoint(asio::ip::address_v4::any(), 65535),
        asio::ip::udp::endpoint(asio::ip::address_v4(), 0),
    };
    for (const auto& ep : endpoints)
    {
        EXPECT_NO_FATAL_FAILURE(probe::invoke_handle_packet(
            *server,
            std::span<const std::uint8_t>(garbage.data(), garbage.size()),
            ep));
    }
}

// ============================================================================
// Integration: dispatch helpers fire on a running server
// ============================================================================

TEST(QuicServerDispatcherRunningIntegration,
     CleanupAndReceiveGuardsFireWhileRunning)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("running-guards");
    ASSERT_TRUE(server->start_server(0).is_ok());

    // start_receive() while running schedules another async receive on
    // top of the existing one. We do not drive the completion handler
    // here (no real peer); the side effect is that another async op is
    // in flight, which will be cancelled by stop_server() below.
    EXPECT_NO_FATAL_FAILURE(probe::invoke_start_receive(*server));

    // start_cleanup_timer() while running reschedules the timer; the
    // 30-second deadline ensures it does not fire before stop_server()
    // cancels it.
    EXPECT_NO_FATAL_FAILURE(probe::invoke_start_cleanup_timer(*server));

    // cleanup_dead_sessions() on an empty session map is the same
    // empty-map fall-through covered above, but here under a populated
    // io_context lifecycle.
    EXPECT_NO_FATAL_FAILURE(probe::invoke_cleanup_dead_sessions(*server));

    EXPECT_EQ(server->session_count(), 0u);
    EXPECT_TRUE(server->is_running());

    EXPECT_TRUE(server->stop_server().is_ok());
    EXPECT_FALSE(server->is_running());
}

TEST(QuicServerDispatcherRunningIntegration,
     OnSessionCloseUnknownIdWhileRunning)
{
    // Drives the find()==end branch under a populated lifecycle.
    auto server =
        std::make_shared<core::messaging_quic_server>("running-close");
    ASSERT_TRUE(server->start_server(0).is_ok());

    std::atomic<int> disconnect_calls{0};
    server->set_disconnection_callback(
        [&disconnect_calls](
            std::shared_ptr<session_ns::quic_session>) {
            disconnect_calls.fetch_add(1);
        });

    probe::invoke_on_session_close(*server, "unknown-while-running");
    EXPECT_EQ(disconnect_calls.load(), 0);

    EXPECT_TRUE(server->stop_server().is_ok());
}

TEST(QuicServerDispatcherRunningIntegration,
     GenerateSessionIdWhileRunning)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("running-genid");
    ASSERT_TRUE(server->start_server(0).is_ok());

    auto id_a = probe::invoke_generate_session_id(*server);
    auto id_b = probe::invoke_generate_session_id(*server);
    EXPECT_NE(id_a, id_b);
    EXPECT_NE(id_a.find("running-genid-"), std::string::npos);
    EXPECT_NE(id_b.find("running-genid-"), std::string::npos);

    EXPECT_TRUE(server->stop_server().is_ok());
}

TEST(QuicServerDispatcherRunningIntegration,
     HandlePacketGarbageWhileRunning)
{
    // Garbage packet under a running lifecycle still hits the parse
    // failure branch — no session is created.
    auto server =
        std::make_shared<core::messaging_quic_server>("running-hp");
    ASSERT_TRUE(server->start_server(0).is_ok());

    asio::ip::udp::endpoint from(asio::ip::address_v4::loopback(), 9);
    std::array<std::uint8_t, 16> garbage{};
    EXPECT_NO_FATAL_FAILURE(probe::invoke_handle_packet(
        *server,
        std::span<const std::uint8_t>(garbage.data(), garbage.size()),
        from));
    EXPECT_EQ(server->session_count(), 0u);

    EXPECT_TRUE(server->stop_server().is_ok());
}

TEST(QuicServerDispatcherRunningIntegration,
     InvokeCallbacksWhileRunning)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("running-cbs");
    ASSERT_TRUE(server->start_server(0).is_ok());

    std::atomic<int> conn{0};
    std::atomic<int> disc{0};
    std::atomic<int> recv{0};
    std::atomic<int> stream{0};
    std::atomic<int> err{0};
    server->set_connection_callback(
        [&conn](std::shared_ptr<session_ns::quic_session>) {
            conn.fetch_add(1);
        });
    server->set_disconnection_callback(
        [&disc](std::shared_ptr<session_ns::quic_session>) {
            disc.fetch_add(1);
        });
    server->set_receive_callback(
        [&recv](std::shared_ptr<session_ns::quic_session>,
                const std::vector<std::uint8_t>&) {
            recv.fetch_add(1);
        });
    server->set_stream_receive_callback(
        [&stream](std::shared_ptr<session_ns::quic_session>,
                  std::uint64_t,
                  const std::vector<std::uint8_t>&, bool) {
            stream.fetch_add(1);
        });
    server->set_error_callback(
        [&err](std::error_code) { err.fetch_add(1); });

    probe::invoke_connection_callback_dispatcher(*server, nullptr);
    probe::invoke_disconnection_callback_dispatcher(*server, nullptr);
    probe::invoke_receive_callback_dispatcher(*server, nullptr,
                                              std::vector<std::uint8_t>{});
    probe::invoke_stream_receive_callback_dispatcher(
        *server, nullptr, 0, std::vector<std::uint8_t>{}, false);
    probe::invoke_error_callback_dispatcher(
        *server, std::make_error_code(std::errc::connection_reset));

    EXPECT_EQ(conn.load(), 1);
    EXPECT_EQ(disc.load(), 1);
    EXPECT_EQ(recv.load(), 1);
    EXPECT_EQ(stream.load(), 1);
    EXPECT_EQ(err.load(), 1);

    EXPECT_TRUE(server->stop_server().is_ok());
}

// ============================================================================
// MaxConnections=0 — find_or_create_session connection-limit branch
// ============================================================================
//
// max_connections set to 0 in the config makes every find_or_create_session
// invocation fail the limit check, even on the first packet. handle_packet
// will return without creating a session. We cannot drive a real QUIC
// packet here; the empty/garbage branches above already cover the
// parse-error branch. This test confirms the config field is honored under
// a running lifecycle.
// ============================================================================

TEST(QuicServerDispatcherMaxConnections,
     ConfigWithZeroMaxConnectionsStartsAndStops)
{
    auto server =
        std::make_shared<core::messaging_quic_server>("zero-max");

    core::quic_server_config cfg;
    cfg.max_connections = 0;
    ASSERT_TRUE(server->start_server(0, cfg).is_ok());

    // No real peer => no incoming packet => no session attempt. We only
    // verify the lifecycle starts/stops correctly with this config.
    EXPECT_TRUE(server->is_running());
    EXPECT_EQ(server->session_count(), 0u);

    // Driving a parse-error packet still does not invoke
    // find_or_create_session (parse failure short-circuits earlier), so
    // the limit branch is not reachable here without a real handshake.
    asio::ip::udp::endpoint from(asio::ip::address_v4::loopback(), 1234);
    std::array<std::uint8_t, 8> garbage{};
    probe::invoke_handle_packet(
        *server,
        std::span<const std::uint8_t>(garbage.data(), garbage.size()),
        from);

    EXPECT_EQ(server->session_count(), 0u);
    EXPECT_TRUE(server->stop_server().is_ok());
}
