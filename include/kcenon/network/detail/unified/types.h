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

#include <chrono>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace kcenon::network::unified {

/**
 * @struct endpoint_info
 * @brief Network endpoint information (host/port or URL)
 *
 * Represents a network endpoint that can be either a host:port combination
 * or a full URL (for protocols like WebSocket that use URLs).
 *
 * ### Thread Safety
 * Immutable after construction, safe for concurrent read access.
 *
 * ### Examples
 * @code
 * // Host/port style
 * endpoint_info tcp_ep{"192.168.1.1", 8080};
 * endpoint_info local_ep{"localhost", 3000};
 *
 * // URL style (for WebSocket, HTTP)
 * endpoint_info ws_ep{"wss://example.com/ws"};
 *
 * // Copy and compare
 * auto ep2 = tcp_ep;
 * if (tcp_ep == ep2) { ... }
 * @endcode
 */
struct endpoint_info {
    std::string host;    ///< Hostname, IP address, or full URL
    uint16_t port = 0;   ///< Port number (0 if embedded in URL)

    /**
     * @brief Default constructor creates an empty endpoint
     */
    endpoint_info() = default;

    /**
     * @brief Constructs endpoint from host and port
     * @param h Hostname or IP address
     * @param p Port number
     */
    endpoint_info(const std::string& h, uint16_t p) : host(h), port(p) {}

    /**
     * @brief Constructs endpoint from C-string host and port
     * @param h Hostname or IP address (C-string)
     * @param p Port number
     */
    endpoint_info(const char* h, uint16_t p) : host(h), port(p) {}

    /**
     * @brief Constructs endpoint from URL (port extracted if present)
     * @param url Full URL (e.g., "wss://example.com:443/ws")
     *
     * For URL-based protocols, the host field contains the full URL
     * and port may be 0 if embedded in the URL.
     */
    explicit endpoint_info(const std::string& url) : host(url), port(0) {}

    /**
     * @brief Constructs endpoint from C-string URL
     * @param url Full URL (C-string)
     */
    explicit endpoint_info(const char* url) : host(url), port(0) {}

    /**
     * @brief Checks if the endpoint is valid (non-empty host)
     * @return true if host is not empty
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool { return !host.empty(); }

    /**
     * @brief Returns string representation of the endpoint
     * @return "host:port" or URL string
     */
    [[nodiscard]] auto to_string() const -> std::string {
        if (port == 0) {
            return host;
        }
        return host + ":" + std::to_string(port);
    }

    /**
     * @brief Equality comparison
     */
    [[nodiscard]] auto operator==(const endpoint_info& other) const noexcept -> bool {
        return host == other.host && port == other.port;
    }

    /**
     * @brief Inequality comparison
     */
    [[nodiscard]] auto operator!=(const endpoint_info& other) const noexcept -> bool {
        return !(*this == other);
    }
};

/**
 * @struct connection_callbacks
 * @brief Callback functions for connection events
 *
 * Groups all callback functions that can be registered for connection
 * lifecycle events. All callbacks are optional (can be empty).
 *
 * ### Thread Safety
 * Callbacks may be invoked from I/O threads. Implementations must ensure
 * their callbacks are thread-safe if they access shared state.
 *
 * ### Examples
 * @code
 * connection_callbacks cbs;
 * cbs.on_connected = []() {
 *     std::cout << "Connected!" << std::endl;
 * };
 * cbs.on_data = [](std::span<const std::byte> data) {
 *     // Process received data
 * };
 * cbs.on_disconnected = []() {
 *     std::cout << "Disconnected!" << std::endl;
 * };
 * cbs.on_error = [](std::error_code ec) {
 *     std::cerr << "Error: " << ec.message() << std::endl;
 * };
 * @endcode
 */
struct connection_callbacks {
    /// Called when connection is established
    std::function<void()> on_connected;

    /// Called when data is received (raw bytes)
    std::function<void(std::span<const std::byte>)> on_data;

    /// Called when connection is closed
    std::function<void()> on_disconnected;

    /// Called when an error occurs
    std::function<void(std::error_code)> on_error;
};

/**
 * @struct listener_callbacks
 * @brief Callback functions for listener/server events
 *
 * Groups all callback functions that can be registered for server-side
 * events including new connections and errors.
 *
 * ### Thread Safety
 * Callbacks may be invoked from I/O threads.
 */
struct listener_callbacks {
    /// Called when a new connection is accepted (connection ID)
    std::function<void(std::string_view)> on_accept;

    /// Called when data is received from a connection (connection ID, data)
    std::function<void(std::string_view, std::span<const std::byte>)> on_data;

    /// Called when a connection is closed (connection ID)
    std::function<void(std::string_view)> on_disconnect;

    /// Called when an error occurs (connection ID, error)
    std::function<void(std::string_view, std::error_code)> on_error;
};

/**
 * @struct connection_options
 * @brief Configuration options for connections
 *
 * Provides optional configuration parameters that can be set on connections.
 */
struct connection_options {
    /// Connection timeout duration (0 = no timeout)
    std::chrono::milliseconds connect_timeout{0};

    /// Read operation timeout (0 = no timeout)
    std::chrono::milliseconds read_timeout{0};

    /// Write operation timeout (0 = no timeout)
    std::chrono::milliseconds write_timeout{0};

    /// Enable TCP keep-alive (where applicable)
    bool keep_alive = false;

    /// TCP no-delay (disable Nagle's algorithm)
    bool no_delay = false;
};

}  // namespace kcenon::network::unified
