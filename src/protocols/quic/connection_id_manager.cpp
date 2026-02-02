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

#include "internal/protocols/quic/connection_id_manager.h"

#include <algorithm>

namespace kcenon::network::protocols::quic
{

// Static member definition
const connection_id connection_id_manager::empty_cid_{};

// ============================================================================
// Constructor
// ============================================================================

connection_id_manager::connection_id_manager(uint64_t active_cid_limit)
    : active_cid_limit_(active_cid_limit)
{
}

// ============================================================================
// Peer Connection ID Management
// ============================================================================

void connection_id_manager::set_initial_peer_cid(const connection_id& cid)
{
    // Initial CID has sequence number 0 and no stateless reset token
    connection_id_entry entry;
    entry.cid = cid;
    entry.sequence_number = 0;
    entry.stateless_reset_token.fill(0);
    entry.retired = false;

    peer_cids_.clear();
    peer_cids_.push_back(entry);
    active_index_ = 0;
}

auto connection_id_manager::add_peer_cid(const connection_id& cid,
                                          uint64_t sequence,
                                          uint64_t retire_prior_to,
                                          const std::array<uint8_t, 16>& reset_token)
    -> VoidResult
{
    // RFC 9000 Section 19.15: retire_prior_to must be <= sequence
    if (retire_prior_to > sequence)
    {
        return error_void(cid_manager_error::invalid_retire_prior_to,
                          "retire_prior_to exceeds sequence number",
                          "connection_id_manager");
    }

    // Check for duplicate sequence number
    auto it = find_by_sequence(sequence);
    if (it != peer_cids_.end())
    {
        // RFC 9000: If a NEW_CONNECTION_ID frame is received with a sequence
        // number already seen, it must be ignored if identical, or a protocol
        // error if different.
        if (it->cid == cid && it->stateless_reset_token == reset_token)
        {
            // Identical frame, ignore
            return ok();
        }
        return error_void(cid_manager_error::duplicate_sequence,
                          "Duplicate sequence number with different CID",
                          "connection_id_manager");
    }

    // Check active CID limit (count non-retired CIDs)
    auto active_count = count_active_cids();
    if (active_count >= active_cid_limit_)
    {
        return error_void(cid_manager_error::active_cid_limit_exceeded,
                          "Active connection ID limit exceeded",
                          "connection_id_manager");
    }

    // Add the new CID
    connection_id_entry entry;
    entry.cid = cid;
    entry.sequence_number = sequence;
    entry.stateless_reset_token = reset_token;
    entry.retired = false;

    peer_cids_.push_back(entry);

    // Process retire_prior_to
    if (retire_prior_to > largest_retire_prior_to_)
    {
        retire_cids_prior_to(retire_prior_to);
        largest_retire_prior_to_ = retire_prior_to;
    }

    return ok();
}

auto connection_id_manager::get_active_peer_cid() const -> const connection_id&
{
    if (peer_cids_.empty() || active_index_ >= peer_cids_.size())
    {
        return empty_cid_;
    }

    return peer_cids_[active_index_].cid;
}

auto connection_id_manager::rotate_peer_cid() -> VoidResult
{
    // Find an unused, non-retired CID
    for (size_t i = 0; i < peer_cids_.size(); ++i)
    {
        if (i != active_index_ && !peer_cids_[i].retired)
        {
            // Mark the old CID for retirement
            if (active_index_ < peer_cids_.size())
            {
                auto old_seq = peer_cids_[active_index_].sequence_number;
                (void)retire_peer_cid(old_seq);
            }

            active_index_ = i;
            return ok();
        }
    }

    return error_void(cid_manager_error::no_available_cid,
                      "No available connection ID for rotation",
                      "connection_id_manager");
}

auto connection_id_manager::available_peer_cids() const -> size_t
{
    size_t count = 0;
    for (size_t i = 0; i < peer_cids_.size(); ++i)
    {
        if (i != active_index_ && !peer_cids_[i].retired)
        {
            ++count;
        }
    }
    return count;
}

auto connection_id_manager::is_stateless_reset_token(
    const std::array<uint8_t, 16>& token) const -> bool
{
    for (const auto& entry : peer_cids_)
    {
        if (entry.stateless_reset_token == token)
        {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Retirement Management
// ============================================================================

void connection_id_manager::retire_cids_prior_to(uint64_t prior_to)
{
    for (auto& entry : peer_cids_)
    {
        if (!entry.retired && entry.sequence_number < prior_to)
        {
            entry.retired = true;

            // Queue RETIRE_CONNECTION_ID frame
            retire_connection_id_frame frame;
            frame.sequence_number = entry.sequence_number;
            pending_retire_frames_.push_back(frame);

            // If this was the active CID, we need to switch
            if (&entry == &peer_cids_[active_index_])
            {
                // Find a new active CID
                for (size_t i = 0; i < peer_cids_.size(); ++i)
                {
                    if (!peer_cids_[i].retired)
                    {
                        active_index_ = i;
                        break;
                    }
                }
            }
        }
    }
}

auto connection_id_manager::get_pending_retire_frames()
    -> std::vector<retire_connection_id_frame>
{
    return pending_retire_frames_;
}

void connection_id_manager::clear_pending_retire_frames()
{
    pending_retire_frames_.clear();
}

auto connection_id_manager::retire_peer_cid(uint64_t sequence) -> VoidResult
{
    auto it = find_by_sequence(sequence);
    if (it == peer_cids_.end())
    {
        return error_void(cid_manager_error::cid_not_found,
                          "Connection ID sequence not found",
                          "connection_id_manager");
    }

    if (!it->retired)
    {
        it->retired = true;

        // Queue RETIRE_CONNECTION_ID frame
        retire_connection_id_frame frame;
        frame.sequence_number = sequence;
        pending_retire_frames_.push_back(frame);
    }

    return ok();
}

// ============================================================================
// State Queries
// ============================================================================

auto connection_id_manager::has_peer_cid(const connection_id& cid) const -> bool
{
    return std::any_of(peer_cids_.begin(), peer_cids_.end(),
                       [&cid](const connection_id_entry& entry)
                       { return entry.cid == cid && !entry.retired; });
}

// ============================================================================
// Private Methods
// ============================================================================

auto connection_id_manager::find_by_sequence(uint64_t sequence)
    -> std::vector<connection_id_entry>::iterator
{
    return std::find_if(peer_cids_.begin(), peer_cids_.end(),
                        [sequence](const connection_id_entry& entry)
                        { return entry.sequence_number == sequence; });
}

auto connection_id_manager::count_active_cids() const -> size_t
{
    return static_cast<size_t>(std::count_if(
        peer_cids_.begin(), peer_cids_.end(),
        [](const connection_id_entry& entry) { return !entry.retired; }));
}

} // namespace kcenon::network::protocols::quic
