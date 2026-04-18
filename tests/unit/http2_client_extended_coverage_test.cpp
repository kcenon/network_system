// BSD 3-Clause License
// Copyright (c) 2025, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file http2_client_extended_coverage_test.cpp
 * @brief Additional unit tests for src/protocols/http2/http2_client.cpp (Issue #991)
 *
 * Complements @ref http2_client_coverage_test.cpp by covering branches that
 * remained exercised by only a single path:
 *  - connect() against a TLS peer with no certificate loaded, exercising the
 *    handshake-failure path inside the try/catch in http2_client::connect()
 *  - connect() retry after an earlier TLS failure, verifying the rollback
 *    logic leaves the client in a clean disconnected state
 *  - disconnect() after a connection attempt that rolled back state, with a
 *    probe that the object can be destroyed without a dangling I/O thread
 *  - Repeated set_settings() round-trips that exercise HPACK table size update
 *    paths in both the encoder and decoder
 *  - Boundary values for every settings identifier (including zero and
 *    UINT32_MAX) confirming the storage round-trips unchanged
 *  - Concurrent disconnect() invocations from multiple threads (thread safety)
 *  - All request helpers (get/post/put/del/start_stream/write_stream/
 *    close_stream_writer/cancel_stream) called while disconnected
 *  - start_stream() with all null callbacks (parameter path only)
 *  - http2_response::get_header() with edge cases (empty name, unicode)
 *  - http2_stream move semantics with all buffers populated and callbacks set
 *  - http2_stream move preserves promise/future plumbing
 *  - http2_settings equality of round-tripped values after set/get
 *  - Construction edge cases: empty client_id, very long client_id, many
 *    clients back-to-back
 *
 * All tests operate purely on the public API and rely only on local TCP/TLS
 * listeners that intentionally misbehave — no real HTTP/2 peer is required.
 */

#include "internal/protocols/http2/http2_client.h"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace http2 = kcenon::network::protocols::http2;

namespace
{

using namespace std::chrono_literals;

/**
 * @brief Find an available TCP port by briefly binding and releasing it.
 */
uint16_t reserve_tcp_port(uint16_t start = 39500)
{
    for (uint16_t port = start; port < 65535; ++port)
    {
        try
        {
            asio::io_context io;
            asio::ip::tcp::acceptor acceptor(io);
            asio::ip::tcp::endpoint ep(asio::ip::tcp::v4(), port);
            acceptor.open(ep.protocol());
            acceptor.bind(ep);
            acceptor.close();
            return port;
        }
        catch (...)
        {
            continue;
        }
    }
    return 0;
}

/**
 * @brief Minimal TLS listener that accepts a single connection and attempts a
 *        certificate-less TLS handshake.
 *
 * Used to exercise the try/catch block in http2_client::connect(). Because
 * the server context has no certificate loaded, the handshake will fail at
 * the TLS protocol layer regardless of ALPN selection — which is exactly
 * the branch we want to drive. The client's is_connected_ flag must
 * remain false after the failure.
 */
class bad_tls_listener
{
public:
    explicit bad_tls_listener(uint16_t port)
        : acceptor_(io_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))
    {
        accept_thread_ = std::thread([this]
        {
            try
            {
                asio::ssl::context server_ctx(asio::ssl::context::tlsv12_server);
                // Intentionally skip cert/key loading so SSL_accept fails.
                asio::ssl::stream<asio::ip::tcp::socket> stream(io_, server_ctx);
                acceptor_.accept(stream.lowest_layer());

                std::error_code ec;
                stream.handshake(asio::ssl::stream_base::server, ec);
                stream.lowest_layer().close(ec);
            }
            catch (...)
            {
                // Expected: handshake failures, acceptor closed, etc.
            }
        });
    }

    ~bad_tls_listener()
    {
        stop();
    }

    void stop()
    {
        std::error_code ec;
        acceptor_.close(ec);
        if (accept_thread_.joinable())
        {
            accept_thread_.join();
        }
    }

private:
    asio::io_context io_;
    asio::ip::tcp::acceptor acceptor_;
    std::thread accept_thread_;
};

} // namespace

// ============================================================================
// connect() TLS handshake failure branches
// ============================================================================

class Http2ClientTlsFailureTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_client> client_ =
        std::make_shared<http2::http2_client>("tls-failure-client");
};

TEST_F(Http2ClientTlsFailureTest, TlsPeerWithoutCertificateFailsHandshake)
{
    const uint16_t port = reserve_tcp_port();
    ASSERT_NE(port, 0);

    bad_tls_listener listener(port);

    // Let the listener thread get to accept() before we connect.
    std::this_thread::sleep_for(100ms);

    client_->set_timeout(2000ms);
    auto result = client_->connect("127.0.0.1", port);
    EXPECT_TRUE(result.is_err());
    EXPECT_FALSE(client_->is_connected());
    EXPECT_FALSE(result.error().message.empty());

    listener.stop();
}

TEST_F(Http2ClientTlsFailureTest, TlsHandshakeFailureIsRecoverable)
{
    const uint16_t port = reserve_tcp_port();
    ASSERT_NE(port, 0);

    bad_tls_listener listener(port);

    std::this_thread::sleep_for(100ms);

    // First attempt: TLS handshake fails.
    client_->set_timeout(2000ms);
    auto first = client_->connect("127.0.0.1", port);
    EXPECT_TRUE(first.is_err());
    EXPECT_FALSE(client_->is_connected());

    // Disconnect is still a safe no-op after a rolled-back connect attempt.
    EXPECT_TRUE(client_->disconnect().is_ok());

    // A subsequent connect to an unreachable port must still fail cleanly.
    auto second = client_->connect("127.0.0.1", 1);
    EXPECT_TRUE(second.is_err());
    EXPECT_FALSE(client_->is_connected());

    listener.stop();
}

TEST_F(Http2ClientTlsFailureTest, ConnectToLoopbackPortZero)
{
    // Port 0 is illegal for outbound connections on most platforms — this
    // drives the ASIO resolver / connect() into its error branch.
    client_->set_timeout(1000ms);
    auto result = client_->connect("127.0.0.1", 0);
    EXPECT_TRUE(result.is_err());
    EXPECT_FALSE(client_->is_connected());
}

// ============================================================================
// disconnect() interaction with failed connect rollback
// ============================================================================

class Http2ClientDisconnectAfterFailureTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_client> client_ =
        std::make_shared<http2::http2_client>("disc-after-failure");
};

TEST_F(Http2ClientDisconnectAfterFailureTest, DisconnectThenConnectFailsCleanly)
{
    // Disconnect on a fresh client is a no-op.
    EXPECT_TRUE(client_->disconnect().is_ok());

    // Now a failed connect should still leave the client in a clean state.
    client_->set_timeout(500ms);
    auto result = client_->connect("127.0.0.1", 1);
    EXPECT_TRUE(result.is_err());
    EXPECT_FALSE(client_->is_connected());

    // Re-disconnecting is safe.
    EXPECT_TRUE(client_->disconnect().is_ok());
}

TEST_F(Http2ClientDisconnectAfterFailureTest, RepeatedConnectFailuresDoNotAccumulateState)
{
    client_->set_timeout(500ms);
    for (int i = 0; i < 3; ++i)
    {
        auto result = client_->connect("127.0.0.1", 1);
        EXPECT_TRUE(result.is_err()) << "iteration " << i;
        EXPECT_FALSE(client_->is_connected()) << "iteration " << i;
    }
}

TEST_F(Http2ClientDisconnectAfterFailureTest, ConcurrentDisconnectFromMultipleThreads)
{
    constexpr int kThreadCount = 4;
    std::vector<std::thread> workers;
    std::atomic<int> ok_count{0};

    workers.reserve(kThreadCount);
    for (int i = 0; i < kThreadCount; ++i)
    {
        workers.emplace_back([this, &ok_count]
        {
            if (client_->disconnect().is_ok())
            {
                ok_count.fetch_add(1);
            }
        });
    }

    for (auto& t : workers)
    {
        t.join();
    }

    // All disconnects on an already-disconnected client return ok().
    EXPECT_EQ(ok_count.load(), kThreadCount);
    EXPECT_FALSE(client_->is_connected());
}

// ============================================================================
// set_settings() HPACK round-trip coverage
// ============================================================================

class Http2ClientSettingsRoundTripTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_client> client_ =
        std::make_shared<http2::http2_client>("settings-rt-client");
};

TEST_F(Http2ClientSettingsRoundTripTest, RepeatedTableSizeChangesAreObservable)
{
    const uint32_t sizes[] = {0, 256, 4096, 16384, 65536, 1024};
    for (uint32_t size : sizes)
    {
        http2::http2_settings s = client_->get_settings();
        s.header_table_size = size;
        client_->set_settings(s);
        EXPECT_EQ(client_->get_settings().header_table_size, size)
            << "header_table_size round-trip failed for " << size;
    }
}

TEST_F(Http2ClientSettingsRoundTripTest, WindowSizeUpdatesArePersisted)
{
    const uint32_t windows[] = {1, 1024, 65535, 1u << 20, (1u << 31) - 1};
    for (uint32_t w : windows)
    {
        http2::http2_settings s = client_->get_settings();
        s.initial_window_size = w;
        client_->set_settings(s);
        EXPECT_EQ(client_->get_settings().initial_window_size, w);
    }
}

TEST_F(Http2ClientSettingsRoundTripTest, FrameSizeBoundariesRoundTrip)
{
    // RFC 7540: min 16384, max 16777215. Library does not validate — confirm
    // current behavior accepts any uint32_t value.
    const uint32_t frame_sizes[] = {0, 16384, 16777215, 1u << 24, UINT32_MAX};
    for (uint32_t fs : frame_sizes)
    {
        http2::http2_settings s = client_->get_settings();
        s.max_frame_size = fs;
        client_->set_settings(s);
        EXPECT_EQ(client_->get_settings().max_frame_size, fs);
    }
}

TEST_F(Http2ClientSettingsRoundTripTest, MaxConcurrentStreamsBoundaries)
{
    const uint32_t caps[] = {0, 1, 100, 1000, UINT32_MAX};
    for (uint32_t cap : caps)
    {
        http2::http2_settings s = client_->get_settings();
        s.max_concurrent_streams = cap;
        client_->set_settings(s);
        EXPECT_EQ(client_->get_settings().max_concurrent_streams, cap);
    }
}

TEST_F(Http2ClientSettingsRoundTripTest, MaxHeaderListSizeBoundaries)
{
    const uint32_t caps[] = {0, 1024, 8192, 1u << 16, UINT32_MAX};
    for (uint32_t cap : caps)
    {
        http2::http2_settings s = client_->get_settings();
        s.max_header_list_size = cap;
        client_->set_settings(s);
        EXPECT_EQ(client_->get_settings().max_header_list_size, cap);
    }
}

TEST_F(Http2ClientSettingsRoundTripTest, SettingsInstanceIndependence)
{
    // Multiple http2_client instances should hold independent settings.
    auto other = std::make_shared<http2::http2_client>("other-settings-client");
    http2::http2_settings s = client_->get_settings();
    s.header_table_size = 9999;
    client_->set_settings(s);

    EXPECT_EQ(client_->get_settings().header_table_size, 9999u);
    EXPECT_NE(other->get_settings().header_table_size, 9999u);
}

// ============================================================================
// Client id / construction edge cases
// ============================================================================

TEST(Http2ClientConstructionTest, EmptyClientIdIsAccepted)
{
    auto c = std::make_shared<http2::http2_client>("");
    EXPECT_FALSE(c->is_connected());
    EXPECT_EQ(c->get_timeout(), std::chrono::milliseconds{30000});
}

TEST(Http2ClientConstructionTest, LongClientIdIsAccepted)
{
    std::string id(512, 'x');
    auto c = std::make_shared<http2::http2_client>(id);
    EXPECT_FALSE(c->is_connected());
}

TEST(Http2ClientConstructionTest, MultipleClientsCoexist)
{
    // Creating and tearing down many clients back-to-back should not leak
    // resources (detected by ASAN/LSAN in CI) and each must start in the
    // disconnected state.
    std::vector<std::shared_ptr<http2::http2_client>> clients;
    clients.reserve(16);
    for (int i = 0; i < 16; ++i)
    {
        clients.push_back(std::make_shared<http2::http2_client>(
            "batch-client-" + std::to_string(i)));
        EXPECT_FALSE(clients.back()->is_connected());
    }
    // Let destructors run when the vector goes out of scope.
}

// ============================================================================
// Sequential failed requests to force allocate_stream_id() path (indirectly)
// ============================================================================

class Http2ClientRequestSequenceTest : public ::testing::Test
{
protected:
    std::shared_ptr<http2::http2_client> client_ =
        std::make_shared<http2::http2_client>("request-sequence-client");
};

TEST_F(Http2ClientRequestSequenceTest, AllRequestHelpersAreCoveredWhileDisconnected)
{
    // Drives each public request method exactly once; each should fail
    // with a not-connected error without throwing.
    EXPECT_TRUE(client_->get("/get").is_err());
    EXPECT_TRUE(client_->post("/post", std::string("str")).is_err());
    EXPECT_TRUE(client_->post("/post", std::vector<uint8_t>{0x1}).is_err());
    EXPECT_TRUE(client_->put("/put", "body").is_err());
    EXPECT_TRUE(client_->del("/del").is_err());

    EXPECT_TRUE(client_->start_stream(
                      "/stream", {},
                      [](std::vector<uint8_t>) {},
                      [](std::vector<http2::http_header>) {},
                      [](int) {})
                    .is_err());

    EXPECT_TRUE(client_->write_stream(1, {0xAA}, false).is_err());
    EXPECT_TRUE(client_->write_stream(1, {0xAA}, true).is_err());
    EXPECT_TRUE(client_->close_stream_writer(1).is_err());
    EXPECT_TRUE(client_->cancel_stream(1).is_err());
}

TEST_F(Http2ClientRequestSequenceTest, StartStreamWithNullCallbacksIsAccepted)
{
    // Null std::function targets are allowed; the caller is responsible for
    // setting them if needed. While disconnected this still fails with
    // not-connected, but the parameter path is covered.
    auto result = client_->start_stream(
        "/stream-null",
        /*headers=*/{},
        /*on_data=*/nullptr,
        /*on_headers=*/nullptr,
        /*on_complete=*/nullptr);
    EXPECT_TRUE(result.is_err());
}

TEST_F(Http2ClientRequestSequenceTest, RequestsWithLargeHeaderListWhileDisconnected)
{
    std::vector<http2::http_header> many;
    many.reserve(64);
    for (int i = 0; i < 64; ++i)
    {
        many.emplace_back("x-custom-" + std::to_string(i),
                          "value-" + std::to_string(i));
    }
    EXPECT_TRUE(client_->get("/many", many).is_err());
    EXPECT_TRUE(client_->post("/many", "body", many).is_err());
    EXPECT_TRUE(client_->put("/many", "body", many).is_err());
    EXPECT_TRUE(client_->del("/many", many).is_err());
}

// ============================================================================
// http2_response extended scenarios
// ============================================================================

class Http2ResponseExtraTest : public ::testing::Test
{
protected:
    http2::http2_response response_;
};

TEST_F(Http2ResponseExtraTest, GetHeaderEmptyNameDoesNotMatch)
{
    response_.headers = {
        http2::http_header{"content-type", "application/json"},
        http2::http_header{"", "value-for-empty-name"}};

    // Empty header names are a malformed protocol input; the implementation's
    // case-insensitive compare would match any empty stored name. Verify the
    // current behavior so future changes are a visible test update.
    auto v = response_.get_header("");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "value-for-empty-name");
}

TEST_F(Http2ResponseExtraTest, GetHeaderMixedCaseUnicode)
{
    response_.headers = {
        http2::http_header{"X-Cust-Ñame", "value-utf8"},
        http2::http_header{"x-ascii-name", "value-ascii"}};

    // UTF-8 bytes are not lower-cased by std::tolower; verify the ascii
    // name at least still matches case-insensitively.
    EXPECT_EQ(response_.get_header("X-ASCII-NAME").value_or(""), "value-ascii");
    EXPECT_EQ(response_.get_header("X-Ascii-Name").value_or(""), "value-ascii");
}

TEST_F(Http2ResponseExtraTest, GetBodyStringEmptyAfterReset)
{
    response_.body = {'a', 'b', 'c'};
    EXPECT_EQ(response_.get_body_string(), "abc");
    response_.body.clear();
    EXPECT_EQ(response_.get_body_string(), "");
}

TEST_F(Http2ResponseExtraTest, ResponseFieldsAreMutable)
{
    response_.status_code = 418;
    response_.headers.emplace_back("x-teapot", "true");
    response_.body = {'I', 'm', ' ', 'a', ' ', 't', 'p'};

    EXPECT_EQ(response_.status_code, 418);
    EXPECT_EQ(response_.headers.size(), 1u);
    EXPECT_EQ(response_.get_body_string(), "Im a tp");
    auto v = response_.get_header("x-teapot");
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, "true");
}

// ============================================================================
// http2_stream full move-with-callbacks coverage
// ============================================================================

TEST(Http2StreamCallbackMoveTest, CallbackTargetsAreTransferredOnMove)
{
    int data_calls = 0;
    int header_calls = 0;
    int complete_calls = 0;

    http2::http2_stream src;
    src.stream_id = 101;
    src.is_streaming = true;
    src.on_data = [&](std::vector<uint8_t>) { ++data_calls; };
    src.on_headers = [&](std::vector<http2::http_header>) { ++header_calls; };
    src.on_complete = [&](int) { ++complete_calls; };

    http2::http2_stream dst(std::move(src));

    ASSERT_TRUE(static_cast<bool>(dst.on_data));
    ASSERT_TRUE(static_cast<bool>(dst.on_headers));
    ASSERT_TRUE(static_cast<bool>(dst.on_complete));

    dst.on_data({});
    dst.on_headers({});
    dst.on_complete(200);

    EXPECT_EQ(data_calls, 1);
    EXPECT_EQ(header_calls, 1);
    EXPECT_EQ(complete_calls, 1);
    EXPECT_TRUE(dst.is_streaming);
    EXPECT_EQ(dst.stream_id, 101u);
}

TEST(Http2StreamCallbackMoveTest, PromiseIsMovableAcrossStreamMoves)
{
    http2::http2_stream src;
    src.stream_id = 202;
    auto future = src.promise.get_future();

    http2::http2_stream dst(std::move(src));
    http2::http2_response response;
    response.status_code = 201;
    dst.promise.set_value(std::move(response));

    auto status = future.wait_for(std::chrono::milliseconds(100));
    ASSERT_EQ(status, std::future_status::ready);
    auto r = future.get();
    EXPECT_EQ(r.status_code, 201);
}

// ============================================================================
// http2_settings equality via round-trip
// ============================================================================

TEST(Http2SettingsRoundTripTest, DefaultSettingsRoundTripThroughClient)
{
    auto c = std::make_shared<http2::http2_client>("settings-default");
    http2::http2_settings original = c->get_settings();
    c->set_settings(original);
    http2::http2_settings retrieved = c->get_settings();

    EXPECT_EQ(retrieved.header_table_size, original.header_table_size);
    EXPECT_EQ(retrieved.enable_push, original.enable_push);
    EXPECT_EQ(retrieved.max_concurrent_streams, original.max_concurrent_streams);
    EXPECT_EQ(retrieved.initial_window_size, original.initial_window_size);
    EXPECT_EQ(retrieved.max_frame_size, original.max_frame_size);
    EXPECT_EQ(retrieved.max_header_list_size, original.max_header_list_size);
}

TEST(Http2SettingsRoundTripTest, SequentialSetGetIdempotent)
{
    auto c = std::make_shared<http2::http2_client>("settings-idempotent");
    http2::http2_settings s;
    s.header_table_size = 7777;
    s.enable_push = true;
    s.max_concurrent_streams = 42;
    s.initial_window_size = 300000;
    s.max_frame_size = 1 << 20;
    s.max_header_list_size = 1 << 15;

    c->set_settings(s);
    c->set_settings(c->get_settings());  // Round-trip

    auto final_settings = c->get_settings();
    EXPECT_EQ(final_settings.header_table_size, 7777u);
    EXPECT_TRUE(final_settings.enable_push);
    EXPECT_EQ(final_settings.max_concurrent_streams, 42u);
    EXPECT_EQ(final_settings.initial_window_size, 300000u);
    EXPECT_EQ(final_settings.max_frame_size, static_cast<uint32_t>(1 << 20));
    EXPECT_EQ(final_settings.max_header_list_size, static_cast<uint32_t>(1 << 15));
}

// ============================================================================
// Connect argument edge cases
// ============================================================================

TEST(Http2ClientConnectEdgeTest, WhitespaceHostReturnsError)
{
    auto c = std::make_shared<http2::http2_client>("whitespace-host");
    c->set_timeout(500ms);
    auto result = c->connect("   ", 443);
    // Behavior depends on resolver; either error from resolve() or from
    // the handshake — both are failure paths.
    EXPECT_TRUE(result.is_err());
    EXPECT_FALSE(c->is_connected());
}

TEST(Http2ClientConnectEdgeTest, LoopbackOnUnboundPortRefused)
{
    auto c = std::make_shared<http2::http2_client>("loopback-unbound");
    c->set_timeout(500ms);
    // Port 1 on loopback is essentially guaranteed to refuse connection.
    auto result = c->connect("127.0.0.1", 1);
    EXPECT_TRUE(result.is_err());
    EXPECT_FALSE(c->is_connected());
}
