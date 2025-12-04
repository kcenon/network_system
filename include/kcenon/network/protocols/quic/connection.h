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

#include "kcenon/network/protocols/quic/connection_id.h"
#include "kcenon/network/protocols/quic/crypto.h"
#include "kcenon/network/protocols/quic/flow_control.h"
#include "kcenon/network/protocols/quic/frame.h"
#include "kcenon/network/protocols/quic/packet.h"
#include "kcenon/network/protocols/quic/stream_manager.h"
#include "kcenon/network/protocols/quic/transport_params.h"
#include "kcenon/network/utils/result_types.h"

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace network_system::protocols::quic
{

// ============================================================================
// Connection State Enums
// ============================================================================

/*!
 * \brief QUIC connection state (RFC 9000 Section 5)
 */
enum class connection_state
{
    idle,        //!< Connection not yet started
    handshaking, //!< TLS handshake in progress
    connected,   //!< Handshake complete, can send/receive data
    closing,     //!< CONNECTION_CLOSE sent, waiting for timeout
    draining,    //!< CONNECTION_CLOSE received, draining period
    closed,      //!< Connection terminated
};

/*!
 * \brief TLS handshake state
 */
enum class handshake_state
{
    initial,              //!< Not started
    waiting_server_hello, //!< Client waiting for ServerHello
    waiting_finished,     //!< Waiting for peer's Finished
    complete,             //!< Handshake complete
};

/*!
 * \brief Convert connection state to string
 */
[[nodiscard]] auto connection_state_to_string(connection_state state) -> const char*;

/*!
 * \brief Convert handshake state to string
 */
[[nodiscard]] auto handshake_state_to_string(handshake_state state) -> const char*;

// ============================================================================
// Connection Error Codes
// ============================================================================

namespace connection_error
{
    constexpr int not_established = -730;
    constexpr int handshake_failed = -731;
    constexpr int invalid_state = -732;
    constexpr int protocol_violation = -733;
    constexpr int connection_refused = -734;
    constexpr int idle_timeout = -735;
    constexpr int connection_closed = -736;
} // namespace connection_error

// ============================================================================
// Sent Packet Tracking
// ============================================================================

/*!
 * \struct sent_packet_info
 * \brief Information about a sent packet for loss detection
 */
struct sent_packet_info
{
    uint64_t packet_number{0};
    std::chrono::steady_clock::time_point sent_time;
    size_t sent_bytes{0};
    bool ack_eliciting{false};
    bool in_flight{false};
    encryption_level level{encryption_level::initial};

    // Frames included in this packet (for retransmission)
    std::vector<frame> frames;
};

// ============================================================================
// Packet Number Space
// ============================================================================

/*!
 * \struct packet_number_space
 * \brief State for each packet number space (Initial, Handshake, Application)
 */
struct packet_number_space
{
    //! Next packet number to use
    uint64_t next_pn{0};

    //! Largest acknowledged packet number
    uint64_t largest_acked{0};

    //! Largest received packet number
    uint64_t largest_received{0};

    //! Time of receiving largest packet
    std::chrono::steady_clock::time_point largest_received_time;

    //! Packets awaiting acknowledgment
    std::map<uint64_t, sent_packet_info> sent_packets;

    //! Whether an ACK is needed
    bool ack_needed{false};

    //! ACK delay for this space
    std::chrono::microseconds ack_delay{0};
};

// ============================================================================
// Connection Class
// ============================================================================

/*!
 * \class connection
 * \brief QUIC connection state machine (RFC 9000 Section 5)
 *
 * Manages the complete lifecycle of a QUIC connection including:
 * - TLS handshake via QUIC-TLS integration
 * - Packet number spaces (Initial, Handshake, Application)
 * - Transport parameters negotiation
 * - Stream management
 * - Flow control
 * - Connection termination
 */
class connection
{
public:
    /*!
     * \brief Construct a connection
     * \param is_server True for server-side, false for client-side
     * \param initial_dcid Initial Destination Connection ID
     */
    connection(bool is_server, const connection_id& initial_dcid);

    ~connection();

    // Non-copyable and non-movable (members are not movable)
    connection(const connection&) = delete;
    auto operator=(const connection&) -> connection& = delete;
    connection(connection&&) = delete;
    auto operator=(connection&&) -> connection& = delete;

    // ========================================================================
    // Connection State
    // ========================================================================

    /*!
     * \brief Get current connection state
     */
    [[nodiscard]] auto state() const noexcept -> connection_state { return state_; }

    /*!
     * \brief Get current handshake state
     */
    [[nodiscard]] auto handshake_state() const noexcept -> enum handshake_state
    {
        return hs_state_;
    }

    /*!
     * \brief Check if connection is established (handshake complete)
     */
    [[nodiscard]] auto is_established() const noexcept -> bool
    {
        return state_ == connection_state::connected;
    }

    /*!
     * \brief Check if connection is draining or closing
     */
    [[nodiscard]] auto is_draining() const noexcept -> bool
    {
        return state_ == connection_state::draining ||
               state_ == connection_state::closing;
    }

    /*!
     * \brief Check if connection is closed
     */
    [[nodiscard]] auto is_closed() const noexcept -> bool
    {
        return state_ == connection_state::closed;
    }

    /*!
     * \brief Check if this is a server-side connection
     */
    [[nodiscard]] auto is_server() const noexcept -> bool { return is_server_; }

    // ========================================================================
    // Connection IDs
    // ========================================================================

    /*!
     * \brief Get local Connection ID
     */
    [[nodiscard]] auto local_cid() const -> const connection_id& { return local_cid_; }

    /*!
     * \brief Get remote Connection ID
     */
    [[nodiscard]] auto remote_cid() const -> const connection_id& { return remote_cid_; }

    /*!
     * \brief Get initial Destination Connection ID (for key derivation)
     */
    [[nodiscard]] auto initial_dcid() const -> const connection_id& { return initial_dcid_; }

    /*!
     * \brief Add a new local Connection ID
     * \param cid Connection ID to add
     * \param sequence Sequence number for this CID
     * \return Success or error
     */
    [[nodiscard]] auto add_local_cid(const connection_id& cid, uint64_t sequence)
        -> VoidResult;

    /*!
     * \brief Retire a Connection ID
     * \param sequence Sequence number of CID to retire
     * \return Success or error
     */
    [[nodiscard]] auto retire_cid(uint64_t sequence) -> VoidResult;

    // ========================================================================
    // Transport Parameters
    // ========================================================================

    /*!
     * \brief Set local transport parameters
     * \param params Parameters to advertise to peer
     */
    void set_local_params(const transport_parameters& params);

    /*!
     * \brief Set remote transport parameters
     * \param params Parameters received from peer
     */
    void set_remote_params(const transport_parameters& params);

    /*!
     * \brief Get local transport parameters
     */
    [[nodiscard]] auto local_params() const -> const transport_parameters&
    {
        return local_params_;
    }

    /*!
     * \brief Get remote transport parameters
     */
    [[nodiscard]] auto remote_params() const -> const transport_parameters&
    {
        return remote_params_;
    }

    // ========================================================================
    // Handshake
    // ========================================================================

    /*!
     * \brief Start the handshake (client only)
     * \param server_name Server hostname for SNI
     * \return Initial crypto data to send or error
     */
    [[nodiscard]] auto start_handshake(const std::string& server_name)
        -> Result<std::vector<uint8_t>>;

    /*!
     * \brief Initialize server handshake
     * \param cert_file Path to certificate file
     * \param key_file Path to private key file
     * \return Success or error
     */
    [[nodiscard]] auto init_server_handshake(const std::string& cert_file,
                                               const std::string& key_file) -> VoidResult;

    // ========================================================================
    // Packet Processing
    // ========================================================================

    /*!
     * \brief Receive and process a packet
     * \param data Raw packet data
     * \return Success or error
     */
    [[nodiscard]] auto receive_packet(std::span<const uint8_t> data) -> VoidResult;

    /*!
     * \brief Generate packets to send
     * \return Vector of packets (may be coalesced for handshake)
     */
    [[nodiscard]] auto generate_packets() -> std::vector<std::vector<uint8_t>>;

    /*!
     * \brief Check if there are packets to send
     */
    [[nodiscard]] auto has_pending_data() const -> bool;

    // ========================================================================
    // Stream Access
    // ========================================================================

    /*!
     * \brief Get the stream manager
     */
    [[nodiscard]] auto streams() -> stream_manager& { return stream_mgr_; }

    /*!
     * \brief Get the stream manager (const)
     */
    [[nodiscard]] auto streams() const -> const stream_manager& { return stream_mgr_; }

    // ========================================================================
    // Flow Control
    // ========================================================================

    /*!
     * \brief Get the connection-level flow controller
     */
    [[nodiscard]] auto flow_control() -> flow_controller& { return flow_ctrl_; }

    /*!
     * \brief Get the connection-level flow controller (const)
     */
    [[nodiscard]] auto flow_control() const -> const flow_controller& { return flow_ctrl_; }

    // ========================================================================
    // Connection Close
    // ========================================================================

    /*!
     * \brief Close the connection
     * \param error_code Error code (0 for normal close)
     * \param reason Optional reason phrase
     * \return Success or error
     */
    [[nodiscard]] auto close(uint64_t error_code, const std::string& reason = "")
        -> VoidResult;

    /*!
     * \brief Close connection due to application error
     * \param error_code Application-level error code
     * \param reason Optional reason phrase
     * \return Success or error
     */
    [[nodiscard]] auto close_application(uint64_t error_code,
                                          const std::string& reason = "") -> VoidResult;

    /*!
     * \brief Get the close error code (if connection was closed)
     */
    [[nodiscard]] auto close_error_code() const -> std::optional<uint64_t>
    {
        return close_error_code_;
    }

    /*!
     * \brief Get the close reason (if connection was closed)
     */
    [[nodiscard]] auto close_reason() const -> const std::string& { return close_reason_; }

    // ========================================================================
    // Timers
    // ========================================================================

    /*!
     * \brief Get the next timeout deadline
     * \return Timeout time point or nullopt if no timer is set
     */
    [[nodiscard]] auto next_timeout() const
        -> std::optional<std::chrono::steady_clock::time_point>;

    /*!
     * \brief Handle timeout event
     */
    void on_timeout();

    /*!
     * \brief Get idle timeout deadline
     */
    [[nodiscard]] auto idle_deadline() const -> std::chrono::steady_clock::time_point
    {
        return idle_deadline_;
    }

    // ========================================================================
    // Crypto Access
    // ========================================================================

    /*!
     * \brief Get crypto handler
     */
    [[nodiscard]] auto crypto() -> quic_crypto& { return crypto_; }

    /*!
     * \brief Get crypto handler (const)
     */
    [[nodiscard]] auto crypto() const -> const quic_crypto& { return crypto_; }

    // ========================================================================
    // Statistics
    // ========================================================================

    /*!
     * \brief Get total bytes sent
     */
    [[nodiscard]] auto bytes_sent() const noexcept -> uint64_t { return bytes_sent_; }

    /*!
     * \brief Get total bytes received
     */
    [[nodiscard]] auto bytes_received() const noexcept -> uint64_t { return bytes_received_; }

    /*!
     * \brief Get packets sent count
     */
    [[nodiscard]] auto packets_sent() const noexcept -> uint64_t { return packets_sent_; }

    /*!
     * \brief Get packets received count
     */
    [[nodiscard]] auto packets_received() const noexcept -> uint64_t
    {
        return packets_received_;
    }

private:
    // ========================================================================
    // Private Members
    // ========================================================================

    bool is_server_;
    connection_state state_{connection_state::idle};
    enum handshake_state hs_state_ { handshake_state::initial };

    // Connection IDs
    connection_id local_cid_;
    connection_id remote_cid_;
    connection_id initial_dcid_;  // For initial key derivation
    std::vector<std::pair<uint64_t, connection_id>> local_cids_;
    uint64_t next_cid_sequence_{1};

    // Transport parameters
    transport_parameters local_params_;
    transport_parameters remote_params_;

    // Subsystems
    quic_crypto crypto_;
    stream_manager stream_mgr_;
    flow_controller flow_ctrl_;

    // Packet number spaces
    packet_number_space initial_space_;
    packet_number_space handshake_space_;
    packet_number_space app_space_;

    // Pending crypto data per encryption level
    std::deque<std::vector<uint8_t>> pending_crypto_initial_;
    std::deque<std::vector<uint8_t>> pending_crypto_handshake_;
    std::deque<std::vector<uint8_t>> pending_crypto_app_;

    // Pending ACKs per encryption level
    bool pending_ack_initial_{false};
    bool pending_ack_handshake_{false};
    bool pending_ack_app_{false};

    // Pending frames to send
    std::deque<frame> pending_frames_;

    // Close state
    bool close_sent_{false};
    bool close_received_{false};
    std::optional<uint64_t> close_error_code_;
    std::string close_reason_;
    bool application_close_{false};

    // Timers
    std::chrono::steady_clock::time_point idle_deadline_;
    std::chrono::steady_clock::time_point pto_deadline_;
    std::chrono::steady_clock::time_point drain_deadline_;

    // Statistics
    uint64_t bytes_sent_{0};
    uint64_t bytes_received_{0};
    uint64_t packets_sent_{0};
    uint64_t packets_received_{0};

    // ========================================================================
    // Private Methods
    // ========================================================================

    /*!
     * \brief Process a long header packet
     */
    [[nodiscard]] auto process_long_header_packet(const long_header& hdr,
                                                   std::span<const uint8_t> payload)
        -> VoidResult;

    /*!
     * \brief Process a short header packet
     */
    [[nodiscard]] auto process_short_header_packet(const short_header& hdr,
                                                    std::span<const uint8_t> payload)
        -> VoidResult;

    /*!
     * \brief Process frames from a decrypted packet
     */
    [[nodiscard]] auto process_frames(std::span<const uint8_t> payload,
                                       encryption_level level) -> VoidResult;

    /*!
     * \brief Handle a specific frame
     */
    [[nodiscard]] auto handle_frame(const frame& frame, encryption_level level)
        -> VoidResult;

    /*!
     * \brief Build a packet at the given encryption level
     */
    [[nodiscard]] auto build_packet(encryption_level level) -> std::vector<uint8_t>;

    /*!
     * \brief Get packet number space for an encryption level
     */
    [[nodiscard]] auto get_pn_space(encryption_level level) -> packet_number_space&;
    [[nodiscard]] auto get_pn_space(encryption_level level) const
        -> const packet_number_space&;

    /*!
     * \brief Update connection state based on handshake progress
     */
    void update_state();

    /*!
     * \brief Reset idle timer
     */
    void reset_idle_timer();

    /*!
     * \brief Transition to draining state
     */
    void enter_draining();

    /*!
     * \brief Transition to closing state
     */
    void enter_closing();

    /*!
     * \brief Apply remote transport parameters
     */
    void apply_remote_params();

    /*!
     * \brief Generate ACK frame for a packet number space
     */
    [[nodiscard]] auto generate_ack_frame(const packet_number_space& space)
        -> std::optional<ack_frame>;
};

} // namespace network_system::protocols::quic
