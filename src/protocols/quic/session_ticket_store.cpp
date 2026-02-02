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

#include "internal/protocols/quic/session_ticket_store.h"

#include <algorithm>

namespace kcenon::network::protocols::quic
{

// ============================================================================
// session_ticket_info Implementation
// ============================================================================

auto session_ticket_info::is_valid() const noexcept -> bool
{
    if (ticket_data.empty())
    {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    return expiry > now;
}

auto session_ticket_info::get_obfuscated_age() const noexcept -> uint32_t
{
    auto now = std::chrono::system_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - received_time);

    // Obfuscate the age by adding ticket_age_add (wrapping on overflow)
    uint32_t age_ms = static_cast<uint32_t>(age.count());
    return age_ms + ticket_age_add;
}

// ============================================================================
// session_ticket_store Implementation
// ============================================================================

auto session_ticket_store::store(const std::string& server,
                                  unsigned short port,
                                  const session_ticket_info& ticket) -> void
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = make_key(server, port);
    tickets_[key] = ticket;
}

auto session_ticket_store::retrieve(const std::string& server,
                                     unsigned short port) const
    -> std::optional<session_ticket_info>
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = make_key(server, port);

    auto it = tickets_.find(key);
    if (it == tickets_.end())
    {
        return std::nullopt;
    }

    // Check if the ticket is still valid
    if (!it->second.is_valid())
    {
        return std::nullopt;
    }

    return it->second;
}

auto session_ticket_store::remove(const std::string& server,
                                   unsigned short port) -> bool
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = make_key(server, port);
    return tickets_.erase(key) > 0;
}

auto session_ticket_store::cleanup_expired() -> size_t
{
    std::lock_guard<std::mutex> lock(mutex_);

    size_t removed = 0;
    auto now = std::chrono::system_clock::now();

    for (auto it = tickets_.begin(); it != tickets_.end();)
    {
        if (it->second.expiry <= now)
        {
            it = tickets_.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }

    return removed;
}

auto session_ticket_store::clear() -> void
{
    std::lock_guard<std::mutex> lock(mutex_);
    tickets_.clear();
}

auto session_ticket_store::size() const -> size_t
{
    std::lock_guard<std::mutex> lock(mutex_);
    return tickets_.size();
}

auto session_ticket_store::has_ticket(const std::string& server,
                                       unsigned short port) const -> bool
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto key = make_key(server, port);

    auto it = tickets_.find(key);
    if (it == tickets_.end())
    {
        return false;
    }

    return it->second.is_valid();
}

auto session_ticket_store::make_key(const std::string& server,
                                     unsigned short port) -> std::string
{
    return server + ":" + std::to_string(port);
}

// ============================================================================
// replay_filter Implementation
// ============================================================================

replay_filter::replay_filter()
    : config_{}
{
}

replay_filter::replay_filter(const config& cfg)
    : config_(cfg)
{
}

auto replay_filter::check_and_record(
    std::span<const uint8_t> nonce,
    std::chrono::system_clock::time_point timestamp) -> bool
{
    std::lock_guard<std::mutex> lock(mutex_);

    // First, cleanup old entries
    auto window_start = timestamp - config_.window_size;

    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [&window_start](const nonce_entry& entry) {
                           return entry.timestamp < window_start;
                       }),
        entries_.end());

    // Check if this nonce already exists
    std::vector<uint8_t> nonce_vec(nonce.begin(), nonce.end());
    for (const auto& entry : entries_)
    {
        if (entry.nonce == nonce_vec)
        {
            // Replay detected
            return false;
        }
    }

    // Enforce max entries limit
    if (entries_.size() >= config_.max_entries)
    {
        // Remove oldest entries
        std::sort(entries_.begin(), entries_.end(),
                  [](const nonce_entry& a, const nonce_entry& b) {
                      return a.timestamp < b.timestamp;
                  });

        size_t to_remove = entries_.size() / 4; // Remove 25% of oldest
        entries_.erase(entries_.begin(),
                       entries_.begin() + static_cast<ptrdiff_t>(to_remove));
    }

    // Record the new nonce
    entries_.push_back({std::move(nonce_vec), timestamp});

    return true;
}

auto replay_filter::cleanup(std::chrono::system_clock::time_point now) -> size_t
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto window_start = now - config_.window_size;
    size_t original_size = entries_.size();

    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                       [&window_start](const nonce_entry& entry) {
                           return entry.timestamp < window_start;
                       }),
        entries_.end());

    return original_size - entries_.size();
}

auto replay_filter::clear() -> void
{
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

auto replay_filter::size() const -> size_t
{
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

} // namespace kcenon::network::protocols::quic
