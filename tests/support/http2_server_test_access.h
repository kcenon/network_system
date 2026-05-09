// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file http2_server_test_access.h
 * @brief Friend-accessor for hermetic dispatcher coverage of http2_server.cpp
 *        (Issue #1121).
 *
 * Reuses the existing Phase 2D friend gate @c NETWORK_ENABLE_TEST_INJECTION
 * (defined PUBLIC on @c network_system when @c BUILD_TESTS=ON, see
 * @c cmake/network_system_targets.cmake). Production builds with
 * @c BUILD_TESTS=OFF compile byte-identical because the macro is undefined
 * and the friend declaration / forward declaration in
 * @c http2_server.h are gated by @c #if defined(NETWORK_ENABLE_TEST_INJECTION).
 *
 * Mirrors @c http2_client_test_access.h (Round 6 / Issue #1115). The server
 * dispatcher (http2_server_connection::process_frame) cannot be reached
 * hermetically from the public API: it requires a live HTTP/2 client that
 * has completed the connection preface and SETTINGS exchange, which itself
 * exceeds the 15 s wait_for budget under coverage instrumentation. Direct
 * invocation via this access class sidesteps the handshake entirely while
 * still routing through the production frame dispatch logic.
 *
 * Round 1 of #953/#1121: covers the seven switch arms of process_frame
 * (settings, headers, data, rst_stream, ping, goaway, window_update) plus
 * the known-stream branches that close_stream / get_or_create_stream /
 * handle_window_update_frame's stream-level path require a populated
 * @c streams_ map to reach.
 */

#include "internal/protocols/http2/frame.h"
#include "internal/protocols/http2/http2_client.h"
#include "internal/protocols/http2/http2_server.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace kcenon::network::tests::support
{

/**
 * @brief Test-only friend class exposing private dispatcher entry points
 *        of @ref kcenon::network::protocols::http2::http2_server_connection.
 *
 * Forward-declared in @c tests/support/network_test_friends.h alongside
 * @c http2_client_test_access. The production header @c http2_server.h
 * forward-declares this class under
 * @c #if defined(NETWORK_ENABLE_TEST_INJECTION) and grants friendship.
 *
 * Each static thunk is a pure forwarding call to a private member of
 * @c http2_server_connection. The class itself holds no state.
 */
class http2_server_test_access
{
public:
    /**
     * @brief Direct invocation of @c http2_server_connection::process_frame.
     *
     * Bypasses the connection-preface read and SETTINGS-handshake gates so
     * frame-dispatch branches can be measured under coverage instrumentation
     * without a live HTTP/2 client.
     *
     * State preconditions: @p connection must be constructed but does not
     * need to have completed the preface read. Handlers that internally call
     * @c send_frame (PING ACK reply, WINDOW_UPDATE on DATA, RST_STREAM on
     * unknown stream) will short-circuit through the closed-socket branch on
     * a connection whose socket is not yet active — this is observed
     * behavior, not a precondition violation. The dispatch arm and handler
     * body coverage is collected regardless.
     *
     * @param connection Target connection instance.
     * @param f          Frame to dispatch. Ownership is transferred.
     * @return Whatever @c http2_server_connection::process_frame returns
     *         (@c VoidResult: ok() or an error_void).
     */
    static auto process_frame(
        ::kcenon::network::protocols::http2::http2_server_connection& connection,
        std::unique_ptr<::kcenon::network::protocols::http2::frame> f)
    {
        return connection.process_frame(std::move(f));
    }

    /**
     * @brief Direct invocation of @c get_or_create_stream.
     *
     * Used to seed a stream entry in @c streams_ before driving DATA / HEADERS
     * dispatch, so the known-stream success-path branch is reachable
     * hermetically. Without this hook, every dispatcher invocation lands on
     * the not-found arm — leaving the success-path body unmeasured under
     * coverage builds.
     *
     * @return Pointer to the (newly-created or existing) stream. The caller
     *         can mutate the returned stream's state to drive specific
     *         branch arms.
     */
    static auto get_or_create_stream(
        ::kcenon::network::protocols::http2::http2_server_connection& connection,
        std::uint32_t stream_id)
        -> ::kcenon::network::protocols::http2::http2_stream*
    {
        return connection.get_or_create_stream(stream_id);
    }

    /**
     * @brief Direct invocation of @c close_stream.
     *
     * Useful for asserting the stream lookup-then-erase branch in isolation
     * from RST_STREAM dispatch. Distinct from process_frame(rst_stream_frame)
     * which also closes streams but covers the dispatch arm in addition.
     */
    static auto close_stream(
        ::kcenon::network::protocols::http2::http2_server_connection& connection,
        std::uint32_t stream_id) -> void
    {
        connection.close_stream(stream_id);
    }

    /**
     * @brief Read the connection-level flow-control window size.
     *
     * Used by @c handle_window_update_frame stream_id == 0 tests to assert
     * the connection window expanded by the increment value — distinct from
     * the per-stream window path covered by stream_window_size_of below.
     */
    static auto connection_window_size(
        const ::kcenon::network::protocols::http2::http2_server_connection&
            connection) -> int32_t
    {
        return connection.connection_window_size_;
    }

    /**
     * @brief Read the per-stream flow-control window for a known stream.
     *
     * Returns 0 when the stream is unknown. Used to assert that
     * @c handle_window_update_frame with non-zero stream_id correctly
     * incremented the per-stream window, distinct from the connection
     * window covered above.
     */
    static auto stream_window_size_of(
        ::kcenon::network::protocols::http2::http2_server_connection& connection,
        std::uint32_t stream_id) -> int32_t
    {
        std::lock_guard<std::mutex> lock(connection.streams_mutex_);
        auto it = connection.streams_.find(stream_id);
        if (it == connection.streams_.end()) {
            return 0;
        }
        return it->second.window_size;
    }

    /**
     * @brief Read the current stream count without taking the public mutex.
     *
     * Mirrors @c stream_count() but is callable on a const reference inside
     * dispatcher tests where the connection lifetime is bounded by the test
     * scope.
     */
    static auto stream_count(
        ::kcenon::network::protocols::http2::http2_server_connection& connection)
        -> std::size_t
    {
        std::lock_guard<std::mutex> lock(connection.streams_mutex_);
        return connection.streams_.size();
    }

    /**
     * @brief Returns true if the streams_ map contains an entry for
     *        @p stream_id.
     *
     * Used to verify post-conditions of close_stream (entry removed) and
     * RST_STREAM dispatch (entry removed) without exposing the entire map.
     */
    static auto has_stream(
        ::kcenon::network::protocols::http2::http2_server_connection& connection,
        std::uint32_t stream_id) -> bool
    {
        std::lock_guard<std::mutex> lock(connection.streams_mutex_);
        return connection.streams_.find(stream_id)
               != connection.streams_.end();
    }

    /**
     * @brief Read the stream state for a known stream.
     *
     * Returns @c stream_state::closed when the stream is unknown. Lets
     * dispatcher tests assert that a HEADERS frame with end_stream=true
     * transitioned the stream to half_closed_remote, or that a DATA frame
     * with end_stream=true did the same.
     */
    static auto stream_state_of(
        ::kcenon::network::protocols::http2::http2_server_connection& connection,
        std::uint32_t stream_id)
        -> ::kcenon::network::protocols::http2::stream_state
    {
        std::lock_guard<std::mutex> lock(connection.streams_mutex_);
        auto it = connection.streams_.find(stream_id);
        if (it == connection.streams_.end()) {
            return ::kcenon::network::protocols::http2::stream_state::closed;
        }
        return it->second.state;
    }

    /**
     * @brief Read the current last_stream_id value.
     *
     * Used by stream-creation tests to assert the monotonic-tracking
     * invariant after a HEADERS frame opens a new stream.
     */
    static auto last_stream_id(
        const ::kcenon::network::protocols::http2::http2_server_connection&
            connection) -> std::uint32_t
    {
        return connection.last_stream_id_;
    }

    /**
     * @brief Read the alive flag.
     *
     * Mirrors @c is_alive() but provides a stable reading even when the
     * public method might be racing against a stop() teardown — the
     * dispatcher tests use this to assert handle_goaway_frame correctly
     * tore down the connection.
     */
    static auto is_alive(
        const ::kcenon::network::protocols::http2::http2_server_connection&
            connection) -> bool
    {
        return connection.is_alive_.load();
    }
};

} // namespace kcenon::network::tests::support
