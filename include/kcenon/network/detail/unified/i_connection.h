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

#include "i_transport.h"
#include "types.h"

#include <chrono>

namespace kcenon::network::unified {

/**
 * @interface i_connection
 * @brief Core interface for active network connections
 *
 * This interface extends i_transport with connection lifecycle operations.
 * It represents:
 * - Client-side connections (initiated by connect())
 * - Accepted connections (from i_listener::accept())
 *
 * ### Design Philosophy
 * By inheriting from i_transport, i_connection provides a unified interface
 * for data transfer while adding connection-specific operations. This allows
 * code that only needs to send/receive data to work with just i_transport,
 * while connection management code uses i_connection.
 *
 * ### Lifecycle
 * 1. Create connection via protocol factory (or accept from listener)
 * 2. Optionally configure callbacks and options
 * 3. Connect to remote endpoint (if not already accepted)
 * 4. Send/receive data via inherited i_transport methods
 * 5. Close connection when done
 *
 * ### Thread Safety
 * All public methods must be thread-safe.
 *
 * ### Usage Example
 * @code
 * // Create connection via protocol factory
 * auto conn = protocol::tcp::connect({"localhost", 8080});
 *
 * // Set callbacks
 * conn->set_callbacks({
 *     .on_connected = []() { std::cout << "Connected!\n"; },
 *     .on_data = [](std::span<const std::byte> data) {
 *         // Process received data
 *     },
 *     .on_disconnected = []() { std::cout << "Disconnected\n"; },
 *     .on_error = [](std::error_code ec) {
 *         std::cerr << "Error: " << ec.message() << "\n";
 *     }
 * });
 *
 * // Connect (may be async depending on implementation)
 * if (auto result = conn->connect({"remote.host.com", 9000}); !result) {
 *     std::cerr << "Connect failed\n";
 *     return;
 * }
 *
 * // Send data (inherited from i_transport)
 * std::vector<std::byte> data = ...;
 * conn->send(data);
 *
 * // Close when done
 * conn->close();
 * @endcode
 *
 * @see i_transport - Base data transport interface
 * @see i_listener - Server-side connection acceptor
 */
class i_connection : public i_transport {
public:
    /**
     * @brief Virtual destructor
     *
     * Closing the connection is implicit when the i_connection is destroyed.
     */
    ~i_connection() override = default;

    // Non-copyable, movable (inherited from i_transport)
    i_connection(const i_connection&) = delete;
    i_connection& operator=(const i_connection&) = delete;
    i_connection(i_connection&&) = default;
    i_connection& operator=(i_connection&&) = default;

    // =========================================================================
    // Connection Lifecycle Operations
    // =========================================================================

    /**
     * @brief Connects to a remote endpoint using host/port
     * @param endpoint The remote endpoint to connect to
     * @return VoidResult indicating success or failure
     *
     * ### Behavior
     * - Initiates connection to the specified endpoint
     * - May block until connection is established (sync mode)
     * - May return immediately and notify via callback (async mode)
     *
     * ### Error Conditions
     * - Returns error if already connected
     * - Returns error if connection fails
     * - Returns error if host resolution fails
     * - Returns error if connection timeout expires
     *
     * ### Thread Safety
     * Thread-safe. Only one connect operation can succeed at a time.
     */
    [[nodiscard]] virtual auto connect(const endpoint_info& endpoint) -> VoidResult = 0;

    /**
     * @brief Connects to a remote endpoint using URL
     * @param url The URL to connect to (e.g., "wss://example.com/ws")
     * @return VoidResult indicating success or failure
     *
     * This overload is primarily for URL-based protocols like WebSocket
     * and HTTP. For socket-based protocols, use the endpoint_info overload.
     *
     * ### Error Conditions
     * Same as connect(endpoint_info) plus:
     * - Returns error if URL is malformed
     * - Returns error if protocol is not supported
     */
    [[nodiscard]] virtual auto connect(std::string_view url) -> VoidResult = 0;

    /**
     * @brief Closes the connection gracefully
     *
     * ### Behavior
     * - Initiates graceful shutdown
     * - Pending sends may be completed before close
     * - Triggers on_disconnected callback when fully closed
     *
     * ### Thread Safety
     * Thread-safe. Safe to call multiple times.
     *
     * ### Note
     * After close(), is_connected() returns false and send() returns error.
     */
    virtual auto close() noexcept -> void = 0;

    // =========================================================================
    // Configuration Operations
    // =========================================================================

    /**
     * @brief Sets all connection callbacks at once
     * @param callbacks The callback structure with handlers
     *
     * Replaces all previously set callbacks. Empty callback functions
     * in the structure will clear the corresponding handler.
     *
     * ### Thread Safety
     * Thread-safe, but callbacks may be invoked from I/O threads.
     */
    virtual auto set_callbacks(connection_callbacks callbacks) -> void = 0;

    /**
     * @brief Sets connection options
     * @param options The configuration options
     *
     * Some options may only be effective before connect() is called.
     *
     * ### Thread Safety
     * Thread-safe. Changes may not affect in-flight operations.
     */
    virtual auto set_options(connection_options options) -> void = 0;

    /**
     * @brief Sets the connection timeout
     * @param timeout Timeout duration (0 = no timeout)
     *
     * Shorthand for setting just the connect_timeout option.
     */
    virtual auto set_timeout(std::chrono::milliseconds timeout) -> void = 0;

    // =========================================================================
    // State Query Operations
    // =========================================================================

    /**
     * @brief Checks if the connection is in the process of connecting
     * @return true if connect() was called but not yet completed
     */
    [[nodiscard]] virtual auto is_connecting() const noexcept -> bool = 0;

    /**
     * @brief Blocks until the component has stopped
     *
     * Waits for all pending operations to complete and the connection
     * to be fully closed.
     *
     * ### Thread Safety
     * Safe to call from any thread. Uses internal synchronization.
     */
    virtual auto wait_for_stop() -> void = 0;

protected:
    /**
     * @brief Default constructor (only for derived classes)
     */
    i_connection() = default;
};

}  // namespace kcenon::network::unified
