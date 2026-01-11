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

#include "kcenon/network/protocols/quic/frame_types.h"

#include <chrono>
#include <cstdint>

namespace kcenon::network::protocols::quic
{

/*!
 * \enum ecn_result
 * \brief Result of ECN counts processing (RFC 9000 Section 13.4)
 */
enum class ecn_result
{
    none,              //!< No congestion signal
    congestion_signal, //!< ECN-CE increased (congestion experienced)
    ecn_failure,       //!< ECN validation failed, should disable ECN
};

/*!
 * \brief Convert ecn_result to string
 */
[[nodiscard]] auto ecn_result_to_string(ecn_result result) noexcept -> const char*;

/*!
 * \enum ecn_marking
 * \brief ECN marking values for IP header (RFC 3168)
 */
enum class ecn_marking : uint8_t
{
    not_ect = 0x00, //!< Not ECN-Capable Transport
    ect1 = 0x01,    //!< ECN Capable Transport (1)
    ect0 = 0x02,    //!< ECN Capable Transport (0)
    ecn_ce = 0x03,  //!< Congestion Experienced
};

/*!
 * \class ecn_tracker
 * \brief ECN (Explicit Congestion Notification) tracker (RFC 9000 Section 13.4, RFC 9002 Section 7.1)
 *
 * Tracks ECN feedback from ACK_ECN frames and validates ECN capability.
 * ECN-CE marks indicate congestion without packet loss, providing more
 * responsive congestion detection than loss-based methods.
 */
class ecn_tracker
{
public:
    /*!
     * \brief Default constructor
     *
     * Starts in ECN testing mode. ECN capability will be validated
     * when the first ACK with ECN counts is received.
     */
    ecn_tracker();

    /*!
     * \brief Process received ECN counts from an ACK_ECN frame
     * \param counts ECN counts from the ACK frame
     * \param packets_acked Number of packets acknowledged in this ACK
     * \param sent_time Sent time of the oldest acknowledged packet (for recovery tracking)
     * \return ECN processing result (congestion signal, failure, or none)
     *
     * This method should be called for every ACK_ECN frame received.
     * It compares the new counts against previously recorded counts
     * to detect ECN-CE increases (congestion signals).
     *
     * RFC 9000 Section 13.4.2.1: Processing ECN Information
     * - If ECN-CE count increased, signal congestion to congestion controller
     * - If ECN counts decreased or don't match expectations, ECN has failed
     */
    [[nodiscard]] auto process_ecn_counts(const ecn_counts& counts,
                                          uint64_t packets_acked,
                                          std::chrono::steady_clock::time_point sent_time)
        -> ecn_result;

    /*!
     * \brief Check if ECN is validated for use
     * \return True if ECN capability has been successfully validated
     *
     * ECN is considered capable when:
     * - Testing phase is complete
     * - ECN counts have been successfully validated
     * - No ECN failure has been detected
     */
    [[nodiscard]] auto is_ecn_capable() const noexcept -> bool
    {
        return state_.capable && !state_.testing;
    }

    /*!
     * \brief Check if ECN is currently in testing phase
     * \return True if still testing ECN capability
     */
    [[nodiscard]] auto is_testing() const noexcept -> bool
    {
        return state_.testing;
    }

    /*!
     * \brief Check if ECN has failed validation
     * \return True if ECN validation has failed
     */
    [[nodiscard]] auto has_failed() const noexcept -> bool
    {
        return state_.failed;
    }

    /*!
     * \brief Get ECN marking to use for outgoing packets
     * \return ECN marking value to set in IP header
     *
     * Returns ECT(0) when ECN is capable or testing,
     * returns Not-ECT when ECN has failed.
     *
     * RFC 9000 Section 13.4.1: ECN-capable QUIC endpoints SHOULD
     * use ECT(0) codepoint.
     */
    [[nodiscard]] auto get_ecn_marking() const noexcept -> ecn_marking
    {
        if (state_.failed)
        {
            return ecn_marking::not_ect;
        }
        return ecn_marking::ect0;
    }

    /*!
     * \brief Record packets sent with ECN marking
     * \param packet_count Number of packets sent with ECN marking
     *
     * Used for ECN validation to track how many ECN-marked packets
     * have been sent.
     */
    auto on_packets_sent(uint64_t packet_count) noexcept -> void;

    /*!
     * \brief Get the sent time when the last ECN-CE congestion was detected
     * \return Time point of the packet that triggered the congestion signal
     *
     * Used by the congestion controller to determine if we're in recovery.
     */
    [[nodiscard]] auto last_congestion_sent_time() const noexcept
        -> std::chrono::steady_clock::time_point
    {
        return state_.last_congestion_sent_time;
    }

    /*!
     * \brief Get current ECN counts
     * \return Reference to current ECN counts
     */
    [[nodiscard]] auto current_counts() const noexcept -> const ecn_counts&
    {
        return state_.counts;
    }

    /*!
     * \brief Reset ECN tracker to initial state
     *
     * Resets all state including ECN counts and validation status.
     * ECN testing will restart.
     */
    auto reset() noexcept -> void;

    /*!
     * \brief Disable ECN tracking
     *
     * Called when ECN should be permanently disabled for this connection.
     * After calling this, get_ecn_marking() will return not_ect.
     */
    auto disable() noexcept -> void;

private:
    /*!
     * \struct validation_state
     * \brief Internal ECN validation state
     */
    struct validation_state
    {
        //! True if ECN capability has been validated
        bool capable{false};

        //! True if still in testing phase
        bool testing{true};

        //! True if ECN validation has failed
        bool failed{false};

        //! Current ECN counts (last received)
        ecn_counts counts{};

        //! Number of ECT-marked packets sent
        uint64_t packets_sent_with_ect{0};

        //! Sent time of the packet that triggered the last congestion signal
        std::chrono::steady_clock::time_point last_congestion_sent_time{};

        //! Number of packets required to complete validation
        static constexpr uint64_t kValidationThreshold = 10;
    };

    /*!
     * \brief Validate ECN capability based on received counts
     * \param counts Received ECN counts
     * \param packets_acked Number of packets acknowledged
     * \return True if validation passed
     *
     * RFC 9000 Section 13.4.2.1:
     * - The sum of ECN counts MUST NOT be less than the total number of
     *   QUIC packets sent with an ECT codepoint.
     * - If ECN counts are less than sent packets, ECN is being stripped.
     */
    [[nodiscard]] auto validate_ecn(const ecn_counts& counts,
                                    uint64_t packets_acked) -> bool;

    //! ECN validation state
    validation_state state_;
};

} // namespace kcenon::network::protocols::quic
