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
#include "kcenon/network/utils/result_types.h"

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace network_system::protocols::quic
{

/*!
 * \brief Stream ID type information
 *
 * Stream ID structure (RFC 9000 Section 2.1):
 * - Bit 0: Initiator (0=client, 1=server)
 * - Bit 1: Directionality (0=bidirectional, 1=unidirectional)
 *
 * Stream types:
 * - 0x00: Client-initiated, bidirectional
 * - 0x01: Server-initiated, bidirectional
 * - 0x02: Client-initiated, unidirectional
 * - 0x03: Server-initiated, unidirectional
 */
namespace stream_id_type
{
    constexpr uint64_t client_bidi = 0x00;
    constexpr uint64_t server_bidi = 0x01;
    constexpr uint64_t client_uni = 0x02;
    constexpr uint64_t server_uni = 0x03;

    /*!
     * \brief Check if stream is client-initiated
     */
    [[nodiscard]] constexpr auto is_client_initiated(uint64_t stream_id) noexcept -> bool
    {
        return (stream_id & 0x01) == 0;
    }

    /*!
     * \brief Check if stream is server-initiated
     */
    [[nodiscard]] constexpr auto is_server_initiated(uint64_t stream_id) noexcept -> bool
    {
        return (stream_id & 0x01) == 1;
    }

    /*!
     * \brief Check if stream is bidirectional
     */
    [[nodiscard]] constexpr auto is_bidirectional(uint64_t stream_id) noexcept -> bool
    {
        return (stream_id & 0x02) == 0;
    }

    /*!
     * \brief Check if stream is unidirectional
     */
    [[nodiscard]] constexpr auto is_unidirectional(uint64_t stream_id) noexcept -> bool
    {
        return (stream_id & 0x02) != 0;
    }

    /*!
     * \brief Get stream type bits (0-3)
     */
    [[nodiscard]] constexpr auto get_type(uint64_t stream_id) noexcept -> uint64_t
    {
        return stream_id & 0x03;
    }

    /*!
     * \brief Get stream sequence number (stream_id >> 2)
     */
    [[nodiscard]] constexpr auto get_sequence(uint64_t stream_id) noexcept -> uint64_t
    {
        return stream_id >> 2;
    }

    /*!
     * \brief Make stream ID from type and sequence number
     */
    [[nodiscard]] constexpr auto make_stream_id(uint64_t type, uint64_t sequence) noexcept -> uint64_t
    {
        return (sequence << 2) | (type & 0x03);
    }
} // namespace stream_id_type

/*!
 * \brief Stream state for sending (RFC 9000 Section 3.1)
 */
enum class send_stream_state
{
    ready,       //!< Stream is ready, can send data
    send,        //!< Sending data
    data_sent,   //!< All data sent, awaiting ACKs
    reset_sent,  //!< RESET_STREAM sent
    reset_recvd, //!< Reset acknowledged by peer (terminal)
    data_recvd,  //!< All data acknowledged (terminal)
};

/*!
 * \brief Stream state for receiving (RFC 9000 Section 3.2)
 */
enum class recv_stream_state
{
    recv,        //!< Receiving data
    size_known,  //!< FIN received, final size known
    data_recvd,  //!< All data received
    reset_recvd, //!< RESET_STREAM received
    data_read,   //!< All data read by application (terminal)
    reset_read,  //!< Reset acknowledged by application (terminal)
};

/*!
 * \brief Stream error codes
 */
namespace stream_error
{
    constexpr int invalid_stream_id = -700;
    constexpr int stream_not_found = -701;
    constexpr int stream_limit_exceeded = -702;
    constexpr int flow_control_error = -703;
    constexpr int final_size_error = -704;
    constexpr int stream_state_error = -705;
    constexpr int stream_reset = -706;
    constexpr int buffer_full = -707;
} // namespace stream_error

/*!
 * \class stream
 * \brief QUIC stream implementation (RFC 9000 Sections 2-4)
 *
 * A QUIC stream is an ordered, reliable, bidirectional or unidirectional
 * byte stream within a QUIC connection.
 */
class stream
{
public:
    /*!
     * \brief Construct a stream
     * \param id Stream identifier
     * \param is_local True if locally initiated
     * \param initial_max_data Initial flow control limit
     */
    explicit stream(uint64_t id, bool is_local, uint64_t initial_max_data = 65536);

    ~stream() = default;

    // Non-copyable
    stream(const stream&) = delete;
    auto operator=(const stream&) -> stream& = delete;

    // Movable
    stream(stream&&) noexcept = default;
    auto operator=(stream&&) noexcept -> stream& = default;

    // ========================================================================
    // Stream Properties
    // ========================================================================

    /*!
     * \brief Get stream identifier
     */
    [[nodiscard]] auto id() const noexcept -> uint64_t { return id_; }

    /*!
     * \brief Check if stream is locally initiated
     */
    [[nodiscard]] auto is_local() const noexcept -> bool { return is_local_; }

    /*!
     * \brief Check if stream is unidirectional
     */
    [[nodiscard]] auto is_unidirectional() const noexcept -> bool
    {
        return stream_id_type::is_unidirectional(id_);
    }

    /*!
     * \brief Check if stream is bidirectional
     */
    [[nodiscard]] auto is_bidirectional() const noexcept -> bool
    {
        return stream_id_type::is_bidirectional(id_);
    }

    // ========================================================================
    // Send Side
    // ========================================================================

    /*!
     * \brief Get send state
     */
    [[nodiscard]] auto send_state() const noexcept -> send_stream_state { return send_state_; }

    /*!
     * \brief Check if stream can send data
     */
    [[nodiscard]] auto can_send() const noexcept -> bool;

    /*!
     * \brief Write data to stream
     * \param data Data to write
     * \return Number of bytes written or error
     */
    [[nodiscard]] auto write(std::span<const uint8_t> data) -> Result<size_t>;

    /*!
     * \brief Mark stream as finished (send FIN)
     * \return Success or error
     */
    [[nodiscard]] auto finish() -> VoidResult;

    /*!
     * \brief Reset the stream with error code
     * \param error_code Application error code
     * \return Success or error
     */
    [[nodiscard]] auto reset(uint64_t error_code) -> VoidResult;

    /*!
     * \brief Check if FIN has been sent
     */
    [[nodiscard]] auto fin_sent() const noexcept -> bool { return fin_sent_; }

    /*!
     * \brief Get number of bytes pending to send
     */
    [[nodiscard]] auto pending_bytes() const noexcept -> size_t { return send_buffer_.size(); }

    /*!
     * \brief Get next STREAM frame to send
     * \param max_size Maximum frame size
     * \return Optional STREAM frame or nullopt if no data
     */
    [[nodiscard]] auto next_stream_frame(size_t max_size) -> std::optional<stream_frame>;

    /*!
     * \brief Acknowledge sent data
     * \param offset Start offset of acknowledged data
     * \param length Length of acknowledged data
     */
    void acknowledge_data(uint64_t offset, uint64_t length);

    // ========================================================================
    // Receive Side
    // ========================================================================

    /*!
     * \brief Get receive state
     */
    [[nodiscard]] auto recv_state() const noexcept -> recv_stream_state { return recv_state_; }

    /*!
     * \brief Check if stream has data to read
     */
    [[nodiscard]] auto has_data() const noexcept -> bool;

    /*!
     * \brief Read data from stream
     * \param buffer Buffer to read into
     * \return Number of bytes read or error
     */
    [[nodiscard]] auto read(std::span<uint8_t> buffer) -> Result<size_t>;

    /*!
     * \brief Check if all data has been received
     */
    [[nodiscard]] auto is_fin_received() const noexcept -> bool { return recv_fin_; }

    /*!
     * \brief Signal that incoming data is no longer wanted
     * \param error_code Application error code
     * \return Success or error
     */
    [[nodiscard]] auto stop_sending(uint64_t error_code) -> VoidResult;

    /*!
     * \brief Receive STREAM frame data
     * \param offset Data offset in stream
     * \param data Frame data
     * \param fin True if this is the final frame
     * \return Success or error
     */
    [[nodiscard]] auto receive_data(uint64_t offset, std::span<const uint8_t> data, bool fin)
        -> VoidResult;

    /*!
     * \brief Handle received RESET_STREAM frame
     * \param error_code Error code from peer
     * \param final_size Final size of stream
     * \return Success or error
     */
    [[nodiscard]] auto receive_reset(uint64_t error_code, uint64_t final_size) -> VoidResult;

    /*!
     * \brief Handle received STOP_SENDING frame
     * \param error_code Error code from peer
     * \return Success or error
     */
    [[nodiscard]] auto receive_stop_sending(uint64_t error_code) -> VoidResult;

    // ========================================================================
    // Flow Control
    // ========================================================================

    /*!
     * \brief Set peer's MAX_STREAM_DATA (our send limit)
     * \param max Maximum data we can send
     */
    void set_max_send_data(uint64_t max);

    /*!
     * \brief Get peer's MAX_STREAM_DATA
     */
    [[nodiscard]] auto max_send_data() const noexcept -> uint64_t { return max_send_offset_; }

    /*!
     * \brief Get available send window
     */
    [[nodiscard]] auto available_send_window() const noexcept -> size_t;

    /*!
     * \brief Update our MAX_STREAM_DATA (peer's send limit)
     * \param max Maximum data peer can send
     */
    void set_max_recv_data(uint64_t max);

    /*!
     * \brief Get our MAX_STREAM_DATA
     */
    [[nodiscard]] auto max_recv_data() const noexcept -> uint64_t { return max_recv_offset_; }

    /*!
     * \brief Get bytes consumed from receive buffer
     */
    [[nodiscard]] auto bytes_consumed() const noexcept -> uint64_t { return recv_offset_; }

    /*!
     * \brief Check if MAX_STREAM_DATA frame should be sent
     */
    [[nodiscard]] auto should_send_max_stream_data() const noexcept -> bool;

    /*!
     * \brief Generate MAX_STREAM_DATA frame if needed
     * \return New MAX_STREAM_DATA value or nullopt
     */
    [[nodiscard]] auto generate_max_stream_data() -> std::optional<uint64_t>;

    // ========================================================================
    // Error Information
    // ========================================================================

    /*!
     * \brief Get reset error code (if stream was reset)
     */
    [[nodiscard]] auto reset_error_code() const noexcept -> std::optional<uint64_t>
    {
        return reset_error_code_;
    }

    /*!
     * \brief Get stop sending error code (if STOP_SENDING received)
     */
    [[nodiscard]] auto stop_sending_error_code() const noexcept -> std::optional<uint64_t>
    {
        return stop_sending_error_code_;
    }

private:
    uint64_t id_;
    bool is_local_;

    // Send state
    send_stream_state send_state_{send_stream_state::ready};
    std::deque<uint8_t> send_buffer_;
    uint64_t send_offset_{0};    // Next byte to send
    uint64_t acked_offset_{0};   // Highest contiguously acknowledged
    bool fin_sent_{false};
    bool fin_acked_{false};

    // Receive state
    recv_stream_state recv_state_{recv_stream_state::recv};
    std::map<uint64_t, std::vector<uint8_t>> recv_buffer_; // offset -> data (for gaps)
    std::deque<uint8_t> recv_ready_;  // Contiguous data ready for reading
    uint64_t recv_offset_{0};         // Next expected offset
    bool recv_fin_{false};
    std::optional<uint64_t> final_size_;

    // Flow control - send
    uint64_t max_send_offset_{0};     // Peer's MAX_STREAM_DATA

    // Flow control - receive
    uint64_t max_recv_offset_{65536}; // Our MAX_STREAM_DATA
    uint64_t recv_window_size_{65536};
    static constexpr double window_update_threshold_{0.5};

    // Error codes
    std::optional<uint64_t> reset_error_code_;
    std::optional<uint64_t> stop_sending_error_code_;

    // Internal helpers
    void reassemble_data();
    void update_send_state();
    void update_recv_state();
};

/*!
 * \brief Get string representation of send stream state
 */
[[nodiscard]] auto send_state_to_string(send_stream_state state) -> const char*;

/*!
 * \brief Get string representation of receive stream state
 */
[[nodiscard]] auto recv_state_to_string(recv_stream_state state) -> const char*;

} // namespace network_system::protocols::quic
