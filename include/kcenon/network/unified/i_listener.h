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

#include "i_connection.h"
#include "types.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace kcenon::network::unified {

/**
 * @interface i_listener
 * @brief Core interface for passive network listeners (server-side)
 *
 * This interface represents a server-side network component that listens
 * for incoming connections. It provides:
 * - Binding to local addresses
 * - Accepting incoming connections
 * - Connection management via callbacks
 *
 * ### Design Philosophy
 * The i_listener interface separates server-side concerns from client-side
 * concerns, while accepted connections share the same i_connection interface
 * as client-initiated connections. This allows code to work with connections
 * regardless of their origin.
 *
 * ### Lifecycle
 * 1. Create listener via protocol factory
 * 2. Configure callbacks for connection events
 * 3. Start listening on a local endpoint
 * 4. Handle incoming connections via callbacks
 * 5. Stop listening when done
 *
 * ### Thread Safety
 * All public methods must be thread-safe. Callbacks may be invoked from
 * I/O threads.
 *
 * ### Usage Example
 * @code
 * // Create listener via protocol factory
 * auto listener = protocol::tcp::listen({"0.0.0.0", 8080});
 *
 * // Set callbacks for connection events
 * listener->set_callbacks({
 *     .on_accept = [](std::string_view conn_id) {
 *         std::cout << "New connection: " << conn_id << "\n";
 *     },
 *     .on_data = [](std::string_view conn_id, std::span<const std::byte> data) {
 *         // Process received data from connection
 *     },
 *     .on_disconnect = [](std::string_view conn_id) {
 *         std::cout << "Connection closed: " << conn_id << "\n";
 *     },
 *     .on_error = [](std::string_view conn_id, std::error_code ec) {
 *         std::cerr << "Error on " << conn_id << ": " << ec.message() << "\n";
 *     }
 * });
 *
 * // Start listening
 * if (auto result = listener->start({"::", 8080}); !result) {
 *     std::cerr << "Failed to start listener\n";
 *     return;
 * }
 *
 * // Server is now accepting connections...
 * // Later, stop the listener
 * listener->stop();
 * @endcode
 *
 * @see i_connection - Interface for accepted connections
 * @see i_transport - Base data transport interface
 */
class i_listener {
public:
    /**
     * @brief Callback for accepted connections
     *
     * Called when a new connection is accepted. The callback receives
     * ownership of the connection via unique_ptr.
     */
    using accept_callback_t = std::function<void(std::unique_ptr<i_connection>)>;

    /**
     * @brief Virtual destructor
     *
     * Stops listening and closes all connections when destroyed.
     */
    virtual ~i_listener() = default;

    // Non-copyable (interface class)
    i_listener(const i_listener&) = delete;
    i_listener& operator=(const i_listener&) = delete;

    // Movable
    i_listener(i_listener&&) = default;
    i_listener& operator=(i_listener&&) = default;

    // =========================================================================
    // Listener Lifecycle Operations
    // =========================================================================

    /**
     * @brief Starts listening for incoming connections
     * @param bind_address The local address to bind to
     * @return VoidResult indicating success or failure
     *
     * ### Behavior
     * - Binds to the specified local address
     * - Begins accepting incoming connections
     * - Accepted connections are delivered via callbacks
     *
     * ### Error Conditions
     * - Returns error if already listening
     * - Returns error if bind fails (e.g., address in use)
     * - Returns error if listen fails
     *
     * ### Thread Safety
     * Thread-safe. Only one start operation can succeed at a time.
     */
    [[nodiscard]] virtual auto start(const endpoint_info& bind_address) -> VoidResult = 0;

    /**
     * @brief Starts listening on a specific port (all interfaces)
     * @param port The port number to listen on
     * @return VoidResult indicating success or failure
     *
     * Convenience overload that binds to all interfaces (0.0.0.0 / ::).
     */
    [[nodiscard]] virtual auto start(uint16_t port) -> VoidResult = 0;

    /**
     * @brief Stops listening and closes all connections
     *
     * ### Behavior
     * - Stops accepting new connections
     * - Closes all active connections
     * - Triggers on_disconnect callback for each connection
     *
     * ### Thread Safety
     * Thread-safe. Safe to call multiple times.
     */
    virtual auto stop() noexcept -> void = 0;

    // =========================================================================
    // Configuration Operations
    // =========================================================================

    /**
     * @brief Sets all listener callbacks at once
     * @param callbacks The callback structure with handlers
     *
     * Replaces all previously set callbacks.
     *
     * ### Thread Safety
     * Thread-safe, but callbacks are invoked from I/O threads.
     */
    virtual auto set_callbacks(listener_callbacks callbacks) -> void = 0;

    /**
     * @brief Sets the accept callback for new connections
     * @param callback Called when a connection is accepted
     *
     * This callback receives ownership of the connection object.
     * If set, the on_accept callback in listener_callbacks is not called.
     *
     * ### Thread Safety
     * Thread-safe. Callback invoked from I/O thread.
     */
    virtual auto set_accept_callback(accept_callback_t callback) -> void = 0;

    // =========================================================================
    // State Query Operations
    // =========================================================================

    /**
     * @brief Checks if the listener is currently listening
     * @return true if listening for connections, false otherwise
     */
    [[nodiscard]] virtual auto is_listening() const noexcept -> bool = 0;

    /**
     * @brief Gets the local endpoint the listener is bound to
     * @return endpoint_info of the local binding
     *
     * Returns valid information only when listening.
     * Returns empty endpoint_info if not bound.
     */
    [[nodiscard]] virtual auto local_endpoint() const noexcept -> endpoint_info = 0;

    /**
     * @brief Gets the number of active connections
     * @return The count of currently connected clients
     */
    [[nodiscard]] virtual auto connection_count() const noexcept -> size_t = 0;

    /**
     * @brief Sends data to a specific connection
     * @param connection_id The ID of the target connection
     * @param data The data to send
     * @return VoidResult indicating success or failure
     *
     * ### Error Conditions
     * - Returns error if connection_id not found
     * - Returns error if send fails
     */
    [[nodiscard]] virtual auto send_to(std::string_view connection_id,
                                       std::span<const std::byte> data) -> VoidResult = 0;

    /**
     * @brief Broadcasts data to all connected clients
     * @param data The data to send to all connections
     * @return VoidResult indicating success or failure
     *
     * ### Behavior
     * - Sends data to all currently connected clients
     * - Returns success if at least one send succeeded
     * - Failures on individual connections are logged but don't fail the call
     */
    [[nodiscard]] virtual auto broadcast(std::span<const std::byte> data) -> VoidResult = 0;

    /**
     * @brief Closes a specific connection
     * @param connection_id The ID of the connection to close
     *
     * Triggers the on_disconnect callback for the connection.
     */
    virtual auto close_connection(std::string_view connection_id) noexcept -> void = 0;

    /**
     * @brief Blocks until the listener has stopped
     *
     * Waits for all connections to close and the listener to fully stop.
     *
     * ### Thread Safety
     * Safe to call from any thread.
     */
    virtual auto wait_for_stop() -> void = 0;

protected:
    /**
     * @brief Default constructor (only for derived classes)
     */
    i_listener() = default;
};

}  // namespace kcenon::network::unified
