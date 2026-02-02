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

#include "ecn_tracker.h"
#include "frame_types.h"
#include "keys.h"
#include "rtt_estimator.h"

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace kcenon::network::protocols::quic
{

/*!
 * \struct sent_packet
 * \brief Information about a sent packet for loss detection (RFC 9002 Section A.1.1)
 */
struct sent_packet
{
    //! Packet number
    uint64_t packet_number{0};

    //! Time the packet was sent
    std::chrono::steady_clock::time_point sent_time;

    //! Number of bytes in the packet
    size_t sent_bytes{0};

    //! True if this packet is ack-eliciting
    bool ack_eliciting{false};

    //! True if the packet is in flight (counted for congestion control)
    bool in_flight{false};

    //! Encryption level of the packet
    encryption_level level{encryption_level::initial};

    //! Frames included in this packet (for retransmission)
    std::vector<frame> frames;
};

/*!
 * \enum loss_detection_event
 * \brief Events that can occur during loss detection
 */
enum class loss_detection_event
{
    none,           //!< No event
    packet_lost,    //!< Packet(s) declared lost
    pto_expired,    //!< Probe timeout expired
};

/*!
 * \struct loss_detection_result
 * \brief Result of loss detection operations
 */
struct loss_detection_result
{
    //! Event that occurred
    loss_detection_event event{loss_detection_event::none};

    //! Packets that were acknowledged
    std::vector<sent_packet> acked_packets;

    //! Packets that were declared lost
    std::vector<sent_packet> lost_packets;

    //! ECN signal from ACK_ECN frame processing
    ecn_result ecn_signal{ecn_result::none};

    //! Sent time of the packet that triggered ECN congestion signal
    //! (used for congestion recovery tracking)
    std::chrono::steady_clock::time_point ecn_congestion_sent_time{};
};

/*!
 * \class loss_detector
 * \brief QUIC loss detection (RFC 9002 Section 6)
 *
 * Implements packet loss detection using both packet number threshold
 * and time threshold algorithms as specified in RFC 9002.
 */
class loss_detector
{
public:
    /*!
     * \brief Constructor
     * \param rtt Reference to RTT estimator (must outlive this object)
     */
    explicit loss_detector(rtt_estimator& rtt);

    /*!
     * \brief Record a sent packet
     * \param packet Information about the sent packet
     */
    auto on_packet_sent(const sent_packet& packet) -> void;

    /*!
     * \brief Process received ACK frame (RFC 9002 Section 6)
     * \param ack ACK frame received from peer
     * \param level Encryption level of the ACK
     * \param recv_time Time the ACK was received
     * \return Loss detection result with acked and lost packets
     */
    [[nodiscard]] auto on_ack_received(const ack_frame& ack,
                                       encryption_level level,
                                       std::chrono::steady_clock::time_point recv_time)
        -> loss_detection_result;

    /*!
     * \brief Get the time of the next scheduled timeout
     * \return Next timeout time point, or nullopt if no timer is set
     */
    [[nodiscard]] auto next_timeout() const
        -> std::optional<std::chrono::steady_clock::time_point>;

    /*!
     * \brief Handle timeout expiry (RFC 9002 Section 6.2)
     * \return Loss detection result
     */
    [[nodiscard]] auto on_timeout() -> loss_detection_result;

    /*!
     * \brief Get the largest acknowledged packet number for a space
     * \param level Encryption level (maps to packet number space)
     * \return Largest acknowledged packet number
     */
    [[nodiscard]] auto largest_acked(encryption_level level) const noexcept -> uint64_t;

    /*!
     * \brief Get the current PTO count
     * \return Number of times PTO has fired without receiving an ACK
     */
    [[nodiscard]] auto pto_count() const noexcept -> uint32_t
    {
        return pto_count_;
    }

    /*!
     * \brief Reset PTO count to zero
     */
    auto reset_pto_count() noexcept -> void
    {
        pto_count_ = 0;
    }

    /*!
     * \brief Check if there are any unacked packets in the given space
     * \param level Encryption level
     * \return True if there are unacked packets
     */
    [[nodiscard]] auto has_unacked_packets(encryption_level level) const -> bool;

    /*!
     * \brief Get bytes in flight for a packet number space
     * \param level Encryption level
     * \return Bytes in flight
     */
    [[nodiscard]] auto bytes_in_flight(encryption_level level) const -> size_t;

    /*!
     * \brief Get total bytes in flight across all spaces
     * \return Total bytes in flight
     */
    [[nodiscard]] auto total_bytes_in_flight() const -> size_t;

    /*!
     * \brief Discard packet number space (e.g., after handshake keys discarded)
     * \param level Encryption level to discard
     */
    auto discard_space(encryption_level level) -> void;

    /*!
     * \brief Set handshake confirmed status
     * \param confirmed True if handshake is confirmed
     */
    auto set_handshake_confirmed(bool confirmed) noexcept -> void
    {
        handshake_confirmed_ = confirmed;
    }

    /*!
     * \brief Get the ECN tracker
     * \return Reference to the ECN tracker
     */
    [[nodiscard]] auto get_ecn_tracker() noexcept -> class ecn_tracker&
    {
        return ecn_tracker_;
    }

    /*!
     * \brief Get the ECN tracker (const)
     * \return Const reference to the ECN tracker
     */
    [[nodiscard]] auto get_ecn_tracker() const noexcept -> const class ecn_tracker&
    {
        return ecn_tracker_;
    }

private:
    /*!
     * \struct space_state
     * \brief Per packet-number-space state (RFC 9002 Appendix A.1)
     */
    struct space_state
    {
        //! Largest acknowledged packet number
        uint64_t largest_acked{0};

        //! True if we've received any ACK in this space
        bool largest_acked_set{false};

        //! Time of the most recent ack-eliciting packet
        std::chrono::steady_clock::time_point time_of_last_ack_eliciting;

        //! Sent packets awaiting acknowledgment
        std::map<uint64_t, sent_packet> sent_packets;

        //! Time at which loss detection should be triggered
        std::chrono::steady_clock::time_point loss_time;

        //! Bytes in flight for this space
        size_t bytes_in_flight{0};
    };

    //! Get space index from encryption level
    [[nodiscard]] auto space_index(encryption_level level) const noexcept -> size_t;

    //! Detect lost packets (RFC 9002 Section 6.1)
    [[nodiscard]] auto detect_lost_packets(encryption_level level,
                                            std::chrono::steady_clock::time_point now)
        -> std::vector<sent_packet>;

    //! Set loss detection timer (RFC 9002 Section 6.2)
    auto set_loss_detection_timer() -> void;

    //! Get loss time and space (RFC 9002 Appendix A.8)
    [[nodiscard]] auto get_loss_time_and_space() const
        -> std::pair<std::chrono::steady_clock::time_point, encryption_level>;

    //! Get PTO time and space (RFC 9002 Appendix A.8)
    [[nodiscard]] auto get_pto_time_and_space() const
        -> std::pair<std::chrono::steady_clock::time_point, encryption_level>;

    //! Reference to RTT estimator
    rtt_estimator& rtt_;

    //! ECN tracker for ECN feedback processing
    class ecn_tracker ecn_tracker_;

    //! Per packet-number-space state (Initial, Handshake, Application)
    std::array<space_state, 3> spaces_;

    //! Number of times PTO has expired without receiving an ACK
    uint32_t pto_count_{0};

    //! True if the handshake is confirmed
    bool handshake_confirmed_{false};

    //! Scheduled loss detection timeout
    std::chrono::steady_clock::time_point loss_detection_timer_;

    //! True if loss detection timer is armed
    bool timer_armed_{false};

    //! Packet threshold (RFC 9002 Section 6.1.1)
    static constexpr uint32_t kPacketThreshold = 3;

    //! Time threshold multiplier (RFC 9002 Section 6.1.2)
    static constexpr double kTimeThreshold = 9.0 / 8.0;  // 1.125

    //! Timer granularity (RFC 9002 Section 6.1.2)
    static constexpr auto kGranularity = std::chrono::milliseconds{1};
};

} // namespace kcenon::network::protocols::quic
