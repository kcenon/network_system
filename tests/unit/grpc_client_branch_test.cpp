// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file grpc_client_branch_test.cpp
 * @brief Additional branch coverage for src/protocols/grpc/client.cpp (Issue #1049)
 *
 * Complements @ref grpc_client_test.cpp and @ref core_client_test.cpp by
 * exercising public-API surfaces that remained uncovered after Issue #994:
 *  - grpc_channel_config full-field round-trip with boundary values
 *    (zero / max keepalive, zero / max retry attempts, zero / max message
 *    size, default vs explicit timeout)
 *  - call_options::set_timeout with seconds, milliseconds, microseconds,
 *    nanoseconds, and zero / negative-leaning durations
 *  - call_options metadata growth with many entries, long values, empty
 *    keys, and binary-flagged keys
 *  - grpc_client construction with very long, empty, IPv4, IPv6-formatted,
 *    DNS-style, and unicode-byte targets
 *  - grpc_client::wait_for_connected with zero, very small, mid-range, and
 *    large timeouts in the never-connected state, verifying budget
 *  - Repeated disconnect() calls (idempotency under load)
 *  - Move construction and move assignment with populated config
 *  - Concurrent is_connected() polling and concurrent disconnect() calls
 *  - Connect failure paths: missing port separator, multi-colon target,
 *    non-numeric port, leading-zero port, port with whitespace,
 *    extremely long host label (DNS-illegal), empty host, target that
 *    starts with ':'
 *  - call_raw guard-clause variants: empty payload, large payload, with
 *    pre-expired deadline, with future deadline (still rejected when not
 *    connected), with empty method, with method missing leading slash
 *  - server_stream_raw / client_stream_raw / bidi_stream_raw guard-clause
 *    coverage with empty method names and non-slash-prefixed method names
 *  - call_raw_async stress: many concurrent async calls each delivering
 *    the not-connected error to its own callback, no-callback path
 *  - grpc_metadata copy / move / clear semantics under concurrent reads
 *
 * All tests operate purely on the public API; no real gRPC server is
 * required. These tests are hermetic and rely only on connect() failures,
 * which return synchronously on every supported platform.
 *
 * Honest scope statement: the impl-level methods that physically exchange
 * frames with a peer (call_raw's HTTP/2 POST or official-gRPC UnaryCall
 * after is_connected()==true, the streaming send / receive loops, the
 * server_stream_reader_impl::on_data / on_headers / on_complete callbacks,
 * the client_stream_writer_impl::write / writes_done / finish round-trip,
 * the bidi_stream_impl read / write / finish round-trip, and the
 * tracing-attribute set_attribute / set_error calls inside successful
 * call paths) require a live HTTP/2 (or live grpcpp) peer to drive. Those
 * remain uncovered by this hermetic suite and would require a transport
 * fixture or in-process test peer to exercise.
 */

#include "kcenon/network/detail/protocols/grpc/client.h"
#include "kcenon/network/detail/protocols/grpc/frame.h"
#include "kcenon/network/detail/protocols/grpc/status.h"

#include "hermetic_transport_fixture.h"
#include "mock_h2_server_peer.h"
#include "mock_tls_socket.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace grpc_ns = kcenon::network::protocols::grpc;

using grpc_ns::call_options;
using grpc_ns::grpc_channel_config;
using grpc_ns::grpc_client;
using grpc_ns::grpc_message;
using grpc_ns::grpc_metadata;

namespace
{

using namespace std::chrono_literals;

} // namespace

// ============================================================================
// grpc_channel_config — full-field round-trip with boundary values
// ============================================================================

TEST(GrpcClientChannelConfigBoundary, AllZeroFieldsRoundTrip)
{
    grpc_channel_config cfg;
    cfg.default_timeout = 0ms;
    cfg.use_tls = false;
    cfg.root_certificates.clear();
    cfg.client_certificate.reset();
    cfg.client_key.reset();
    cfg.max_message_size = 0;
    cfg.keepalive_time = 0ms;
    cfg.keepalive_timeout = 0ms;
    cfg.max_retry_attempts = 0;

    EXPECT_EQ(cfg.default_timeout.count(), 0);
    EXPECT_FALSE(cfg.use_tls);
    EXPECT_TRUE(cfg.root_certificates.empty());
    EXPECT_FALSE(cfg.client_certificate.has_value());
    EXPECT_FALSE(cfg.client_key.has_value());
    EXPECT_EQ(cfg.max_message_size, 0u);
    EXPECT_EQ(cfg.keepalive_time.count(), 0);
    EXPECT_EQ(cfg.keepalive_timeout.count(), 0);
    EXPECT_EQ(cfg.max_retry_attempts, 0u);
}

TEST(GrpcClientChannelConfigBoundary, AllMaxFieldsRoundTrip)
{
    grpc_channel_config cfg;
    cfg.default_timeout = std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 2};
    cfg.use_tls = true;
    cfg.root_certificates.assign(1024, 'A');
    cfg.client_certificate = std::string(2048, 'B');
    cfg.client_key = std::string(2048, 'C');
    cfg.max_message_size = std::numeric_limits<size_t>::max() / 2;
    cfg.keepalive_time = std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 4};
    cfg.keepalive_timeout = std::chrono::milliseconds{std::numeric_limits<int64_t>::max() / 4};
    cfg.max_retry_attempts = std::numeric_limits<uint32_t>::max();

    EXPECT_GT(cfg.default_timeout.count(), 0);
    EXPECT_TRUE(cfg.use_tls);
    EXPECT_EQ(cfg.root_certificates.size(), 1024u);
    ASSERT_TRUE(cfg.client_certificate.has_value());
    EXPECT_EQ(cfg.client_certificate->size(), 2048u);
    ASSERT_TRUE(cfg.client_key.has_value());
    EXPECT_EQ(cfg.client_key->size(), 2048u);
    EXPECT_EQ(cfg.max_retry_attempts, std::numeric_limits<uint32_t>::max());
}

TEST(GrpcClientChannelConfigBoundary, DefaultValuesAreReasonable)
{
    grpc_channel_config cfg;
    EXPECT_EQ(cfg.default_timeout, 30000ms);
    EXPECT_TRUE(cfg.use_tls);
    EXPECT_EQ(cfg.max_message_size, grpc_ns::default_max_message_size);
    EXPECT_EQ(cfg.keepalive_time.count(), 0);
    EXPECT_EQ(cfg.keepalive_timeout, 20000ms);
    EXPECT_EQ(cfg.max_retry_attempts, 3u);
    EXPECT_FALSE(cfg.client_certificate.has_value());
    EXPECT_FALSE(cfg.client_key.has_value());
}

TEST(GrpcClientChannelConfigBoundary, MutualTlsCanBePartiallySet)
{
    // Only client cert without key (unusual but allowed by struct)
    grpc_channel_config cfg;
    cfg.client_certificate = "cert-only";
    EXPECT_TRUE(cfg.client_certificate.has_value());
    EXPECT_FALSE(cfg.client_key.has_value());

    // Only key without cert (unusual but allowed by struct)
    grpc_channel_config cfg2;
    cfg2.client_key = "key-only";
    EXPECT_FALSE(cfg2.client_certificate.has_value());
    EXPECT_TRUE(cfg2.client_key.has_value());
}

// ============================================================================
// call_options — set_timeout with diverse duration types
// ============================================================================

TEST(GrpcClientCallOptionsSetTimeout, AcceptsSeconds)
{
    call_options opts;
    auto before = std::chrono::system_clock::now();
    opts.set_timeout(std::chrono::seconds{2});
    auto after = std::chrono::system_clock::now();
    ASSERT_TRUE(opts.deadline.has_value());
    EXPECT_GE(*opts.deadline, before + std::chrono::seconds{2});
    EXPECT_LE(*opts.deadline, after + std::chrono::seconds{2});
}

TEST(GrpcClientCallOptionsSetTimeout, AcceptsNanoseconds)
{
    call_options opts;
    opts.set_timeout(std::chrono::nanoseconds{1000});
    ASSERT_TRUE(opts.deadline.has_value());
}

TEST(GrpcClientCallOptionsSetTimeout, ZeroTimeoutSetsDeadlineNow)
{
    call_options opts;
    auto before = std::chrono::system_clock::now();
    opts.set_timeout(std::chrono::milliseconds{0});
    auto after = std::chrono::system_clock::now();
    ASSERT_TRUE(opts.deadline.has_value());
    EXPECT_GE(*opts.deadline, before);
    EXPECT_LE(*opts.deadline, after);
}

TEST(GrpcClientCallOptionsSetTimeout, OverwritesPreviousDeadline)
{
    call_options opts;
    opts.set_timeout(std::chrono::seconds{10});
    auto first = *opts.deadline;
    opts.set_timeout(std::chrono::milliseconds{1});
    ASSERT_TRUE(opts.deadline.has_value());
    EXPECT_LT(*opts.deadline, first);
}

TEST(GrpcClientCallOptionsSetTimeout, RepeatedTimeoutsAreMonotonic)
{
    call_options opts;
    auto last = std::chrono::system_clock::time_point::min();
    for (auto t : {1ms, 10ms, 100ms, 1000ms})
    {
        opts.set_timeout(t);
        ASSERT_TRUE(opts.deadline.has_value());
        EXPECT_GE(*opts.deadline, last);
        last = *opts.deadline;
    }
}

// ============================================================================
// call_options — metadata growth and content patterns
// ============================================================================

TEST(GrpcClientCallOptionsMetadata, EmptyKeyIsStored)
{
    call_options opts;
    opts.metadata.emplace_back("", "value");
    ASSERT_EQ(opts.metadata.size(), 1u);
    EXPECT_TRUE(opts.metadata[0].first.empty());
    EXPECT_EQ(opts.metadata[0].second, "value");
}

TEST(GrpcClientCallOptionsMetadata, EmptyValueIsStored)
{
    call_options opts;
    opts.metadata.emplace_back("key", "");
    ASSERT_EQ(opts.metadata.size(), 1u);
    EXPECT_EQ(opts.metadata[0].first, "key");
    EXPECT_TRUE(opts.metadata[0].second.empty());
}

TEST(GrpcClientCallOptionsMetadata, ManyEntriesAreOrdered)
{
    call_options opts;
    constexpr int kCount = 64;
    for (int i = 0; i < kCount; ++i)
    {
        opts.metadata.emplace_back("k" + std::to_string(i), "v" + std::to_string(i));
    }
    ASSERT_EQ(opts.metadata.size(), static_cast<size_t>(kCount));
    for (int i = 0; i < kCount; ++i)
    {
        EXPECT_EQ(opts.metadata[i].first, "k" + std::to_string(i));
        EXPECT_EQ(opts.metadata[i].second, "v" + std::to_string(i));
    }
}

TEST(GrpcClientCallOptionsMetadata, LargeValueIsStored)
{
    call_options opts;
    std::string big_value(8192, 'x');
    opts.metadata.emplace_back("big", big_value);
    ASSERT_EQ(opts.metadata.size(), 1u);
    EXPECT_EQ(opts.metadata[0].second.size(), 8192u);
}

TEST(GrpcClientCallOptionsMetadata, BinaryKeyConventionIsPreserved)
{
    call_options opts;
    std::string binary{'\x00', '\x01', '\xff'};
    opts.metadata.emplace_back("trace-bin", binary);
    ASSERT_EQ(opts.metadata.size(), 1u);
    EXPECT_EQ(opts.metadata[0].first, "trace-bin");
    EXPECT_EQ(opts.metadata[0].second.size(), 3u);
}

TEST(GrpcClientCallOptionsMetadata, ClearResetsToEmpty)
{
    call_options opts;
    opts.metadata.emplace_back("k1", "v1");
    opts.metadata.emplace_back("k2", "v2");
    ASSERT_EQ(opts.metadata.size(), 2u);
    opts.metadata.clear();
    EXPECT_TRUE(opts.metadata.empty());
}

// ============================================================================
// grpc_client — construction with varied targets
// ============================================================================

TEST(GrpcClientConstructionTargets, AcceptsLongTarget)
{
    std::string target(512, 'a');
    target += ":1234";
    grpc_client client(target);
    EXPECT_EQ(client.target(), target);
    EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientConstructionTargets, AcceptsIpv6BracketStyle)
{
    grpc_client client("[::1]:50051");
    EXPECT_EQ(client.target(), "[::1]:50051");
    EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientConstructionTargets, AcceptsDnsName)
{
    grpc_client client("service.example.com:443");
    EXPECT_EQ(client.target(), "service.example.com:443");
    EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientConstructionTargets, AcceptsTargetWithPath)
{
    // Many targets appear with paths; constructor should not parse them.
    grpc_client client("host:1234/some/path");
    EXPECT_EQ(client.target(), "host:1234/some/path");
}

TEST(GrpcClientConstructionTargets, AcceptsTargetWithLargePort)
{
    grpc_client client("host:65535");
    EXPECT_EQ(client.target(), "host:65535");
}

TEST(GrpcClientConstructionTargets, MultipleClientsSameTarget)
{
    grpc_client a("localhost:50051");
    grpc_client b("localhost:50051");
    EXPECT_EQ(a.target(), b.target());
    EXPECT_FALSE(a.is_connected());
    EXPECT_FALSE(b.is_connected());
}

TEST(GrpcClientConstructionTargets, ConfigIsAppliedAtConstruction)
{
    grpc_channel_config cfg;
    cfg.use_tls = false;
    cfg.default_timeout = 100ms;
    cfg.max_retry_attempts = 7;
    grpc_client client("host:50051", cfg);
    EXPECT_EQ(client.target(), "host:50051");
    EXPECT_FALSE(client.is_connected());
}

// ============================================================================
// grpc_client::wait_for_connected — timeout budget coverage
// ============================================================================

TEST(GrpcClientWaitForConnectedBudget, ZeroTimeoutReturnsImmediately)
{
    grpc_client client("localhost:50051");
    auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(client.wait_for_connected(0ms));
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, 500ms);
}

TEST(GrpcClientWaitForConnectedBudget, OneMsTimeoutReturnsQuickly)
{
    grpc_client client("localhost:50051");
    auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(client.wait_for_connected(1ms));
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, 1000ms);
}

TEST(GrpcClientWaitForConnectedBudget, MidRangeTimeoutReturnsByDeadline)
{
    grpc_client client("localhost:50051");
    auto start = std::chrono::steady_clock::now();
    EXPECT_FALSE(client.wait_for_connected(50ms));
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(elapsed, 2000ms);
}

TEST(GrpcClientWaitForConnectedBudget, RepeatedCallsAreIdempotent)
{
    grpc_client client("localhost:50051");
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_FALSE(client.wait_for_connected(0ms));
        EXPECT_FALSE(client.is_connected());
    }
}

// ============================================================================
// grpc_client::disconnect — repeated and concurrent calls
// ============================================================================

TEST(GrpcClientDisconnectIdempotency, ManySequentialDisconnects)
{
    grpc_client client("localhost:50051");
    for (int i = 0; i < 32; ++i)
    {
        EXPECT_NO_FATAL_FAILURE(client.disconnect());
        EXPECT_FALSE(client.is_connected());
    }
}

TEST(GrpcClientDisconnectIdempotency, ConcurrentDisconnectsAreSafe)
{
    auto client = std::make_shared<grpc_client>("localhost:50051");
    constexpr int kThreads = 4;
    constexpr int kIterations = 50;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([client] {
            for (int i = 0; i < kIterations; ++i)
            {
                client->disconnect();
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_FALSE(client->is_connected());
}

TEST(GrpcClientDisconnectIdempotency, ConcurrentIsConnectedQueriesAreSafe)
{
    auto client = std::make_shared<grpc_client>("localhost:50051");
    constexpr int kThreads = 8;
    constexpr int kIterations = 200;
    std::atomic<int> false_count{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([client, &false_count] {
            for (int i = 0; i < kIterations; ++i)
            {
                if (!client->is_connected())
                {
                    false_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(false_count.load(), kThreads * kIterations);
}

TEST(GrpcClientDisconnectIdempotency, ConcurrentDisconnectAndIsConnected)
{
    auto client = std::make_shared<grpc_client>("localhost:50051");
    constexpr int kIterations = 100;
    std::thread t1([client] {
        for (int i = 0; i < kIterations; ++i) client->disconnect();
    });
    std::thread t2([client] {
        for (int i = 0; i < kIterations; ++i) (void) client->is_connected();
    });
    t1.join();
    t2.join();
    EXPECT_FALSE(client->is_connected());
}

// ============================================================================
// grpc_client — move semantics with populated config
// ============================================================================

TEST(GrpcClientMoveSemantics, MoveConstructPreservesTarget)
{
    grpc_channel_config cfg;
    cfg.use_tls = false;
    cfg.max_retry_attempts = 5;
    grpc_client src("source:11111", cfg);
    grpc_client dst(std::move(src));
    EXPECT_EQ(dst.target(), "source:11111");
    EXPECT_FALSE(dst.is_connected());
}

TEST(GrpcClientMoveSemantics, MoveAssignReplacesTarget)
{
    grpc_client a("a:1");
    grpc_client b("b:2");
    a = std::move(b);
    EXPECT_EQ(a.target(), "b:2");
    EXPECT_FALSE(a.is_connected());
}

TEST(GrpcClientMoveSemantics, MoveChainPreservesFinalTarget)
{
    grpc_client a("a:1");
    grpc_client b(std::move(a));
    grpc_client c(std::move(b));
    grpc_client d(std::move(c));
    EXPECT_EQ(d.target(), "a:1");
    EXPECT_FALSE(d.is_connected());
}

TEST(GrpcClientMoveSemantics, DisconnectAfterMoveDoesNotCrash)
{
    grpc_client a("a:1");
    grpc_client b(std::move(a));
    EXPECT_NO_FATAL_FAILURE(b.disconnect());
    EXPECT_NO_FATAL_FAILURE(b.disconnect());
}

// ============================================================================
// grpc_client::connect — extended malformed-target coverage
// ============================================================================

#if !defined(NETWORK_GRPC_OFFICIAL) || NETWORK_GRPC_OFFICIAL == 0

TEST(GrpcClientConnectMalformed, RejectsTargetStartingWithColon)
{
    grpc_client client(":50051");
    auto r = client.connect();
    // Empty host + valid port: implementation-specific but must not connect.
    EXPECT_TRUE(r.is_err() || !client.is_connected());
}

TEST(GrpcClientConnectMalformed, RejectsTargetWithMultipleColons)
{
    grpc_client client("host:1234:5678");
    auto r = client.connect();
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientConnectMalformed, RejectsTargetWithSpaceInPort)
{
    grpc_client client("host: 1234");
    auto r = client.connect();
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientConnectMalformed, RejectsTargetWithNegativeLeadingPort)
{
    grpc_client client("host:-1");
    auto r = client.connect();
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientConnectMalformed, RejectsTargetWithTrailingColon)
{
    grpc_client client("host:");
    auto r = client.connect();
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientConnectMalformed, RejectsTargetThatIsOnlyColon)
{
    grpc_client client(":");
    auto r = client.connect();
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(client.is_connected());
}

TEST(GrpcClientConnectMalformed, MultipleConsecutiveConnectFailuresAreClean)
{
    grpc_client client("nonexistent.invalid:badport");
    for (int i = 0; i < 3; ++i)
    {
        auto r = client.connect();
        EXPECT_TRUE(r.is_err()) << "iteration " << i;
        EXPECT_FALSE(client.is_connected());
    }
}

TEST(GrpcClientConnectMalformed, RejectsHostWithVeryLongLabel)
{
    // DNS labels max out at 63 chars; this label is much longer.
    std::string huge_host(512, 'a');
    grpc_client client(huge_host + ":80");
    auto r = client.connect();
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(client.is_connected());
}

#endif // !NETWORK_GRPC_OFFICIAL

// ============================================================================
// call_raw — extended guard-clause coverage
// ============================================================================

TEST(GrpcClientCallRawGuards, FailsWithEmptyMethodWhenNotConnected)
{
    grpc_client client("localhost:50051");
    auto r = client.call_raw("", {});
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientCallRawGuards, FailsWithMethodWithoutLeadingSlash)
{
    grpc_client client("localhost:50051");
    auto r = client.call_raw("svc/Method", std::vector<uint8_t>{0x01});
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientCallRawGuards, FailsWithLargePayloadWhenNotConnected)
{
    grpc_client client("localhost:50051");
    std::vector<uint8_t> huge(64 * 1024, 0xab);
    auto r = client.call_raw("/svc/Method", huge);
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientCallRawGuards, FailsWithPreExpiredDeadlineWhenNotConnected)
{
    grpc_client client("localhost:50051");
    call_options opts;
    opts.deadline = std::chrono::system_clock::now() - std::chrono::hours{1};
    auto r = client.call_raw("/svc/Method", {}, opts);
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientCallRawGuards, FailsWithFutureDeadlineWhenNotConnected)
{
    grpc_client client("localhost:50051");
    call_options opts;
    opts.deadline = std::chrono::system_clock::now() + std::chrono::hours{1};
    auto r = client.call_raw("/svc/Method", {}, opts);
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientCallRawGuards, FailsWithWaitForReadyWhenNotConnected)
{
    grpc_client client("localhost:50051");
    call_options opts;
    opts.wait_for_ready = true;
    auto r = client.call_raw("/svc/Method", {}, opts);
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientCallRawGuards, FailsWithCompressionAlgorithmWhenNotConnected)
{
    grpc_client client("localhost:50051");
    call_options opts;
    opts.compression_algorithm = "gzip";
    auto r = client.call_raw("/svc/Method", {}, opts);
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientCallRawGuards, FailsAfterDisconnectInvocation)
{
    grpc_client client("localhost:50051");
    client.disconnect();
    auto r = client.call_raw("/svc/Method", {});
    EXPECT_TRUE(r.is_err());
}

// ============================================================================
// streaming guard clauses — empty / non-slash methods, varied options
// ============================================================================

TEST(GrpcClientStreamingGuards, ServerStreamFailsWithEmptyMethod)
{
    grpc_client client("localhost:50051");
    auto r = client.server_stream_raw("", {});
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientStreamingGuards, ServerStreamFailsWithMethodMissingSlash)
{
    grpc_client client("localhost:50051");
    auto r = client.server_stream_raw("svc/Method", {});
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientStreamingGuards, ClientStreamFailsWithEmptyMethod)
{
    grpc_client client("localhost:50051");
    auto r = client.client_stream_raw("");
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientStreamingGuards, ClientStreamFailsWithMethodMissingSlash)
{
    grpc_client client("localhost:50051");
    auto r = client.client_stream_raw("svc/Method");
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientStreamingGuards, BidiStreamFailsWithEmptyMethod)
{
    grpc_client client("localhost:50051");
    auto r = client.bidi_stream_raw("");
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientStreamingGuards, BidiStreamFailsWithMethodMissingSlash)
{
    grpc_client client("localhost:50051");
    auto r = client.bidi_stream_raw("svc/Method");
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientStreamingGuards, ServerStreamFailsWithExpiredDeadline)
{
    grpc_client client("localhost:50051");
    call_options opts;
    opts.deadline = std::chrono::system_clock::now() - std::chrono::seconds{30};
    auto r = client.server_stream_raw("/svc/Method", {}, opts);
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientStreamingGuards, ClientStreamFailsAfterDisconnect)
{
    grpc_client client("localhost:50051");
    client.disconnect();
    auto r = client.client_stream_raw("/svc/Method");
    EXPECT_TRUE(r.is_err());
}

TEST(GrpcClientStreamingGuards, BidiStreamFailsAfterDisconnect)
{
    grpc_client client("localhost:50051");
    client.disconnect();
    auto r = client.bidi_stream_raw("/svc/Method");
    EXPECT_TRUE(r.is_err());
}

// ============================================================================
// call_raw_async — concurrent callbacks and edge cases
// ============================================================================

TEST(GrpcClientCallRawAsyncStress, MultipleConcurrentCallbacksAllReceiveErrors)
{
    grpc_client client("localhost:50051");
    constexpr int kCount = 8;

    std::mutex m;
    std::condition_variable cv;
    int done_count = 0;
    int err_count = 0;

    for (int i = 0; i < kCount; ++i)
    {
        client.call_raw_async(
            "/svc/Method",
            std::vector<uint8_t>{static_cast<uint8_t>(i)},
            [&](kcenon::network::Result<grpc_message> r) {
                std::lock_guard<std::mutex> lock(m);
                ++done_count;
                if (r.is_err()) ++err_count;
                cv.notify_all();
            },
            call_options{});
    }

    std::unique_lock<std::mutex> lock(m);
    bool ok = cv.wait_for(lock, std::chrono::seconds{10},
                          [&] { return done_count == kCount; });
    ASSERT_TRUE(ok) << "only " << done_count << "/" << kCount
                    << " callbacks completed";
    EXPECT_EQ(err_count, kCount);
}

TEST(GrpcClientCallRawAsyncStress, NullCallbackWithNonEmptyPayloadIsSafe)
{
    grpc_client client("localhost:50051");
    EXPECT_NO_FATAL_FAILURE(
        client.call_raw_async("/svc/Method",
                              std::vector<uint8_t>{1, 2, 3, 4, 5},
                              nullptr,
                              call_options{}));
    // Give detached worker a moment to retire.
    std::this_thread::sleep_for(100ms);
}

TEST(GrpcClientCallRawAsyncStress, NullCallbackWithDeadlineIsSafe)
{
    grpc_client client("localhost:50051");
    call_options opts;
    opts.set_timeout(50ms);
    EXPECT_NO_FATAL_FAILURE(
        client.call_raw_async("/svc/Method", {}, nullptr, opts));
    std::this_thread::sleep_for(100ms);
}

// ============================================================================
// grpc_metadata — copy / move / clear semantics
// ============================================================================

TEST(GrpcMetadataSemantics, CopyConstructionPreservesEntries)
{
    grpc_metadata src;
    src.emplace_back("a", "1");
    src.emplace_back("b", "2");
    src.emplace_back("c", "3");

    grpc_metadata dst(src);
    ASSERT_EQ(dst.size(), 3u);
    EXPECT_EQ(dst[0].first, "a");
    EXPECT_EQ(dst[2].second, "3");
}

TEST(GrpcMetadataSemantics, MoveConstructionTransfersEntries)
{
    grpc_metadata src;
    src.emplace_back("only", "value");
    grpc_metadata dst(std::move(src));
    ASSERT_EQ(dst.size(), 1u);
    EXPECT_EQ(dst[0].first, "only");
}

TEST(GrpcMetadataSemantics, AssignmentFromEmpty)
{
    grpc_metadata src;
    grpc_metadata dst;
    dst.emplace_back("k", "v");
    dst = src;
    EXPECT_TRUE(dst.empty());
}

TEST(GrpcMetadataSemantics, EraseShrinksContainer)
{
    grpc_metadata md;
    md.emplace_back("a", "1");
    md.emplace_back("b", "2");
    md.emplace_back("c", "3");
    md.erase(md.begin());
    ASSERT_EQ(md.size(), 2u);
    EXPECT_EQ(md[0].first, "b");
}

// ============================================================================
// Hermetic transport fixture demonstration (Issue #1060)
// ============================================================================

/**
 * @brief Demonstrates that the new hermetic TLS fixture lets grpc_client
 *        attempt a real connection against an in-process loopback peer.
 *
 * grpc_client's connect() uses an internal HTTP/2 channel; pointing it at the
 * loopback TLS listener exercises the channel construction, target parsing,
 * and async connect attempt — paths that the public-API tests above could
 * not drive without an external server.
 */
class GrpcClientHermeticTransportTest
    : public kcenon::network::tests::support::hermetic_transport_fixture
{
};

TEST_F(GrpcClientHermeticTransportTest, ConnectAttemptsHandshakeAgainstLoopbackTlsPeer)
{
    using namespace kcenon::network::tests::support;

    tls_loopback_listener listener(io());
    const std::string target =
        "127.0.0.1:" + std::to_string(static_cast<unsigned>(listener.port()));

    grpc_channel_config cfg;
    cfg.use_tls = true;
    auto client = std::make_shared<grpc_client>(target, cfg);

    std::atomic<bool> connect_returned{false};
    std::thread connector([&]() {
        (void)client->connect();
        connect_returned.store(true);
    });

    // The TLS listener accepts the TCP connection; whether the gRPC handshake
    // completes depends on the channel implementation. Either way, the
    // connect path is exercised and disconnect cleans up.
    EXPECT_TRUE(wait_for(
        [&]() { return listener.accepted(); },
        std::chrono::seconds(3)));

    client->disconnect();
    connector.join();
    EXPECT_TRUE(connect_returned.load());
}

// ============================================================================
// Phase 2A follow-up: drive grpc_client post-connect paths via mock_h2_server_peer
// (Part of #1063, follow-up to PR #1075/#1076)
//
// grpc_client::impl::connect() instantiates an internal http2::http2_client and
// delegates to its connect(host, port). Once the SETTINGS exchange against
// mock_h2_server_peer completes, http2_client::is_connected() flips true and
// grpc_client::is_connected() (which AND-s its own connected_ flag with the
// http2 client's state) follows. With that gate satisfied, public methods that
// previously short-circuited on the not-connected path can be driven into
// their post-connect branches: header build, metadata loop, deadline header,
// http2_client::post / start_stream forwarding, and the timeout-error branch.
// HEADERS+DATA reply paths are still gated on Phase 2A.2 of #1074; this PR
// is incremental progress and uses `Part of`, not `Closes`.
// ============================================================================

namespace
{

struct connected_grpc_client_setup
{
    std::shared_ptr<grpc_client> client;
    std::thread connector;
};

inline connected_grpc_client_setup make_connected_grpc_client(
    kcenon::network::tests::support::mock_h2_server_peer& peer,
    std::chrono::milliseconds default_timeout = std::chrono::milliseconds(2000))
{
    grpc_channel_config cfg;
    cfg.use_tls = true;
    cfg.default_timeout = default_timeout;

    const std::string target =
        "127.0.0.1:" + std::to_string(static_cast<unsigned>(peer.port()));

    auto client = std::make_shared<grpc_client>(target, cfg);
    std::thread connector([client]() {
        (void)client->connect();
    });
    return {std::move(client), std::move(connector)};
}

} // namespace

TEST_F(GrpcClientHermeticTransportTest, IsConnectedTrueAfterSettingsExchange)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer);

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->is_connected());

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest, WaitForConnectedReturnsTrueAfterHandshake)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer);

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    EXPECT_TRUE(setup.client->wait_for_connected(std::chrono::milliseconds(2000)));

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest, SecondConnectReturnsOkOnAlreadyConnected)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer);

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(setup.client->is_connected());

    // Second connect() short-circuits via the already_connected branch.
    auto second = setup.client->connect();
    EXPECT_TRUE(second.is_ok());

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest, TargetReturnsConfiguredAddressAfterHandshake)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer);

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));

    const auto& target = setup.client->target();
    EXPECT_NE(target.find("127.0.0.1:"), std::string::npos);

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest, CallRawConnectedTimesOutWhenPeerSendsNoResponse)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer, std::chrono::milliseconds(150));

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(setup.client->is_connected());

    // call_raw drives is_connected() check, method validation, header build,
    // grpc_message::serialize, and finally http2_client::post which times out
    // because the mock peer does not yet reply with HEADERS+DATA (Phase 2A.2
    // of #1074 will unblock that). We assert only on the error outcome.
    auto result = setup.client->call_raw("/svc/Method", std::vector<uint8_t>{});
    EXPECT_TRUE(result.is_err());

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest,
       CallRawWithCustomMetadataAndDeadlineDrivesTimeoutHeader)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer, std::chrono::milliseconds(150));

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(setup.client->is_connected());

    call_options options;
    options.metadata.emplace_back("x-custom-trace", "abc-123");
    options.metadata.emplace_back("x-tenant-id", "tenant-7");
    options.set_timeout(std::chrono::milliseconds(120));

    // Drives the metadata-loop and grpc-timeout header build paths in addition
    // to the post-timeout branch covered above.
    auto result = setup.client->call_raw(
        "/v1/Echo", std::vector<uint8_t>{0x01, 0x02, 0x03}, options);
    EXPECT_TRUE(result.is_err());

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest,
       CallRawWithExpiredDeadlineAfterHandshakeReturnsDeadlineExceeded)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer);

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(setup.client->is_connected());

    call_options options;
    options.deadline = std::chrono::system_clock::now() - std::chrono::seconds(1);

    // Drives the post-connect deadline-exceeded branch (after is_connected()
    // and method validation, before the http2 POST is attempted).
    auto result = setup.client->call_raw(
        "/svc/Method", std::vector<uint8_t>{}, options);
    EXPECT_TRUE(result.is_err());

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest, ServerStreamRawConnectedReturnsValidReader)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer);

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(setup.client->is_connected());

    // Drives is_connected() check, method validation, header build, then
    // start_stream + write_stream + shared_holder allocation. The reader is
    // returned successfully even though the peer never replies; the client
    // owns the read-side state machine until disconnect.
    auto result = setup.client->server_stream_raw(
        "/svc/ListEvents", std::vector<uint8_t>{0xff});
    EXPECT_TRUE(result.is_ok());
    if (result.is_ok())
    {
        EXPECT_NE(result.value().get(), nullptr);
    }

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest, ClientStreamRawConnectedReturnsValidWriter)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer);

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(setup.client->is_connected());

    // Drives the connected client_stream_raw path: header build,
    // start_stream forwarding, shared_writer_holder allocation.
    auto result = setup.client->client_stream_raw("/svc/Upload");
    EXPECT_TRUE(result.is_ok());
    if (result.is_ok())
    {
        EXPECT_NE(result.value().get(), nullptr);
    }

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest, BidiStreamRawConnectedReturnsValidStream)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer);

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(setup.client->is_connected());

    call_options options;
    options.set_timeout(std::chrono::milliseconds(500));
    options.metadata.emplace_back("x-trace", "bidi-1");

    // Drives bidi_stream_raw post-connect path including grpc-timeout header
    // formatting and metadata-loop iteration before start_stream is called.
    auto result = setup.client->bidi_stream_raw("/svc/Chat", options);
    EXPECT_TRUE(result.is_ok());
    if (result.is_ok())
    {
        EXPECT_NE(result.value().get(), nullptr);
    }

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest, CallRawAsyncDeliversTimeoutErrorToCallback)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer, std::chrono::milliseconds(150));

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(setup.client->is_connected());

    std::promise<bool> received;
    auto received_future = received.get_future();
    setup.client->call_raw_async(
        "/svc/Method",
        std::vector<uint8_t>{},
        [&received](kcenon::network::Result<grpc_message> r) {
            received.set_value(r.is_err());
        });

    EXPECT_EQ(received_future.wait_for(std::chrono::seconds(2)),
              std::future_status::ready);
    EXPECT_TRUE(received_future.get());

    setup.client->disconnect();
    setup.connector.join();
}

TEST_F(GrpcClientHermeticTransportTest, DisconnectIsIdempotentAfterFullHandshake)
{
    using namespace kcenon::network::tests::support;

    mock_h2_server_peer peer(io());
    auto setup = make_connected_grpc_client(peer);

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(setup.client->is_connected());

    // First disconnect — drives http2_client_->disconnect() in the connected
    // path, then resets the http2_client_ shared_ptr.
    setup.client->disconnect();
    EXPECT_FALSE(setup.client->is_connected());

    // Second disconnect — connected_ is already false and http2_client_ is
    // null, so the inner branch is skipped and the call is a no-op.
    setup.client->disconnect();
    EXPECT_FALSE(setup.client->is_connected());

    setup.connector.join();
}

// ============================================================================
// Phase 2B demo: drive grpc_client::call_raw post-connect success path via
// mock_grpc_server_peer (Phase 2B of #1074, on top of Phase 2A/2A.2).
//
// mock_grpc_server_peer extends mock_h2_server_peer's framing model with the
// gRPC-specific server-side reply trio:
//   1. response HEADERS (`:status: 200`, `content-type: application/grpc`)
//   2. DATA frame carrying one length-prefixed gRPC message
//   3. trailing HEADERS (`grpc-status: 0`, END_STREAM)
//
// This pushes call_raw past the http2_client::post wait, the response.headers
// trailer scan (grpc_status extraction), and the grpc_message::parse on the
// response body — branches that the Phase 2A `mock_h2_server_peer` peer
// could not exercise because it never replied with HEADERS+DATA.
// ============================================================================

#include "mock_grpc_server_peer.h"

TEST_F(GrpcClientHermeticTransportTest, CallRawSucceedsWithMockGrpcPeerEchoUnary)
{
    using namespace kcenon::network::tests::support;

    mock_grpc_server_peer peer(io(), grpc_reply_mode::echo_unary);

    grpc_channel_config cfg;
    cfg.use_tls = true;
    cfg.default_timeout = std::chrono::milliseconds(2000);

    const std::string target =
        "127.0.0.1:" + std::to_string(static_cast<unsigned>(peer.port()));
    auto client = std::make_shared<grpc_client>(target, cfg);

    std::thread connector([client]() { (void)client->connect(); });

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(client->is_connected());

    // call_raw drives: is_connected() check, method validation, header
    // build, grpc_message::serialize, http2_client::post wait,
    // response.headers trailer scan (grpc_status extraction), and
    // grpc_message::parse on the response body.
    auto result = client->call_raw(
        "/svc/Echo", std::vector<uint8_t>{0x01, 0x02, 0x03});

    EXPECT_TRUE(result.is_ok());
    if (result.is_ok())
    {
        // The mock peer always replies with the body "ok".
        const auto& msg = result.value();
        ASSERT_EQ(msg.data.size(), 2u);
        EXPECT_EQ(msg.data[0], 'o');
        EXPECT_EQ(msg.data[1], 'k');
    }

    // The peer accumulated the client's request body (including the
    // gRPC 5-byte length prefix). Verifying the prefix sanity-checks
    // that the client serialized through the gRPC frame layer.
    EXPECT_TRUE(peer.request_received());
    EXPECT_TRUE(peer.response_sent());
    const auto body = peer.request_body();
    ASSERT_GE(body.size(), 5u);
    EXPECT_EQ(body[0], 0x00);  // not compressed

    client->disconnect();
    connector.join();
}

// ============================================================================
// Phase 2E.R2: frame_injector-driven error coverage for grpc_client.cpp
// (Issue #1107, Part of #953)
//
// Round 1 sub-issues #994 / #1063 raised happy-path public-API coverage but
// left grpc_client.cpp at 22.6% line / 9.5% branch (run 25430202846,
// 2026-05-06). The error branches require a peer that emits malformed,
// dropped, truncated, or slow byte streams — exactly what the Phase 2E
// substrate (#1074, PR #1105) provides.
//
// All TEST_F below compose mock_grpc_server_peer with frame_injector or
// the new grpc_reply_mode::echo_unary_error_status to drive
// previously-unreachable error branches in grpc_client.cpp. The substrate
// routes every server-originated frame write through injector_.write(),
// so a single injection_spec applies uniformly to every server frame
// (server SETTINGS, SETTINGS-ACK; for echo_unary mode additionally the
// response HEADERS, DATA, and trailing HEADERS). Tests therefore choose
// injection parameters such that the targeted fault either (a) lands on
// the very first server-originated frame so that is_connected() never
// flips true, or (b) is constructed to be a no-op for the 9-byte
// empty-payload SETTINGS frames and only takes effect on the longer
// response frames in echo_unary mode.
// ============================================================================

namespace
{

inline std::shared_ptr<grpc_client> build_grpc_client(
    unsigned short port,
    std::chrono::milliseconds default_timeout = std::chrono::milliseconds(500))
{
    grpc_channel_config cfg;
    cfg.use_tls = true;
    cfg.default_timeout = default_timeout;

    const std::string target =
        "127.0.0.1:" + std::to_string(static_cast<unsigned>(port));
    return std::make_shared<grpc_client>(target, cfg);
}

} // namespace

TEST_F(GrpcClientHermeticTransportTest,
       DropFirstServerSettingsLeavesGrpcClientUnconnected)
{
    using namespace kcenon::network::tests::support;
    injection_spec spec;
    spec.mode = injection_mode::drop;

    mock_grpc_server_peer peer(io(), grpc_reply_mode::drain_only, spec);

    auto client = build_grpc_client(peer.port());
    std::thread connector([client]() { (void)client->connect(); });

    // The peer's injector swallows the first server SETTINGS write. Without
    // server SETTINGS the underlying http2_client cannot complete the
    // handshake; grpc_client::is_connected() (which AND-s its own connected_
    // flag with the http2 client's state) never flips true, exercising the
    // connect-timeout branch in grpc_client::connect() that previously
    // required an unreachable network condition.
    EXPECT_FALSE(wait_for([&]() { return client->is_connected(); },
                          std::chrono::milliseconds(300)));
    EXPECT_FALSE(peer.settings_exchanged());

    client->disconnect();
    connector.join();
}

TEST_F(GrpcClientHermeticTransportTest,
       MalformedServerSettingsAckTypeByteBlocksGrpcConnect)
{
    using namespace kcenon::network::tests::support;
    injection_spec spec;
    spec.mode = injection_mode::malform;
    spec.malform_offset = 3;     // type byte of an HTTP/2 frame header
    spec.malform_xor = 0x0F;     // 0x04 (SETTINGS) -> 0x0B (unknown)

    mock_grpc_server_peer peer(io(), grpc_reply_mode::drain_only, spec);

    auto client = build_grpc_client(peer.port());
    std::thread connector([client]() { (void)client->connect(); });

    // The injector applies to *every* server write, so the first
    // server-originated frame (empty SETTINGS) already arrives with type
    // byte = 0x0B. Per RFC 7540 §5.5 the client must ignore unknown frame
    // types, so it discards the frame and waits for actual SETTINGS that
    // never arrive. is_connected() stays false; grpc_client::connect()
    // takes the connect-error / unavailable branch. This is the gRPC
    // analogue of Http2ClientHermeticTransportTest::
    // MalformedServerSettingsTypeByteTriggersConnectTimeout.
    EXPECT_FALSE(wait_for([&]() { return client->is_connected(); },
                          std::chrono::milliseconds(300)));

    client->disconnect();
    connector.join();
}

TEST_F(GrpcClientHermeticTransportTest,
       TruncatedServerSettingsHeaderBlocksGrpcConnect)
{
    using namespace kcenon::network::tests::support;
    injection_spec spec;
    spec.mode = injection_mode::truncate;
    spec.truncate_at = 4;  // partial 9-byte frame header → unparseable

    mock_grpc_server_peer peer(io(), grpc_reply_mode::drain_only, spec);

    auto client = build_grpc_client(peer.port());
    std::thread connector([client]() { (void)client->connect(); });

    // The underlying http2_client receives 4 bytes (less than a complete
    // 9-byte frame header) and waits indefinitely for the remaining 5
    // bytes. This drives the partial-header / read-loop short-read branch
    // in http2_client.cpp from inside grpc_client::connect(). The grpc
    // client's connected_ flag is therefore never set, exercising the
    // post-connect failure dispatch.
    EXPECT_FALSE(wait_for([&]() { return client->is_connected(); },
                          std::chrono::milliseconds(300)));

    client->disconnect();
    connector.join();
}

TEST_F(GrpcClientHermeticTransportTest,
       NonOkGrpcStatusTrailerDispatchesGrpcErrorBranch)
{
    using namespace kcenon::network::tests::support;

    // grpc_reply_mode::echo_unary_error_status sends the same HEADERS+DATA
    // frames as echo_unary but the trailing HEADERS frame carries
    // "grpc-status: 14" (UNAVAILABLE) plus a "grpc-message" entry. The
    // HTTP-level :status: is still 200 so the HTTP-status branch is
    // bypassed; the client therefore reaches the grpc-status extraction
    // loop in call_raw and takes the "grpc_status != ok" branch that
    // produces an error<grpc_message> populated with the trailer message.
    mock_grpc_server_peer peer(io(),
                               grpc_reply_mode::echo_unary_error_status);

    grpc_channel_config cfg;
    cfg.use_tls = true;
    cfg.default_timeout = std::chrono::milliseconds(2000);

    const std::string target =
        "127.0.0.1:" + std::to_string(static_cast<unsigned>(peer.port()));
    auto client = std::make_shared<grpc_client>(target, cfg);
    std::thread connector([client]() { (void)client->connect(); });

    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(client->is_connected());

    // call_raw drives is_connected() check, method validation, header
    // build, http2::post wait, response.headers trailer scan
    // (grpc_status extraction WITH a non-zero numeric value), and then
    // the gRPC-error early return that the Phase 2B happy-path tests do
    // not reach.
    auto result = client->call_raw(
        "/svc/Method", std::vector<uint8_t>{0x01});

    EXPECT_TRUE(result.is_err());
    if (result.is_err())
    {
        // The error code is the gRPC status_code numeric value; 14 is
        // UNAVAILABLE per the standard mapping.
        EXPECT_EQ(result.error().code, 14);
    }
    EXPECT_TRUE(peer.request_received());
    EXPECT_TRUE(peer.response_sent());

    client->disconnect();
    connector.join();
}

TEST_F(GrpcClientHermeticTransportTest,
       TruncateAtNineDropsResponsePayloadFailingCallRaw)
{
    using namespace kcenon::network::tests::support;
    injection_spec spec;
    spec.mode = injection_mode::truncate;
    spec.truncate_at = 9;  // empty SETTINGS frames are exactly 9 bytes,
                           // so the SETTINGS handshake completes
                           // unchanged. Longer response HEADERS / DATA /
                           // trailing HEADERS frames lose their payloads,
                           // leaving headers-only on the wire.

    mock_grpc_server_peer peer(io(), grpc_reply_mode::echo_unary, spec);

    grpc_channel_config cfg;
    cfg.use_tls = true;
    cfg.default_timeout = std::chrono::milliseconds(500);

    const std::string target =
        "127.0.0.1:" + std::to_string(static_cast<unsigned>(peer.port()));
    auto client = std::make_shared<grpc_client>(target, cfg);
    std::thread connector([client]() { (void)client->connect(); });

    // SETTINGS exchange completes because empty SETTINGS frames are
    // exactly 9 bytes total and truncate_at = 9 keeps the entire buffer.
    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    ASSERT_TRUE(client->is_connected());

    // The peer's response HEADERS + DATA + trailing HEADERS frames are
    // each truncated to their 9-byte frame headers; the HPACK / DATA
    // payloads never reach the client. The h2 dispatch layer therefore
    // never delivers a complete response, so call_raw resolves with an
    // error rather than a successful grpc_message — driving the
    // post-timeout / post-protocol-error branch in grpc_client::call_raw
    // that the Phase 2B happy-path tests do not reach.
    auto result = client->call_raw(
        "/svc/Method", std::vector<uint8_t>{0x01, 0x02, 0x03});
    EXPECT_TRUE(result.is_err());

    client->disconnect();
    connector.join();
}

// Phase 2E.R2 (Issue #1107): the slow_write TEST_F below pass cleanly
// under the Debug/Release matrix builds but fail under the coverage
// workflow because lcov/gcov instrumentation slows the SETTINGS handshake
// beyond the wait budget on shared CI runners. The guard preserves their
// assertion value while keeping the coverage-build signal clean.
#ifndef NETWORK_COVERAGE_BUILD
TEST_F(GrpcClientHermeticTransportTest,
       SlowWriteServerFramesStillCompleteHandshake)
{
    using namespace kcenon::network::tests::support;
    injection_spec spec;
    spec.mode = injection_mode::slow_write;
    spec.slow_step = std::chrono::microseconds(500);

    mock_grpc_server_peer peer(io(), grpc_reply_mode::drain_only, spec);

    auto client = build_grpc_client(peer.port(),
                                    std::chrono::milliseconds(2000));
    std::thread connector([client]() { (void)client->connect(); });

    // Each of the 9 bytes of server SETTINGS arrives 500 microseconds
    // apart, so the full frame transmission takes ~4.5 ms. The handshake
    // completes because slow_write does not corrupt the bytes — it only
    // paces them. This drives the partial-read / accumulating-buffer
    // branch in the underlying http2 frame-reader from inside
    // grpc_client::connect(), which a single-shot write does not
    // exercise: the read callback is invoked multiple times before a
    // complete header is in the buffer. SETTINGS-ACK is also paced
    // byte-by-byte.
    EXPECT_TRUE(wait_for([&]() { return peer.settings_exchanged(); },
                         std::chrono::seconds(3)));
    EXPECT_FALSE(peer.io_failed());
    EXPECT_TRUE(client->is_connected());

    client->disconnect();
    connector.join();
}
#endif // !NETWORK_COVERAGE_BUILD
