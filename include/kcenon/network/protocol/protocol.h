// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file protocol.h
 * @brief Convenience header for all protocol factory functions
 *
 * This header provides a single include point for all protocol-specific
 * factory functions that create unified interface implementations.
 *
 * ### Available Protocol Factories
 *
 * | Namespace | Factory Functions |
 * |-----------|-------------------|
 * | `protocol::tcp` | `connect()`, `listen()`, `create_connection()`, `create_listener()` |
 * | `protocol::udp` | `connect()`, `listen()`, `create_connection()`, `create_listener()` |
 * | `protocol::websocket` | `connect()`, `listen()`, `create_connection()`, `create_listener()` |
 * | `protocol::quic` | `connect()`, `listen()`, `create_connection()`, `create_listener()` |
 *
 * ### Usage Example
 * @code
 * #include <kcenon/network/protocol/protocol.h>
 *
 * using namespace kcenon::network;
 *
 * // TCP client
 * auto tcp_conn = protocol::tcp::connect({"localhost", 8080});
 * tcp_conn->set_callbacks({
 *     .on_connected = []() { std::cout << "Connected!\n"; },
 *     .on_data = [](std::span<const std::byte> data) {
 *         // Handle received data
 *     }
 * });
 *
 * // TCP server
 * auto tcp_server = protocol::tcp::listen(8080);
 * tcp_server->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) {
 *         std::cout << "New connection: " << conn_id << "\n";
 *     }
 * });
 *
 * // UDP client
 * auto udp_conn = protocol::udp::connect({"localhost", 5555});
 * udp_conn->set_callbacks({
 *     .on_data = [](std::span<const std::byte> data) {
 *         // Handle received datagram
 *     }
 * });
 *
 * // UDP server
 * auto udp_server = protocol::udp::listen(5555);
 * udp_server->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) {
 *         std::cout << "New endpoint: " << conn_id << "\n";
 *     },
 *     .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
 *         // Handle received datagram from conn_id
 *     }
 * });
 *
 * // WebSocket client
 * auto ws_conn = protocol::websocket::connect("ws://localhost:8080/ws");
 * ws_conn->set_callbacks({
 *     .on_connected = []() { std::cout << "WebSocket connected!\n"; },
 *     .on_data = [](std::span<const std::byte> data) {
 *         // Handle received WebSocket message
 *     }
 * });
 *
 * // WebSocket server
 * auto ws_server = protocol::websocket::listen(8080, "/ws");
 * ws_server->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) {
 *         std::cout << "New WebSocket client: " << conn_id << "\n";
 *     },
 *     .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
 *         // Handle received message from conn_id
 *     }
 * });
 *
 * // QUIC client
 * protocol::quic::quic_config quic_cfg;
 * quic_cfg.server_name = "example.com";
 * quic_cfg.alpn_protocols = {"h3"};
 * auto quic_conn = protocol::quic::connect({"example.com", 443}, quic_cfg);
 * quic_conn->set_callbacks({
 *     .on_connected = []() { std::cout << "QUIC connected!\n"; },
 *     .on_data = [](std::span<const std::byte> data) {
 *         // Handle received QUIC data
 *     }
 * });
 *
 * // QUIC server
 * protocol::quic::quic_config server_cfg;
 * server_cfg.cert_file = "/path/to/cert.pem";
 * server_cfg.key_file = "/path/to/key.pem";
 * auto quic_server = protocol::quic::listen(443, server_cfg);
 * quic_server->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) {
 *         std::cout << "New QUIC client: " << conn_id << "\n";
 *     },
 *     .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
 *         // Handle received QUIC message from conn_id
 *     }
 * });
 * @endcode
 *
 * @see unified::i_connection
 * @see unified::i_listener
 * @see unified::i_transport
 */

// Protocol factory headers (from detail directory)
#include "kcenon/network/detail/protocol/tcp.h"
#include "kcenon/network/detail/protocol/udp.h"
#include "kcenon/network/detail/protocol/websocket.h"
#include "kcenon/network/detail/protocol/quic.h"

// Protocol tag types
#include "kcenon/network/detail/protocol/protocol_tags.h"
