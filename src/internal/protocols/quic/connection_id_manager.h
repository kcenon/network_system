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

#include "kcenon/network/detail/protocols/quic/connection_id.h"
#include "frame_types.h"
#include "kcenon/network/detail/utils/result_types.h"

#include <array>
#include <cstdint>
#include <vector>

namespace kcenon::network::protocols::quic
{

// ============================================================================
// Connection ID Manager Error Codes
// ============================================================================

namespace cid_manager_error
{
    constexpr int duplicate_sequence = -740;
    constexpr int sequence_too_low = -741;
    constexpr int no_available_cid = -742;
    constexpr int cid_not_found = -743;
    constexpr int invalid_retire_prior_to = -744;
    constexpr int active_cid_limit_exceeded = -745;
} // namespace cid_manager_error

// ============================================================================
// Connection ID Entry
// ============================================================================

/*!
 * \struct connection_id_entry
 * \brief Entry for storing a peer's connection ID with metadata
 *
 * Per RFC 9000 Section 5.1, endpoints must track:
 * - The connection ID itself
 * - The sequence number assigned to the CID
 * - The stateless reset token (16 bytes)
 * - Whether the CID has been retired
 */
struct connection_id_entry
{
    connection_id cid;
    uint64_t sequence_number{0};
    std::array<uint8_t, 16> stateless_reset_token{};
    bool retired{false};
};

// ============================================================================
// Connection ID Manager
// ============================================================================

/*!
 * \class connection_id_manager
 * \brief Manages peer connection IDs for QUIC connections (RFC 9000 Section 5.1)
 *
 * This class handles:
 * - Storage of connection IDs received via NEW_CONNECTION_ID frames
 * - Tracking sequence numbers and retire_prior_to values
 * - Connection ID rotation for path migration
 * - Generation of RETIRE_CONNECTION_ID frames
 * - Stateless reset token validation
 *
 * RFC 9000 Section 5.1 Requirements:
 * - Endpoints must store connection IDs provided by peer
 * - Must track sequence numbers and retire_prior_to values
 * - Must support Connection ID rotation for path migration
 * - Must honor retire_prior_to in NEW_CONNECTION_ID frames
 */
class connection_id_manager
{
public:
    /*!
     * \brief Construct a connection ID manager
     * \param active_cid_limit Maximum number of active CIDs (from transport params)
     */
    explicit connection_id_manager(uint64_t active_cid_limit = 8);

    ~connection_id_manager() = default;

    // Non-copyable but movable
    connection_id_manager(const connection_id_manager&) = delete;
    auto operator=(const connection_id_manager&) -> connection_id_manager& = delete;
    connection_id_manager(connection_id_manager&&) = default;
    auto operator=(connection_id_manager&&) -> connection_id_manager& = default;

    // ========================================================================
    // Peer Connection ID Management
    // ========================================================================

    /*!
     * \brief Set the initial peer connection ID (from Initial packet)
     * \param cid The initial connection ID
     *
     * This is called when processing the first packet from the peer.
     * The initial CID has an implicit sequence number of 0.
     */
    void set_initial_peer_cid(const connection_id& cid);

    /*!
     * \brief Add a new peer connection ID from NEW_CONNECTION_ID frame
     * \param cid The connection ID
     * \param sequence The sequence number for this CID
     * \param retire_prior_to CIDs below this sequence should be retired
     * \param reset_token Stateless reset token (16 bytes)
     * \return Success or error
     */
    [[nodiscard]] auto add_peer_cid(const connection_id& cid,
                                     uint64_t sequence,
                                     uint64_t retire_prior_to,
                                     const std::array<uint8_t, 16>& reset_token) -> VoidResult;

    /*!
     * \brief Get the currently active peer connection ID
     * \return Reference to the active CID
     */
    [[nodiscard]] auto get_active_peer_cid() const -> const connection_id&;

    /*!
     * \brief Rotate to a new peer connection ID
     * \return Success or error if no unused CID is available
     *
     * This is used during path migration or proactive rotation.
     * The current CID is marked for retirement and a new one is selected.
     */
    [[nodiscard]] auto rotate_peer_cid() -> VoidResult;

    /*!
     * \brief Get the number of available (non-retired, non-active) peer CIDs
     */
    [[nodiscard]] auto available_peer_cids() const -> size_t;

    /*!
     * \brief Check if a stateless reset token matches any stored CID
     * \param token The 16-byte token to check
     * \return True if the token matches a stored CID's reset token
     */
    [[nodiscard]] auto is_stateless_reset_token(
        const std::array<uint8_t, 16>& token) const -> bool;

    // ========================================================================
    // Retirement Management
    // ========================================================================

    /*!
     * \brief Retire CIDs with sequence numbers less than the given value
     * \param prior_to Sequence number threshold
     *
     * Called when processing a NEW_CONNECTION_ID frame with retire_prior_to.
     */
    void retire_cids_prior_to(uint64_t prior_to);

    /*!
     * \brief Get pending RETIRE_CONNECTION_ID frames to send
     * \return Vector of frames for retired CIDs that need acknowledgment
     */
    [[nodiscard]] auto get_pending_retire_frames() -> std::vector<retire_connection_id_frame>;

    /*!
     * \brief Clear the pending retire frames queue
     *
     * Called after the frames have been successfully sent and acknowledged.
     */
    void clear_pending_retire_frames();

    /*!
     * \brief Mark a specific CID sequence as retired
     * \param sequence The sequence number to retire
     * \return Success or error if sequence not found
     */
    [[nodiscard]] auto retire_peer_cid(uint64_t sequence) -> VoidResult;

    // ========================================================================
    // State Queries
    // ========================================================================

    /*!
     * \brief Get the largest retire_prior_to value received
     */
    [[nodiscard]] auto largest_retire_prior_to() const -> uint64_t
    {
        return largest_retire_prior_to_;
    }

    /*!
     * \brief Get the current number of peer CIDs (including retired)
     */
    [[nodiscard]] auto peer_cid_count() const -> size_t { return peer_cids_.size(); }

    /*!
     * \brief Get the active connection ID limit
     */
    [[nodiscard]] auto active_cid_limit() const -> uint64_t { return active_cid_limit_; }

    /*!
     * \brief Set the active connection ID limit
     * \param limit New limit (from transport parameters)
     */
    void set_active_cid_limit(uint64_t limit) { active_cid_limit_ = limit; }

    /*!
     * \brief Check if a given CID matches any stored peer CID
     * \param cid The connection ID to check
     * \return True if the CID is known
     */
    [[nodiscard]] auto has_peer_cid(const connection_id& cid) const -> bool;

private:
    //! Maximum number of active CIDs
    uint64_t active_cid_limit_;

    //! Peer connection IDs
    std::vector<connection_id_entry> peer_cids_;

    //! Index of the currently active peer CID
    size_t active_index_{0};

    //! Largest retire_prior_to value received
    uint64_t largest_retire_prior_to_{0};

    //! Pending RETIRE_CONNECTION_ID frames to send
    std::vector<retire_connection_id_frame> pending_retire_frames_;

    //! Empty CID for error cases
    static const connection_id empty_cid_;

    /*!
     * \brief Find an entry by sequence number
     */
    [[nodiscard]] auto find_by_sequence(uint64_t sequence)
        -> std::vector<connection_id_entry>::iterator;

    /*!
     * \brief Count non-retired CIDs
     */
    [[nodiscard]] auto count_active_cids() const -> size_t;
};

} // namespace kcenon::network::protocols::quic
