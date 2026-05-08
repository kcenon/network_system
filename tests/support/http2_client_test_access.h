// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file http2_client_test_access.h
 * @brief Friend-accessor for hermetic dispatcher coverage (Issue #1115).
 *
 * Production builds compile byte-identical: this header is only consumed by
 * the test target that defines @c NETWORK_HTTP2_CLIENT_FRIEND_TESTS, and
 * @ref kcenon::network::protocols::http2::http2_client befriends
 * @ref Http2ClientTestAccess only when that macro is defined.
 *
 * Rationale (PR #1111 Round-4 diagnosis, Issue #1115 Round-6 pivot): the
 * SETTINGS handshake under coverage instrumentation exceeds 15 s on shared
 * CI runners, exhausting any reasonable @c wait_for budget. Tests that
 * exercise the @c http2_client::process_frame dispatcher cannot reach the
 * dispatcher branches via the async SETTINGS path within a coverage-friendly
 * budget. Direct invocation via this access struct sidesteps the handshake
 * entirely while still routing through the production frame dispatch logic.
 */

#include "internal/protocols/http2/frame.h"
#include "internal/protocols/http2/http2_client.h"

#include <memory>

namespace kcenon::network::tests::support
{

/**
 * @brief Test-only friend struct exposing private dispatcher entry points
 *        of @ref http2_client.
 *
 * Only declared and defined when @c NETWORK_HTTP2_CLIENT_FRIEND_TESTS is
 * defined on the consuming translation unit. The production header
 * forward-declares this struct under the same guard and befriends it; with
 * the macro undefined the struct does not exist and cannot be instantiated.
 *
 * Each static thunk is a pure forwarding call to a private @c http2_client
 * method. The struct itself holds no state.
 */
struct Http2ClientTestAccess
{
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
     * @param client Target client instance (typically heap-allocated via
     *               @c std::make_shared).
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
     * also having @c is_connected_ flipped (the production
     * @c is_connected() returns @c is_connected_ && !goaway_received_, so a
     * fresh client where @c is_connected_ is @c false would mask the
     * goaway-received signal otherwise).
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
};

} // namespace kcenon::network::tests::support
