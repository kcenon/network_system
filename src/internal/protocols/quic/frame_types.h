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

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace kcenon::network::protocols::quic
{

/*!
 * \brief QUIC frame types as defined in RFC 9000 Section 12.4
 *
 * Frame types indicate which fields are present in a frame. Some frame types
 * use the low-order bits to encode specific flags.
 */
enum class frame_type : uint64_t
{
    padding = 0x00,
    ping = 0x01,
    ack = 0x02,
    ack_ecn = 0x03,
    reset_stream = 0x04,
    stop_sending = 0x05,
    crypto = 0x06,
    new_token = 0x07,
    // STREAM frames: 0x08-0x0f (use stream_base and flags)
    stream_base = 0x08,
    max_data = 0x10,
    max_stream_data = 0x11,
    max_streams_bidi = 0x12,
    max_streams_uni = 0x13,
    data_blocked = 0x14,
    stream_data_blocked = 0x15,
    streams_blocked_bidi = 0x16,
    streams_blocked_uni = 0x17,
    new_connection_id = 0x18,
    retire_connection_id = 0x19,
    path_challenge = 0x1a,
    path_response = 0x1b,
    connection_close = 0x1c,
    connection_close_app = 0x1d,
    handshake_done = 0x1e,
};

/*!
 * \brief STREAM frame type flags (bits 0-2 of type byte)
 *
 * When the frame type is in range 0x08-0x0f:
 * - Bit 0 (0x01): FIN - Stream is complete
 * - Bit 1 (0x02): LEN - Length field is present
 * - Bit 2 (0x04): OFF - Offset field is present
 */
namespace stream_flags
{
    constexpr uint8_t fin = 0x01;    //!< Stream is finished
    constexpr uint8_t len = 0x02;    //!< Length field present
    constexpr uint8_t off = 0x04;    //!< Offset field present
    constexpr uint8_t mask = 0x07;   //!< Mask for all flags
    constexpr uint8_t base = 0x08;   //!< Base type for STREAM frames
} // namespace stream_flags

/*!
 * \brief Check if a frame type value represents a STREAM frame
 */
[[nodiscard]] constexpr auto is_stream_frame(uint64_t type) noexcept -> bool
{
    return (type >= 0x08 && type <= 0x0f);
}

/*!
 * \brief Extract STREAM flags from frame type
 */
[[nodiscard]] constexpr auto get_stream_flags(uint64_t type) noexcept -> uint8_t
{
    return static_cast<uint8_t>(type & stream_flags::mask);
}

/*!
 * \brief Build STREAM frame type from flags
 */
[[nodiscard]] constexpr auto make_stream_type(bool has_fin, bool has_length,
                                               bool has_offset) noexcept -> uint8_t
{
    uint8_t type = stream_flags::base;
    if (has_fin) type |= stream_flags::fin;
    if (has_length) type |= stream_flags::len;
    if (has_offset) type |= stream_flags::off;
    return type;
}

// ============================================================================
// Frame Structures (RFC 9000 Section 19)
// ============================================================================

/*!
 * \brief PADDING frame (RFC 9000 Section 19.1)
 *
 * A PADDING frame has no semantic value. PADDING frames can be used to
 * increase the size of a packet.
 */
struct padding_frame
{
    size_t count{1}; //!< Number of padding bytes
};

/*!
 * \brief PING frame (RFC 9000 Section 19.2)
 *
 * Endpoints can use PING frames to verify peer liveness or to check
 * reachability to the peer.
 */
struct ping_frame
{
    // PING frames have no content
};

/*!
 * \brief ACK Range for ACK frames
 */
struct ack_range
{
    uint64_t gap{0};    //!< Number of contiguous unacknowledged packets
    uint64_t length{0}; //!< Number of contiguous acknowledged packets
};

/*!
 * \brief ECN counts for ACK_ECN frames
 */
struct ecn_counts
{
    uint64_t ect0{0};  //!< ECT(0) count
    uint64_t ect1{0};  //!< ECT(1) count
    uint64_t ecn_ce{0}; //!< ECN-CE count
};

/*!
 * \brief ACK frame (RFC 9000 Section 19.3)
 *
 * Receivers send ACK frames to inform senders of packets they have
 * received and processed.
 */
struct ack_frame
{
    uint64_t largest_acknowledged{0}; //!< Largest packet number acknowledged
    uint64_t ack_delay{0};            //!< Time since receiving largest_acknowledged (encoded)
    std::vector<ack_range> ranges;    //!< Additional ACK ranges
    std::optional<ecn_counts> ecn;    //!< ECN counts (for ACK_ECN frames)
};

/*!
 * \brief RESET_STREAM frame (RFC 9000 Section 19.4)
 *
 * An endpoint uses a RESET_STREAM frame to abruptly terminate the
 * sending part of a stream.
 */
struct reset_stream_frame
{
    uint64_t stream_id{0};              //!< Stream identifier
    uint64_t application_error_code{0}; //!< Application error code
    uint64_t final_size{0};             //!< Final size of stream
};

/*!
 * \brief STOP_SENDING frame (RFC 9000 Section 19.5)
 *
 * An endpoint uses a STOP_SENDING frame to communicate that incoming
 * data is no longer wanted.
 */
struct stop_sending_frame
{
    uint64_t stream_id{0};              //!< Stream identifier
    uint64_t application_error_code{0}; //!< Application error code
};

/*!
 * \brief CRYPTO frame (RFC 9000 Section 19.6)
 *
 * A CRYPTO frame is used to transmit cryptographic handshake messages.
 */
struct crypto_frame
{
    uint64_t offset{0};            //!< Byte offset in crypto stream
    std::vector<uint8_t> data;     //!< Cryptographic handshake data
};

/*!
 * \brief NEW_TOKEN frame (RFC 9000 Section 19.7)
 *
 * A server sends a NEW_TOKEN frame to provide the client with a token
 * to send in the header of an Initial packet for a future connection.
 */
struct new_token_frame
{
    std::vector<uint8_t> token; //!< Opaque token
};

/*!
 * \brief STREAM frame (RFC 9000 Section 19.8)
 *
 * STREAM frames implicitly create streams and carry stream data.
 */
struct stream_frame
{
    uint64_t stream_id{0};         //!< Stream identifier
    uint64_t offset{0};            //!< Byte offset in stream (0 if not present)
    std::vector<uint8_t> data;     //!< Stream data
    bool fin{false};               //!< True if this is the final data
};

/*!
 * \brief MAX_DATA frame (RFC 9000 Section 19.9)
 *
 * A MAX_DATA frame is used in flow control to inform the peer of the
 * maximum amount of data that can be sent on the connection as a whole.
 */
struct max_data_frame
{
    uint64_t maximum_data{0}; //!< Maximum data that can be sent
};

/*!
 * \brief MAX_STREAM_DATA frame (RFC 9000 Section 19.10)
 *
 * A MAX_STREAM_DATA frame is used in flow control to inform a peer of
 * the maximum amount of data that can be sent on a stream.
 */
struct max_stream_data_frame
{
    uint64_t stream_id{0};         //!< Stream identifier
    uint64_t maximum_stream_data{0}; //!< Maximum stream data
};

/*!
 * \brief MAX_STREAMS frame (RFC 9000 Section 19.11)
 *
 * A MAX_STREAMS frame informs the peer of the cumulative number of
 * streams of a given type it is permitted to open.
 */
struct max_streams_frame
{
    uint64_t maximum_streams{0}; //!< Maximum number of streams
    bool bidirectional{true};    //!< True for bidi, false for uni
};

/*!
 * \brief DATA_BLOCKED frame (RFC 9000 Section 19.12)
 *
 * A sender sends a DATA_BLOCKED frame when it wishes to send data but
 * is unable to do so due to connection-level flow control.
 */
struct data_blocked_frame
{
    uint64_t maximum_data{0}; //!< Connection-level limit at which blocking occurred
};

/*!
 * \brief STREAM_DATA_BLOCKED frame (RFC 9000 Section 19.13)
 *
 * A sender sends a STREAM_DATA_BLOCKED frame when it wishes to send
 * data but is unable to do so due to stream-level flow control.
 */
struct stream_data_blocked_frame
{
    uint64_t stream_id{0};         //!< Stream identifier
    uint64_t maximum_stream_data{0}; //!< Stream-level limit at which blocking occurred
};

/*!
 * \brief STREAMS_BLOCKED frame (RFC 9000 Section 19.14)
 *
 * A sender sends a STREAMS_BLOCKED frame when it wishes to open a
 * stream but is unable to do so due to the maximum stream limit.
 */
struct streams_blocked_frame
{
    uint64_t maximum_streams{0}; //!< Stream limit at which blocking occurred
    bool bidirectional{true};    //!< True for bidi, false for uni
};

/*!
 * \brief NEW_CONNECTION_ID frame (RFC 9000 Section 19.15)
 *
 * An endpoint sends a NEW_CONNECTION_ID frame to provide its peer with
 * alternative connection IDs that can be used to break linkability.
 */
struct new_connection_id_frame
{
    uint64_t sequence_number{0};         //!< Sequence number for this CID
    uint64_t retire_prior_to{0};         //!< CIDs below this should be retired
    std::vector<uint8_t> connection_id;  //!< Connection ID (1-20 bytes)
    std::array<uint8_t, 16> stateless_reset_token{}; //!< Stateless reset token
};

/*!
 * \brief RETIRE_CONNECTION_ID frame (RFC 9000 Section 19.16)
 *
 * An endpoint sends a RETIRE_CONNECTION_ID frame to indicate that it
 * will no longer use a connection ID that was issued by its peer.
 */
struct retire_connection_id_frame
{
    uint64_t sequence_number{0}; //!< Sequence number of CID to retire
};

/*!
 * \brief PATH_CHALLENGE frame (RFC 9000 Section 19.17)
 *
 * Endpoints can use PATH_CHALLENGE frames to check reachability and
 * for path validation during connection migration.
 */
struct path_challenge_frame
{
    std::array<uint8_t, 8> data{}; //!< Arbitrary 8-byte data
};

/*!
 * \brief PATH_RESPONSE frame (RFC 9000 Section 19.18)
 *
 * A PATH_RESPONSE frame is sent in response to a PATH_CHALLENGE frame.
 */
struct path_response_frame
{
    std::array<uint8_t, 8> data{}; //!< Data from PATH_CHALLENGE
};

/*!
 * \brief CONNECTION_CLOSE frame (RFC 9000 Section 19.19)
 *
 * An endpoint sends a CONNECTION_CLOSE frame to notify its peer that
 * the connection is being closed.
 */
struct connection_close_frame
{
    uint64_t error_code{0};        //!< Error code indicating reason
    uint64_t frame_type{0};        //!< Type of frame that triggered (transport close only)
    std::string reason_phrase;     //!< Human-readable reason
    bool is_application_error{false}; //!< True if application-level error
};

/*!
 * \brief HANDSHAKE_DONE frame (RFC 9000 Section 19.20)
 *
 * The server uses a HANDSHAKE_DONE frame to signal confirmation of
 * the handshake to the client.
 */
struct handshake_done_frame
{
    // HANDSHAKE_DONE frames have no content
};

// ============================================================================
// Frame Variant
// ============================================================================

/*!
 * \brief Variant type holding any QUIC frame
 */
using frame = std::variant<
    padding_frame,
    ping_frame,
    ack_frame,
    reset_stream_frame,
    stop_sending_frame,
    crypto_frame,
    new_token_frame,
    stream_frame,
    max_data_frame,
    max_stream_data_frame,
    max_streams_frame,
    data_blocked_frame,
    stream_data_blocked_frame,
    streams_blocked_frame,
    new_connection_id_frame,
    retire_connection_id_frame,
    path_challenge_frame,
    path_response_frame,
    connection_close_frame,
    handshake_done_frame
>;

/*!
 * \brief Get the frame type for a frame variant
 */
[[nodiscard]] auto get_frame_type(const frame& f) -> frame_type;

/*!
 * \brief Get string name for a frame type
 */
[[nodiscard]] auto frame_type_to_string(frame_type type) -> std::string;

} // namespace kcenon::network::protocols::quic
