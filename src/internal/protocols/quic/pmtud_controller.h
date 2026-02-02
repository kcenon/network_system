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

#include <chrono>
#include <cstddef>
#include <optional>

namespace kcenon::network::protocols::quic
{

/*!
 * \enum pmtud_state
 * \brief States for DPLPMTUD state machine (RFC 8899 Section 5.2)
 */
enum class pmtud_state
{
    disabled,        //!< PMTUD is disabled
    base,            //!< Using BASE_PLPMTU (minimum MTU)
    searching,       //!< Binary search for larger MTU
    search_complete, //!< Maximum MTU found and validated
    error,           //!< Black hole detected, reset to base
};

/*!
 * \brief Convert PMTUD state to string
 */
[[nodiscard]] auto pmtud_state_to_string(pmtud_state state) noexcept -> const char*;

/*!
 * \struct pmtud_config
 * \brief Configuration for PMTUD controller
 */
struct pmtud_config
{
    //! Minimum MTU (RFC 9000 requires 1200 bytes for QUIC)
    size_t min_mtu = 1200;

    //! Maximum MTU to probe (typical Ethernet is 1500)
    size_t max_probe_mtu = 1500;

    //! Step size for probing (binary search uses mid-point)
    size_t probe_step = 32;

    //! Timeout for probe packets before considering them lost
    std::chrono::seconds probe_timeout{3};

    //! Maximum number of probes before giving up at current size
    size_t max_probes = 3;

    //! Interval between probe attempts during search
    std::chrono::milliseconds probe_interval{1000};

    //! Interval for re-validation after search is complete
    std::chrono::seconds confirmation_interval{600}; // 10 minutes
};

/*!
 * \class pmtud_controller
 * \brief Path MTU Discovery controller for QUIC (RFC 8899 DPLPMTUD)
 *
 * Implements Datagram Packetization Layer PMTU Discovery (DPLPMTUD) as
 * specified in RFC 8899 for use with QUIC (RFC 9000 Section 14).
 *
 * The controller maintains a state machine that:
 * - Starts at the minimum QUIC MTU (1200 bytes)
 * - Probes for larger MTU using binary search
 * - Handles probe acknowledgments and losses
 * - Responds to ICMP Packet Too Big messages
 * - Periodically re-validates discovered MTU
 */
class pmtud_controller
{
public:
    /*!
     * \brief Default constructor with default configuration
     */
    pmtud_controller();

    /*!
     * \brief Constructor with custom configuration
     * \param config Configuration parameters
     */
    explicit pmtud_controller(pmtud_config config);

    // ========================================================================
    // MTU Access
    // ========================================================================

    /*!
     * \brief Get current validated MTU
     * \return Current MTU to use for sending packets
     */
    [[nodiscard]] auto current_mtu() const noexcept -> size_t
    {
        return current_mtu_;
    }

    /*!
     * \brief Get minimum MTU
     * \return Minimum guaranteed MTU (BASE_PLPMTU)
     */
    [[nodiscard]] auto min_mtu() const noexcept -> size_t
    {
        return config_.min_mtu;
    }

    /*!
     * \brief Get maximum MTU being probed
     * \return Maximum MTU target
     */
    [[nodiscard]] auto max_mtu() const noexcept -> size_t
    {
        return config_.max_probe_mtu;
    }

    // ========================================================================
    // State Access
    // ========================================================================

    /*!
     * \brief Get current PMTUD state
     * \return Current state
     */
    [[nodiscard]] auto state() const noexcept -> pmtud_state
    {
        return state_;
    }

    /*!
     * \brief Check if PMTUD is enabled
     * \return True if PMTUD is enabled and running
     */
    [[nodiscard]] auto is_enabled() const noexcept -> bool
    {
        return state_ != pmtud_state::disabled;
    }

    /*!
     * \brief Check if MTU search is complete
     * \return True if maximum MTU has been found
     */
    [[nodiscard]] auto is_search_complete() const noexcept -> bool
    {
        return state_ == pmtud_state::search_complete;
    }

    // ========================================================================
    // Control
    // ========================================================================

    /*!
     * \brief Enable PMTUD and start probing
     *
     * Transitions from disabled state to base state and begins
     * the MTU discovery process.
     */
    void enable();

    /*!
     * \brief Disable PMTUD
     *
     * Stops probing and reverts to minimum MTU.
     */
    void disable();

    /*!
     * \brief Reset to initial state
     *
     * Clears all state and restarts discovery from minimum MTU.
     */
    void reset();

    // ========================================================================
    // Probing
    // ========================================================================

    /*!
     * \brief Check if a probe should be sent
     * \param now Current time
     * \return True if a probe packet should be generated
     */
    [[nodiscard]] auto should_probe(
        std::chrono::steady_clock::time_point now) const noexcept -> bool;

    /*!
     * \brief Get the size for next probe packet
     * \return Size in bytes for the probe packet, or nullopt if no probe needed
     */
    [[nodiscard]] auto probe_size() const noexcept -> std::optional<size_t>;

    /*!
     * \brief Record that a probe was sent
     * \param size Size of the probe packet sent
     * \param sent_time Time the probe was sent
     */
    void on_probe_sent(size_t size, std::chrono::steady_clock::time_point sent_time);

    /*!
     * \brief Handle probe acknowledgment
     * \param size Size of the probed packet that was acknowledged
     *
     * Called when a probe packet is acknowledged by the peer.
     * If the probed size is larger than current MTU, updates
     * the validated MTU and continues searching.
     */
    void on_probe_acked(size_t size);

    /*!
     * \brief Handle probe loss
     * \param size Size of the probe packet that was lost
     *
     * Called when a probe packet is declared lost.
     * Adjusts the search range and may retry probing.
     */
    void on_probe_lost(size_t size);

    // ========================================================================
    // ICMP Handling
    // ========================================================================

    /*!
     * \brief Handle ICMP Packet Too Big message
     * \param reported_mtu MTU reported in the ICMP message
     *
     * Called when an ICMP PTB message is received.
     * Updates the MTU immediately if the reported value is smaller.
     */
    void on_packet_too_big(size_t reported_mtu);

    // ========================================================================
    // Timers
    // ========================================================================

    /*!
     * \brief Get next timeout deadline for PMTUD
     * \return Time point for next probe or confirmation, or nullopt if none
     */
    [[nodiscard]] auto next_timeout() const noexcept
        -> std::optional<std::chrono::steady_clock::time_point>;

    /*!
     * \brief Handle timeout event
     * \param now Current time
     *
     * Called when the PMTUD timer expires.
     * May trigger probe retransmission or state transitions.
     */
    void on_timeout(std::chrono::steady_clock::time_point now);

private:
    // ========================================================================
    // Private Methods
    // ========================================================================

    /*!
     * \brief Start probing from base MTU
     */
    void start_search();

    /*!
     * \brief Calculate next probe size using binary search
     * \return Next MTU to probe
     */
    [[nodiscard]] auto calculate_next_probe_size() const noexcept -> size_t;

    /*!
     * \brief Transition to search_complete state
     */
    void complete_search();

    /*!
     * \brief Handle black hole detection
     */
    void handle_black_hole();

    // ========================================================================
    // Private Members
    // ========================================================================

    //! Configuration
    pmtud_config config_;

    //! Current state
    pmtud_state state_{pmtud_state::disabled};

    //! Current validated MTU
    size_t current_mtu_;

    //! Lower bound for binary search (known good MTU)
    size_t search_low_;

    //! Upper bound for binary search (target MTU)
    size_t search_high_;

    //! Current probe size being tested
    size_t probing_mtu_{0};

    //! Number of probes sent at current size
    size_t probe_count_{0};

    //! Number of consecutive probe failures (for black hole detection)
    size_t consecutive_failures_{0};

    //! Time of last probe sent
    std::chrono::steady_clock::time_point last_probe_time_;

    //! Time when search was completed (for re-validation)
    std::chrono::steady_clock::time_point search_complete_time_;

    //! Whether a probe is currently in flight
    bool probe_in_flight_{false};

    //! Threshold for black hole detection
    static constexpr size_t kBlackHoleThreshold = 6;
};

} // namespace kcenon::network::protocols::quic
