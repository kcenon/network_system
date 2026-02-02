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

#include "stream.h"
#include "kcenon/network/utils/result_types.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace kcenon::network::protocols::quic
{

/*!
 * \class stream_manager
 * \brief Manages QUIC streams within a connection
 *
 * Handles stream creation, lookup, and lifecycle management according to
 * RFC 9000 Sections 2-4. Supports both client and server roles with proper
 * stream ID assignment.
 */
class stream_manager
{
public:
    /*!
     * \brief Construct a stream manager
     * \param is_server True if this is a server-side manager
     * \param initial_max_stream_data Initial flow control limit per stream
     */
    explicit stream_manager(bool is_server, uint64_t initial_max_stream_data = 65536);

    ~stream_manager() = default;

    // Non-copyable
    stream_manager(const stream_manager&) = delete;
    auto operator=(const stream_manager&) -> stream_manager& = delete;

    // Non-movable (due to mutex)
    stream_manager(stream_manager&&) = delete;
    auto operator=(stream_manager&&) -> stream_manager& = delete;

    // ========================================================================
    // Stream Creation
    // ========================================================================

    /*!
     * \brief Create a new locally-initiated bidirectional stream
     * \return Stream ID or error
     */
    [[nodiscard]] auto create_bidirectional_stream() -> Result<uint64_t>;

    /*!
     * \brief Create a new locally-initiated unidirectional stream
     * \return Stream ID or error
     */
    [[nodiscard]] auto create_unidirectional_stream() -> Result<uint64_t>;

    // ========================================================================
    // Stream Access
    // ========================================================================

    /*!
     * \brief Get a stream by ID
     * \param stream_id Stream identifier
     * \return Pointer to stream or nullptr if not found
     */
    [[nodiscard]] auto get_stream(uint64_t stream_id) -> stream*;

    /*!
     * \brief Get a stream by ID (const version)
     * \param stream_id Stream identifier
     * \return Pointer to stream or nullptr if not found
     */
    [[nodiscard]] auto get_stream(uint64_t stream_id) const -> const stream*;

    /*!
     * \brief Get or create a stream for a peer-initiated stream ID
     * \param stream_id Stream identifier
     * \return Pointer to stream or error
     *
     * This is used when receiving data for a stream that may not exist yet.
     * Peer-initiated streams are implicitly created when first referenced.
     */
    [[nodiscard]] auto get_or_create_stream(uint64_t stream_id) -> Result<stream*>;

    /*!
     * \brief Check if a stream exists
     * \param stream_id Stream identifier
     */
    [[nodiscard]] auto has_stream(uint64_t stream_id) const -> bool;

    /*!
     * \brief Get all active stream IDs
     */
    [[nodiscard]] auto stream_ids() const -> std::vector<uint64_t>;

    /*!
     * \brief Get number of active streams
     */
    [[nodiscard]] auto stream_count() const -> size_t;

    // ========================================================================
    // Stream Limits (Transport Parameters)
    // ========================================================================

    /*!
     * \brief Set maximum bidirectional streams peer can initiate
     * \param max Maximum stream count
     */
    void set_peer_max_streams_bidi(uint64_t max);

    /*!
     * \brief Set maximum unidirectional streams peer can initiate
     * \param max Maximum stream count
     */
    void set_peer_max_streams_uni(uint64_t max);

    /*!
     * \brief Get our maximum bidirectional streams (advertised to peer)
     */
    [[nodiscard]] auto local_max_streams_bidi() const noexcept -> uint64_t
    {
        return local_max_streams_bidi_;
    }

    /*!
     * \brief Get our maximum unidirectional streams (advertised to peer)
     */
    [[nodiscard]] auto local_max_streams_uni() const noexcept -> uint64_t
    {
        return local_max_streams_uni_;
    }

    /*!
     * \brief Set our maximum bidirectional streams (advertised to peer)
     * \param max Maximum stream count
     */
    void set_local_max_streams_bidi(uint64_t max);

    /*!
     * \brief Set our maximum unidirectional streams (advertised to peer)
     * \param max Maximum stream count
     */
    void set_local_max_streams_uni(uint64_t max);

    /*!
     * \brief Get peer's maximum bidirectional streams (limits our creation)
     */
    [[nodiscard]] auto peer_max_streams_bidi() const noexcept -> uint64_t
    {
        return peer_max_streams_bidi_;
    }

    /*!
     * \brief Get peer's maximum unidirectional streams (limits our creation)
     */
    [[nodiscard]] auto peer_max_streams_uni() const noexcept -> uint64_t
    {
        return peer_max_streams_uni_;
    }

    // ========================================================================
    // Stream Queries
    // ========================================================================

    /*!
     * \brief Get streams with pending data to send
     * \return Vector of stream pointers
     */
    [[nodiscard]] auto streams_with_pending_data() -> std::vector<stream*>;

    /*!
     * \brief Get streams that need MAX_STREAM_DATA updates
     * \return Vector of stream pointers
     */
    [[nodiscard]] auto streams_needing_flow_control_update() -> std::vector<stream*>;

    /*!
     * \brief Iterate over all streams
     * \param callback Function to call for each stream
     */
    void for_each_stream(const std::function<void(stream&)>& callback);

    /*!
     * \brief Iterate over all streams (const version)
     * \param callback Function to call for each stream
     */
    void for_each_stream(const std::function<void(const stream&)>& callback) const;

    // ========================================================================
    // Stream Lifecycle
    // ========================================================================

    /*!
     * \brief Remove closed/terminal streams
     * \return Number of streams removed
     */
    auto remove_closed_streams() -> size_t;

    /*!
     * \brief Close all streams with an error code
     * \param error_code Error code to use for reset
     */
    void close_all_streams(uint64_t error_code);

    /*!
     * \brief Reset manager state (for connection close)
     */
    void reset();

    // ========================================================================
    // Statistics
    // ========================================================================

    /*!
     * \brief Get count of locally-initiated bidirectional streams
     */
    [[nodiscard]] auto local_bidi_streams_count() const noexcept -> uint64_t
    {
        return next_local_bidi_id_ / 4;
    }

    /*!
     * \brief Get count of locally-initiated unidirectional streams
     */
    [[nodiscard]] auto local_uni_streams_count() const noexcept -> uint64_t
    {
        return (next_local_uni_id_ - 2) / 4;
    }

    /*!
     * \brief Get count of peer-initiated bidirectional streams
     */
    [[nodiscard]] auto peer_bidi_streams_count() const noexcept -> uint64_t
    {
        return (highest_peer_bidi_id_ + 4 - peer_bidi_type_) / 4;
    }

    /*!
     * \brief Get count of peer-initiated unidirectional streams
     */
    [[nodiscard]] auto peer_uni_streams_count() const noexcept -> uint64_t
    {
        return (highest_peer_uni_id_ + 4 - peer_uni_type_) / 4;
    }

private:
    bool is_server_;
    uint64_t initial_max_stream_data_;

    // Stream ID generators
    // Client: bidi=0,4,8..., uni=2,6,10...
    // Server: bidi=1,5,9..., uni=3,7,11...
    uint64_t next_local_bidi_id_;
    uint64_t next_local_uni_id_;

    // Peer stream ID tracking
    uint64_t highest_peer_bidi_id_{0};
    uint64_t highest_peer_uni_id_{0};
    uint64_t peer_bidi_type_;
    uint64_t peer_uni_type_;

    // Stream limits
    uint64_t local_max_streams_bidi_{100};
    uint64_t local_max_streams_uni_{100};
    uint64_t peer_max_streams_bidi_{0};
    uint64_t peer_max_streams_uni_{0};

    // Active streams
    mutable std::shared_mutex streams_mutex_;
    std::map<uint64_t, std::unique_ptr<stream>> streams_;

    // Internal helpers
    [[nodiscard]] auto validate_stream_id(uint64_t stream_id) const -> VoidResult;
    [[nodiscard]] auto is_local_stream(uint64_t stream_id) const noexcept -> bool;
    [[nodiscard]] auto can_create_local_stream(bool bidirectional) const -> bool;
    [[nodiscard]] auto can_accept_peer_stream(uint64_t stream_id) const -> bool;
};

} // namespace kcenon::network::protocols::quic
