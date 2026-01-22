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

#include "types.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::unified {

/**
 * @interface i_transport
 * @brief Core interface for data transport abstraction
 *
 * This interface defines the fundamental operations for sending and receiving
 * data across any network transport. It serves as the base abstraction that
 * all protocol-specific implementations share.
 *
 * ### Design Philosophy
 * The i_transport interface embodies Kent Beck's "Fewest Elements" rule by
 * providing a minimal, protocol-agnostic API for data transfer. Protocol-
 * specific details are handled by factory functions, not interface variations.
 *
 * ### Supported Operations
 * - **Send**: Transmit raw bytes to the remote endpoint
 * - **State Query**: Check connection status
 * - **Endpoint Info**: Get remote endpoint information
 *
 * ### Thread Safety
 * All public methods must be thread-safe. Implementations should use
 * appropriate synchronization for internal state.
 *
 * ### Usage Example
 * @code
 * // Works with any transport implementation
 * void send_message(i_transport& transport, std::span<const std::byte> data) {
 *     if (!transport.is_connected()) {
 *         return; // Not connected
 *     }
 *
 *     auto result = transport.send(data);
 *     if (!result) {
 *         // Handle error
 *         std::cerr << "Send failed: " << result.error().message << std::endl;
 *     }
 * }
 * @endcode
 *
 * @see i_connection - Active connection (client-side or accepted)
 * @see i_listener - Passive listener (server-side)
 */
class i_transport {
public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes
     */
    virtual ~i_transport() = default;

    // Non-copyable (interface class, typically used via unique_ptr/shared_ptr)
    i_transport(const i_transport&) = delete;
    i_transport& operator=(const i_transport&) = delete;

    // Movable (for transfer of ownership)
    i_transport(i_transport&&) = default;
    i_transport& operator=(i_transport&&) = default;

    // =========================================================================
    // Data Transfer Operations
    // =========================================================================

    /**
     * @brief Sends raw data to the remote endpoint
     * @param data The data to send as a span of bytes
     * @return VoidResult indicating success or failure
     *
     * ### Error Conditions
     * - Returns error if not connected
     * - Returns error if send operation fails
     * - Returns error if data size exceeds protocol limits
     *
     * ### Thread Safety
     * Thread-safe. Multiple sends may be queued internally.
     *
     * ### Performance Note
     * Data is typically copied to an internal send buffer. For zero-copy
     * sends, see protocol-specific implementations.
     */
    [[nodiscard]] virtual auto send(std::span<const std::byte> data) -> VoidResult = 0;

    /**
     * @brief Sends data from a uint8_t vector
     * @param data The data to send as a vector of uint8_t
     * @return VoidResult indicating success or failure
     *
     * Convenience overload for compatibility with existing code using
     * std::vector<uint8_t>.
     */
    [[nodiscard]] virtual auto send(std::vector<uint8_t>&& data) -> VoidResult = 0;

    // =========================================================================
    // State Query Operations
    // =========================================================================

    /**
     * @brief Checks if the transport is currently connected
     * @return true if connected and ready to send/receive, false otherwise
     *
     * ### Thread Safety
     * Thread-safe. Uses atomic operations internally.
     *
     * ### Note
     * A false return may indicate:
     * - Connection not yet established
     * - Connection closed by remote peer
     * - Connection closed locally
     * - Network error occurred
     */
    [[nodiscard]] virtual auto is_connected() const noexcept -> bool = 0;

    /**
     * @brief Gets the unique identifier for this transport/connection
     * @return A string view of the transport ID
     *
     * The ID is unique within the application and remains constant
     * for the lifetime of the transport instance.
     */
    [[nodiscard]] virtual auto id() const noexcept -> std::string_view = 0;

    /**
     * @brief Gets the remote endpoint information
     * @return endpoint_info of the remote peer
     *
     * Returns valid information only when connected.
     * Returns empty endpoint_info if not connected.
     */
    [[nodiscard]] virtual auto remote_endpoint() const noexcept -> endpoint_info = 0;

    /**
     * @brief Gets the local endpoint information
     * @return endpoint_info of the local side
     *
     * Returns valid information only when connected/listening.
     * Returns empty endpoint_info if not bound.
     */
    [[nodiscard]] virtual auto local_endpoint() const noexcept -> endpoint_info = 0;

protected:
    /**
     * @brief Default constructor (only for derived classes)
     */
    i_transport() = default;
};

}  // namespace kcenon::network::unified
