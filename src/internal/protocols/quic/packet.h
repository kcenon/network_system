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

#include <cstdint>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace kcenon::network::protocols::quic
{

// ============================================================================
// QUIC Versions
// ============================================================================

/*!
 * \brief Well-known QUIC version numbers
 */
namespace quic_version
{
    //! QUIC version 1 (RFC 9000)
    constexpr uint32_t version_1 = 0x00000001;

    //! QUIC version 2 (RFC 9369)
    constexpr uint32_t version_2 = 0x6b3343cf;

    //! Version Negotiation (special value)
    constexpr uint32_t negotiation = 0x00000000;
} // namespace quic_version

// ============================================================================
// Packet Types
// ============================================================================

/*!
 * \brief QUIC packet types (RFC 9000 Section 17)
 *
 * Long header packets (used during handshake):
 * - Initial (0x00): First handshake packet
 * - Zero-RTT (0x01): Early data (0-RTT)
 * - Handshake (0x02): Handshake continuation
 * - Retry (0x03): Address validation token
 *
 * Short header packets (used after handshake):
 * - One-RTT: Minimal header for application data
 */
enum class packet_type : uint8_t
{
    initial = 0x00,
    zero_rtt = 0x01,
    handshake = 0x02,
    retry = 0x03,
    one_rtt = 0xFF,  // Special marker for short header packets
};

/*!
 * \brief Convert packet type to string for debugging
 */
[[nodiscard]] auto packet_type_to_string(packet_type type) -> std::string;

// ============================================================================
// Header Structures
// ============================================================================

/*!
 * \struct long_header
 * \brief QUIC Long Header format (RFC 9000 Section 17.2)
 *
 * Long headers are used during connection establishment. They include
 * source and destination connection IDs.
 *
 * Format:
 * +-+-+-+-+-+-+-+-+
 * |1|1|T T|X X X X|  First byte: Header Form (1), Fixed Bit (1),
 * +-+-+-+-+-+-+-+-+              Long Packet Type (2), Type-Specific Bits (4)
 * |   Version (32)              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |DCID Len (8)|
 * +-+-+-+-+-+-+-+-+
 * |Destination Connection ID (0..160)|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |SCID Len (8)|
 * +-+-+-+-+-+-+-+-+
 * |Source Connection ID (0..160)|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct long_header
{
    uint8_t first_byte{0};           //!< Header form, fixed bit, type, reserved, PN length
    uint32_t version{0};             //!< QUIC version
    connection_id dest_conn_id;      //!< Destination Connection ID
    connection_id src_conn_id;       //!< Source Connection ID

    // Type-specific fields
    std::vector<uint8_t> token;      //!< Token (Initial and Retry only)
    uint64_t packet_number{0};       //!< Packet number (after header protection removal)
    size_t packet_number_length{0};  //!< Packet number length (1-4 bytes)

    //! Retry integrity tag (Retry packets only, 16 bytes)
    std::array<uint8_t, 16> retry_integrity_tag{};

    /*!
     * \brief Get the packet type from first byte
     * \return Packet type (Initial, 0-RTT, Handshake, or Retry)
     */
    [[nodiscard]] auto type() const noexcept -> packet_type;

    /*!
     * \brief Check if this is a Retry packet (has integrity tag, no packet number)
     */
    [[nodiscard]] auto is_retry() const noexcept -> bool;
};

/*!
 * \struct short_header
 * \brief QUIC Short Header format (RFC 9000 Section 17.3)
 *
 * Short headers are used after the handshake completes. They are more
 * compact and include only the destination connection ID.
 *
 * Format:
 * +-+-+-+-+-+-+-+-+
 * |0|1|S|R|R|K|P P|  First byte: Header Form (0), Fixed Bit (1),
 * +-+-+-+-+-+-+-+-+              Spin Bit, Reserved, Key Phase, PN Length
 * |Destination Connection ID (0..160)|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Packet Number (8..32)            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct short_header
{
    uint8_t first_byte{0};           //!< Header form, fixed bit, spin, reserved, key phase, PN length
    connection_id dest_conn_id;      //!< Destination Connection ID
    uint64_t packet_number{0};       //!< Packet number (after header protection removal)
    size_t packet_number_length{0};  //!< Packet number length (1-4 bytes)

    /*!
     * \brief Get the spin bit value
     * \return Spin bit (for latency measurement)
     */
    [[nodiscard]] auto spin_bit() const noexcept -> bool;

    /*!
     * \brief Get the key phase bit
     * \return Key phase bit (for key updates)
     */
    [[nodiscard]] auto key_phase() const noexcept -> bool;
};

/*!
 * \brief Variant type for packet headers
 */
using packet_header = std::variant<long_header, short_header>;

// ============================================================================
// Packet Number Encoding
// ============================================================================

/*!
 * \class packet_number
 * \brief QUIC packet number utilities (RFC 9000 Section 17.1)
 *
 * Packet numbers are encoded using a variable-length encoding based on
 * the difference from the largest acknowledged packet number.
 */
class packet_number
{
public:
    /*!
     * \brief Encode a packet number for transmission
     * \param full_pn Full packet number to encode
     * \param largest_acked Largest acknowledged packet number (0 if none)
     * \return Pair of (encoded bytes, number of bytes used)
     */
    [[nodiscard]] static auto encode(uint64_t full_pn, uint64_t largest_acked)
        -> std::pair<std::vector<uint8_t>, size_t>;

    /*!
     * \brief Decode a packet number from received data
     * \param truncated_pn Truncated packet number from packet
     * \param pn_length Length of the truncated packet number (1-4)
     * \param largest_pn Largest packet number received so far
     * \return Full recovered packet number
     */
    [[nodiscard]] static auto decode(uint64_t truncated_pn, size_t pn_length,
                                      uint64_t largest_pn) noexcept -> uint64_t;

    /*!
     * \brief Get the minimum number of bytes needed to encode a packet number
     * \param full_pn Full packet number
     * \param largest_acked Largest acknowledged packet number
     * \return Number of bytes (1-4)
     */
    [[nodiscard]] static auto encoded_length(uint64_t full_pn,
                                              uint64_t largest_acked) noexcept -> size_t;
};

// ============================================================================
// Packet Parser
// ============================================================================

/*!
 * \class packet_parser
 * \brief Parser for QUIC packet headers (RFC 9000 Section 17)
 *
 * Parses raw bytes into QUIC packet header structures. Note that this
 * parser handles the unprotected header format; header protection must
 * be removed before parsing packet numbers.
 */
class packet_parser
{
public:
    /*!
     * \brief Check if a packet has a long header
     * \param first_byte First byte of the packet
     * \return true if the packet has a long header
     */
    [[nodiscard]] static constexpr auto is_long_header(uint8_t first_byte) noexcept -> bool
    {
        return (first_byte & 0x80) != 0;
    }

    /*!
     * \brief Check if the fixed bit is set correctly
     * \param first_byte First byte of the packet
     * \return true if the fixed bit is 1 (as required by RFC 9000)
     */
    [[nodiscard]] static constexpr auto has_valid_fixed_bit(uint8_t first_byte) noexcept -> bool
    {
        return (first_byte & 0x40) != 0;
    }

    /*!
     * \brief Parse a packet header (without header protection removal)
     * \param data Input buffer containing the packet
     * \return Result containing (header, header_length) or error
     * \note For short headers, the caller must provide the expected connection ID length
     *       via parse_short_header instead
     */
    [[nodiscard]] static auto parse_header(std::span<const uint8_t> data)
        -> Result<std::pair<packet_header, size_t>>;

    /*!
     * \brief Parse a long header packet
     * \param data Input buffer
     * \return Result containing (header, header_length) or error
     */
    [[nodiscard]] static auto parse_long_header(std::span<const uint8_t> data)
        -> Result<std::pair<long_header, size_t>>;

    /*!
     * \brief Parse a short header packet
     * \param data Input buffer
     * \param conn_id_length Expected destination connection ID length
     * \return Result containing (header, header_length) or error
     */
    [[nodiscard]] static auto parse_short_header(std::span<const uint8_t> data,
                                                  size_t conn_id_length)
        -> Result<std::pair<short_header, size_t>>;

    /*!
     * \brief Get the packet type from long header's first byte
     * \param first_byte First byte of a long header packet
     * \return Packet type
     */
    [[nodiscard]] static constexpr auto get_long_packet_type(uint8_t first_byte) noexcept
        -> packet_type
    {
        return static_cast<packet_type>((first_byte >> 4) & 0x03);
    }

    /*!
     * \brief Check if this is a version negotiation packet
     * \param data Input buffer (at least 5 bytes)
     * \return true if this is a version negotiation packet
     */
    [[nodiscard]] static auto is_version_negotiation(std::span<const uint8_t> data) noexcept
        -> bool;
};

// ============================================================================
// Packet Builder
// ============================================================================

/*!
 * \class packet_builder
 * \brief Builder for QUIC packet headers (RFC 9000 Section 17)
 *
 * Builds raw bytes from QUIC packet header structures. Note that header
 * protection must be applied after building.
 */
class packet_builder
{
public:
    /*!
     * \brief Build an Initial packet header
     * \param dest_cid Destination Connection ID
     * \param src_cid Source Connection ID
     * \param token Token for address validation (empty for client initial)
     * \param packet_number Packet number
     * \param version QUIC version (default: version 1)
     * \return Serialized header bytes
     */
    [[nodiscard]] static auto build_initial(
        const connection_id& dest_cid,
        const connection_id& src_cid,
        const std::vector<uint8_t>& token,
        uint64_t packet_number,
        uint32_t version = quic_version::version_1) -> std::vector<uint8_t>;

    /*!
     * \brief Build a Handshake packet header
     * \param dest_cid Destination Connection ID
     * \param src_cid Source Connection ID
     * \param packet_number Packet number
     * \param version QUIC version (default: version 1)
     * \return Serialized header bytes
     */
    [[nodiscard]] static auto build_handshake(
        const connection_id& dest_cid,
        const connection_id& src_cid,
        uint64_t packet_number,
        uint32_t version = quic_version::version_1) -> std::vector<uint8_t>;

    /*!
     * \brief Build a 0-RTT packet header
     * \param dest_cid Destination Connection ID
     * \param src_cid Source Connection ID
     * \param packet_number Packet number
     * \param version QUIC version (default: version 1)
     * \return Serialized header bytes
     */
    [[nodiscard]] static auto build_zero_rtt(
        const connection_id& dest_cid,
        const connection_id& src_cid,
        uint64_t packet_number,
        uint32_t version = quic_version::version_1) -> std::vector<uint8_t>;

    /*!
     * \brief Build a Retry packet header
     * \param dest_cid Destination Connection ID
     * \param src_cid Source Connection ID
     * \param token Retry token
     * \param integrity_tag Retry Integrity Tag (16 bytes)
     * \param version QUIC version (default: version 1)
     * \return Serialized header bytes
     */
    [[nodiscard]] static auto build_retry(
        const connection_id& dest_cid,
        const connection_id& src_cid,
        const std::vector<uint8_t>& token,
        const std::array<uint8_t, 16>& integrity_tag,
        uint32_t version = quic_version::version_1) -> std::vector<uint8_t>;

    /*!
     * \brief Build a Short Header (1-RTT) packet
     * \param dest_cid Destination Connection ID
     * \param packet_number Packet number
     * \param key_phase Key phase bit
     * \param spin_bit Spin bit for latency measurement
     * \return Serialized header bytes
     */
    [[nodiscard]] static auto build_short(
        const connection_id& dest_cid,
        uint64_t packet_number,
        bool key_phase = false,
        bool spin_bit = false) -> std::vector<uint8_t>;

    /*!
     * \brief Build a long header from a header structure
     * \param header Long header structure
     * \return Serialized header bytes
     */
    [[nodiscard]] static auto build(const long_header& header) -> std::vector<uint8_t>;

    /*!
     * \brief Build a short header from a header structure
     * \param header Short header structure
     * \return Serialized header bytes
     */
    [[nodiscard]] static auto build(const short_header& header) -> std::vector<uint8_t>;

private:
    //! Helper to append bytes to buffer
    static void append_bytes(std::vector<uint8_t>& buffer,
                             std::span<const uint8_t> data);
};

} // namespace kcenon::network::protocols::quic
