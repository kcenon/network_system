/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "kcenon/network/detail/utils/result_types.h"

#include <cstdint>
#include <optional>

namespace kcenon::network::protocols::quic
{

/*!
 * \brief Flow control error codes
 */
namespace flow_control_error
{
    constexpr int send_blocked = -710;
    constexpr int receive_overflow = -711;
    constexpr int window_exceeded = -712;
} // namespace flow_control_error

/*!
 * \class flow_controller
 * \brief Connection-level flow control for QUIC (RFC 9000 Section 4)
 *
 * QUIC provides connection-level and stream-level flow control.
 * This class handles connection-level flow control, tracking the total
 * amount of data that can be sent/received across all streams.
 */
class flow_controller
{
public:
    /*!
     * \brief Construct a flow controller
     * \param initial_window Initial window size (from transport parameters)
     */
    explicit flow_controller(uint64_t initial_window = 1048576);

    ~flow_controller() = default;

    // Copyable
    flow_controller(const flow_controller&) = default;
    auto operator=(const flow_controller&) -> flow_controller& = default;

    // Movable
    flow_controller(flow_controller&&) noexcept = default;
    auto operator=(flow_controller&&) noexcept -> flow_controller& = default;

    // ========================================================================
    // Send Side (Connection-level limits on our sending)
    // ========================================================================

    /*!
     * \brief Get available send window
     * \return Number of bytes we can send
     */
    [[nodiscard]] auto available_send_window() const noexcept -> uint64_t;

    /*!
     * \brief Try to consume send window for outgoing data
     * \param bytes Number of bytes to consume
     * \return Success or error if blocked
     */
    [[nodiscard]] auto consume_send_window(uint64_t bytes) -> VoidResult;

    /*!
     * \brief Update the send limit (from peer's MAX_DATA)
     * \param max_data New maximum data limit
     */
    void update_send_limit(uint64_t max_data);

    /*!
     * \brief Check if send is blocked
     */
    [[nodiscard]] auto is_send_blocked() const noexcept -> bool;

    /*!
     * \brief Get the current send limit (peer's MAX_DATA)
     */
    [[nodiscard]] auto send_limit() const noexcept -> uint64_t { return send_limit_; }

    /*!
     * \brief Get total bytes sent
     */
    [[nodiscard]] auto bytes_sent() const noexcept -> uint64_t { return bytes_sent_; }

    // ========================================================================
    // Receive Side (Connection-level limits on peer's sending)
    // ========================================================================

    /*!
     * \brief Record received data
     * \param bytes Number of bytes received
     * \return Success or error if limit exceeded
     */
    [[nodiscard]] auto record_received(uint64_t bytes) -> VoidResult;

    /*!
     * \brief Record data consumed by application
     * \param bytes Number of bytes consumed
     */
    void record_consumed(uint64_t bytes);

    /*!
     * \brief Get our receive limit (advertised as MAX_DATA)
     */
    [[nodiscard]] auto receive_limit() const noexcept -> uint64_t { return receive_limit_; }

    /*!
     * \brief Get total bytes received
     */
    [[nodiscard]] auto bytes_received() const noexcept -> uint64_t { return bytes_received_; }

    /*!
     * \brief Get bytes consumed by application
     */
    [[nodiscard]] auto bytes_consumed() const noexcept -> uint64_t { return bytes_consumed_; }

    // ========================================================================
    // Flow Control Frame Generation
    // ========================================================================

    /*!
     * \brief Check if MAX_DATA frame should be sent
     */
    [[nodiscard]] auto should_send_max_data() const noexcept -> bool;

    /*!
     * \brief Generate MAX_DATA value if update needed
     * \return New MAX_DATA value or nullopt
     */
    [[nodiscard]] auto generate_max_data() -> std::optional<uint64_t>;

    /*!
     * \brief Check if DATA_BLOCKED frame should be sent
     */
    [[nodiscard]] auto should_send_data_blocked() const noexcept -> bool;

    /*!
     * \brief Mark DATA_BLOCKED as sent (to avoid repeated sending)
     */
    void mark_data_blocked_sent();

    // ========================================================================
    // Configuration
    // ========================================================================

    /*!
     * \brief Set the window size for auto-tuning
     * \param window New window size
     */
    void set_window_size(uint64_t window);

    /*!
     * \brief Get the current window size
     */
    [[nodiscard]] auto window_size() const noexcept -> uint64_t { return window_size_; }

    /*!
     * \brief Set the threshold for sending MAX_DATA updates (0.0 - 1.0)
     * \param threshold Fraction of window that triggers update
     */
    void set_update_threshold(double threshold);

    // ========================================================================
    // Reset
    // ========================================================================

    /*!
     * \brief Reset flow controller state
     * \param initial_window New initial window size
     */
    void reset(uint64_t initial_window);

private:
    // Send side (peer's limits on us)
    uint64_t send_limit_{0};       // Peer's MAX_DATA (our send limit)
    uint64_t bytes_sent_{0};       // Total bytes we've sent
    bool data_blocked_sent_{false}; // DATA_BLOCKED already sent

    // Receive side (our limits on peer)
    uint64_t receive_limit_{0};    // Our MAX_DATA (peer's send limit)
    uint64_t bytes_received_{0};   // Total bytes received
    uint64_t bytes_consumed_{0};   // Bytes consumed by application

    // Window management
    uint64_t window_size_;
    double update_threshold_{0.5}; // Send MAX_DATA when 50% consumed

    // Track if we need to send MAX_DATA
    uint64_t last_sent_max_data_{0};
};

/*!
 * \struct flow_control_stats
 * \brief Statistics for flow control monitoring
 */
struct flow_control_stats
{
    // Send side
    uint64_t send_limit{0};
    uint64_t bytes_sent{0};
    uint64_t send_window_available{0};
    bool send_blocked{false};

    // Receive side
    uint64_t receive_limit{0};
    uint64_t bytes_received{0};
    uint64_t bytes_consumed{0};
    uint64_t receive_window_available{0};
};

/*!
 * \brief Get flow control statistics
 * \param fc Flow controller to query
 * \return Flow control statistics
 */
[[nodiscard]] auto get_flow_control_stats(const flow_controller& fc) -> flow_control_stats;

} // namespace kcenon::network::protocols::quic
