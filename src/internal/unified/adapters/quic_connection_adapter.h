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
#include "kcenon/network/protocol/quic.h"
#include "internal/protocols/quic/connection.h"
#include "internal/protocols/quic/stream.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace kcenon::network::unified::adapters {

/**
 * @class quic_connection_adapter
 * @brief Adapter that wraps QUIC connection to implement i_connection
 *
 * This adapter bridges the QUIC protocol implementation with the unified
 * interface, enabling protocol factory functions to return i_connection
 * while using the QUIC connection underneath.
 *
 * ### QUIC Specifics
 * - Uses Stream 0 (bidirectional) for default data transfer
 * - Supports multiplexed streams internally
 * - Built-in TLS 1.3 encryption
 * - Connection migration support
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * ### Ownership
 * The adapter owns the underlying QUIC connection and manages its lifecycle.
 */
class quic_connection_adapter : public i_connection {
public:
    /**
     * @brief Constructs an adapter with QUIC configuration
     * @param config QUIC configuration options
     * @param connection_id Unique identifier for this connection
     */
    quic_connection_adapter(const protocol::quic::quic_config& config,
                            std::string_view connection_id);

    /**
     * @brief Destructor ensures proper cleanup
     */
    ~quic_connection_adapter() override;

    // Non-copyable, non-movable (contains mutex and thread)
    quic_connection_adapter(const quic_connection_adapter&) = delete;
    quic_connection_adapter& operator=(const quic_connection_adapter&) = delete;
    quic_connection_adapter(quic_connection_adapter&&) = delete;
    quic_connection_adapter& operator=(quic_connection_adapter&&) = delete;

    // =========================================================================
    // i_transport interface implementation
    // =========================================================================

    [[nodiscard]] auto send(std::span<const std::byte> data) -> VoidResult override;
    [[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;
    [[nodiscard]] auto is_connected() const noexcept -> bool override;
    [[nodiscard]] auto id() const noexcept -> std::string_view override;
    [[nodiscard]] auto remote_endpoint() const noexcept -> endpoint_info override;
    [[nodiscard]] auto local_endpoint() const noexcept -> endpoint_info override;

    // =========================================================================
    // i_connection interface implementation
    // =========================================================================

    [[nodiscard]] auto connect(const endpoint_info& endpoint) -> VoidResult override;
    [[nodiscard]] auto connect(std::string_view url) -> VoidResult override;
    auto close() noexcept -> void override;
    auto set_callbacks(connection_callbacks callbacks) -> void override;
    auto set_options(connection_options options) -> void override;
    auto set_timeout(std::chrono::milliseconds timeout) -> void override;
    [[nodiscard]] auto is_connecting() const noexcept -> bool override;
    auto wait_for_stop() -> void override;

private:
    /**
     * @brief I/O processing thread function
     */
    auto io_thread_func() -> void;

    /**
     * @brief Process outgoing packets
     */
    auto process_outgoing() -> void;

    /**
     * @brief Process incoming packets
     */
    auto process_incoming() -> void;

    /**
     * @brief Handle connection state changes
     */
    auto handle_state_change() -> void;

    /**
     * @brief Parse URL to extract host and port
     */
    auto parse_url(std::string_view url) -> std::pair<std::string, uint16_t>;

    std::string connection_id_;
    protocol::quic::quic_config config_;

    // QUIC connection (owned)
    std::unique_ptr<protocols::quic::connection> quic_conn_;

    // UDP socket for QUIC transport
    int socket_fd_{-1};

    // I/O thread
    std::thread io_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Callbacks
    mutable std::mutex callbacks_mutex_;
    connection_callbacks callbacks_;

    // Endpoint information
    mutable std::mutex endpoint_mutex_;
    endpoint_info remote_endpoint_;
    endpoint_info local_endpoint_;

    // State
    std::atomic<bool> is_connecting_{false};
    std::atomic<bool> is_connected_{false};
    connection_options options_;

    // Stop synchronization
    mutable std::mutex stop_mutex_;
    std::condition_variable stop_cv_;

    // Send queue
    mutable std::mutex send_queue_mutex_;
    std::deque<std::vector<uint8_t>> send_queue_;
};

}  // namespace kcenon::network::unified::adapters
