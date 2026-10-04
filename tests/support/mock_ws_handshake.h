// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file mock_ws_handshake.h
 * @brief WebSocket (RFC 6455) byte-level helpers for hermetic tests
 *        (Issue #1060)
 *
 * @c messaging_websocket_server's @c handle_new_connection accepts an already-
 * connected @c asio::ip::tcp::socket and drives the upgrade handshake from
 * its own side. Tests construct one half of a loopback TCP pair via
 * @ref make_loopback_tcp_pair and write the synthesized upgrade request from
 * the peer side using these helpers.
 *
 *  - @ref make_websocket_upgrade_request — RFC 6455 §4.1 client GET request
 *    bytes, including the (deterministic) Sec-WebSocket-Key.
 *  - @ref make_websocket_text_frame — RFC 6455 §5.2 text frame, masked or
 *    unmasked.
 *  - @ref make_websocket_close_frame — RFC 6455 §5.5.1 close frame with
 *    optional status code.
 */

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::network::tests::support
{

/**
 * @brief Build an RFC 6455 client upgrade request as raw bytes.
 *
 * The returned string ends with @c \\r\\n\\r\\n. The Sec-WebSocket-Key is a
 * fixed test vector ("dGhlIHNhbXBsZSBub25jZQ==") so the expected response key
 * is deterministic — useful when validating server responses byte-by-byte.
 *
 * @param host @c Host header value (e.g., "127.0.0.1:54321").
 * @param resource @c Request-URI path (defaults to "/").
 */
[[nodiscard]] std::string make_websocket_upgrade_request(
    std::string_view host,
    std::string_view resource = "/");

/**
 * @brief Build a single-fragment WebSocket text frame.
 *
 * @param payload payload bytes (interpreted as UTF-8 by the server).
 * @param masked if @c true, use a fixed test mask key; if @c false, send
 *        unmasked (server-to-client direction or non-conforming client).
 */
[[nodiscard]] std::vector<uint8_t> make_websocket_text_frame(
    std::string_view payload,
    bool masked = true);

/**
 * @brief Build a single-fragment WebSocket binary frame.
 */
[[nodiscard]] std::vector<uint8_t> make_websocket_binary_frame(
    std::span<const uint8_t> payload,
    bool masked = true);

/**
 * @brief Build an RFC 6455 close control frame.
 *
 * @param code optional status code; if 0, no body is sent.
 * @param reason optional human-readable reason; ignored if @p code == 0.
 */
[[nodiscard]] std::vector<uint8_t> make_websocket_close_frame(
    uint16_t code = 1000,
    std::string_view reason = "");

} // namespace kcenon::network::tests::support
