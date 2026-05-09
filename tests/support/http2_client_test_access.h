// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file http2_client_test_access.h
 * @brief Friend-accessor for hermetic dispatcher coverage (Issue #1115).
 *
 * Reuses the existing Phase 2D friend gate @c NETWORK_ENABLE_TEST_INJECTION
 * (defined PUBLIC on @c network_system when @c BUILD_TESTS=ON, see
 * @c cmake/network_system_targets.cmake). Production builds with
 * @c BUILD_TESTS=OFF compile byte-identical because the macro is undefined
 * and the friend declaration / forward declaration in
 * @c http2_client.h are gated by @c #if defined(NETWORK_ENABLE_TEST_INJECTION).
 *
 * Rationale (PR #1111 Round-4 diagnosis, Issue #1115 Round-6 pivot): the
 * SETTINGS handshake under coverage instrumentation exceeds 15 s on shared
 * CI runners, exhausting any reasonable @c wait_for budget. Tests that
 * exercise the @c http2_client::process_frame dispatcher cannot reach the
 * dispatcher branches via the async SETTINGS path within a coverage-friendly
 * budget. Direct invocation via this access class sidesteps the handshake
 * entirely while still routing through the production frame dispatch logic.
 *
 * Issue #1119 extension (Round 3 of #953): adds stream-seeding accessors so
 * direct-dispatch tests can drive the known-stream branches of
 * @c handle_rst_stream_frame, @c handle_data_frame, @c handle_headers_frame,
 * and @c handle_window_update_frame. Without a way to insert an entry into
 * the private @c streams_ map, every dispatcher invocation lands on the
 * stream-not-found arm — leaving roughly half of each handler's logic
 * unmeasured under coverage builds.
 */

#include "internal/protocols/http2/frame.h"
#include "internal/protocols/http2/http2_client.h"

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kcenon::network::tests::support
{

/**
 * @brief Test-only friend class exposing private dispatcher entry points
 *        of @ref kcenon::network::protocols::http2::http2_client.
 *
 * Forward-declared in @c tests/support/network_test_friends.h alongside
 * @c quic_server_probe and @c ws_server_probe. The production header
 * @c http2_client.h forward-declares this class under
 * @c #if defined(NETWORK_ENABLE_TEST_INJECTION) and grants friendship.
 *
 * Each static thunk is a pure forwarding call to a private member of
 * @c http2_client. The class itself holds no state.
 */
class http2_client_test_access
{
public:
    /**
     * @brief Direct invocation of @c http2_client::process_frame.
     *
     * Bypasses the connect / SETTINGS-handshake gate so frame-dispatch
     * branches can be measured under coverage instrumentation without a
     * 15 s+ async wait.
     *
     * State preconditions: @p client must be constructed but does not need
     * to be connected. Handlers that internally call @c send_frame (such as
     * non-ACK PING and non-ACK SETTINGS) will short-circuit through the
     * @c connection_closed branch when invoked on an unconnected client —
     * this is observed behavior, not a precondition violation.
     *
     * @param client Target client instance.
     * @param f      Frame to dispatch. Ownership is transferred.
     * @return Whatever @c http2_client::process_frame returns
     *         (@c VoidResult: ok() or an error_void).
     */
    static auto process_frame(
        ::kcenon::network::protocols::http2::http2_client& client,
        std::unique_ptr<::kcenon::network::protocols::http2::frame> f)
    {
        return client.process_frame(std::move(f));
    }

    /**
     * @brief Read the current goaway-received flag.
     *
     * Mirrors the post-condition observable from @c is_connected() for
     * tests that need to assert @c handle_goaway_frame fired without
     * also having @c is_connected_ flipped — the production
     * @c is_connected() returns @c is_connected_ && !goaway_received_, so a
     * fresh client where @c is_connected_ is @c false would mask the
     * goaway-received signal otherwise.
     */
    static auto goaway_received(
        const ::kcenon::network::protocols::http2::http2_client& client) -> bool
    {
        return client.goaway_received_.load();
    }

    /**
     * @brief Read the connection-level flow-control window size.
     *
     * Used by @c handle_window_update_frame stream_id == 0 tests to
     * assert the window expanded by the increment value.
     */
    static auto connection_window_size(
        const ::kcenon::network::protocols::http2::http2_client& client) -> int32_t
    {
        return client.connection_window_size_;
    }

    /**
     * @brief Seed a stream entry in @c streams_ so dispatcher tests can
     *        exercise the known-stream branches.
     *
     * Required for direct-dispatch coverage of @c handle_rst_stream_frame,
     * @c handle_data_frame, @c handle_headers_frame, and
     * @c handle_window_update_frame on the connected/known-stream arm.
     * Without this, every dispatcher invocation falls through to the
     * not-found branch, leaving the success-path body unmeasured.
     *
     * The caller controls the @p stream_id (use the same id when building
     * the frame) and the initial @p state (typically @c stream_state::open
     * for write-side or @c stream_state::half_closed_local for response
     * arrivals). The window size starts at the protocol default of 65535.
     *
     * @param client       Target client; the streams_ map is locked while
     *                     the entry is inserted.
     * @param stream_id    Stream id to register.
     * @param state        Initial stream state.
     * @param is_streaming When true, the seeded stream behaves like a
     *                     start_stream() registration: incoming DATA / HEADERS
     *                     frames invoke the @c on_data / @c on_headers /
     *                     @c on_complete callbacks instead of buffering and
     *                     fulfilling the promise. Default false matches the
     *                     send_request() registration.
     * @return Future paired with the stream's promise. The caller can use
     *         the future's @c wait_for to assert that the dispatcher
     *         fulfilled the promise (success body). For streaming streams
     *         the promise is never set; the caller can ignore the future.
     */
    static auto seed_stream(
        ::kcenon::network::protocols::http2::http2_client& client,
        std::uint32_t stream_id,
        ::kcenon::network::protocols::http2::stream_state state =
            ::kcenon::network::protocols::http2::stream_state::open,
        bool is_streaming = false)
        -> std::future<::kcenon::network::protocols::http2::http2_response>
    {
        std::lock_guard<std::mutex> lock(client.streams_mutex_);
        auto& s = client.streams_[stream_id];
        s.stream_id = stream_id;
        s.state = state;
        s.is_streaming = is_streaming;
        return s.promise.get_future();
    }

    /**
     * @brief Install streaming callbacks on a previously-seeded stream.
     *
     * Call after @c seed_stream(... is_streaming=true) to wire up callbacks
     * so the dispatcher tests can observe @c on_data / @c on_headers /
     * @c on_complete invocation.
     */
    static auto set_stream_callbacks(
        ::kcenon::network::protocols::http2::http2_client& client,
        std::uint32_t stream_id,
        std::function<void(std::vector<std::uint8_t>)> on_data,
        std::function<void(std::vector<
            ::kcenon::network::protocols::http2::http_header>)> on_headers,
        std::function<void(int)> on_complete) -> void
    {
        std::lock_guard<std::mutex> lock(client.streams_mutex_);
        auto it = client.streams_.find(stream_id);
        if (it == client.streams_.end())
        {
            return;
        }
        it->second.on_data = std::move(on_data);
        it->second.on_headers = std::move(on_headers);
        it->second.on_complete = std::move(on_complete);
    }

    /**
     * @brief Read the current state of a seeded stream.
     *
     * Used by dispatcher tests to assert that a handler correctly
     * transitioned the stream (e.g. RST_STREAM closes it, DATA with
     * END_STREAM closes it).
     */
    static auto stream_state_of(
        ::kcenon::network::protocols::http2::http2_client& client,
        std::uint32_t stream_id)
        -> ::kcenon::network::protocols::http2::stream_state
    {
        std::lock_guard<std::mutex> lock(client.streams_mutex_);
        auto it = client.streams_.find(stream_id);
        if (it == client.streams_.end())
        {
            return ::kcenon::network::protocols::http2::stream_state::closed;
        }
        return it->second.state;
    }

    /**
     * @brief Read the per-stream flow-control window size.
     *
     * Used to assert that @c handle_window_update_frame with a non-zero
     * stream_id increments the per-stream window (distinct branch from the
     * connection-level path covered by @c connection_window_size above).
     */
    static auto stream_window_size_of(
        ::kcenon::network::protocols::http2::http2_client& client,
        std::uint32_t stream_id) -> int32_t
    {
        std::lock_guard<std::mutex> lock(client.streams_mutex_);
        auto it = client.streams_.find(stream_id);
        if (it == client.streams_.end())
        {
            return 0;
        }
        return it->second.window_size;
    }

    /**
     * @brief Append a header to a seeded stream's response_headers.
     *
     * Lets dispatcher tests pre-stage a @c :status header so a follow-up
     * DATA frame with @c END_STREAM resolves to a real status code path
     * (e.g. 200 success, 404 error, malformed status code triggering the
     * @c std::stoi catch arm). Without this hook the response_headers are
     * empty when @c handle_data_frame's status-code extraction runs, so the
     * status_code is left at 0 and the catch arm cannot fire.
     */
    static auto append_response_header(
        ::kcenon::network::protocols::http2::http2_client& client,
        std::uint32_t stream_id,
        std::string name,
        std::string value) -> void
    {
        std::lock_guard<std::mutex> lock(client.streams_mutex_);
        auto it = client.streams_.find(stream_id);
        if (it == client.streams_.end())
        {
            return;
        }
        it->second.response_headers.push_back(
            ::kcenon::network::protocols::http2::http_header{
                std::move(name), std::move(value)});
    }

    /**
     * @brief Read the buffered response body for a non-streaming stream.
     *
     * Used to assert that @c handle_data_frame appended the DATA payload to
     * the stream's response_body when the streaming flag is false (the
     * branch that bypasses @c on_data and instead buffers).
     */
    static auto response_body_of(
        ::kcenon::network::protocols::http2::http2_client& client,
        std::uint32_t stream_id) -> std::vector<std::uint8_t>
    {
        std::lock_guard<std::mutex> lock(client.streams_mutex_);
        auto it = client.streams_.find(stream_id);
        if (it == client.streams_.end())
        {
            return {};
        }
        return it->second.response_body;
    }

    /**
     * @brief Build the next stream id without dispatching a request.
     *
     * Forwards to @c http2_client::allocate_stream_id so tests can verify
     * the monotonic increment-by-2 contract (RFC 7540 §5.1.1: client streams
     * are odd-numbered).
     */
    static auto allocate_stream_id(
        ::kcenon::network::protocols::http2::http2_client& client)
        -> std::uint32_t
    {
        return client.allocate_stream_id();
    }

    /**
     * @brief Build the canonical request header set for direct testing.
     *
     * Forwards to @c http2_client::build_headers so the empty-path → "/"
     * default and the leading-colon pseudo-header skip can be observed
     * without going through @c send_request (which requires a connected
     * client).
     */
    static auto build_headers(
        ::kcenon::network::protocols::http2::http2_client& client,
        const std::string& method,
        const std::string& path,
        const std::vector<
            ::kcenon::network::protocols::http2::http_header>& additional)
        -> std::vector<::kcenon::network::protocols::http2::http_header>
    {
        return client.build_headers(method, path, additional);
    }

    /**
     * @brief Inspect the next-stream-id counter (raw load).
     *
     * Used by allocator tests to assert the post-state without taking a
     * second token and skewing a follow-up assertion.
     */
    static auto next_stream_id(
        const ::kcenon::network::protocols::http2::http2_client& client)
        -> std::uint32_t
    {
        return client.next_stream_id_.load();
    }
};

} // namespace kcenon::network::tests::support
