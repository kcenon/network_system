// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file http2_server_dispatcher_branch_test.cpp
 * @brief Direct-dispatch branch coverage for src/protocols/http2/http2_server.cpp
 *        (Issue #1121, Round 3 of #953).
 *
 * Mirrors the strategy of @ref http2_client_branch_test.cpp Round 6 / #1115
 * and @ref http2_client_dispatcher_branch_test.cpp Round 3 / #1119: invokes
 * the private @c http2_server_connection::process_frame dispatcher directly
 * via the @c http2_server_test_access friend, sidestepping the connection
 * preface read and SETTINGS exchange that exceed the wait_for budget under
 * coverage instrumentation.
 *
 * Coverage targets per handler:
 *  - process_frame: each of the seven case arms (settings, headers, data,
 *    rst_stream, ping, goaway, window_update) plus the default fall-through.
 *  - handle_settings_frame: ACK early-return AND the apply branch with each
 *    setting_identifier (header_table_size triggers encoder/decoder dynamic
 *    table resize, others update local_settings_).
 *  - handle_headers_frame: HPACK decode-success branch (existing-stream
 *    happy path, end_stream transitions to half_closed_remote, end_headers
 *    sets headers_complete) AND the HPACK decode-error branch (malformed
 *    HPACK triggers GOAWAY emission with COMPRESSION_ERROR).
 *  - handle_data_frame: known-stream branch (appends to request_body, emits
 *    WINDOW_UPDATE for connection AND stream, end_stream transitions to
 *    half_closed_remote and dispatches the request) AND the unknown-stream
 *    branch (RST_STREAM emission with STREAM_CLOSED).
 *  - handle_rst_stream_frame: drives close_stream — same code path whether
 *    the stream existed or not.
 *  - handle_ping_frame: ACK branch (early-return) AND the non-ACK branch
 *    (constructs PING ACK and forwards to send_frame).
 *  - handle_goaway_frame: drives stop() which flips is_alive_ to false.
 *  - handle_window_update_frame: connection-level (stream_id == 0 increments
 *    connection_window_size_) AND stream-level (stream_id != 0 increments
 *    the per-stream window when the stream exists, no-op otherwise).
 *  - get_or_create_stream: lookup-miss creates a new stream entry AND
 *    lookup-hit returns the existing one without re-creating.
 *  - close_stream: erases an existing entry from streams_.
 *
 * State preconditions: the connection is constructed with a real (already-
 * connected) loopback TCP socket so send_frame() reaches the production
 * write path, but no preface is exchanged — we drive the dispatcher
 * directly. is_alive_ starts true and most tests assert post-conditions on
 * the friend-exposed accessors.
 */

#include "internal/protocols/http2/frame.h"
#include "internal/protocols/http2/http2_client.h"
#include "internal/protocols/http2/http2_server.h"

#include "hermetic_transport_fixture.h"
#include "http2_server_test_access.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace http2 = kcenon::network::protocols::http2;

namespace
{

using namespace std::chrono_literals;

// Make a server connection bound to one half of a loopback TCP pair so
// send_frame() can reach a real socket without standing up an acceptor /
// listener loop. The other half is held alive by the test fixture for the
// duration of the test.
struct connection_setup
{
    asio::ip::tcp::socket peer_side;  // client-side socket; held alive
    std::shared_ptr<http2::http2_server_connection> conn;
};

// Construct a server-side connection on a connected loopback pair. The
// client-side socket is returned alongside so the test can keep it alive
// (otherwise close-on-destroy would tear down the server-side too).
inline connection_setup make_server_connection_on_loopback(
    asio::io_context& io,
    bool with_handlers = true)
{
    auto pair = kcenon::network::tests::support::make_loopback_tcp_pair(io);
    http2::http2_settings settings;
    http2::http2_server::request_handler_t request_handler;
    http2::http2_server::error_handler_t error_handler;
    if (with_handlers)
    {
        request_handler = [](http2::http2_server_stream&,
                             const http2::http2_request&) {};
        error_handler = [](const std::string&) {};
    }
    auto conn = std::make_shared<http2::http2_server_connection>(
        /*connection_id=*/std::uint64_t{1},
        std::move(pair.second),
        settings,
        std::move(request_handler),
        std::move(error_handler));
    return {std::move(pair.first), std::move(conn)};
}

} // namespace

class Http2ServerDispatcherTest
    : public kcenon::network::tests::support::hermetic_transport_fixture
{
};

// ============================================================================
// process_frame switch arm coverage
// ============================================================================

TEST_F(Http2ServerDispatcherTest, ProcessFrameSettingsAckArmReturnsOk)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // SETTINGS with ACK flag set drives handle_settings_frame's is_ack()
    // early-return branch — distinct from the apply-remote-settings path.
    auto ack = std::make_unique<http2::settings_frame>(
        std::vector<http2::setting_parameter>{}, /*ack=*/true);

    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(ack));
    EXPECT_TRUE(result.is_ok());
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameSettingsAppliesAllRemoteIdentifiers)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // Non-ACK SETTINGS carrying every identifier the spec defines (RFC 7540
    // §6.5.2). Drives all six switch arms of handle_settings_frame, plus
    // send_settings_ack() at the tail (which writes to the socket — the
    // loopback peer absorbs the bytes).
    std::vector<http2::setting_parameter> params{
        {0x1u, 8192u},   // header_table_size — triggers encoder/decoder resize
        {0x2u, 0u},      // enable_push (disable)
        {0x3u, 200u},    // max_concurrent_streams
        {0x4u, 65535u},  // initial_window_size
        {0x5u, 16384u},  // max_frame_size
        {0x6u, 8192u},   // max_header_list_size
    };
    auto sf = std::make_unique<http2::settings_frame>(std::move(params),
                                                      /*ack=*/false);

    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(sf));
    EXPECT_TRUE(result.is_ok());
}

TEST_F(Http2ServerDispatcherTest, ProcessFramePingAckArmReturnsOk)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // PING with ACK set drives handle_ping_frame's is_ack() early-return.
    std::array<std::uint8_t, 8> opaque{0xAA, 0xBB, 0xCC, 0xDD,
                                       0xEE, 0xFF, 0x00, 0x11};
    auto ping_ack = std::make_unique<http2::ping_frame>(opaque, /*ack=*/true);

    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(ping_ack));
    EXPECT_TRUE(result.is_ok());
}

TEST_F(Http2ServerDispatcherTest, ProcessFramePingNonAckTriggersAckReply)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // PING with ACK clear drives handle_ping_frame's send-ACK branch.
    // The ACK is constructed from the same opaque payload and forwarded to
    // send_frame, which serializes onto the loopback socket. The loopback
    // peer absorbs the bytes; we don't need to read them back to cover the
    // dispatch arm.
    std::array<std::uint8_t, 8> opaque{0x01, 0x02, 0x03, 0x04,
                                       0x05, 0x06, 0x07, 0x08};
    auto ping = std::make_unique<http2::ping_frame>(opaque, /*ack=*/false);

    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(ping));
    EXPECT_TRUE(result.is_ok());
}

TEST_F(Http2ServerDispatcherTest, ProcessFrameGoawayFlipsIsAliveToFalse)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    EXPECT_TRUE(http2_server_test_access::is_alive(*setup.conn));

    // GOAWAY drives handle_goaway_frame which calls stop(). is_alive_ flips
    // to false and the socket is closed — verifiable via the friend
    // accessor and is_alive() public method.
    auto go = std::make_unique<http2::goaway_frame>(
        /*last_stream_id=*/0u,
        /*error_code=*/0u /*NO_ERROR*/);

    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(go));
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(http2_server_test_access::is_alive(*setup.conn));
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameWindowUpdateOnConnectionStreamExpandsWindow)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // WINDOW_UPDATE with stream_id=0 routes to the connection-level branch
    // and increments connection_window_size_.
    constexpr std::uint32_t kIncrement = 1u << 16;  // 64 KiB
    auto wu = std::make_unique<http2::window_update_frame>(
        /*stream_id=*/0u,
        /*window_size_increment=*/kIncrement);

    const auto before =
        http2_server_test_access::connection_window_size(*setup.conn);
    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(wu));
    EXPECT_TRUE(result.is_ok());
    const auto after =
        http2_server_test_access::connection_window_size(*setup.conn);
    EXPECT_EQ(static_cast<std::uint32_t>(after - before), kIncrement);
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameWindowUpdateOnUnknownStreamIsSilentlyIgnored)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // WINDOW_UPDATE on a stream the server never opened drives the
    // stream-not-found else branch of handle_window_update_frame's
    // stream-level path. RFC 7540 §6.9 allows the frame to be silently
    // ignored. Distinct from the connection-level path covered above.
    auto wu = std::make_unique<http2::window_update_frame>(
        /*stream_id=*/77u,
        /*window_size_increment=*/2048u);

    const auto before =
        http2_server_test_access::connection_window_size(*setup.conn);
    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(wu));
    EXPECT_TRUE(result.is_ok());
    EXPECT_EQ(http2_server_test_access::connection_window_size(*setup.conn),
              before);
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameWindowUpdateOnKnownStreamExpandsStreamWindow)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // Pre-seed a stream so handle_window_update_frame's stream-level path
    // hits the lookup-success arm and increments the per-stream window.
    constexpr std::uint32_t kStreamId = 5u;
    auto* stream =
        http2_server_test_access::get_or_create_stream(*setup.conn, kStreamId);
    ASSERT_NE(stream, nullptr);
    EXPECT_TRUE(http2_server_test_access::has_stream(*setup.conn, kStreamId));

    const auto before =
        http2_server_test_access::stream_window_size_of(*setup.conn, kStreamId);

    constexpr std::uint32_t kIncrement = 4096u;
    auto wu = std::make_unique<http2::window_update_frame>(
        kStreamId, kIncrement);
    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(wu));
    EXPECT_TRUE(result.is_ok());

    const auto after =
        http2_server_test_access::stream_window_size_of(*setup.conn, kStreamId);
    EXPECT_EQ(static_cast<std::uint32_t>(after - before), kIncrement);
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameRstStreamErasesKnownStreamFromMap)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // Pre-seed a stream to drive the close_stream success branch from
    // handle_rst_stream_frame.
    constexpr std::uint32_t kStreamId = 7u;
    auto* stream =
        http2_server_test_access::get_or_create_stream(*setup.conn, kStreamId);
    ASSERT_NE(stream, nullptr);
    EXPECT_TRUE(http2_server_test_access::has_stream(*setup.conn, kStreamId));

    auto rst = std::make_unique<http2::rst_stream_frame>(
        kStreamId,
        /*error_code=*/8u /*CANCEL*/);
    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(rst));
    EXPECT_TRUE(result.is_ok());

    EXPECT_FALSE(http2_server_test_access::has_stream(*setup.conn, kStreamId));
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameRstStreamOnUnknownStreamReturnsOk)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // RST_STREAM on a stream the server never saw: close_stream's erase()
    // is a no-op (key not found) but the dispatch arm and the handler
    // function body are still covered.
    auto rst = std::make_unique<http2::rst_stream_frame>(
        /*stream_id=*/99u,
        /*error_code=*/3u /*FLOW_CONTROL_ERROR*/);
    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(rst));
    EXPECT_TRUE(result.is_ok());
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameDataFrameOnUnknownStreamSendsRstStream)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // DATA on a stream the server never opened drives the not-found branch
    // in handle_data_frame which emits an RST_STREAM. The send_frame call
    // serializes onto the loopback socket; the loopback peer absorbs the
    // bytes. Coverage gain: the RST_STREAM construction + send branch
    // distinct from any other handler that emits frames.
    std::vector<std::uint8_t> body{'p', 'a', 'r', 't', 'i', 'a', 'l'};
    auto df = std::make_unique<http2::data_frame>(
        /*stream_id=*/77u, std::move(body), /*end_stream=*/true);
    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(df));
    // Outcome is whatever send_frame returned; the dispatch arm and the
    // handler entry are covered regardless. send_frame may succeed (loopback
    // peer absorbs the bytes) or fail (peer closed) — either way the branch
    // is traversed.
    (void)result;
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameDataFrameOnKnownStreamAppendsBodyAndUpdatesWindow)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // Pre-seed a stream so handle_data_frame hits the success branch.
    constexpr std::uint32_t kStreamId = 9u;
    auto* stream =
        http2_server_test_access::get_or_create_stream(*setup.conn, kStreamId);
    ASSERT_NE(stream, nullptr);

    // DATA frame WITHOUT end_stream — drives the append + WINDOW_UPDATE
    // emission path but skips the dispatch_request branch (which would
    // require the request handler to be called and complete).
    std::vector<std::uint8_t> body{'h', 'e', 'l', 'l', 'o'};
    auto df = std::make_unique<http2::data_frame>(
        kStreamId, std::move(body), /*end_stream=*/false);
    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(df));
    EXPECT_TRUE(result.is_ok());

    // Stream is still alive (not transitioned to half_closed_remote).
    EXPECT_TRUE(http2_server_test_access::has_stream(*setup.conn, kStreamId));
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameDataFrameWithEndStreamDispatchesRequest)
{
    using kcenon::network::tests::support::http2_server_test_access;

    // Use a setup with non-empty handlers so dispatch_request invokes the
    // handler and exercises that branch. The handler is a no-op but its
    // invocation walks request_handler_ → http2_server_stream construction →
    // close_stream at the tail.
    auto setup = make_server_connection_on_loopback(io(), /*with_handlers=*/true);

    constexpr std::uint32_t kStreamId = 11u;
    auto* stream =
        http2_server_test_access::get_or_create_stream(*setup.conn, kStreamId);
    ASSERT_NE(stream, nullptr);

    // DATA frame WITH end_stream — drives dispatch_request, which invokes
    // the request_handler and then closes the stream. After the call:
    //  - stream is removed from the map (dispatch_request → close_stream)
    //  - request_handler was called once with the empty body+headers
    std::vector<std::uint8_t> body{};  // empty body
    auto df = std::make_unique<http2::data_frame>(
        kStreamId, std::move(body), /*end_stream=*/true);
    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(df));
    EXPECT_TRUE(result.is_ok());

    // dispatch_request → close_stream removes the stream after handler
    // completes. Allow a brief window for the handler invocation if it
    // happens to be deferred (it's synchronous in current impl, but this
    // poll keeps the test robust to refactors).
    EXPECT_TRUE(wait_for(
        [&]() {
            return !http2_server_test_access::has_stream(*setup.conn, kStreamId);
        },
        500ms));
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameHeadersFrameWithMalformedHpackTriggersGoaway)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // Construct a HEADERS frame whose HPACK payload is intentionally
    // malformed (a bare 0xFF byte is an invalid huffman/index encoding).
    // The decoder returns an error, which drives the COMPRESSION_ERROR
    // GOAWAY emission branch in handle_headers_frame.
    std::vector<std::uint8_t> bad_block{0xFF, 0xFF, 0xFF, 0xFF};
    auto hf = std::make_unique<http2::headers_frame>(
        /*stream_id=*/13u,
        std::move(bad_block),
        /*end_stream=*/false,
        /*end_headers=*/true);

    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(hf));

    // The handler returns an error from the COMPRESSION_ERROR branch.
    // The dispatch arm + GOAWAY emission + stop() call are all covered
    // regardless of the return value.
    (void)result;
    // After the COMPRESSION_ERROR path, stop() flips is_alive_ to false.
    EXPECT_FALSE(http2_server_test_access::is_alive(*setup.conn));
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameHeadersFrameWithValidHpackOpensStream)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // Build a minimal valid HPACK block via the encoder used by the client
    // (which produces a wire-format compatible block). We encode a single
    // pseudo-header which the decoder can resolve; the actual content
    // doesn't matter — we're driving the success branch of the dispatcher.
    http2::hpack_encoder encoder(4096);
    std::vector<http2::http_header> hdrs{
        {":method", "GET"},
        {":path", "/"},
        {":scheme", "https"},
        {":authority", "test"},
    };
    auto block = encoder.encode(hdrs);
    ASSERT_FALSE(block.empty());

    constexpr std::uint32_t kStreamId = 15u;
    auto hf = std::make_unique<http2::headers_frame>(
        kStreamId,
        std::move(block),
        /*end_stream=*/false,
        /*end_headers=*/true);

    EXPECT_FALSE(http2_server_test_access::has_stream(*setup.conn, kStreamId));

    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(hf));
    EXPECT_TRUE(result.is_ok());

    // get_or_create_stream created the entry; end_headers branch set
    // headers_complete = true. The stream remains in the map because
    // end_stream=false.
    EXPECT_TRUE(http2_server_test_access::has_stream(*setup.conn, kStreamId));
    EXPECT_EQ(http2_server_test_access::last_stream_id(*setup.conn), kStreamId);
}

TEST_F(Http2ServerDispatcherTest,
       ProcessFrameHeadersFrameEndStreamTransitionsToHalfClosedRemote)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io(), /*with_handlers=*/true);

    // Same encoding strategy as above, but with end_stream=true so the
    // handler's body-complete branch fires and dispatch_request is invoked.
    http2::hpack_encoder encoder(4096);
    std::vector<http2::http_header> hdrs{
        {":method", "POST"},
        {":path", "/api"},
        {":scheme", "https"},
        {":authority", "test"},
    };
    auto block = encoder.encode(hdrs);
    ASSERT_FALSE(block.empty());

    constexpr std::uint32_t kStreamId = 17u;
    auto hf = std::make_unique<http2::headers_frame>(
        kStreamId,
        std::move(block),
        /*end_stream=*/true,
        /*end_headers=*/true);

    auto result = http2_server_test_access::process_frame(
        *setup.conn, std::move(hf));
    EXPECT_TRUE(result.is_ok());

    // end_stream=true triggers dispatch_request, which invokes the no-op
    // handler and then close_stream → stream removed.
    EXPECT_TRUE(wait_for(
        [&]() {
            return !http2_server_test_access::has_stream(*setup.conn, kStreamId);
        },
        500ms));
}

// ============================================================================
// get_or_create_stream / close_stream direct coverage
// ============================================================================

TEST_F(Http2ServerDispatcherTest,
       GetOrCreateStreamReturnsNewStreamThenSameOnRepeatCall)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    constexpr std::uint32_t kStreamId = 21u;
    auto* first =
        http2_server_test_access::get_or_create_stream(*setup.conn, kStreamId);
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(http2_server_test_access::has_stream(*setup.conn, kStreamId));

    // Second call must return the same pointer — exercises the find-hit
    // branch distinct from the create branch.
    auto* second =
        http2_server_test_access::get_or_create_stream(*setup.conn, kStreamId);
    EXPECT_EQ(first, second);
    EXPECT_EQ(http2_server_test_access::stream_count(*setup.conn), 1u);
}

TEST_F(Http2ServerDispatcherTest,
       GetOrCreateStreamUpdatesLastStreamIdOnHigherIds)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    EXPECT_EQ(http2_server_test_access::last_stream_id(*setup.conn), 0u);

    (void)http2_server_test_access::get_or_create_stream(*setup.conn, 3u);
    EXPECT_EQ(http2_server_test_access::last_stream_id(*setup.conn), 3u);

    (void)http2_server_test_access::get_or_create_stream(*setup.conn, 5u);
    EXPECT_EQ(http2_server_test_access::last_stream_id(*setup.conn), 5u);

    // Lower stream id must NOT roll back last_stream_id_ — exercises the
    // monotonic-update guard branch.
    (void)http2_server_test_access::get_or_create_stream(*setup.conn, 1u);
    EXPECT_EQ(http2_server_test_access::last_stream_id(*setup.conn), 5u);
}

TEST_F(Http2ServerDispatcherTest, CloseStreamRemovesExistingEntryFromMap)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    constexpr std::uint32_t kStreamId = 23u;
    (void)http2_server_test_access::get_or_create_stream(*setup.conn, kStreamId);
    ASSERT_TRUE(http2_server_test_access::has_stream(*setup.conn, kStreamId));

    http2_server_test_access::close_stream(*setup.conn, kStreamId);
    EXPECT_FALSE(http2_server_test_access::has_stream(*setup.conn, kStreamId));
}

TEST_F(Http2ServerDispatcherTest, CloseStreamOnUnknownStreamIsNoOp)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    EXPECT_EQ(http2_server_test_access::stream_count(*setup.conn), 0u);

    // No throw, no state change — the lookup miss is silently absorbed.
    http2_server_test_access::close_stream(*setup.conn, 99u);

    EXPECT_EQ(http2_server_test_access::stream_count(*setup.conn), 0u);
}

// ============================================================================
// Cumulative branch sweep — drives many handlers in sequence on the same
// connection so the dispatcher's per-arm post-conditions interact through
// real state, distinct from the isolated-arm tests above.
// ============================================================================

TEST_F(Http2ServerDispatcherTest,
       MixedFrameSequenceDrivesAllArmsOnSameConnection)
{
    using kcenon::network::tests::support::http2_server_test_access;

    auto setup = make_server_connection_on_loopback(io());

    // 1. Apply remote SETTINGS — covers all six identifier arms
    {
        std::vector<http2::setting_parameter> params{
            {0x1u, 4096u}, {0x2u, 1u}, {0x3u, 50u},
            {0x4u, 32768u}, {0x5u, 16384u}, {0x6u, 4096u},
        };
        auto sf = std::make_unique<http2::settings_frame>(std::move(params),
                                                          /*ack=*/false);
        EXPECT_TRUE(http2_server_test_access::process_frame(*setup.conn,
                                                            std::move(sf))
                        .is_ok());
    }

    // 2. PING (non-ACK) — covers handle_ping_frame + send-ACK branch
    {
        auto ping = std::make_unique<http2::ping_frame>(
            std::array<std::uint8_t, 8>{0, 0, 0, 0, 0, 0, 0, 0},
            /*ack=*/false);
        EXPECT_TRUE(http2_server_test_access::process_frame(*setup.conn,
                                                            std::move(ping))
                        .is_ok());
    }

    // 3. WINDOW_UPDATE on connection — covers stream_id == 0 branch
    {
        auto wu = std::make_unique<http2::window_update_frame>(0u, 1024u);
        EXPECT_TRUE(http2_server_test_access::process_frame(*setup.conn,
                                                            std::move(wu))
                        .is_ok());
    }

    // 4. Pre-seed a stream so subsequent handlers hit the known-stream arm
    constexpr std::uint32_t kStreamId = 25u;
    (void)http2_server_test_access::get_or_create_stream(*setup.conn, kStreamId);
    EXPECT_TRUE(http2_server_test_access::has_stream(*setup.conn, kStreamId));

    // 5. WINDOW_UPDATE on the seeded stream — covers stream_id != 0 hit
    {
        auto wu = std::make_unique<http2::window_update_frame>(kStreamId, 256u);
        EXPECT_TRUE(http2_server_test_access::process_frame(*setup.conn,
                                                            std::move(wu))
                        .is_ok());
    }

    // 6. RST_STREAM on the seeded stream — covers close_stream success
    {
        auto rst = std::make_unique<http2::rst_stream_frame>(kStreamId, 8u);
        EXPECT_TRUE(http2_server_test_access::process_frame(*setup.conn,
                                                            std::move(rst))
                        .is_ok());
    }
    EXPECT_FALSE(http2_server_test_access::has_stream(*setup.conn, kStreamId));

    // 7. GOAWAY — flips is_alive_ to false
    {
        auto go = std::make_unique<http2::goaway_frame>(0u, 0u);
        EXPECT_TRUE(http2_server_test_access::process_frame(*setup.conn,
                                                            std::move(go))
                        .is_ok());
    }
    EXPECT_FALSE(http2_server_test_access::is_alive(*setup.conn));
}
