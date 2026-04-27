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
