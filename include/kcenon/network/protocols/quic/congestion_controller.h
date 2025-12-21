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

#include "kcenon/network/protocols/quic/loss_detector.h"
#include "kcenon/network/protocols/quic/rtt_estimator.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace kcenon::network::protocols::quic
{

/*!
 * \enum congestion_state
 * \brief States of the congestion controller (RFC 9002 Section 7)
 */
enum class congestion_state
{
    slow_start,           //!< Exponential growth of cwnd
    congestion_avoidance, //!< Linear growth of cwnd
    recovery,             //!< Congestion recovery after loss
};

/*!
 * \brief Convert congestion state to string
 */
[[nodiscard]] auto congestion_state_to_string(congestion_state state) noexcept
    -> const char*;

/*!
 * \class congestion_controller
 * \brief QUIC congestion control (RFC 9002 Section 7)
 *
 * Implements the NewReno congestion control algorithm as specified in RFC 9002.
 * Manages the congestion window, slow start threshold, and bytes in flight.
 */
class congestion_controller
{
public:
    /*!
     * \brief Default constructor
     *
     * Initializes with default parameters:
     * - Initial window: 10 * max_datagram_size (14720 bytes for 1472 byte datagrams)
     * - Minimum window: 2 * max_datagram_size
     * - Max datagram size: 1200 bytes (QUIC minimum)
     */
    congestion_controller();

    /*!
     * \brief Constructor with custom max datagram size
     * \param max_datagram_size Maximum datagram size in bytes
     */
    explicit congestion_controller(size_t max_datagram_size);

    /*!
     * \brief Check if we can send more data
     * \param bytes Number of bytes to send (default 0 means any)
     * \return True if there is available congestion window
     */
    [[nodiscard]] auto can_send(size_t bytes = 0) const noexcept -> bool;

    /*!
     * \brief Get available congestion window
     * \return Number of bytes that can be sent
     */
    [[nodiscard]] auto available_window() const noexcept -> size_t;

    /*!
     * \brief Record bytes sent
     * \param bytes Number of bytes sent
     */
    auto on_packet_sent(size_t bytes) -> void;

    /*!
     * \brief Handle packet acknowledgment (RFC 9002 Section 7.3)
     * \param packet Acknowledged packet information
     * \param ack_time Time the ACK was received
     */
    auto on_packet_acked(const sent_packet& packet,
                         std::chrono::steady_clock::time_point ack_time) -> void;

    /*!
     * \brief Handle packet loss (RFC 9002 Section 7.3.2)
     * \param packet Lost packet information
     */
    auto on_packet_lost(const sent_packet& packet) -> void;

    /*!
     * \brief Handle congestion event (RFC 9002 Section 7.3.2)
     * \param sent_time Time the packet was sent
     *
     * Called when ECN or persistent congestion is detected.
     */
    auto on_congestion_event(std::chrono::steady_clock::time_point sent_time) -> void;

    /*!
     * \brief Handle persistent congestion detection (RFC 9002 Section 7.6)
     * \param rtt RTT estimator for PTO calculation
     */
    auto on_persistent_congestion(const rtt_estimator& rtt) -> void;

    /*!
     * \brief Get current congestion window
     * \return Current congestion window in bytes
     */
    [[nodiscard]] auto cwnd() const noexcept -> size_t
    {
        return cwnd_;
    }

    /*!
     * \brief Get slow start threshold
     * \return Current slow start threshold
     */
    [[nodiscard]] auto ssthresh() const noexcept -> size_t
    {
        return ssthresh_;
    }

    /*!
     * \brief Get bytes in flight
     * \return Current bytes in flight
     */
    [[nodiscard]] auto bytes_in_flight() const noexcept -> size_t
    {
        return bytes_in_flight_;
    }

    /*!
     * \brief Get current congestion state
     * \return Current state
     */
    [[nodiscard]] auto state() const noexcept -> congestion_state
    {
        return state_;
    }

    /*!
     * \brief Get max datagram size
     * \return Maximum datagram size
     */
    [[nodiscard]] auto max_datagram_size() const noexcept -> size_t
    {
        return max_datagram_size_;
    }

    /*!
     * \brief Set max datagram size
     * \param size New maximum datagram size
     */
    auto set_max_datagram_size(size_t size) -> void;

    /*!
     * \brief Reset congestion controller to initial state
     */
    auto reset() -> void;

private:
    /*!
     * \brief Check if currently in recovery period
     * \return True if in recovery
     */
    [[nodiscard]] auto is_in_recovery(
        std::chrono::steady_clock::time_point sent_time) const noexcept -> bool;

    //! Current congestion state
    congestion_state state_{congestion_state::slow_start};

    //! Congestion window in bytes
    size_t cwnd_;

    //! Slow start threshold
    size_t ssthresh_{std::numeric_limits<size_t>::max()};

    //! Bytes currently in flight
    size_t bytes_in_flight_{0};

    //! Maximum datagram size
    size_t max_datagram_size_;

    //! Start of the current congestion recovery period
    std::chrono::steady_clock::time_point congestion_recovery_start_;

    //! Initial window size (for reset)
    size_t initial_window_;

    //! Minimum window size
    size_t minimum_window_;

    //! Loss reduction factor (RFC 9002 Section 7.3.2)
    static constexpr double kLossReductionFactor = 0.5;

    //! Default max datagram size (QUIC minimum guaranteed MTU)
    static constexpr size_t kDefaultMaxDatagramSize = 1200;

    //! Number of datagrams for initial window
    static constexpr size_t kInitialWindowPackets = 10;

    //! Minimum window in packets
    static constexpr size_t kMinimumWindowPackets = 2;
};

} // namespace kcenon::network::protocols::quic
