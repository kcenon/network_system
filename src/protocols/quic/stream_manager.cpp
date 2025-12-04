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

#include "kcenon/network/protocols/quic/stream_manager.h"

namespace network_system::protocols::quic
{

stream_manager::stream_manager(bool is_server, uint64_t initial_max_stream_data)
    : is_server_{is_server}
    , initial_max_stream_data_{initial_max_stream_data}
    , next_local_bidi_id_{is_server ? stream_id_type::server_bidi : stream_id_type::client_bidi}
    , next_local_uni_id_{is_server ? stream_id_type::server_uni : stream_id_type::client_uni}
    , peer_bidi_type_{is_server ? stream_id_type::client_bidi : stream_id_type::server_bidi}
    , peer_uni_type_{is_server ? stream_id_type::client_uni : stream_id_type::server_uni}
{
}

// ============================================================================
// Stream Creation
// ============================================================================

auto stream_manager::create_bidirectional_stream() -> Result<uint64_t>
{
    if (!can_create_local_stream(true)) {
        return error<uint64_t>(stream_error::stream_limit_exceeded,
                              "Bidirectional stream limit reached",
                              "quic::stream_manager");
    }

    std::unique_lock lock(streams_mutex_);

    uint64_t stream_id = next_local_bidi_id_;
    next_local_bidi_id_ += 4;

    auto new_stream = std::make_unique<stream>(stream_id, true, initial_max_stream_data_);
    streams_[stream_id] = std::move(new_stream);

    return ok(std::move(stream_id));
}

auto stream_manager::create_unidirectional_stream() -> Result<uint64_t>
{
    if (!can_create_local_stream(false)) {
        return error<uint64_t>(stream_error::stream_limit_exceeded,
                              "Unidirectional stream limit reached",
                              "quic::stream_manager");
    }

    std::unique_lock lock(streams_mutex_);

    uint64_t stream_id = next_local_uni_id_;
    next_local_uni_id_ += 4;

    auto new_stream = std::make_unique<stream>(stream_id, true, initial_max_stream_data_);
    streams_[stream_id] = std::move(new_stream);

    return ok(std::move(stream_id));
}

// ============================================================================
// Stream Access
// ============================================================================

auto stream_manager::get_stream(uint64_t stream_id) -> stream*
{
    std::shared_lock lock(streams_mutex_);
    auto it = streams_.find(stream_id);
    return (it != streams_.end()) ? it->second.get() : nullptr;
}

auto stream_manager::get_stream(uint64_t stream_id) const -> const stream*
{
    std::shared_lock lock(streams_mutex_);
    auto it = streams_.find(stream_id);
    return (it != streams_.end()) ? it->second.get() : nullptr;
}

auto stream_manager::get_or_create_stream(uint64_t stream_id) -> Result<stream*>
{
    // First, try to get existing stream
    {
        std::shared_lock lock(streams_mutex_);
        auto it = streams_.find(stream_id);
        if (it != streams_.end()) {
            return ok(it->second.get());
        }
    }

    // Validate stream ID
    auto validation = validate_stream_id(stream_id);
    if (validation.is_err()) {
        return error<stream*>(validation.error().code,
                             validation.error().message,
                             "quic::stream_manager");
    }

    // Check if this is a peer-initiated stream
    if (is_local_stream(stream_id)) {
        return error<stream*>(stream_error::stream_not_found,
                             "Local stream must be explicitly created",
                             "quic::stream_manager");
    }

    // Check stream limits for peer-initiated stream
    if (!can_accept_peer_stream(stream_id)) {
        return error<stream*>(stream_error::stream_limit_exceeded,
                             "Peer stream limit exceeded",
                             "quic::stream_manager");
    }

    // Create implicit streams as needed (RFC 9000 Section 2.1)
    // When a stream is created, all lower-numbered streams of the same type
    // must also be created
    std::unique_lock lock(streams_mutex_);

    const auto type = stream_id_type::get_type(stream_id);
    const bool is_bidi = stream_id_type::is_bidirectional(stream_id);

    // Determine the first stream ID of this type we need to create
    uint64_t first_id = type;
    if (is_bidi) {
        if (highest_peer_bidi_id_ > 0 || streams_.count(peer_bidi_type_) > 0) {
            first_id = highest_peer_bidi_id_ + 4;
        }
    } else {
        if (highest_peer_uni_id_ > 0 || streams_.count(peer_uni_type_) > 0) {
            first_id = highest_peer_uni_id_ + 4;
        }
    }

    // Create all streams up to and including the requested one
    for (uint64_t id = first_id; id <= stream_id; id += 4) {
        if (streams_.count(id) == 0) {
            auto new_stream = std::make_unique<stream>(id, false, initial_max_stream_data_);
            streams_[id] = std::move(new_stream);
        }
    }

    // Update highest peer stream ID
    if (is_bidi) {
        if (stream_id > highest_peer_bidi_id_ || highest_peer_bidi_id_ == 0) {
            highest_peer_bidi_id_ = stream_id;
        }
    } else {
        if (stream_id > highest_peer_uni_id_ || highest_peer_uni_id_ == 0) {
            highest_peer_uni_id_ = stream_id;
        }
    }

    return ok(streams_[stream_id].get());
}

auto stream_manager::has_stream(uint64_t stream_id) const -> bool
{
    std::shared_lock lock(streams_mutex_);
    return streams_.count(stream_id) > 0;
}

auto stream_manager::stream_ids() const -> std::vector<uint64_t>
{
    std::shared_lock lock(streams_mutex_);
    std::vector<uint64_t> ids;
    ids.reserve(streams_.size());
    for (const auto& [id, _] : streams_) {
        ids.push_back(id);
    }
    return ids;
}

auto stream_manager::stream_count() const -> size_t
{
    std::shared_lock lock(streams_mutex_);
    return streams_.size();
}

// ============================================================================
// Stream Limits
// ============================================================================

void stream_manager::set_peer_max_streams_bidi(uint64_t max)
{
    peer_max_streams_bidi_ = max;
}

void stream_manager::set_peer_max_streams_uni(uint64_t max)
{
    peer_max_streams_uni_ = max;
}

void stream_manager::set_local_max_streams_bidi(uint64_t max)
{
    local_max_streams_bidi_ = max;
}

void stream_manager::set_local_max_streams_uni(uint64_t max)
{
    local_max_streams_uni_ = max;
}

// ============================================================================
// Stream Queries
// ============================================================================

auto stream_manager::streams_with_pending_data() -> std::vector<stream*>
{
    std::shared_lock lock(streams_mutex_);
    std::vector<stream*> result;
    for (auto& [_, s] : streams_) {
        if (s->pending_bytes() > 0 || (s->fin_sent() && !s->is_fin_received())) {
            result.push_back(s.get());
        }
    }
    return result;
}

auto stream_manager::streams_needing_flow_control_update() -> std::vector<stream*>
{
    std::shared_lock lock(streams_mutex_);
    std::vector<stream*> result;
    for (auto& [_, s] : streams_) {
        if (s->should_send_max_stream_data()) {
            result.push_back(s.get());
        }
    }
    return result;
}

void stream_manager::for_each_stream(const std::function<void(stream&)>& callback)
{
    std::shared_lock lock(streams_mutex_);
    for (auto& [_, s] : streams_) {
        callback(*s);
    }
}

void stream_manager::for_each_stream(const std::function<void(const stream&)>& callback) const
{
    std::shared_lock lock(streams_mutex_);
    for (const auto& [_, s] : streams_) {
        callback(*s);
    }
}

// ============================================================================
// Stream Lifecycle
// ============================================================================

auto stream_manager::remove_closed_streams() -> size_t
{
    std::unique_lock lock(streams_mutex_);
    size_t removed = 0;

    for (auto it = streams_.begin(); it != streams_.end();) {
        const auto& s = it->second;

        // Check if stream is in terminal state on both sides
        bool send_terminal = (s->send_state() == send_stream_state::data_recvd ||
                             s->send_state() == send_stream_state::reset_recvd);
        bool recv_terminal = (s->recv_state() == recv_stream_state::data_read ||
                             s->recv_state() == recv_stream_state::reset_read);

        // For unidirectional streams, only check the relevant side
        if (s->is_unidirectional()) {
            if (s->is_local()) {
                // Local unidirectional: only check send side
                if (send_terminal) {
                    it = streams_.erase(it);
                    ++removed;
                    continue;
                }
            } else {
                // Peer unidirectional: only check receive side
                if (recv_terminal) {
                    it = streams_.erase(it);
                    ++removed;
                    continue;
                }
            }
        } else {
            // Bidirectional: check both sides
            if (send_terminal && recv_terminal) {
                it = streams_.erase(it);
                ++removed;
                continue;
            }
        }

        ++it;
    }

    return removed;
}

void stream_manager::close_all_streams(uint64_t error_code)
{
    std::unique_lock lock(streams_mutex_);
    for (auto& [_, s] : streams_) {
        if (s->can_send()) {
            (void)s->reset(error_code);
        }
    }
}

void stream_manager::reset()
{
    std::unique_lock lock(streams_mutex_);
    streams_.clear();

    // Reset stream ID counters
    next_local_bidi_id_ = is_server_ ? stream_id_type::server_bidi : stream_id_type::client_bidi;
    next_local_uni_id_ = is_server_ ? stream_id_type::server_uni : stream_id_type::client_uni;
    highest_peer_bidi_id_ = 0;
    highest_peer_uni_id_ = 0;
}

// ============================================================================
// Internal Helpers
// ============================================================================

auto stream_manager::validate_stream_id(uint64_t stream_id) const -> VoidResult
{
    const auto type = stream_id_type::get_type(stream_id);

    // Check if stream type is valid for the role
    if (is_local_stream(stream_id)) {
        // Local stream: check against our limits
        const bool is_bidi = stream_id_type::is_bidirectional(stream_id);
        if (is_bidi) {
            if (stream_id >= next_local_bidi_id_) {
                return error_void(stream_error::invalid_stream_id,
                                 "Local bidi stream ID too high",
                                 "quic::stream_manager");
            }
        } else {
            if (stream_id >= next_local_uni_id_) {
                return error_void(stream_error::invalid_stream_id,
                                 "Local uni stream ID too high",
                                 "quic::stream_manager");
            }
        }
    }

    return ok();
}

auto stream_manager::is_local_stream(uint64_t stream_id) const noexcept -> bool
{
    const bool is_server_initiated = stream_id_type::is_server_initiated(stream_id);
    return is_server_initiated == is_server_;
}

auto stream_manager::can_create_local_stream(bool bidirectional) const -> bool
{
    if (bidirectional) {
        // Check against peer's MAX_STREAMS_BIDI
        const uint64_t current_count = next_local_bidi_id_ / 4;
        return current_count < peer_max_streams_bidi_;
    } else {
        // Check against peer's MAX_STREAMS_UNI
        const uint64_t current_count = (next_local_uni_id_ - 2) / 4;
        return current_count < peer_max_streams_uni_;
    }
}

auto stream_manager::can_accept_peer_stream(uint64_t stream_id) const -> bool
{
    const bool is_bidi = stream_id_type::is_bidirectional(stream_id);
    const uint64_t stream_seq = stream_id_type::get_sequence(stream_id);

    if (is_bidi) {
        // Check against our MAX_STREAMS_BIDI
        return stream_seq < local_max_streams_bidi_;
    } else {
        // Check against our MAX_STREAMS_UNI
        return stream_seq < local_max_streams_uni_;
    }
}

} // namespace network_system::protocols::quic
