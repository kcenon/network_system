// BSD 3-Clause License
// Copyright (c) 2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file http2.h
 * @brief Main public header for network-http2 library
 *
 * This file includes all public HTTP/2 protocol headers for convenience.
 * Users can include this single header to access all HTTP/2 functionality.
 *
 * ## Features
 *
 * - HTTP/2 protocol support (RFC 7540)
 * - HPACK header compression (RFC 7541)
 * - TLS 1.3 with ALPN negotiation
 * - Stream multiplexing
 * - Flow control
 * - Server push (disabled by default)
 *
 * ## Usage Example
 *
 * ### Client
 * @code
 * #include <network_http2/http2.h>
 *
 * auto client = std::make_shared<http2_client>("my-client");
 * auto connect_result = client->connect("example.com", 443);
 * if (connect_result) {
 *     auto response = client->get("/api/users");
 *     if (response) {
 *         std::cout << "Status: " << response->status_code << "\n";
 *     }
 * }
 * client->disconnect();
 * @endcode
 *
 * ### Server
 * @code
 * #include <network_http2/http2.h>
 *
 * auto server = std::make_shared<http2_server>("my-server");
 * server->set_request_handler([](http2_server_stream& stream,
 *                                const http2_request& request) {
 *     if (request.method == "GET" && request.path == "/api/health") {
 *         stream.send_headers(200, {{"content-type", "application/json"}});
 *         stream.send_data(R"({"status": "ok"})", true);
 *     }
 * });
 *
 * tls_config config;
 * config.cert_file = "/path/to/cert.pem";
 * config.key_file = "/path/to/key.pem";
 * server->start_tls(8443, config);
 * server->wait();
 * @endcode
 */

// HTTP/2 frame encoding/decoding
#include "network_http2/internal/frame.h"

// HPACK header compression
#include "network_http2/internal/hpack.h"

// HTTP/2 request structure
#include "network_http2/http2_request.h"

// HTTP/2 client
#include "network_http2/http2_client.h"

// HTTP/2 server
#include "network_http2/http2_server.h"

// HTTP/2 server stream
#include "network_http2/http2_server_stream.h"

/**
 * @namespace network_http2
 * @brief HTTP/2 protocol implementation namespace
 *
 * This namespace contains all HTTP/2 related classes and functions,
 * including the client, server, frame handling, and HPACK compression.
 */
namespace network_http2
{
    // Re-export key types from kcenon::network::protocols::http2
    using kcenon::network::protocols::http2::http2_client;
    using kcenon::network::protocols::http2::http2_server;
    using kcenon::network::protocols::http2::http2_server_stream;
    using kcenon::network::protocols::http2::http2_request;
    using kcenon::network::protocols::http2::http2_response;
    using kcenon::network::protocols::http2::http2_settings;
    using kcenon::network::protocols::http2::http2_stream;
    using kcenon::network::protocols::http2::tls_config;
    using kcenon::network::protocols::http2::stream_state;
    using kcenon::network::protocols::http2::http_header;
    using kcenon::network::protocols::http2::error_code;

} // namespace network_http2
