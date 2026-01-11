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

#include "kcenon/network/protocols/quic/transport_params.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace kcenon::network::protocols::quic
{

/*!
 * \struct session_ticket_info
 * \brief Contains session ticket data for 0-RTT resumption
 *
 * This structure holds all information needed to resume a QUIC connection
 * using 0-RTT (zero round-trip time) session resumption as defined in
 * RFC 9001 Section 4.6.
 */
struct session_ticket_info
{
    //! Raw session ticket data from TLS 1.3 NewSessionTicket
    std::vector<uint8_t> ticket_data;

    //! Ticket expiration time
    std::chrono::system_clock::time_point expiry;

    //! Server name (for SNI matching)
    std::string server_name;

    //! Server port (for endpoint matching)
    unsigned short port{0};

    //! Saved transport parameters from the original connection
    transport_parameters saved_params;

    //! Maximum early data size allowed (from max_early_data_size extension)
    uint32_t max_early_data_size{0};

    //! Ticket age add value for obfuscation (RFC 8446)
    uint32_t ticket_age_add{0};

    //! Time when the ticket was received
    std::chrono::system_clock::time_point received_time;

    /*!
     * \brief Check if the ticket is still valid (not expired)
     * \return true if ticket can be used for resumption
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    /*!
     * \brief Get obfuscated ticket age (RFC 8446 Section 4.2.11.1)
     * \return Obfuscated age in milliseconds
     */
    [[nodiscard]] auto get_obfuscated_age() const noexcept -> uint32_t;
};

/*!
 * \class session_ticket_store
 * \brief Thread-safe storage for QUIC session tickets
 *
 * Manages session tickets for 0-RTT connection resumption. Each ticket
 * is associated with a server endpoint (host:port) and includes the
 * transport parameters from the original connection.
 *
 * ### Thread Safety
 * All public methods are thread-safe and can be called concurrently.
 *
 * ### Usage Example
 * \code
 * session_ticket_store store;
 *
 * // Store a ticket after successful handshake
 * session_ticket_info info;
 * info.ticket_data = received_ticket;
 * info.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
 * info.server_name = "example.com";
 * info.port = 443;
 * store.store("example.com", 443, info);
 *
 * // Retrieve for subsequent connection
 * auto ticket = store.retrieve("example.com", 443);
 * if (ticket && ticket->is_valid()) {
 *     // Use ticket for 0-RTT
 * }
 * \endcode
 */
class session_ticket_store
{
public:
    /*!
     * \brief Default constructor
     */
    session_ticket_store() = default;

    /*!
     * \brief Destructor
     */
    ~session_ticket_store() = default;

    // Non-copyable, non-movable (contains mutex)
    session_ticket_store(const session_ticket_store&) = delete;
    session_ticket_store& operator=(const session_ticket_store&) = delete;
    session_ticket_store(session_ticket_store&&) = delete;
    session_ticket_store& operator=(session_ticket_store&&) = delete;

    /*!
     * \brief Store a session ticket for a server
     * \param server Server hostname
     * \param port Server port
     * \param ticket Ticket information to store
     *
     * If a ticket already exists for this server, it will be replaced.
     */
    auto store(const std::string& server,
               unsigned short port,
               const session_ticket_info& ticket) -> void;

    /*!
     * \brief Retrieve a session ticket for a server
     * \param server Server hostname
     * \param port Server port
     * \return Ticket info if found and valid, nullopt otherwise
     *
     * Returns nullopt if:
     * - No ticket exists for the server
     * - The ticket has expired
     */
    [[nodiscard]] auto retrieve(const std::string& server,
                                 unsigned short port) const
        -> std::optional<session_ticket_info>;

    /*!
     * \brief Remove a session ticket for a server
     * \param server Server hostname
     * \param port Server port
     * \return true if a ticket was removed, false if none existed
     */
    auto remove(const std::string& server, unsigned short port) -> bool;

    /*!
     * \brief Remove all expired tickets from the store
     * \return Number of tickets removed
     */
    auto cleanup_expired() -> size_t;

    /*!
     * \brief Clear all stored tickets
     */
    auto clear() -> void;

    /*!
     * \brief Get the number of stored tickets
     * \return Number of tickets currently stored
     */
    [[nodiscard]] auto size() const -> size_t;

    /*!
     * \brief Check if a valid ticket exists for a server
     * \param server Server hostname
     * \param port Server port
     * \return true if a valid ticket exists
     */
    [[nodiscard]] auto has_ticket(const std::string& server,
                                   unsigned short port) const -> bool;

private:
    /*!
     * \brief Generate a key for the ticket map
     * \param server Server hostname
     * \param port Server port
     * \return Key string in format "server:port"
     */
    [[nodiscard]] static auto make_key(const std::string& server,
                                        unsigned short port) -> std::string;

    //! Thread-safety mutex
    mutable std::mutex mutex_;

    //! Ticket storage (key: "server:port")
    std::unordered_map<std::string, session_ticket_info> tickets_;
};

/*!
 * \class replay_filter
 * \brief Anti-replay protection for 0-RTT data
 *
 * Implements a time-based replay filter to prevent replay attacks on
 * 0-RTT early data. Uses a sliding window approach based on client hello
 * random values.
 *
 * ### RFC 8446 Section 8
 * Servers that accept 0-RTT must implement anti-replay protection.
 * This implementation uses a combination of:
 * - Time-based window (reject old tickets)
 * - Nonce tracking (reject duplicate tickets within window)
 *
 * ### Thread Safety
 * All public methods are thread-safe.
 */
class replay_filter
{
public:
    /*!
     * \brief Configuration for the replay filter
     */
    struct config
    {
        //! Window size for tracking nonces (default: 10 seconds)
        std::chrono::seconds window_size{10};

        //! Maximum number of nonces to track (default: 100000)
        size_t max_entries{100000};
    };

    /*!
     * \brief Construct a replay filter with default configuration
     */
    replay_filter();

    /*!
     * \brief Construct a replay filter with custom configuration
     * \param cfg Configuration parameters
     */
    explicit replay_filter(const config& cfg);

    /*!
     * \brief Check if data should be accepted (not a replay)
     * \param nonce Unique identifier (e.g., client hello random)
     * \param timestamp Time of the request
     * \return true if this is NOT a replay (accept), false if replay (reject)
     *
     * This method both checks and records the nonce atomically.
     */
    [[nodiscard]] auto check_and_record(
        std::span<const uint8_t> nonce,
        std::chrono::system_clock::time_point timestamp
            = std::chrono::system_clock::now()) -> bool;

    /*!
     * \brief Remove old entries outside the window
     * \param now Current time
     * \return Number of entries removed
     */
    auto cleanup(std::chrono::system_clock::time_point now
                     = std::chrono::system_clock::now()) -> size_t;

    /*!
     * \brief Clear all recorded nonces
     */
    auto clear() -> void;

    /*!
     * \brief Get the number of tracked nonces
     * \return Number of entries in the filter
     */
    [[nodiscard]] auto size() const -> size_t;

private:
    struct nonce_entry
    {
        std::vector<uint8_t> nonce;
        std::chrono::system_clock::time_point timestamp;
    };

    config config_;
    mutable std::mutex mutex_;
    std::vector<nonce_entry> entries_;
};

} // namespace kcenon::network::protocols::quic
