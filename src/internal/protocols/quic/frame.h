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

#include "frame_types.h"
#include "kcenon/network/detail/utils/result_types.h"

#include <span>
#include <utility>
#include <vector>

namespace kcenon::network::protocols::quic
{

/*!
 * \class frame_parser
 * \brief Parser for QUIC frames (RFC 9000 Section 12)
 *
 * Parses raw bytes into QUIC frame structures. Handles all frame types
 * defined in RFC 9000 Section 19.
 */
class frame_parser
{
public:
    /*!
     * \brief Parse a single frame from buffer
     * \param data Input buffer containing frame data
     * \return Result containing (parsed_frame, bytes_consumed) or error
     */
    [[nodiscard]] static auto parse(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    /*!
     * \brief Parse all frames from buffer
     * \param data Input buffer containing one or more frames
     * \return Result containing vector of parsed frames or error
     */
    [[nodiscard]] static auto parse_all(std::span<const uint8_t> data)
        -> Result<std::vector<frame>>;

    /*!
     * \brief Get the frame type from raw data without full parsing
     * \param data Input buffer
     * \return Result containing (frame_type_value, bytes_consumed) or error
     */
    [[nodiscard]] static auto peek_type(std::span<const uint8_t> data)
        -> Result<std::pair<uint64_t, size_t>>;

private:
    // Individual frame parsers
    [[nodiscard]] static auto parse_padding(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_ping(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_ack(std::span<const uint8_t> data, bool has_ecn)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_reset_stream(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_stop_sending(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_crypto(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_new_token(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_stream(std::span<const uint8_t> data, uint8_t flags)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_max_data(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_max_stream_data(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_max_streams(std::span<const uint8_t> data, bool bidi)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_data_blocked(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_stream_data_blocked(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_streams_blocked(std::span<const uint8_t> data, bool bidi)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_new_connection_id(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_retire_connection_id(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_path_challenge(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_path_response(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_connection_close(std::span<const uint8_t> data,
                                                      bool is_app)
        -> Result<std::pair<frame, size_t>>;

    [[nodiscard]] static auto parse_handshake_done(std::span<const uint8_t> data)
        -> Result<std::pair<frame, size_t>>;
};

/*!
 * \class frame_builder
 * \brief Builder for QUIC frames (RFC 9000 Section 12)
 *
 * Serializes QUIC frame structures into raw bytes.
 */
class frame_builder
{
public:
    /*!
     * \brief Build any frame from variant
     * \param f Frame to serialize
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build(const frame& f) -> std::vector<uint8_t>;

    /*!
     * \brief Build PADDING frame
     * \param count Number of padding bytes (default 1)
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_padding(size_t count = 1) -> std::vector<uint8_t>;

    /*!
     * \brief Build PING frame
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_ping() -> std::vector<uint8_t>;

    /*!
     * \brief Build ACK frame
     * \param f ACK frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_ack(const ack_frame& f) -> std::vector<uint8_t>;

    /*!
     * \brief Build RESET_STREAM frame
     * \param f RESET_STREAM frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_reset_stream(const reset_stream_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build STOP_SENDING frame
     * \param f STOP_SENDING frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_stop_sending(const stop_sending_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build CRYPTO frame
     * \param f CRYPTO frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_crypto(const crypto_frame& f) -> std::vector<uint8_t>;

    /*!
     * \brief Build NEW_TOKEN frame
     * \param f NEW_TOKEN frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_new_token(const new_token_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build STREAM frame
     * \param f STREAM frame structure
     * \param include_length Whether to include explicit length field
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_stream(const stream_frame& f,
                                            bool include_length = true)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build MAX_DATA frame
     * \param f MAX_DATA frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_max_data(const max_data_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build MAX_STREAM_DATA frame
     * \param f MAX_STREAM_DATA frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_max_stream_data(const max_stream_data_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build MAX_STREAMS frame
     * \param f MAX_STREAMS frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_max_streams(const max_streams_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build DATA_BLOCKED frame
     * \param f DATA_BLOCKED frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_data_blocked(const data_blocked_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build STREAM_DATA_BLOCKED frame
     * \param f STREAM_DATA_BLOCKED frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_stream_data_blocked(const stream_data_blocked_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build STREAMS_BLOCKED frame
     * \param f STREAMS_BLOCKED frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_streams_blocked(const streams_blocked_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build NEW_CONNECTION_ID frame
     * \param f NEW_CONNECTION_ID frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_new_connection_id(const new_connection_id_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build RETIRE_CONNECTION_ID frame
     * \param f RETIRE_CONNECTION_ID frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_retire_connection_id(const retire_connection_id_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build PATH_CHALLENGE frame
     * \param f PATH_CHALLENGE frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_path_challenge(const path_challenge_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build PATH_RESPONSE frame
     * \param f PATH_RESPONSE frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_path_response(const path_response_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build CONNECTION_CLOSE frame
     * \param f CONNECTION_CLOSE frame structure
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_connection_close(const connection_close_frame& f)
        -> std::vector<uint8_t>;

    /*!
     * \brief Build HANDSHAKE_DONE frame
     * \return Serialized frame bytes
     */
    [[nodiscard]] static auto build_handshake_done() -> std::vector<uint8_t>;

private:
    // Helper to append varint-encoded value
    static void append_varint(std::vector<uint8_t>& buffer, uint64_t value);

    // Helper to append raw bytes
    static void append_bytes(std::vector<uint8_t>& buffer,
                             std::span<const uint8_t> data);
};

} // namespace kcenon::network::protocols::quic
