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
#include "kcenon/network/utils/result_types.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace network_system::protocols::quic
{

/*!
 * \brief Transport parameter IDs as defined in RFC 9000 Section 18.2
 */
namespace transport_param_id
{
    constexpr uint64_t original_destination_connection_id = 0x00;
    constexpr uint64_t max_idle_timeout = 0x01;
    constexpr uint64_t stateless_reset_token = 0x02;
    constexpr uint64_t max_udp_payload_size = 0x03;
    constexpr uint64_t initial_max_data = 0x04;
    constexpr uint64_t initial_max_stream_data_bidi_local = 0x05;
    constexpr uint64_t initial_max_stream_data_bidi_remote = 0x06;
    constexpr uint64_t initial_max_stream_data_uni = 0x07;
    constexpr uint64_t initial_max_streams_bidi = 0x08;
    constexpr uint64_t initial_max_streams_uni = 0x09;
    constexpr uint64_t ack_delay_exponent = 0x0a;
    constexpr uint64_t max_ack_delay = 0x0b;
    constexpr uint64_t disable_active_migration = 0x0c;
    constexpr uint64_t preferred_address = 0x0d;
    constexpr uint64_t active_connection_id_limit = 0x0e;
    constexpr uint64_t initial_source_connection_id = 0x0f;
    constexpr uint64_t retry_source_connection_id = 0x10;
} // namespace transport_param_id

/*!
 * \brief Transport parameter error codes
 */
namespace transport_param_error
{
    constexpr int invalid_parameter = -720;
    constexpr int decode_error = -721;
    constexpr int duplicate_parameter = -722;
    constexpr int missing_required_parameter = -723;
    constexpr int invalid_value = -724;
} // namespace transport_param_error

/*!
 * \struct preferred_address_info
 * \brief QUIC preferred address transport parameter (RFC 9000 Section 18.2)
 */
struct preferred_address_info
{
    std::array<uint8_t, 4> ipv4_address{};
    uint16_t ipv4_port{0};
    std::array<uint8_t, 16> ipv6_address{};
    uint16_t ipv6_port{0};
    connection_id connection_id;
    std::array<uint8_t, 16> stateless_reset_token{};
};

/*!
 * \struct transport_parameters
 * \brief QUIC transport parameters (RFC 9000 Section 18)
 *
 * Transport parameters are exchanged during connection establishment
 * and control various aspects of the QUIC connection.
 */
struct transport_parameters
{
    // ========================================================================
    // Connection IDs (only set by server or during retry)
    // ========================================================================

    //! Original Destination Connection ID (server only)
    std::optional<connection_id> original_destination_connection_id;

    //! Initial Source Connection ID
    std::optional<connection_id> initial_source_connection_id;

    //! Retry Source Connection ID (server only, after Retry)
    std::optional<connection_id> retry_source_connection_id;

    //! Stateless reset token (server only, 16 bytes)
    std::optional<std::array<uint8_t, 16>> stateless_reset_token;

    // ========================================================================
    // Timing Parameters
    // ========================================================================

    //! Maximum idle timeout in milliseconds (0 = disabled)
    uint64_t max_idle_timeout{0};

    //! ACK delay exponent (default: 3, meaning 8 microseconds)
    uint64_t ack_delay_exponent{3};

    //! Maximum ACK delay in milliseconds (default: 25)
    uint64_t max_ack_delay{25};

    // ========================================================================
    // Flow Control Parameters
    // ========================================================================

    //! Maximum UDP payload size (default: 65527)
    uint64_t max_udp_payload_size{65527};

    //! Initial maximum data for connection (default: 0)
    uint64_t initial_max_data{0};

    //! Initial maximum data for locally-initiated bidirectional streams
    uint64_t initial_max_stream_data_bidi_local{0};

    //! Initial maximum data for remotely-initiated bidirectional streams
    uint64_t initial_max_stream_data_bidi_remote{0};

    //! Initial maximum data for unidirectional streams
    uint64_t initial_max_stream_data_uni{0};

    // ========================================================================
    // Stream Limits
    // ========================================================================

    //! Initial maximum bidirectional streams
    uint64_t initial_max_streams_bidi{0};

    //! Initial maximum unidirectional streams
    uint64_t initial_max_streams_uni{0};

    // ========================================================================
    // Connection Options
    // ========================================================================

    //! Whether active connection migration is disabled
    bool disable_active_migration{false};

    //! Maximum number of connection IDs from the peer
    uint64_t active_connection_id_limit{2};

    //! Preferred address for migration (server only)
    std::optional<preferred_address_info> preferred_address;

    // ========================================================================
    // Encoding / Decoding
    // ========================================================================

    /*!
     * \brief Encode transport parameters to binary format
     * \return Encoded bytes
     */
    [[nodiscard]] auto encode() const -> std::vector<uint8_t>;

    /*!
     * \brief Decode transport parameters from binary format
     * \param data Encoded transport parameters
     * \return Decoded parameters or error
     */
    [[nodiscard]] static auto decode(std::span<const uint8_t> data)
        -> Result<transport_parameters>;

    // ========================================================================
    // Validation
    // ========================================================================

    /*!
     * \brief Validate transport parameters
     * \param is_server True if validating server parameters
     * \return Success or error
     */
    [[nodiscard]] auto validate(bool is_server) const -> VoidResult;

    /*!
     * \brief Apply default values for unset parameters
     */
    void apply_defaults();

    // ========================================================================
    // Comparison
    // ========================================================================

    [[nodiscard]] auto operator==(const transport_parameters& other) const -> bool = default;
};

/*!
 * \brief Create default client transport parameters
 * \return Transport parameters with reasonable defaults for a client
 */
[[nodiscard]] auto make_default_client_params() -> transport_parameters;

/*!
 * \brief Create default server transport parameters
 * \return Transport parameters with reasonable defaults for a server
 */
[[nodiscard]] auto make_default_server_params() -> transport_parameters;

} // namespace network_system::protocols::quic
