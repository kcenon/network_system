/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#pragma once

#include "kcenon/network/unified/i_connection.h"
#include "kcenon/network/unified/i_listener.h"
#include "kcenon/network/unified/types.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace kcenon::network::protocol::quic {

/**
 * @struct quic_config
 * @brief Configuration options for QUIC connections
 *
 * QUIC-specific configuration parameters that affect connection behavior,
 * security settings, and performance characteristics.
 *
 * ### Thread Safety
 * Immutable after construction, safe for concurrent read access.
 */
struct quic_config {
    /// Server name for TLS SNI (required for client connections)
    std::string server_name;

    /// Path to certificate file (required for server)
    std::string cert_file;

    /// Path to private key file (required for server)
    std::string key_file;

    /// ALPN (Application-Layer Protocol Negotiation) protocols
    std::vector<std::string> alpn_protocols;

    /// Maximum idle timeout (0 = use default)
    std::chrono::milliseconds idle_timeout{0};

    /// Maximum number of bidirectional streams
    uint64_t max_bidi_streams{100};

    /// Maximum number of unidirectional streams
    uint64_t max_uni_streams{100};

    /// Initial maximum data (connection-level flow control)
    uint64_t initial_max_data{10 * 1024 * 1024};  // 10 MB

    /// Initial maximum stream data for bidirectional streams
    uint64_t initial_max_stream_data_bidi{1024 * 1024};  // 1 MB

    /// Initial maximum stream data for unidirectional streams
    uint64_t initial_max_stream_data_uni{1024 * 1024};  // 1 MB

    /// Enable 0-RTT (early data)
    bool enable_early_data{false};

    /// Enable Path MTU Discovery
    bool enable_pmtud{true};

    /// Disable certificate verification (for testing only)
    bool insecure_skip_verify{false};
};

/**
 * @brief Creates a QUIC connection (not yet connected)
 * @param config QUIC configuration options
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance
 *
 * The returned connection is not connected. Call connect() to establish
 * the connection to a remote endpoint.
 *
 * ### QUIC Semantics
 * QUIC connections have built-in TLS 1.3 encryption, multiplexed streams,
 * and support for connection migration.
 * - connect() initiates the QUIC handshake
 * - is_connected() returns true after handshake completion
 * - send() sends data on the default stream
 *
 * ### Usage Example
 * @code
 * quic_config cfg;
 * cfg.server_name = "example.com";
 * cfg.alpn_protocols = {"h3"};
 *
 * auto conn = protocol::quic::create_connection(cfg, "my-quic-client");
 * conn->set_callbacks({
 *     .on_connected = []() { std::cout << "Connected!\n"; },
 *     .on_data = [](std::span<const std::byte> data) {
 *         // Handle received data
 *     }
 * });
 * conn->connect({"example.com", 443});
 * @endcode
 */
[[nodiscard]] auto create_connection(const quic_config& config = {},
                                     std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates and connects a QUIC connection in one call
 * @param endpoint The remote endpoint to connect to
 * @param config QUIC configuration options
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance (connecting)
 *
 * This is a convenience function that creates a connection and immediately
 * initiates the QUIC handshake to the specified endpoint.
 *
 * ### Usage Example
 * @code
 * quic_config cfg;
 * cfg.server_name = "example.com";
 *
 * auto conn = protocol::quic::connect({"example.com", 443}, cfg);
 * conn->set_callbacks({
 *     .on_connected = []() { std::cout << "Connected!\n"; }
 * });
 * // QUIC handshake is already in progress
 * @endcode
 */
[[nodiscard]] auto connect(const unified::endpoint_info& endpoint,
                           const quic_config& config = {},
                           std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates and connects a QUIC connection using URL format
 * @param url The URL to connect to (format: "quic://host:port" or "host:port")
 * @param config QUIC configuration options
 * @param id Optional unique identifier for the connection
 * @return Unique pointer to an i_connection instance (connecting)
 *
 * If server_name is not set in config, it will be extracted from the URL.
 */
[[nodiscard]] auto connect(std::string_view url,
                           const quic_config& config = {},
                           std::string_view id = "")
    -> std::unique_ptr<unified::i_connection>;

/**
 * @brief Creates a QUIC listener (not yet listening)
 * @param config QUIC configuration options (cert_file and key_file required)
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance
 *
 * The returned listener is not listening. Call start() to begin
 * accepting connections.
 *
 * ### QUIC Server Requirements
 * QUIC servers require TLS certificates. The cert_file and key_file
 * must be set in the configuration.
 *
 * ### Usage Example
 * @code
 * quic_config cfg;
 * cfg.cert_file = "/path/to/cert.pem";
 * cfg.key_file = "/path/to/key.pem";
 * cfg.alpn_protocols = {"h3"};
 *
 * auto listener = protocol::quic::create_listener(cfg, "my-quic-server");
 * listener->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) {
 *         std::cout << "New connection: " << conn_id << "\n";
 *     },
 *     .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
 *         // Handle received data from conn_id
 *     }
 * });
 * listener->start(443);
 * @endcode
 */
[[nodiscard]] auto create_listener(const quic_config& config,
                                   std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

/**
 * @brief Creates and starts a QUIC listener in one call
 * @param bind_address The local address to bind to
 * @param config QUIC configuration options (cert_file and key_file required)
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance (listening)
 *
 * This is a convenience function that creates a listener and immediately
 * starts listening on the specified address.
 *
 * ### Usage Example
 * @code
 * quic_config cfg;
 * cfg.cert_file = "/path/to/cert.pem";
 * cfg.key_file = "/path/to/key.pem";
 *
 * auto listener = protocol::quic::listen({"0.0.0.0", 443}, cfg);
 * listener->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) { }
 * });
 * // Listener is already accepting connections
 * @endcode
 */
[[nodiscard]] auto listen(const unified::endpoint_info& bind_address,
                          const quic_config& config,
                          std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

/**
 * @brief Creates and starts a QUIC listener on a specific port
 * @param port The port number to listen on
 * @param config QUIC configuration options (cert_file and key_file required)
 * @param id Optional unique identifier for the listener
 * @return Unique pointer to an i_listener instance (listening)
 *
 * Convenience overload that binds to all interfaces (0.0.0.0).
 */
[[nodiscard]] auto listen(uint16_t port,
                          const quic_config& config,
                          std::string_view id = "")
    -> std::unique_ptr<unified::i_listener>;

}  // namespace kcenon::network::protocol::quic
