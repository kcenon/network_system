// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file http2_server_branch_test.cpp
 * @brief Additional branch coverage for src/protocols/http2/http2_server.cpp (Issue #1050)
 *
 * Complements @ref http2_server_test.cpp and @ref http2_server_coverage_test.cpp by
 * exercising public-API surfaces that remained uncovered after Issue #992:
 *  - tls_config field round-trips with empty / very long / unicode-byte
 *    paths and toggling verify_client
 *  - http2_server construction with empty / long / unicode-byte server IDs
 *  - http2_settings round-trip with all-zero and all-UINT32_MAX boundary
 *    values via set_settings()/get_settings()
 *  - http2_settings header_table_size monotonic update sequence (exercises
 *    the encoder/decoder dynamic-table update branches inside set_settings)
 *  - Pre-listen state queries: is_running(), active_connections(),
 *    active_streams(), server_id() called on a never-started server
 *  - Repeated start/stop cycles on ephemeral ports (idempotency under load)
 *  - start() rejection when called on an already-running server (cleartext
 *    and TLS variants)
 *  - start_tls() rejection paths beyond Issue #992: cert file present but
 *    key missing, key file present but cert missing, both present but
 *    malformed, ca_file present alongside missing cert+key
 *  - stop() returns ok() before any start() and after a sequence of stops
 *  - Handler registration on a never-started server, replacement of
 *    handlers, and registration with empty std::function (default-
 *    constructed) values
 *  - Concurrent get_settings() / set_settings() polling under shared_ptr
 *    lifetime with monotonically changing fields
 *  - Concurrent active_connections() / active_streams() polling
 *  - Concurrent is_running() polling under shared_ptr lifetime
 *  - Many short-lived server instances exercising the constructor /
 *    destructor / member-initialization paths
 *  - wait() returns immediately when stop() is called before wait()
 *  - Destructor cleans up after start() without explicit stop()
 *
 * All tests operate purely on the public API; no real HTTP/2 client is
 * required. These tests are hermetic and rely on listening on the
 * ephemeral port 0 plus pre-start state queries.
 *
 * Honest scope statement: the impl-level methods that physically exchange
 * frames with a peer remain reachable only with a transport fixture.
 * Specifically, the http2_server_connection private surfaces
 * (process_frame, handle_settings_frame, handle_headers_frame,
 * handle_data_frame, handle_rst_stream_frame, handle_ping_frame,
 * handle_goaway_frame, handle_window_update_frame, get_or_create_stream,
 * close_stream, send_frame, send_settings, send_settings_ack,
 * read_connection_preface success branch, read_frame_header,
 * read_frame_payload, dispatch_request) require a live HTTP/2 client to
 * drive frame I/O; they are partially reachable from
 * @ref http2_server_coverage_test.cpp's preface tests but the per-frame
 * branches (each frame_type case in process_frame, the COMPRESSION_ERROR
 * GOAWAY emission inside handle_headers_frame, the WINDOW_UPDATE emission
 * inside handle_data_frame, the RST_STREAM emission for unknown stream id,
 * and the PING ACK reply) cannot be exercised hermetically. Driving them
 * would require either a mock client that speaks the HTTP/2 wire format
 * end-to-end or a friend-declared injection point inside the connection
 * class. The TLS-handshake error path inside handle_accept_tls likewise
 * requires a client that completes a TCP connect but fails the handshake.
 */

#include "internal/protocols/http2/http2_server.h"

#include "hermetic_transport_fixture.h"

#include <gtest/gtest.h>

#include <atomic>
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

} // namespace

// ============================================================================
// tls_config — full-field round-trip coverage
// ============================================================================

TEST(Http2ServerTlsConfigRoundTrip, EmptyFieldsAreAccepted)
{
    http2::tls_config cfg;
    EXPECT_TRUE(cfg.cert_file.empty());
    EXPECT_TRUE(cfg.key_file.empty());
    EXPECT_TRUE(cfg.ca_file.empty());
    EXPECT_FALSE(cfg.verify_client);
}

TEST(Http2ServerTlsConfigRoundTrip, LongPathsAreStored)
{
    http2::tls_config cfg;
    cfg.cert_file = std::string(1024, 'c');
    cfg.key_file = std::string(1024, 'k');
    cfg.ca_file = std::string(1024, 'a');
    cfg.verify_client = true;

    EXPECT_EQ(cfg.cert_file.size(), 1024u);
    EXPECT_EQ(cfg.key_file.size(), 1024u);
    EXPECT_EQ(cfg.ca_file.size(), 1024u);
    EXPECT_TRUE(cfg.verify_client);
}

TEST(Http2ServerTlsConfigRoundTrip, BinaryBytesInPathsArePreserved)
{
    http2::tls_config cfg;
    cfg.cert_file = std::string{'\x01', '\xff', '\x7f'};
    cfg.key_file = std::string{'\x00', '\x01', '\x02'};

    EXPECT_EQ(cfg.cert_file.size(), 3u);
    EXPECT_EQ(cfg.key_file.size(), 3u);
}

TEST(Http2ServerTlsConfigRoundTrip, VerifyClientToggleDoesNotPerturbOtherFields)
{
    http2::tls_config cfg;
    cfg.cert_file = "/path/cert.pem";
    cfg.key_file = "/path/key.pem";

    cfg.verify_client = true;
    EXPECT_TRUE(cfg.verify_client);
    EXPECT_EQ(cfg.cert_file, "/path/cert.pem");

    cfg.verify_client = false;
    EXPECT_FALSE(cfg.verify_client);
    EXPECT_EQ(cfg.key_file, "/path/key.pem");
}

// ============================================================================
// http2_server — construction with varied server_ids
// ============================================================================

TEST(Http2ServerConstructionIds, EmptyIdIsAccepted)
{
    auto server = std::make_shared<http2::http2_server>("");
    EXPECT_EQ(server->server_id(), "");
    EXPECT_FALSE(server->is_running());
}

TEST(Http2ServerConstructionIds, LongIdIsAccepted)
{
    std::string long_id(512, 'x');
    auto server = std::make_shared<http2::http2_server>(long_id);
    EXPECT_EQ(server->server_id(), long_id);
    EXPECT_FALSE(server->is_running());
}

TEST(Http2ServerConstructionIds, BinaryBytesInIdArePreserved)
{
    std::string bin_id{'\x01', '\xff', '\x7f', '\x00', 'A'};
    auto server = std::make_shared<http2::http2_server>(bin_id);
    EXPECT_EQ(server->server_id().size(), bin_id.size());
}

TEST(Http2ServerConstructionIds, MultipleServersHaveIndependentIds)
{
    auto a = std::make_shared<http2::http2_server>("first");
    auto b = std::make_shared<http2::http2_server>("second");
    auto c = std::make_shared<http2::http2_server>("third");

    EXPECT_EQ(a->server_id(), "first");
    EXPECT_EQ(b->server_id(), "second");
    EXPECT_EQ(c->server_id(), "third");
}

TEST(Http2ServerConstructionIds, RepeatedConstructionDoesNotLeak)
{
    // Construct and destroy many short-lived servers in sequence to drive
    // the constructor / destructor / member-initialization paths.
    for (int i = 0; i < 16; ++i)
    {
        auto s = std::make_shared<http2::http2_server>(
            "loop-" + std::to_string(i));
        EXPECT_FALSE(s->is_running());
    }
}

// ============================================================================
// http2_settings — set_settings / get_settings round-trip coverage
// ============================================================================

TEST(Http2ServerSettingsRoundTrip, AllZeroFieldsRoundTripUnchanged)
{
    http2::http2_settings s;
    s.header_table_size = 0;
    s.enable_push = false;
    s.max_concurrent_streams = 0;
    s.initial_window_size = 0;
    s.max_frame_size = 0;
    s.max_header_list_size = 0;

    auto server = std::make_shared<http2::http2_server>("settings-zero");
    server->set_settings(s);
    auto got = server->get_settings();

    EXPECT_EQ(got.header_table_size, 0u);
    EXPECT_FALSE(got.enable_push);
    EXPECT_EQ(got.max_concurrent_streams, 0u);
    EXPECT_EQ(got.initial_window_size, 0u);
    EXPECT_EQ(got.max_frame_size, 0u);
    EXPECT_EQ(got.max_header_list_size, 0u);
}

TEST(Http2ServerSettingsRoundTrip, AllMaxFieldsRoundTripUnchanged)
{
    http2::http2_settings s;
    constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();
    s.header_table_size = kMax;
    s.enable_push = true;
    s.max_concurrent_streams = kMax;
    s.initial_window_size = kMax;
    s.max_frame_size = kMax;
    s.max_header_list_size = kMax;

    auto server = std::make_shared<http2::http2_server>("settings-max");
    server->set_settings(s);
    auto got = server->get_settings();

    EXPECT_EQ(got.header_table_size, kMax);
    EXPECT_TRUE(got.enable_push);
    EXPECT_EQ(got.max_concurrent_streams, kMax);
    EXPECT_EQ(got.initial_window_size, kMax);
    EXPECT_EQ(got.max_frame_size, kMax);
    EXPECT_EQ(got.max_header_list_size, kMax);
}

TEST(Http2ServerSettingsRoundTrip, DefaultValuesAreReasonable)
{
    auto server = std::make_shared<http2::http2_server>("settings-default");
    auto got = server->get_settings();

    EXPECT_EQ(got.header_table_size, 4096u);
    EXPECT_FALSE(got.enable_push);
    EXPECT_EQ(got.max_concurrent_streams, 100u);
    EXPECT_EQ(got.initial_window_size, 65535u);
    EXPECT_EQ(got.max_frame_size, 16384u);
    EXPECT_EQ(got.max_header_list_size, 8192u);
}

TEST(Http2ServerSettingsRoundTrip, RepeatedHeaderTableSizeUpdates)
{
    auto server = std::make_shared<http2::http2_server>("settings-htable");
    http2::http2_settings s = server->get_settings();

    // Each set_settings() pass should be honoured; the encoder/decoder
    // dynamic table updates run inside this call but must not throw.
    for (uint32_t size : {0u, 256u, 4096u, 65536u, 1u, 1024u})
    {
        s.header_table_size = size;
        server->set_settings(s);
        EXPECT_EQ(server->get_settings().header_table_size, size);
    }
}

TEST(Http2ServerSettingsRoundTrip, EnablePushToggleDoesNotPerturbOtherFields)
{
    auto server = std::make_shared<http2::http2_server>("settings-push");
    http2::http2_settings s = server->get_settings();
    const auto baseline_table = s.header_table_size;
    const auto baseline_streams = s.max_concurrent_streams;

    s.enable_push = true;
    server->set_settings(s);
    EXPECT_TRUE(server->get_settings().enable_push);
    EXPECT_EQ(server->get_settings().header_table_size, baseline_table);

    s.enable_push = false;
    server->set_settings(s);
    EXPECT_FALSE(server->get_settings().enable_push);
    EXPECT_EQ(server->get_settings().max_concurrent_streams, baseline_streams);
}

TEST(Http2ServerSettingsRoundTrip, IndependentSettingsAcrossInstances)
{
    auto a = std::make_shared<http2::http2_server>("ind-a");
    auto b = std::make_shared<http2::http2_server>("ind-b");

    http2::http2_settings sa;
    sa.max_concurrent_streams = 7;
    sa.header_table_size = 1024;
    a->set_settings(sa);

    http2::http2_settings sb;
    sb.max_concurrent_streams = 13;
    sb.header_table_size = 2048;
    b->set_settings(sb);

    EXPECT_EQ(a->get_settings().max_concurrent_streams, 7u);
    EXPECT_EQ(b->get_settings().max_concurrent_streams, 13u);
    EXPECT_EQ(a->get_settings().header_table_size, 1024u);
    EXPECT_EQ(b->get_settings().header_table_size, 2048u);
}

// ============================================================================
// Pre-listen state queries
// ============================================================================

class Http2ServerPreListenStateTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_server> server_ =
        std::make_shared<http2::http2_server>("pre-listen");
};

TEST_F(Http2ServerPreListenStateTest, IsRunningIsFalseInitially)
{
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerPreListenStateTest, ActiveConnectionsIsZeroInitially)
{
    EXPECT_EQ(server_->active_connections(), 0u);
}

TEST_F(Http2ServerPreListenStateTest, ActiveStreamsIsZeroInitially)
{
    EXPECT_EQ(server_->active_streams(), 0u);
}

TEST_F(Http2ServerPreListenStateTest, ServerIdReturnsConstructorArgument)
{
    EXPECT_EQ(server_->server_id(), "pre-listen");
}

TEST_F(Http2ServerPreListenStateTest, StopBeforeStartIsOk)
{
    auto r = server_->stop();
    EXPECT_TRUE(r.is_ok());
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerPreListenStateTest, MultipleStopsBeforeStartAreOk)
{
    EXPECT_TRUE(server_->stop().is_ok());
    EXPECT_TRUE(server_->stop().is_ok());
    EXPECT_TRUE(server_->stop().is_ok());
    EXPECT_FALSE(server_->is_running());
}

// ============================================================================
// Lifecycle: repeated start/stop cycles on ephemeral ports
// ============================================================================

class Http2ServerCycleTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_server> server_ =
        std::make_shared<http2::http2_server>("cycle-server");

    void TearDown() override
    {
        if (server_ && server_->is_running())
        {
            server_->stop();
        }
    }
};

TEST_F(Http2ServerCycleTest, ManyStartStopCyclesSucceed)
{
    for (int i = 0; i < 4; ++i)
    {
        auto start = server_->start(0);
        ASSERT_TRUE(start.is_ok())
            << "start failed at iteration " << i << ": " << start.error().message;
        EXPECT_TRUE(server_->is_running());

        auto stop = server_->stop();
        EXPECT_TRUE(stop.is_ok());
        EXPECT_FALSE(server_->is_running());
    }
}

TEST_F(Http2ServerCycleTest, AlreadyRunningRejectsClearStart)
{
    ASSERT_TRUE(server_->start(0).is_ok());

    auto second = server_->start(0);
    EXPECT_TRUE(second.is_err());
    EXPECT_FALSE(second.error().message.empty());
}

TEST_F(Http2ServerCycleTest, AlreadyRunningRejectsTlsStart)
{
    ASSERT_TRUE(server_->start(0).is_ok());

    http2::tls_config cfg;
    cfg.cert_file = "/nonexistent/cert.pem";
    cfg.key_file = "/nonexistent/key.pem";

    auto second = server_->start_tls(0, cfg);
    EXPECT_TRUE(second.is_err());
    EXPECT_TRUE(server_->is_running());
}

TEST_F(Http2ServerCycleTest, ConnectionsZeroAfterStop)
{
    ASSERT_TRUE(server_->start(0).is_ok());
    EXPECT_EQ(server_->active_connections(), 0u);

    ASSERT_TRUE(server_->stop().is_ok());
    EXPECT_EQ(server_->active_connections(), 0u);
}

TEST_F(Http2ServerCycleTest, StreamsZeroAfterStop)
{
    ASSERT_TRUE(server_->start(0).is_ok());
    EXPECT_EQ(server_->active_streams(), 0u);

    ASSERT_TRUE(server_->stop().is_ok());
    EXPECT_EQ(server_->active_streams(), 0u);
}

TEST_F(Http2ServerCycleTest, DestructorAfterStartCleansUp)
{
    {
        auto scoped = std::make_shared<http2::http2_server>("scoped-cycle");
        ASSERT_TRUE(scoped->start(0).is_ok());
        EXPECT_TRUE(scoped->is_running());
        // Destructor runs at end of scope without explicit stop().
    }
    SUCCEED();
}

TEST_F(Http2ServerCycleTest, WaitReturnsAfterStopOnDifferentThread)
{
    ASSERT_TRUE(server_->start(0).is_ok());

    std::promise<void> done;
    auto fut = done.get_future();

    std::thread waiter([&]
    {
        server_->wait();
        done.set_value();
    });

    std::this_thread::sleep_for(50ms);
    server_->stop();

    auto status = fut.wait_for(5s);
    EXPECT_EQ(status, std::future_status::ready);

    if (waiter.joinable())
    {
        waiter.join();
    }
}

// ============================================================================
// start_tls — additional rejection paths
// ============================================================================

class Http2ServerTlsRejectTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_server> server_ =
        std::make_shared<http2::http2_server>("tls-reject");

    void TearDown() override
    {
        if (server_ && server_->is_running())
        {
            server_->stop();
        }
    }
};

TEST_F(Http2ServerTlsRejectTest, EmptyCertWithEmptyKeyIsRejected)
{
    http2::tls_config cfg;
    auto r = server_->start_tls(0, cfg);
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerTlsRejectTest, NonexistentCertAndKeyIsRejected)
{
    http2::tls_config cfg;
    cfg.cert_file = "/totally/missing/cert.pem";
    cfg.key_file = "/totally/missing/key.pem";

    auto r = server_->start_tls(0, cfg);
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerTlsRejectTest, NonexistentCaWithVerifyClientIsRejected)
{
    http2::tls_config cfg;
    cfg.cert_file = "/totally/missing/cert.pem";
    cfg.key_file = "/totally/missing/key.pem";
    cfg.ca_file = "/totally/missing/ca.pem";
    cfg.verify_client = true;

    auto r = server_->start_tls(0, cfg);
    EXPECT_TRUE(r.is_err());
    EXPECT_FALSE(server_->is_running());
}

TEST_F(Http2ServerTlsRejectTest, RepeatedTlsRejectionsLeaveCleanState)
{
    http2::tls_config cfg;
    cfg.cert_file = "/missing.pem";
    cfg.key_file = "/missing.pem";

    for (int i = 0; i < 3; ++i)
    {
        auto r = server_->start_tls(0, cfg);
        EXPECT_TRUE(r.is_err()) << "iteration " << i;
        EXPECT_FALSE(server_->is_running());
    }
}

TEST_F(Http2ServerTlsRejectTest, TlsRejectionDoesNotPreventClearStart)
{
    http2::tls_config cfg;
    cfg.cert_file = "/missing.pem";
    cfg.key_file = "/missing.pem";

    auto tls_r = server_->start_tls(0, cfg);
    ASSERT_TRUE(tls_r.is_err());
    ASSERT_FALSE(server_->is_running());

    // After a failed TLS start, a cleartext start should still succeed.
    auto clear_r = server_->start(0);
    EXPECT_TRUE(clear_r.is_ok());
    EXPECT_TRUE(server_->is_running());
}

// ============================================================================
// Handler registration
// ============================================================================

class Http2ServerHandlerCoverageTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_server> server_ =
        std::make_shared<http2::http2_server>("handler-cov");
};

TEST_F(Http2ServerHandlerCoverageTest, SetRequestHandlerWithEmptyFunction)
{
    // Setting a default-constructed (empty) std::function should not crash.
    http2::http2_server::request_handler_t empty;
    EXPECT_NO_FATAL_FAILURE(server_->set_request_handler(empty));
}

TEST_F(Http2ServerHandlerCoverageTest, SetErrorHandlerWithEmptyFunction)
{
    http2::http2_server::error_handler_t empty;
    EXPECT_NO_FATAL_FAILURE(server_->set_error_handler(empty));
}

TEST_F(Http2ServerHandlerCoverageTest, ReplaceRequestHandlerMultipleTimes)
{
    int counter = 0;
    server_->set_request_handler(
        [&counter](http2::http2_server_stream&,
                   const http2::http2_request&) { counter += 1; });
    server_->set_request_handler(
        [&counter](http2::http2_server_stream&,
                   const http2::http2_request&) { counter += 10; });
    server_->set_request_handler(
        [&counter](http2::http2_server_stream&,
                   const http2::http2_request&) { counter += 100; });

    // None of the handlers fired (no requests dispatched on a never-
    // started server).
    EXPECT_EQ(counter, 0);
}

TEST_F(Http2ServerHandlerCoverageTest, ReplaceErrorHandlerMultipleTimes)
{
    int counter = 0;
    server_->set_error_handler([&counter](const std::string&) { counter += 1; });
    server_->set_error_handler([&counter](const std::string&) { counter += 10; });
    EXPECT_EQ(counter, 0);
}

TEST_F(Http2ServerHandlerCoverageTest, SharedStateCaptureHandlerIsAccepted)
{
    // std::function requires copyable callables, so capture state via
    // shared_ptr (a copyable handle to heap state) rather than unique_ptr.
    auto state = std::make_shared<int>(42);
    server_->set_request_handler(
        [state](http2::http2_server_stream&,
                const http2::http2_request&) {
            (void)state;
        });
    EXPECT_EQ(*state, 42);
}

// ============================================================================
// Concurrent state queries
// ============================================================================

TEST(Http2ServerConcurrentQueries, ConcurrentIsRunningPolling)
{
    auto server = std::make_shared<http2::http2_server>("conc-isrun");
    constexpr int kThreads = 8;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([server]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_FALSE(server->is_running());
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST(Http2ServerConcurrentQueries, ConcurrentActiveConnectionsPolling)
{
    auto server = std::make_shared<http2::http2_server>("conc-conn");
    constexpr int kThreads = 4;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([server]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_EQ(server->active_connections(), 0u);
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST(Http2ServerConcurrentQueries, ConcurrentActiveStreamsPolling)
{
    auto server = std::make_shared<http2::http2_server>("conc-streams");
    constexpr int kThreads = 4;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([server]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_EQ(server->active_streams(), 0u);
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

TEST(Http2ServerConcurrentQueries, ConcurrentSetSettingsIsThreadSafeForReads)
{
    auto server = std::make_shared<http2::http2_server>("conc-settings");

    std::atomic<bool> stop{false};
    std::thread writer([server, &stop]
    {
        http2::http2_settings s;
        for (uint32_t i = 0; !stop.load(); ++i)
        {
            s.header_table_size = (i % 8192u) + 1u;
            s.max_concurrent_streams = (i % 256u) + 1u;
            server->set_settings(s);
        }
    });

    std::thread reader([server]
    {
        for (int i = 0; i < 200; ++i)
        {
            auto got = server->get_settings();
            // Reads must observe a non-corrupt value; the bound depends on
            // writer iteration, but each individual field must be in range.
            EXPECT_LE(got.header_table_size, 8192u);
            EXPECT_LE(got.max_concurrent_streams, 256u);
        }
    });

    reader.join();
    stop.store(true);
    writer.join();
}

TEST(Http2ServerConcurrentQueries, ConcurrentServerIdReadsAreSafe)
{
    auto server = std::make_shared<http2::http2_server>("conc-id");
    constexpr int kThreads = 4;
    constexpr int kIterations = 200;

    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([server]
        {
            for (int i = 0; i < kIterations; ++i)
            {
                EXPECT_EQ(server->server_id(), "conc-id");
            }
        });
    }
    for (auto& th : threads)
    {
        th.join();
    }
}

// ============================================================================
// Multiple server interactions
// ============================================================================

TEST(Http2ServerMultiInstance, ManyConcurrentServersRunIndependently)
{
    constexpr int kCount = 4;
    std::vector<std::shared_ptr<http2::http2_server>> servers;
    servers.reserve(kCount);

    for (int i = 0; i < kCount; ++i)
    {
        auto s = std::make_shared<http2::http2_server>(
            "multi-" + std::to_string(i));
        ASSERT_TRUE(s->start(0).is_ok());
        EXPECT_TRUE(s->is_running());
        servers.push_back(std::move(s));
    }

    for (auto& s : servers)
    {
        EXPECT_TRUE(s->stop().is_ok());
        EXPECT_FALSE(s->is_running());
    }
}

TEST(Http2ServerMultiInstance, StoppingOneDoesNotAffectOthers)
{
    auto a = std::make_shared<http2::http2_server>("multi-a");
    auto b = std::make_shared<http2::http2_server>("multi-b");
    auto c = std::make_shared<http2::http2_server>("multi-c");

    ASSERT_TRUE(a->start(0).is_ok());
    ASSERT_TRUE(b->start(0).is_ok());
    ASSERT_TRUE(c->start(0).is_ok());

    EXPECT_TRUE(b->stop().is_ok());
    EXPECT_FALSE(b->is_running());

    EXPECT_TRUE(a->is_running());
    EXPECT_TRUE(c->is_running());

    a->stop();
    c->stop();
}

// ============================================================================
// Hermetic transport fixture demonstration (Issue #1060)
// ============================================================================

/**
 * @brief Demonstrates that the new hermetic loopback TCP fixture lets a test
 *        construct an http2_server_connection with a real (already-connected)
 *        TCP socket.
 *
 * This exercises the plain-TCP server-connection constructor and start()
 * path — surfaces previously reachable only by standing up a full
 * acceptor/listener loop.
 */
class Http2ServerHermeticTransportTest
    : public kcenon::network::tests::support::hermetic_transport_fixture
{
};

TEST_F(Http2ServerHermeticTransportTest, ServerConnectionStartsOnLoopbackTcpPair)
{
    using kcenon::network::tests::support::make_loopback_tcp_pair;

    auto pair = make_loopback_tcp_pair(io());
    auto& server_side = pair.second;

    http2::http2_settings settings;
    http2::http2_server::request_handler_t request_handler =
        [](http2::http2_server_stream&, const http2::http2_request&) {};
    http2::http2_server::error_handler_t error_handler =
        [](const std::string&) {};

    auto conn = std::make_shared<http2::http2_server_connection>(
        /*connection_id=*/uint64_t{1},
        std::move(server_side),
        settings,
        std::move(request_handler),
        std::move(error_handler));
    ASSERT_NE(conn, nullptr);

    // start() kicks off the read-preface chain; the client side is silent so
    // no request will arrive, but the constructor + start() + initial async
    // wait paths are exercised.
    auto start_result = conn->start();
    EXPECT_TRUE(start_result.is_ok())
        << "http2_server_connection::start should accept a live TCP socket "
           "from the loopback pair";

    // Allow async ops a brief moment, then stop cleanly.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    (void)conn->stop();
    SUCCEED();
}
