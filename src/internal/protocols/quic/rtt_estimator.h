// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <chrono>
#include <cstdint>

namespace kcenon::network::protocols::quic
{

/*!
 * \class rtt_estimator
 * \brief RTT estimation for QUIC (RFC 9002 Section 5)
 *
 * Implements the RTT estimation algorithm as specified in RFC 9002.
 * Tracks smoothed RTT, RTT variance, minimum RTT, and calculates
 * the probe timeout (PTO) duration.
 */
class rtt_estimator
{
public:
    /*!
     * \brief Default constructor
     *
     * Initializes RTT estimator with default values:
     * - Initial RTT: 333ms (RFC 9002 recommendation)
     * - Max ACK delay: 25ms (RFC 9002 default)
     */
    rtt_estimator();

    /*!
     * \brief Constructor with custom initial RTT
     * \param initial_rtt Initial RTT estimate
     * \param max_ack_delay Maximum ACK delay (from transport parameters)
     */
    explicit rtt_estimator(std::chrono::microseconds initial_rtt,
                           std::chrono::microseconds max_ack_delay =
                               std::chrono::microseconds{25000});

    /*!
     * \brief Update RTT estimate from an ACK (RFC 9002 Section 5.3)
     * \param latest_rtt RTT sample from the most recent ACK
     * \param ack_delay ACK delay reported by the peer
     * \param is_handshake_confirmed True if handshake is confirmed
     *
     * Updates smoothed_rtt, rttvar, and min_rtt based on the sample.
     * ACK delay is only applied when handshake is confirmed.
     */
    auto update(std::chrono::microseconds latest_rtt,
                std::chrono::microseconds ack_delay,
                bool is_handshake_confirmed = true) -> void;

    /*!
     * \brief Get smoothed RTT
     * \return Current smoothed RTT estimate
     */
    [[nodiscard]] auto smoothed_rtt() const noexcept -> std::chrono::microseconds
    {
        return smoothed_rtt_;
    }

    /*!
     * \brief Get RTT variance
     * \return Current RTT variance estimate
     */
    [[nodiscard]] auto rttvar() const noexcept -> std::chrono::microseconds
    {
        return rttvar_;
    }

    /*!
     * \brief Get minimum observed RTT
     * \return Minimum RTT seen so far
     */
    [[nodiscard]] auto min_rtt() const noexcept -> std::chrono::microseconds
    {
        return min_rtt_;
    }

    /*!
     * \brief Get latest RTT sample
     * \return Most recent RTT measurement
     */
    [[nodiscard]] auto latest_rtt() const noexcept -> std::chrono::microseconds
    {
        return latest_rtt_;
    }

    /*!
     * \brief Calculate probe timeout duration (RFC 9002 Section 6.2.1)
     * \return PTO duration
     *
     * PTO = smoothed_rtt + max(4*rttvar, kGranularity) + max_ack_delay
     */
    [[nodiscard]] auto pto() const noexcept -> std::chrono::microseconds;

    /*!
     * \brief Set maximum ACK delay
     * \param delay Maximum ACK delay from transport parameters
     */
    auto set_max_ack_delay(std::chrono::microseconds delay) noexcept -> void
    {
        max_ack_delay_ = delay;
    }

    /*!
     * \brief Get maximum ACK delay
     * \return Current maximum ACK delay
     */
    [[nodiscard]] auto max_ack_delay() const noexcept -> std::chrono::microseconds
    {
        return max_ack_delay_;
    }

    /*!
     * \brief Check if any RTT sample has been received
     * \return true if at least one RTT sample has been processed
     */
    [[nodiscard]] auto has_sample() const noexcept -> bool
    {
        return !first_sample_;
    }

    /*!
     * \brief Reset RTT estimator to initial state
     */
    auto reset() -> void;

private:
    //! Smoothed RTT (RFC 9002 Section 5.3)
    std::chrono::microseconds smoothed_rtt_;

    //! RTT variance (RFC 9002 Section 5.3)
    std::chrono::microseconds rttvar_;

    //! Minimum observed RTT (RFC 9002 Section 5.2)
    std::chrono::microseconds min_rtt_;

    //! Most recent RTT sample
    std::chrono::microseconds latest_rtt_;

    //! Maximum ACK delay (from transport parameters)
    std::chrono::microseconds max_ack_delay_;

    //! Initial RTT value (used for reset)
    std::chrono::microseconds initial_rtt_;

    //! True if no RTT sample has been received yet
    bool first_sample_;

    //! Timer granularity (RFC 9002 Section 6.1.2)
    static constexpr auto kGranularity = std::chrono::milliseconds{1};

    //! Default initial RTT (RFC 9002 Section 6.2.2)
    static constexpr auto kInitialRtt = std::chrono::microseconds{333000};

    //! Default max ACK delay
    static constexpr auto kDefaultMaxAckDelay = std::chrono::microseconds{25000};
};

} // namespace kcenon::network::protocols::quic
