// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/*!
 * \file network.h
 * \brief Umbrella header for all network protocol libraries
 *
 * This header provides a single include point for all network protocols
 * available in the network_system library collection:
 *
 * - TCP: Reliable stream-based communication
 * - UDP: Unreliable datagram-based communication
 * - WebSocket: Full-duplex communication over HTTP
 * - HTTP/2: Modern HTTP with multiplexing and server push
 * - QUIC: UDP-based reliable transport with TLS 1.3
 * - gRPC: High-performance RPC framework
 *
 * ## Quick Start
 *
 * \code
 * #include <network_all/network.h>
 *
 * using namespace kcenon::network;
 *
 * // TCP client
 * auto tcp_conn = protocol::tcp::connect({"localhost", 8080});
 *
 * // UDP connection
 * auto udp_conn = protocol::udp::create_connection();
 *
 * // WebSocket client
 * auto ws_conn = protocol::websocket::connect("ws://localhost:8081/chat");
 * \endcode
 *
 * ## Selective Includes
 *
 * For reduced compilation time, include only the protocols you need:
 *
 * \code
 * // TCP only
 * #include <network_tcp/protocol/tcp.h>
 *
 * // UDP only
 * #include <network_udp/protocol/udp.h>
 *
 * // WebSocket only
 * #include <network_websocket/websocket.h>
 *
 * // HTTP/2 only
 * #include <network_http2/http2.h>
 *
 * // QUIC only
 * #include <network_quic/quic.h>
 *
 * // gRPC only
 * #include <network_grpc/grpc.h>
 * \endcode
 */

//=============================================================================
// Core Interfaces
//=============================================================================

// Note: Core interfaces are included transitively through protocol headers

//=============================================================================
// TCP Protocol
//=============================================================================

#if __has_include(<network_tcp/protocol/tcp.h>)
#include <network_tcp/protocol/tcp.h>
#define NETWORK_ALL_HAS_TCP 1
#else
#define NETWORK_ALL_HAS_TCP 0
#endif

//=============================================================================
// UDP Protocol
//=============================================================================

#if __has_include(<network_udp/protocol/udp.h>)
#include <network_udp/protocol/udp.h>
#define NETWORK_ALL_HAS_UDP 1
#else
#define NETWORK_ALL_HAS_UDP 0
#endif

//=============================================================================
// WebSocket Protocol
//=============================================================================

#if __has_include(<network_websocket/websocket.h>)
#include <network_websocket/websocket.h>
#define NETWORK_ALL_HAS_WEBSOCKET 1
#else
#define NETWORK_ALL_HAS_WEBSOCKET 0
#endif

//=============================================================================
// HTTP/2 Protocol
//=============================================================================

#if __has_include(<network_http2/http2.h>)
#include <network_http2/http2.h>
#define NETWORK_ALL_HAS_HTTP2 1
#else
#define NETWORK_ALL_HAS_HTTP2 0
#endif

//=============================================================================
// QUIC Protocol
//=============================================================================

#if __has_include(<network_quic/quic.h>)
#include <network_quic/quic.h>
#define NETWORK_ALL_HAS_QUIC 1
#else
#define NETWORK_ALL_HAS_QUIC 0
#endif

//=============================================================================
// gRPC Protocol
//=============================================================================

#if __has_include(<network_grpc/grpc.h>)
#include <network_grpc/grpc.h>
#define NETWORK_ALL_HAS_GRPC 1
#else
#define NETWORK_ALL_HAS_GRPC 0
#endif

//=============================================================================
// Version Information
//=============================================================================

namespace kcenon::network::all {

/// Version of the network-all umbrella package
constexpr const char* version = "0.1.0";

/// Check if a protocol is available at compile time
struct protocols {
    static constexpr bool tcp = NETWORK_ALL_HAS_TCP;
    static constexpr bool udp = NETWORK_ALL_HAS_UDP;
    static constexpr bool websocket = NETWORK_ALL_HAS_WEBSOCKET;
    static constexpr bool http2 = NETWORK_ALL_HAS_HTTP2;
    static constexpr bool quic = NETWORK_ALL_HAS_QUIC;
    static constexpr bool grpc = NETWORK_ALL_HAS_GRPC;
};

}  // namespace kcenon::network::all
