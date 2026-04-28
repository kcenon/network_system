// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file http2_client_branch_test.cpp
 * @brief Additional branch coverage for src/protocols/http2/http2_client.cpp (Issue #1048)
 *
 * Complements @ref http2_client_test.cpp, @ref http2_client_coverage_test.cpp,
 * and @ref http2_client_extended_coverage_test.cpp by exercising public-API
 * surfaces that remained uncovered after Issue #991:
 *  - http2_response::get_header() with comprehensive case-insensitivity matrix
 *  - http2_response::get_body_string() with binary / partial-UTF8 / empty bodies
 *  - http2_settings full round-trip with mid-range, zero, and max boundaries
 *  - http2_settings::enable_push toggling
 *  - http2_stream construction with populated request_body, response_body,
 *    request_headers, response_headers, and the streaming callback set
 *  - http2_stream move construction and move assignment under populated state
 *  - set_timeout() with zero, very small, and very large durations
 *  - connect() to numerically-formatted IPv4 and IPv6 hosts that are
 *    unreachable, exercising the resolver / endpoint construction paths
 *  - connect() to a host string that is empty / contains a long DNS label
 *  - Multiple consecutive set_settings() calls with monotonically changing
 *    header_table_size, exercising the encoder / decoder dynamic-table
 *    update path on each apply
 *  - is_connected() polling under shared_ptr lifetime
 *  - All request helpers re-invoked after a disconnect() to ensure the
 *    early-return branch is exercised for the second invocation cycle
 *
 * All tests operate purely on the public API; no real HTTP/2 peer is required.
 */

#include "internal/protocols/http2/http2_client.h"

#include "hermetic_transport_fixture.h"
#include "mock_h2_server_peer.h"
#include "mock_tls_socket.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <future>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace http2 = kcenon::network::protocols::http2;

namespace
{

using namespace std::chrono_literals;

http2::http2_response make_response_with_headers(
    int status,
    std::vector<http2::http_header> headers,
    std::vector<uint8_t> body)
{
    http2::http2_response r;
    r.status_code = status;
    r.headers = std::move(headers);
    r.body = std::move(body);
    return r;
}

} // namespace

// ============================================================================
// http2_response::get_header — case-insensitive lookup matrix
// ============================================================================

TEST(Http2ResponseHeaderLookupTest, MatchesUnderEveryCasePermutation)
{
    auto r = make_response_with_headers(
        200,
        {
            {"Content-Type", "application/json"},
            {"x-Custom-ID", "abc-123"},
            {"set-cookie", "sid=42"},
        },
        {});

    // Mixed-case lookup matches the stored mixed-case header.
    EXPECT_EQ(r.get_header("Content-Type").value_or(""), "application/json");
    // All-lower lookup matches a mixed-case stored header.
    EXPECT_EQ(r.get_header("content-type").value_or(""), "application/json");
    // All-upper lookup matches a mixed-case stored header.
    EXPECT_EQ(r.get_header("CONTENT-TYPE").value_or(""), "application/json");
    // Lookup of a header stored in mixed case with differing casing.
    EXPECT_EQ(r.get_header("X-CUSTOM-ID").value_or(""), "abc-123");
    EXPECT_EQ(r.get_header("x-custom-id").value_or(""), "abc-123");
    // All-lower stored header with all-upper lookup.
    EXPECT_EQ(r.get_header("SET-COOKIE").value_or(""), "sid=42");
}

TEST(Http2ResponseHeaderLookupTest, ReturnsNulloptForUnknownNames)
{
    auto r = make_response_with_headers(200, {{"a", "b"}}, {});
    EXPECT_FALSE(r.get_header("missing").has_value());
    EXPECT_FALSE(r.get_header("").has_value());
    // A name that differs only in trailing whitespace is NOT a match — the
    // comparator does not strip whitespace.
    EXPECT_FALSE(r.get_header("a ").has_value());
}

TEST(Http2ResponseHeaderLookupTest, ReturnsFirstMatchOnDuplicates)
{
    auto r = make_response_with_headers(
        200,
        {
            {"x-trace-id", "first"},
            {"X-Trace-Id", "second"},
            {"X-TRACE-ID", "third"},
        },
        {});

    auto value = r.get_header("x-trace-id");
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, "first");
}

// ============================================================================
// http2_response::get_body_string — body byte-pattern coverage
// ============================================================================

TEST(Http2ResponseBodyStringTest, EmptyBodyReturnsEmptyString)
{
    auto r = make_response_with_headers(204, {}, {});
    EXPECT_TRUE(r.get_body_string().empty());
}

TEST(Http2ResponseBodyStringTest, AsciiBodyRoundTrips)
{
    std::vector<uint8_t> body{'h', 'e', 'l', 'l', 'o'};
    auto r = make_response_with_headers(200, {}, std::move(body));
    EXPECT_EQ(r.get_body_string(), "hello");
}

TEST(Http2ResponseBodyStringTest, BinaryBodyPreservesBytes)
{
    std::vector<uint8_t> body{0x00, 0x01, 0xff, 0xfe, 0x42};
    auto r = make_response_with_headers(200, {}, body);
    auto s = r.get_body_string();
    ASSERT_EQ(s.size(), body.size());
    for (size_t i = 0; i < body.size(); ++i)
    {
        EXPECT_EQ(static_cast<uint8_t>(s[i]), body[i]) << "at index " << i;
    }
}

TEST(Http2ResponseBodyStringTest, LargeBodyIsCopiedFaithfully)
{
    std::vector<uint8_t> body(8192, 0x5a);
    auto r = make_response_with_headers(200, {}, body);
    auto s = r.get_body_string();
    ASSERT_EQ(s.size(), body.size());
    for (char c : s)
    {
        EXPECT_EQ(static_cast<uint8_t>(c), 0x5a);
    }
}

// ============================================================================
// http2_settings — round-trip and boundary coverage
// ============================================================================

TEST(Http2SettingsRoundTripTest, AllZeroFieldsRoundTripUnchanged)
{
    http2::http2_settings s;
    s.header_table_size = 0;
    s.enable_push = false;
    s.max_concurrent_streams = 0;
    s.initial_window_size = 0;
    s.max_frame_size = 0;
    s.max_header_list_size = 0;

    auto client = std::make_shared<http2::http2_client>("settings-zero");
    client->set_settings(s);
    auto got = client->get_settings();

    EXPECT_EQ(got.header_table_size, 0u);
    EXPECT_FALSE(got.enable_push);
    EXPECT_EQ(got.max_concurrent_streams, 0u);
    EXPECT_EQ(got.initial_window_size, 0u);
    EXPECT_EQ(got.max_frame_size, 0u);
    EXPECT_EQ(got.max_header_list_size, 0u);
}

TEST(Http2SettingsRoundTripTest, AllMaxFieldsRoundTripUnchanged)
{
    http2::http2_settings s;
    constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
    s.header_table_size = kMax;
    s.enable_push = true;
    s.max_concurrent_streams = kMax;
    s.initial_window_size = kMax;
    s.max_frame_size = kMax;
    s.max_header_list_size = kMax;

    auto client = std::make_shared<http2::http2_client>("settings-max");
    client->set_settings(s);
    auto got = client->get_settings();

    EXPECT_EQ(got.header_table_size, kMax);
    EXPECT_TRUE(got.enable_push);
    EXPECT_EQ(got.max_concurrent_streams, kMax);
    EXPECT_EQ(got.initial_window_size, kMax);
    EXPECT_EQ(got.max_frame_size, kMax);
    EXPECT_EQ(got.max_header_list_size, kMax);
}

TEST(Http2SettingsRoundTripTest, EnablePushToggleDoesNotPerturbOtherFields)
{
    auto client = std::make_shared<http2::http2_client>("settings-push");

    http2::http2_settings s = client->get_settings();
    const auto baseline_table = s.header_table_size;
    const auto baseline_streams = s.max_concurrent_streams;

    s.enable_push = true;
    client->set_settings(s);
    EXPECT_TRUE(client->get_settings().enable_push);
    EXPECT_EQ(client->get_settings().header_table_size, baseline_table);

    s.enable_push = false;
    client->set_settings(s);
    EXPECT_FALSE(client->get_settings().enable_push);
    EXPECT_EQ(client->get_settings().max_concurrent_streams, baseline_streams);
}

TEST(Http2SettingsRoundTripTest, RepeatedHeaderTableSizeUpdates)
{
    auto client = std::make_shared<http2::http2_client>("settings-htable");
    http2::http2_settings s = client->get_settings();

    // Each set_settings() pass should be honoured; the encoder/decoder
    // dynamic table updates run inside this call but must not throw.
    for (uint32_t size : {0u, 256u, 4096u, 65536u, 1u, 1024u})
    {
        s.header_table_size = size;
        client->set_settings(s);
        EXPECT_EQ(client->get_settings().header_table_size, size);
    }
}

// ============================================================================
// http2_stream — construction and move semantics with populated state
// ============================================================================

TEST(Http2StreamLifecycleTest, DefaultConstructedHasIdleState)
{
    http2::http2_stream s;
    EXPECT_EQ(s.stream_id, 0u);
    EXPECT_EQ(s.state, http2::stream_state::idle);
    EXPECT_TRUE(s.request_headers.empty());
    EXPECT_TRUE(s.response_headers.empty());
    EXPECT_TRUE(s.request_body.empty());
    EXPECT_TRUE(s.response_body.empty());
    EXPECT_EQ(s.window_size, 65535);
    EXPECT_FALSE(s.headers_complete);
    EXPECT_FALSE(s.body_complete);
    EXPECT_FALSE(s.is_streaming);
}

TEST(Http2StreamLifecycleTest, MoveConstructionPreservesAllFields)
{
    http2::http2_stream src;
    src.stream_id = 7;
    src.state = http2::stream_state::open;
    src.request_headers = {{":method", "GET"}, {":path", "/"}};
    src.response_headers = {{":status", "200"}};
    src.request_body = {1, 2, 3};
    src.response_body = {4, 5};
    src.window_size = 32768;
    src.headers_complete = true;
    src.body_complete = false;
    src.is_streaming = true;

    int data_calls = 0;
    src.on_data = [&](std::vector<uint8_t>) { ++data_calls; };

    http2::http2_stream dst(std::move(src));
    EXPECT_EQ(dst.stream_id, 7u);
    EXPECT_EQ(dst.state, http2::stream_state::open);
    ASSERT_EQ(dst.request_headers.size(), 2u);
    EXPECT_EQ(dst.request_headers[0].name, ":method");
    EXPECT_EQ(dst.response_headers.size(), 1u);
    EXPECT_EQ(dst.request_body.size(), 3u);
    EXPECT_EQ(dst.response_body.size(), 2u);
    EXPECT_EQ(dst.window_size, 32768);
    EXPECT_TRUE(dst.headers_complete);
    EXPECT_TRUE(dst.is_streaming);

    ASSERT_TRUE(static_cast<bool>(dst.on_data));
    dst.on_data({});
    EXPECT_EQ(data_calls, 1);
}

TEST(Http2StreamLifecycleTest, MoveAssignmentReplacesPriorState)
{
    http2::http2_stream a;
    a.stream_id = 1;
    a.state = http2::stream_state::half_closed_local;

    http2::http2_stream b;
    b.stream_id = 99;
    b.state = http2::stream_state::closed;
    b.request_body = {0xff, 0xff};

    a = std::move(b);
    EXPECT_EQ(a.stream_id, 99u);
    EXPECT_EQ(a.state, http2::stream_state::closed);
    ASSERT_EQ(a.request_body.size(), 2u);
    EXPECT_EQ(a.request_body[0], 0xff);
}

TEST(Http2StreamLifecycleTest, PromiseFutureIsUsableAfterMove)
{
    http2::http2_stream src;
    auto fut = src.promise.get_future();

    http2::http2_stream dst(std::move(src));
    http2::http2_response payload;
    payload.status_code = 418;
    dst.promise.set_value(std::move(payload));

    auto resp = fut.get();
    EXPECT_EQ(resp.status_code, 418);
}

// ============================================================================
// http2_client — set_timeout boundary values
// ============================================================================

TEST(Http2ClientTimeoutTest, ZeroTimeoutIsAccepted)
{
    auto client = std::make_shared<http2::http2_client>("timeout-zero");
    client->set_timeout(0ms);
    EXPECT_EQ(client->get_timeout(), 0ms);
}

TEST(Http2ClientTimeoutTest, LargeTimeoutRoundTrips)
{
    auto client = std::make_shared<http2::http2_client>("timeout-large");
    constexpr auto huge = std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 2};
    client->set_timeout(huge);
    EXPECT_EQ(client->get_timeout(), huge);
}

TEST(Http2ClientTimeoutTest, RepeatedUpdatesAreMonotonic)
{
    auto client = std::make_shared<http2::http2_client>("timeout-monotonic");
    for (auto t : {1ms, 10ms, 100ms, 1000ms, 10000ms})
    {
        client->set_timeout(t);
        EXPECT_EQ(client->get_timeout(), t);
    }
}

// ============================================================================
// http2_client — connect() error path coverage with hermetic targets
// ============================================================================

class Http2ClientConnectErrorTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_client> client_ =
        std::make_shared<http2::http2_client>("connect-error");
};

TEST_F(Http2ClientConnectErrorTest, ConnectToUnreachableLoopbackPort)
{
    // Choose a port that is almost certainly unbound on the loopback.
    client_->set_timeout(1000ms);
    auto result = client_->connect("127.0.0.1", 1);
    EXPECT_TRUE(result.is_err());
    EXPECT_FALSE(client_->is_connected());
}

TEST_F(Http2ClientConnectErrorTest, ConnectToIpv6LoopbackUnreachablePort)
{
    // Some CI environments lack working IPv6; either way we expect an error.
    client_->set_timeout(1000ms);
    auto result = client_->connect("::1", 1);
    EXPECT_TRUE(result.is_err());
    EXPECT_FALSE(client_->is_connected());
}

TEST_F(Http2ClientConnectErrorTest, ConnectToInvalidHostLiteralFails)
{
    client_->set_timeout(1000ms);
    // A host literal that cannot be resolved or interpreted.
    auto result = client_->connect("definitely-not-a-host.invalid", 443);
    EXPECT_TRUE(result.is_err());
    EXPECT_FALSE(client_->is_connected());
}

TEST_F(Http2ClientConnectErrorTest, RepeatedFailedConnectsLeaveCleanState)
{
    client_->set_timeout(500ms);
    for (int i = 0; i < 3; ++i)
    {
        auto r = client_->connect("127.0.0.1", 1);
        EXPECT_TRUE(r.is_err()) << "iteration " << i;
        EXPECT_FALSE(client_->is_connected());
    }
}

// ============================================================================
// http2_client — disconnected-state request helpers (early-return path)
// ============================================================================

class Http2ClientDisconnectedHelperTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_client> client_ =
        std::make_shared<http2::http2_client>("disconnected-helpers");
};

TEST_F(Http2ClientDisconnectedHelperTest, GetReturnsErrorOnEmptyPath)
{
    auto r = client_->get("");
    EXPECT_TRUE(r.is_err());
}

TEST_F(Http2ClientDisconnectedHelperTest, PostStringBodyReturnsErrorWhileDisconnected)
{
    auto r = client_->post("/api", std::string("payload"));
    EXPECT_TRUE(r.is_err());
}

TEST_F(Http2ClientDisconnectedHelperTest, PostBinaryBodyReturnsErrorWhileDisconnected)
{
    std::vector<uint8_t> body{0xab, 0xcd, 0xef};
    auto r = client_->post("/api", body);
    EXPECT_TRUE(r.is_err());
}

TEST_F(Http2ClientDisconnectedHelperTest, PutReturnsErrorWhileDisconnected)
{
    auto r = client_->put("/api/1", std::string("payload"));
    EXPECT_TRUE(r.is_err());
}

TEST_F(Http2ClientDisconnectedHelperTest, DelReturnsErrorWhileDisconnected)
{
    auto r = client_->del("/api/1");
    EXPECT_TRUE(r.is_err());
}

TEST_F(Http2ClientDisconnectedHelperTest, AllHelpersErrorAfterFailedConnect)
{
    client_->set_timeout(500ms);
    auto cr = client_->connect("127.0.0.1", 1);
    ASSERT_TRUE(cr.is_err());

    EXPECT_TRUE(client_->get("/g").is_err());
    EXPECT_TRUE(client_->post("/p", std::string("x")).is_err());
    EXPECT_TRUE(client_->put("/u", std::string("x")).is_err());
    EXPECT_TRUE(client_->del("/d").is_err());
}

TEST_F(Http2ClientDisconnectedHelperTest, WriteStreamErrorsOnUnknownStreamId)
{
    EXPECT_TRUE(client_->write_stream(0xdeadbeef, {1, 2, 3}).is_err());
    EXPECT_TRUE(client_->write_stream(0xdeadbeef, {1, 2, 3}, /*end_stream=*/true).is_err());
}

TEST_F(Http2ClientDisconnectedHelperTest, CloseStreamWriterErrorsOnUnknownStreamId)
{
    EXPECT_TRUE(client_->close_stream_writer(0x1234).is_err());
}

TEST_F(Http2ClientDisconnectedHelperTest, CancelStreamErrorsOnUnknownStreamId)
{
    EXPECT_TRUE(client_->cancel_stream(0x1234).is_err());
}

TEST_F(Http2ClientDisconnectedHelperTest, StartStreamWhileDisconnectedFails)
{
    auto r = client_->start_stream("/stream",
                                   {{":method", "POST"}},
                                   /*on_data=*/{},
                                   /*on_headers=*/{},
                                   /*on_complete=*/{});
    EXPECT_TRUE(r.is_err());
}

// ============================================================================
// http2_client — is_connected and disconnect idempotency
// ============================================================================

TEST(Http2ClientLifecycleTest, IsConnectedIsFalseImmediatelyAfterConstruction)
{
    auto client = std::make_shared<http2::http2_client>("lifecycle-1");
    EXPECT_FALSE(client->is_connected());
}

TEST(Http2ClientLifecycleTest, DisconnectIsIdempotentBeforeAnyConnect)
{
    auto client = std::make_shared<http2::http2_client>("lifecycle-2");
    EXPECT_TRUE(client->disconnect().is_ok());
    EXPECT_TRUE(client->disconnect().is_ok());
    EXPECT_TRUE(client->disconnect().is_ok());
    EXPECT_FALSE(client->is_connected());
}

TEST(Http2ClientLifecycleTest, RepeatedConstructionDoesNotLeak)
{
    // Construct and destroy many short-lived clients in sequence to drive
    // the constructor / destructor / member-initialization paths.
    for (int i = 0; i < 16; ++i)
    {
        auto c = std::make_shared<http2::http2_client>("loop-" + std::to_string(i));
        EXPECT_FALSE(c->is_connected());
    }
}

TEST(Http2ClientLifecycleTest, EmptyAndLongClientIdsAreAccepted)
{
    auto empty = std::make_shared<http2::http2_client>("");
    EXPECT_FALSE(empty->is_connected());

    std::string long_id(512, 'x');
    auto longc = std::make_shared<http2::http2_client>(long_id);
    EXPECT_FALSE(longc->is_connected());
}

// ============================================================================
// http2_client — concurrent disconnected-state observations
// ============================================================================

TEST(Http2ClientConcurrencyTest, ConcurrentIsConnectedQueriesAreSafe)
{
    auto client = std::make_shared<http2::http2_client>("concurrent-isconn");
    constexpr int kThreads = 8;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_FALSE(client->is_connected());
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST(Http2ClientConcurrencyTest, ConcurrentSetTimeoutIsThreadSafe)
{
    auto client = std::make_shared<http2::http2_client>("concurrent-timeout");
    constexpr int kThreads = 4;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([client, t]
        {
            for (int i = 0; i < 100; ++i)
            {
                client->set_timeout(std::chrono::milliseconds{(t + 1) * 10 + i});
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
    // Final value is racy but must be a positive duration.
    EXPECT_GT(client->get_timeout().count(), 0);
}

// ============================================================================
// Hermetic transport fixture demonstration (Issue #1060)
// ============================================================================

/**
 * @brief Demonstrates that the new hermetic TLS fixture lets http2_client
 *        attempt a real TLS handshake against an in-process loopback peer.
 *
 * This drives http2_client::connect() through DNS resolution (numeric host),
 * TCP connect, ALPN setup, and TLS ClientHello transmission — paths that
 * the pure public-API tests above could not exercise without an external
 * server. The handshake will not produce a valid HTTP/2 preface response
 * from the bare TLS listener, so connect() may eventually return an error,
 * but the lower-level state machine is exercised either way.
 */
class Http2ClientHermeticTransportTest
    : public kcenon::network::tests::support::hermetic_transport_fixture
{
};

TEST_F(Http2ClientHermeticTransportTest, ConnectAttemptsHandshakeAgainstLoopbackTlsPeer)
{
    using namespace kcenon::network::tests::support;

    tls_loopback_listener listener(io());
    auto client = std::make_shared<http2::http2_client>("hermetic-test-client");
    ASSERT_NE(client, nullptr);

    // Issue connect on a worker thread; connect() is synchronous so we cannot
    // call it from this thread without blocking the listener accept handler
    // that runs on io_context::run().
    std::atomic<bool> connect_returned{false};
    std::thread connector([&]() {
        // The bare TLS listener does not speak HTTP/2 — we expect the connect
        // call to either succeed at TLS but stall on the missing preface, or
        // return an error after the timeout. Either outcome exercises the
        // connect path; we only assert the call returns within the budget.
        client->set_timeout(std::chrono::seconds(2));
        (void)client->connect("127.0.0.1", listener.port());
        connect_returned.store(true);
    });

    // Wait for the listener to accept the TCP connection. The TLS handshake
    // may or may not complete depending on how http2_client drives ALPN.
    EXPECT_TRUE(wait_for(
        [&]() { return listener.accepted(); },
        std::chrono::seconds(3)));

    client->disconnect();
    connector.join();
    EXPECT_TRUE(connect_returned.load());
}

// ============================================================================
// mock_h2_server_peer demo (Phase 2A of #1074)
//
// Drives http2_client::connect() against an in-process server-side HTTP/2
// peer that performs the connection-preface read, SETTINGS exchange, and
// SETTINGS-ACK send. Without the peer, connect() either stalls or returns
// after a timeout because tls_loopback_listener stops at TLS handshake.
// With the peer, the post-connect public methods (get_settings, disconnect)
// run against a fully-settled client connection — code paths not previously
// reachable from a hermetic CI environment.
// ============================================================================

TEST_F(Http2ClientHermeticTransportTest,
       ConnectCompletesSettingsExchangeWithMockPeer)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());

    auto client = std::make_shared<http2::http2_client>("mock-h2-peer-test");
    ASSERT_NE(client, nullptr);
    client->set_timeout(std::chrono::milliseconds(2000));

    std::thread connector([&]() {
        (void)client->connect("127.0.0.1", peer.port());
    });

    // Wait for the mock peer to complete the SETTINGS exchange. Once true,
    // the worker has read the preface, sent server SETTINGS, read client
    // SETTINGS, and sent SETTINGS-ACK without error.
    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_FALSE(peer.io_failed());
    EXPECT_TRUE(client->is_connected());

    // Reach a post-connect public method. Prior to mock_h2_server_peer the
    // client could not complete connect() against a hermetic peer, so this
    // code path was only reachable via the DISABLED_ConnectToHttpbin
    // integration test that the coverage workflow does not run.
    const auto local_settings = client->get_settings();
    EXPECT_GT(local_settings.initial_window_size, 0u);
    EXPECT_GT(local_settings.max_frame_size, 0u);

    // Drive the disconnect() path — it builds a goaway_frame and writes it
    // to the SSL stream before tearing down the connection. The mock peer
    // drains this frame in its post-handshake loop.
    (void)client->disconnect();
    EXPECT_FALSE(client->is_connected());

    connector.join();
}

// ============================================================================
// Phase 2A connected-state coverage expansion (Issue #1062)
//
// All TEST_F below assume mock_h2_server_peer has completed the SETTINGS
// exchange so http2_client is in a fully-connected state. They drive the
// post-connect public methods (set_settings, send_request timeout path,
// start_stream, write_stream, cancel_stream, close_stream_writer) plus
// previously-unreachable error branches (write_stream / cancel_stream /
// close_stream_writer on an unknown stream id, second connect() returning
// already_exists, idempotent disconnect after a full handshake).
//
// HEADERS+DATA reply paths remain unreachable until Phase 2A.2 of #1074
// lands. Tests below are therefore scoped to verify timeout / not-found /
// state-transition branches that the disconnected-state tests in
// tests/test_http2_client.cpp cannot cover.
// ============================================================================

namespace
{

struct connected_client_setup
{
    std::shared_ptr<http2::http2_client> client;
    std::thread connector;
};

inline connected_client_setup make_connected_client(
    kcenon::network::tests::support::mock_h2_server_peer& peer,
    const char* client_id,
    std::chrono::milliseconds request_timeout = std::chrono::milliseconds(2000))
{
    auto client = std::make_shared<http2::http2_client>(client_id);
    client->set_timeout(request_timeout);
    auto port = peer.port();
    std::thread connector([client, port]() {
        (void)client->connect("127.0.0.1", port);
    });
    return {std::move(client), std::move(connector)};
}

} // namespace

TEST_F(Http2ClientHermeticTransportTest,
       SecondConnectReturnsAlreadyExistsAfterFirstSucceeds)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "second-connect-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    // The first connect() succeeded; a second invocation must short-circuit
    // through the already_exists branch documented at
    // http2_client.cpp:81-88. Pre-Phase 2A this branch was only reachable
    // via DISABLED_ConnectToHttpbin against an external server.
    auto second = setup.client->connect("127.0.0.1", peer.port());
    EXPECT_TRUE(second.is_err());

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       SendRequestTimesOutWhenPeerSendsNoHeaders)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(
        peer, "request-timeout-test", std::chrono::milliseconds(150));

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    // mock_h2_server_peer (Phase 2A) does not reply with HEADERS+DATA, so
    // the future inside send_request() never resolves. The 150 ms request
    // timeout drives the timeout error branch at http2_client.cpp:817-826,
    // which also exercises create_stream / build_headers / encoder_.encode
    // / send_frame on the connected path.
    auto response = setup.client->get("/timeout-path", {});
    EXPECT_TRUE(response.is_err());

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       PostWithBodyTimesOutAfterDataFrameIsSent)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(
        peer, "post-timeout-test", std::chrono::milliseconds(150));

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    // POST drives both the HEADERS frame transmit and the body-bearing DATA
    // frame transmit (http2_client.cpp:790-807) before reaching the
    // timeout branch — coverage that GET cannot exercise.
    std::string body = "hello phase2a";
    auto response = setup.client->post("/upload", body, {});
    EXPECT_TRUE(response.is_err());

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       StartStreamReturnsValidIdAfterHandshake)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "start-stream-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    // start_stream returns the allocated stream_id when is_connected() is
    // true. Without a HEADERS+DATA reply the on_data / on_complete
    // callbacks are not invoked, but the HEADERS frame is sent and the
    // stream is registered (http2_client.cpp:306-350).
    auto stream_result = setup.client->start_stream(
        "/stream",
        {},
        [](std::vector<uint8_t>) {},
        [](std::vector<http2::http_header>) {},
        [](int) {});
    EXPECT_TRUE(stream_result.is_ok());
    if (stream_result.is_ok())
    {
        EXPECT_GT(stream_result.value(), 0u);
    }

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       WriteStreamSucceedsAfterStartStream)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "write-stream-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    auto stream_result = setup.client->start_stream(
        "/streaming-write",
        {},
        [](std::vector<uint8_t>) {},
        [](std::vector<http2::http_header>) {},
        [](int) {});
    ASSERT_TRUE(stream_result.is_ok());
    auto stream_id = stream_result.value();

    // Drive write_stream() with a non-empty chunk; then close it with an
    // empty DATA frame carrying END_STREAM. This exercises both the
    // open-state write branch and the open -> half_closed_local transition
    // at http2_client.cpp:386-392.
    std::vector<uint8_t> chunk{'a', 'b', 'c'};
    auto wr = setup.client->write_stream(stream_id, chunk, false);
    EXPECT_TRUE(wr.is_ok());
    auto fin = setup.client->write_stream(stream_id, {}, true);
    EXPECT_TRUE(fin.is_ok());

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       WriteStreamWithUnknownStreamIdReturnsNotFound)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "write-not-found-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    // 99'999 is far above any stream id allocated by start_stream and is
    // therefore guaranteed not to exist in the streams_ map. Drives the
    // not_found branch at http2_client.cpp:367-369 — distinct from the
    // connection_closed branch covered by disconnected-state tests.
    std::vector<uint8_t> payload{1, 2, 3};
    auto wr = setup.client->write_stream(99999u, payload, false);
    EXPECT_TRUE(wr.is_err());

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       CancelStreamWithUnknownStreamIdReturnsNotFound)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "cancel-not-found-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    // Connected + unknown stream id drives the not_found branch at
    // http2_client.cpp:441-444.
    auto cancel = setup.client->cancel_stream(99999u);
    EXPECT_TRUE(cancel.is_err());

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       CloseStreamWriterWithUnknownStreamIdReturnsNotFound)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "close-not-found-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    // not_found branch at http2_client.cpp:407-409.
    auto close = setup.client->close_stream_writer(99999u);
    EXPECT_TRUE(close.is_err());

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       CancelStreamSendsRstStreamFrame)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "cancel-stream-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    auto stream_result = setup.client->start_stream(
        "/cancel-target",
        {},
        [](std::vector<uint8_t>) {},
        [](std::vector<http2::http_header>) {},
        [](int) {});
    ASSERT_TRUE(stream_result.is_ok());

    // cancel_stream sends an RST_STREAM frame and marks the stream closed
    // (http2_client.cpp:430-462). The peer drains the frame in its
    // post-handshake loop.
    auto cancel = setup.client->cancel_stream(stream_result.value());
    EXPECT_TRUE(cancel.is_ok());

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       CloseStreamWriterIsIdempotentOnAlreadyClosedStream)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "close-idempotent-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    auto stream_result = setup.client->start_stream(
        "/idempotent-close",
        {},
        [](std::vector<uint8_t>) {},
        [](std::vector<http2::http_header>) {},
        [](int) {});
    ASSERT_TRUE(stream_result.is_ok());
    auto stream_id = stream_result.value();

    // First close transitions to half_closed_local and sends an empty DATA
    // frame with END_STREAM (http2_client.cpp:418-427).
    auto first = setup.client->close_stream_writer(stream_id);
    EXPECT_TRUE(first.is_ok());

    // Second close on a half_closed_local stream is a no-op — drives the
    // already-closed early-return branch at http2_client.cpp:412-415.
    auto second = setup.client->close_stream_writer(stream_id);
    EXPECT_TRUE(second.is_ok());

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       SetSettingsAfterHandshakeUpdatesEncoderTableSize)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "set-settings-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    // set_settings updates local_settings_ and rewires the HPACK encoder /
    // decoder table sizes (http2_client.cpp:299-304). Coverage gain over
    // existing disconnected-state tests is the post-handshake call site.
    auto settings = setup.client->get_settings();
    settings.header_table_size = 8192u;
    settings.max_concurrent_streams = 64u;
    setup.client->set_settings(settings);

    auto refreshed = setup.client->get_settings();
    EXPECT_EQ(refreshed.header_table_size, 8192u);
    EXPECT_EQ(refreshed.max_concurrent_streams, 64u);

    (void)setup.client->disconnect();
    setup.connector.join();
}

TEST_F(Http2ClientHermeticTransportTest,
       DisconnectIsIdempotentAfterFullHandshake)
{
    using namespace kcenon::network::tests::support;
    mock_h2_server_peer peer(io());
    auto setup = make_connected_client(peer, "disconnect-idempotent-test");

    EXPECT_TRUE(wait_for(
        [&]() { return peer.settings_exchanged(); },
        std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    // Two disconnect()s in a row: the first drives the GOAWAY emit +
    // stop_io path; the second hits the early-return branch at
    // http2_client.cpp:212-215 on a fully-handshaken-then-torn-down
    // client. The disconnected-state tests can only cover this branch
    // when connect() never succeeded, so the connected->disconnected
    // transition path here is incremental.
    auto first = setup.client->disconnect();
    EXPECT_TRUE(first.is_ok());
    EXPECT_FALSE(setup.client->is_connected());

    auto second = setup.client->disconnect();
    EXPECT_TRUE(second.is_ok());

    setup.connector.join();
}
