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

#include "kcenon/network/unified/i_listener.h"
#include "kcenon/network/protocol/quic.h"
#include "kcenon/network/protocols/quic/connection.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace kcenon::network::unified::adapters {

/**
 * @class quic_listener_adapter
 * @brief Adapter that implements i_listener for QUIC protocol
 *
 * This adapter creates a QUIC server that accepts incoming connections.
 * Each accepted connection is managed internally and can be accessed via
 * the listener's connection management methods.
 *
 * ### QUIC Server Specifics
 * - Requires TLS certificates (cert_file and key_file)
 * - Supports 0-RTT for session resumption
 * - Multiplexed streams per connection
 * - Connection migration support
 *
 * ### Thread Safety
 * Thread-safe. All methods can be called from any thread.
 *
 * ### Connection Management
 * Accepted connections are tracked internally and can be accessed via
 * send_to(), broadcast(), and close_connection() methods.
 */
class quic_listener_adapter : public i_listener {
public:
    /**
     * @brief Constructs an adapter with QUIC configuration
     * @param config QUIC configuration options (cert_file and key_file required)
     * @param listener_id Unique identifier for this listener
     */
    quic_listener_adapter(const protocol::quic::quic_config& config,
                          std::string_view listener_id);

    /**
     * @brief Destructor ensures proper cleanup
     */
    ~quic_listener_adapter() override;

    // Non-copyable, non-movable (contains mutex and thread)
    quic_listener_adapter(const quic_listener_adapter&) = delete;
    quic_listener_adapter& operator=(const quic_listener_adapter&) = delete;
    quic_listener_adapter(quic_listener_adapter&&) = delete;
    quic_listener_adapter& operator=(quic_listener_adapter&&) = delete;

    // =========================================================================
    // i_listener interface implementation
    // =========================================================================

    [[nodiscard]] auto start(const endpoint_info& bind_address) -> VoidResult override;
    [[nodiscard]] auto start(uint16_t port) -> VoidResult override;
    auto stop() noexcept -> void override;
    auto set_callbacks(listener_callbacks callbacks) -> void override;
    auto set_accept_callback(accept_callback_t callback) -> void override;
    [[nodiscard]] auto is_listening() const noexcept -> bool override;
    [[nodiscard]] auto local_endpoint() const noexcept -> endpoint_info override;
    [[nodiscard]] auto connection_count() const noexcept -> size_t override;
    [[nodiscard]] auto send_to(std::string_view connection_id,
                               std::span<const std::byte> data) -> VoidResult override;
    [[nodiscard]] auto broadcast(std::span<const std::byte> data) -> VoidResult override;
    auto close_connection(std::string_view connection_id) noexcept -> void override;
    auto wait_for_stop() -> void override;

private:
    /**
     * @brief Accept thread function
     */
    auto accept_thread_func() -> void;

    /**
     * @brief Process incoming packets for all connections
     */
    auto process_connections() -> void;

    /**
     * @brief Generate unique connection ID
     */
    auto generate_connection_id(const protocols::quic::connection_id& cid) -> std::string;

    /**
     * @brief Handle new connection
     */
    auto handle_new_connection(std::unique_ptr<protocols::quic::connection> conn,
                               const std::string& conn_id) -> void;

    /**
     * @brief Setup UDP socket for listening
     */
    [[nodiscard]] auto setup_socket(const endpoint_info& bind_address) -> VoidResult;

    std::string listener_id_;
    protocol::quic::quic_config config_;

    // UDP socket for QUIC transport
    int socket_fd_{-1};

    // Accept thread
    std::thread accept_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};

    // Callbacks
    mutable std::mutex callbacks_mutex_;
    listener_callbacks callbacks_;
    accept_callback_t accept_callback_;

    // Endpoint information
    mutable std::mutex endpoint_mutex_;
    endpoint_info local_endpoint_;

    // Connection tracking
    struct connection_info {
        std::unique_ptr<protocols::quic::connection> conn;
        std::chrono::steady_clock::time_point last_activity;
    };

    mutable std::mutex connections_mutex_;
    std::unordered_map<std::string, connection_info> connections_;

    // Stop synchronization
    mutable std::mutex stop_mutex_;
    std::condition_variable stop_cv_;
};

}  // namespace kcenon::network::unified::adapters
