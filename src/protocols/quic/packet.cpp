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

#include "internal/protocols/quic/packet.h"
#include "internal/protocols/quic/varint.h"

#include <algorithm>

namespace kcenon::network::protocols::quic
{

namespace
{
    constexpr const char* source = "quic::packet";

    template<typename T>
    auto make_error(int code, const std::string& message,
                    const std::string& details = "") -> Result<T>
    {
        return error<T>(code, message, source, details);
    }

    // Header form bits
    constexpr uint8_t header_form_long = 0x80;
    constexpr uint8_t fixed_bit = 0x40;

    // Long header type mask
    constexpr uint8_t long_packet_type_mask = 0x30;
    constexpr uint8_t long_packet_type_shift = 4;

    // Short header bits
    constexpr uint8_t spin_bit_mask = 0x20;
    constexpr uint8_t key_phase_mask = 0x04;
    constexpr uint8_t pn_length_mask = 0x03;

} // namespace

// ============================================================================
// packet_type_to_string
// ============================================================================

auto packet_type_to_string(packet_type type) -> std::string
{
    switch (type)
    {
        case packet_type::initial: return "Initial";
        case packet_type::zero_rtt: return "0-RTT";
        case packet_type::handshake: return "Handshake";
        case packet_type::retry: return "Retry";
        case packet_type::one_rtt: return "1-RTT";
        default: return "Unknown";
    }
}

// ============================================================================
// long_header
// ============================================================================

auto long_header::type() const noexcept -> packet_type
{
    return static_cast<packet_type>((first_byte >> 4) & 0x03);
}

auto long_header::is_retry() const noexcept -> bool
{
    return type() == packet_type::retry;
}

// ============================================================================
// short_header
// ============================================================================

auto short_header::spin_bit() const noexcept -> bool
{
    return (first_byte & spin_bit_mask) != 0;
}

auto short_header::key_phase() const noexcept -> bool
{
    return (first_byte & key_phase_mask) != 0;
}

// ============================================================================
// packet_number
// ============================================================================

auto packet_number::encode(uint64_t full_pn, uint64_t largest_acked)
    -> std::pair<std::vector<uint8_t>, size_t>
{
    size_t len = encoded_length(full_pn, largest_acked);
    std::vector<uint8_t> result(len);

    // Encode in big-endian order
    for (size_t i = 0; i < len; ++i)
    {
        result[len - 1 - i] = static_cast<uint8_t>(full_pn >> (i * 8));
    }

    return {result, len};
}

auto packet_number::decode(uint64_t truncated_pn, size_t pn_length,
                           uint64_t largest_pn) noexcept -> uint64_t
{
    // RFC 9000 Appendix A
    uint64_t expected_pn = largest_pn + 1;
    uint64_t pn_win = 1ULL << (pn_length * 8);
    uint64_t pn_hwin = pn_win / 2;
    uint64_t pn_mask = pn_win - 1;

    // Reconstruct the full packet number
    uint64_t candidate_pn = (expected_pn & ~pn_mask) | truncated_pn;

    // Handle wrap-around cases
    if (candidate_pn <= expected_pn - pn_hwin && candidate_pn < (1ULL << 62) - pn_win)
    {
        return candidate_pn + pn_win;
    }
    if (candidate_pn > expected_pn + pn_hwin && candidate_pn >= pn_win)
    {
        return candidate_pn - pn_win;
    }
    return candidate_pn;
}

auto packet_number::encoded_length(uint64_t full_pn, uint64_t largest_acked) noexcept -> size_t
{
    uint64_t num_unacked = (full_pn > largest_acked) ? (full_pn - largest_acked) : 1;

    if (num_unacked < (1ULL << 7))
    {
        return 1;
    }
    if (num_unacked < (1ULL << 15))
    {
        return 2;
    }
    if (num_unacked < (1ULL << 23))
    {
        return 3;
    }
    return 4;
}

// ============================================================================
// packet_parser
// ============================================================================

auto packet_parser::is_version_negotiation(std::span<const uint8_t> data) noexcept -> bool
{
    if (data.size() < 5)
    {
        return false;
    }
    // Long header with version 0
    if ((data[0] & header_form_long) == 0)
    {
        return false;
    }
    uint32_t version = (static_cast<uint32_t>(data[1]) << 24) |
                       (static_cast<uint32_t>(data[2]) << 16) |
                       (static_cast<uint32_t>(data[3]) << 8) |
                       static_cast<uint32_t>(data[4]);
    return version == 0;
}

auto packet_parser::parse_header(std::span<const uint8_t> data)
    -> Result<std::pair<packet_header, size_t>>
{
    if (data.empty())
    {
        return make_error<std::pair<packet_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Empty packet data");
    }

    if (is_long_header(data[0]))
    {
        auto result = parse_long_header(data);
        if (result.is_err())
        {
            return make_error<std::pair<packet_header, size_t>>(
                result.error().code, result.error().message);
        }
        auto& [header, len] = result.value();
        return ok(std::make_pair(packet_header{std::move(header)}, len));
    }
    else
    {
        // For short headers, we need to know the connection ID length
        // Return an error directing caller to use parse_short_header
        return make_error<std::pair<packet_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Short header requires known connection ID length. Use parse_short_header().");
    }
}

auto packet_parser::parse_long_header(std::span<const uint8_t> data)
    -> Result<std::pair<long_header, size_t>>
{
    // Minimum long header: 1 + 4 + 1 + 1 = 7 bytes (with empty CIDs)
    if (data.size() < 7)
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Insufficient data for long header");
    }

    long_header header;
    size_t offset = 0;

    // First byte
    header.first_byte = data[offset++];

    // Validate header form
    if (!is_long_header(header.first_byte))
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Not a long header packet");
    }

    // Validate fixed bit
    if (!has_valid_fixed_bit(header.first_byte))
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Invalid fixed bit in long header");
    }

    // Version (4 bytes, big-endian)
    if (data.size() < offset + 4)
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Insufficient data for version");
    }
    header.version = (static_cast<uint32_t>(data[offset]) << 24) |
                     (static_cast<uint32_t>(data[offset + 1]) << 16) |
                     (static_cast<uint32_t>(data[offset + 2]) << 8) |
                     static_cast<uint32_t>(data[offset + 3]);
    offset += 4;

    // Destination Connection ID Length (1 byte)
    if (data.size() < offset + 1)
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Insufficient data for DCID length");
    }
    uint8_t dcid_len = data[offset++];
    if (dcid_len > connection_id::max_length)
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "DCID length exceeds maximum");
    }

    // Destination Connection ID
    if (data.size() < offset + dcid_len)
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Insufficient data for DCID");
    }
    header.dest_conn_id = connection_id(data.subspan(offset, dcid_len));
    offset += dcid_len;

    // Source Connection ID Length (1 byte)
    if (data.size() < offset + 1)
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Insufficient data for SCID length");
    }
    uint8_t scid_len = data[offset++];
    if (scid_len > connection_id::max_length)
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "SCID length exceeds maximum");
    }

    // Source Connection ID
    if (data.size() < offset + scid_len)
    {
        return make_error<std::pair<long_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Insufficient data for SCID");
    }
    header.src_conn_id = connection_id(data.subspan(offset, scid_len));
    offset += scid_len;

    // Type-specific fields
    auto ptype = header.type();

    if (ptype == packet_type::initial)
    {
        // Token Length (varint)
        auto token_len_result = varint::decode(data.subspan(offset));
        if (token_len_result.is_err())
        {
            return make_error<std::pair<long_header, size_t>>(
                error_codes::common_errors::invalid_argument,
                "Failed to decode token length");
        }
        auto [token_len, token_len_bytes] = token_len_result.value();
        offset += token_len_bytes;

        // Token
        if (data.size() < offset + token_len)
        {
            return make_error<std::pair<long_header, size_t>>(
                error_codes::common_errors::invalid_argument,
                "Insufficient data for token");
        }
        header.token.assign(data.begin() + static_cast<ptrdiff_t>(offset),
                           data.begin() + static_cast<ptrdiff_t>(offset + token_len));
        offset += token_len;

        // Packet Length (varint)
        auto pkt_len_result = varint::decode(data.subspan(offset));
        if (pkt_len_result.is_err())
        {
            return make_error<std::pair<long_header, size_t>>(
                error_codes::common_errors::invalid_argument,
                "Failed to decode packet length");
        }
        offset += pkt_len_result.value().second;

        // Packet number length from reserved bits
        header.packet_number_length = (header.first_byte & pn_length_mask) + 1;
    }
    else if (ptype == packet_type::handshake || ptype == packet_type::zero_rtt)
    {
        // Packet Length (varint)
        auto pkt_len_result = varint::decode(data.subspan(offset));
        if (pkt_len_result.is_err())
        {
            return make_error<std::pair<long_header, size_t>>(
                error_codes::common_errors::invalid_argument,
                "Failed to decode packet length");
        }
        offset += pkt_len_result.value().second;

        // Packet number length from reserved bits
        header.packet_number_length = (header.first_byte & pn_length_mask) + 1;
    }
    else if (ptype == packet_type::retry)
    {
        // Retry packets have token (remaining bytes except last 16) and integrity tag
        // We don't parse the actual payload here, just note that it's a retry
        // The token is everything between the header and the integrity tag
    }

    return ok(std::make_pair(std::move(header), offset));
}

auto packet_parser::parse_short_header(std::span<const uint8_t> data,
                                        size_t conn_id_length)
    -> Result<std::pair<short_header, size_t>>
{
    // Minimum: 1 byte first + conn_id + 1 byte packet number
    if (data.size() < 1 + conn_id_length + 1)
    {
        return make_error<std::pair<short_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Insufficient data for short header");
    }

    short_header header;
    size_t offset = 0;

    // First byte
    header.first_byte = data[offset++];

    // Validate header form (should be 0 for short header)
    if (is_long_header(header.first_byte))
    {
        return make_error<std::pair<short_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Not a short header packet");
    }

    // Validate fixed bit
    if (!has_valid_fixed_bit(header.first_byte))
    {
        return make_error<std::pair<short_header, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Invalid fixed bit in short header");
    }

    // Destination Connection ID
    if (conn_id_length > 0)
    {
        if (data.size() < offset + conn_id_length)
        {
            return make_error<std::pair<short_header, size_t>>(
                error_codes::common_errors::invalid_argument,
                "Insufficient data for DCID");
        }
        header.dest_conn_id = connection_id(data.subspan(offset, conn_id_length));
        offset += conn_id_length;
    }

    // Packet number length from reserved bits
    header.packet_number_length = (header.first_byte & pn_length_mask) + 1;

    return ok(std::make_pair(std::move(header), offset));
}

// ============================================================================
// packet_builder
// ============================================================================

void packet_builder::append_bytes(std::vector<uint8_t>& buffer,
                                   std::span<const uint8_t> data)
{
    buffer.insert(buffer.end(), data.begin(), data.end());
}

auto packet_builder::build_initial(
    const connection_id& dest_cid,
    const connection_id& src_cid,
    const std::vector<uint8_t>& token,
    uint64_t packet_num,
    uint32_t version) -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;

    // Calculate packet number length
    size_t pn_len = packet_number::encoded_length(packet_num, 0);

    // First byte: Long header (1) + Fixed bit (1) + Type (00) + Reserved (00) + PN Length
    uint8_t first_byte = header_form_long | fixed_bit |
                         (static_cast<uint8_t>(packet_type::initial) << long_packet_type_shift) |
                         static_cast<uint8_t>(pn_len - 1);
    buffer.push_back(first_byte);

    // Version (4 bytes, big-endian)
    buffer.push_back(static_cast<uint8_t>(version >> 24));
    buffer.push_back(static_cast<uint8_t>(version >> 16));
    buffer.push_back(static_cast<uint8_t>(version >> 8));
    buffer.push_back(static_cast<uint8_t>(version));

    // DCID Length + DCID
    buffer.push_back(static_cast<uint8_t>(dest_cid.length()));
    append_bytes(buffer, dest_cid.data());

    // SCID Length + SCID
    buffer.push_back(static_cast<uint8_t>(src_cid.length()));
    append_bytes(buffer, src_cid.data());

    // Token Length (varint) + Token
    auto token_len_encoded = varint::encode(token.size());
    append_bytes(buffer, token_len_encoded);
    append_bytes(buffer, token);

    // Packet number (placeholder - actual encoding done separately)
    auto [pn_bytes, _] = packet_number::encode(packet_num, 0);
    append_bytes(buffer, pn_bytes);

    return buffer;
}

auto packet_builder::build_handshake(
    const connection_id& dest_cid,
    const connection_id& src_cid,
    uint64_t packet_num,
    uint32_t version) -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;

    size_t pn_len = packet_number::encoded_length(packet_num, 0);

    // First byte: Long header (1) + Fixed bit (1) + Type (10) + Reserved (00) + PN Length
    uint8_t first_byte = header_form_long | fixed_bit |
                         (static_cast<uint8_t>(packet_type::handshake) << long_packet_type_shift) |
                         static_cast<uint8_t>(pn_len - 1);
    buffer.push_back(first_byte);

    // Version
    buffer.push_back(static_cast<uint8_t>(version >> 24));
    buffer.push_back(static_cast<uint8_t>(version >> 16));
    buffer.push_back(static_cast<uint8_t>(version >> 8));
    buffer.push_back(static_cast<uint8_t>(version));

    // DCID
    buffer.push_back(static_cast<uint8_t>(dest_cid.length()));
    append_bytes(buffer, dest_cid.data());

    // SCID
    buffer.push_back(static_cast<uint8_t>(src_cid.length()));
    append_bytes(buffer, src_cid.data());

    // Packet number
    auto [pn_bytes, _] = packet_number::encode(packet_num, 0);
    append_bytes(buffer, pn_bytes);

    return buffer;
}

auto packet_builder::build_zero_rtt(
    const connection_id& dest_cid,
    const connection_id& src_cid,
    uint64_t packet_num,
    uint32_t version) -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;

    size_t pn_len = packet_number::encoded_length(packet_num, 0);

    // First byte: Long header (1) + Fixed bit (1) + Type (01) + Reserved (00) + PN Length
    uint8_t first_byte = header_form_long | fixed_bit |
                         (static_cast<uint8_t>(packet_type::zero_rtt) << long_packet_type_shift) |
                         static_cast<uint8_t>(pn_len - 1);
    buffer.push_back(first_byte);

    // Version
    buffer.push_back(static_cast<uint8_t>(version >> 24));
    buffer.push_back(static_cast<uint8_t>(version >> 16));
    buffer.push_back(static_cast<uint8_t>(version >> 8));
    buffer.push_back(static_cast<uint8_t>(version));

    // DCID
    buffer.push_back(static_cast<uint8_t>(dest_cid.length()));
    append_bytes(buffer, dest_cid.data());

    // SCID
    buffer.push_back(static_cast<uint8_t>(src_cid.length()));
    append_bytes(buffer, src_cid.data());

    // Packet number
    auto [pn_bytes, _] = packet_number::encode(packet_num, 0);
    append_bytes(buffer, pn_bytes);

    return buffer;
}

auto packet_builder::build_retry(
    const connection_id& dest_cid,
    const connection_id& src_cid,
    const std::vector<uint8_t>& token,
    const std::array<uint8_t, 16>& integrity_tag,
    uint32_t version) -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;

    // First byte: Long header (1) + Fixed bit (1) + Type (11) + Unused (0000)
    uint8_t first_byte = header_form_long | fixed_bit |
                         (static_cast<uint8_t>(packet_type::retry) << long_packet_type_shift);
    buffer.push_back(first_byte);

    // Version
    buffer.push_back(static_cast<uint8_t>(version >> 24));
    buffer.push_back(static_cast<uint8_t>(version >> 16));
    buffer.push_back(static_cast<uint8_t>(version >> 8));
    buffer.push_back(static_cast<uint8_t>(version));

    // DCID
    buffer.push_back(static_cast<uint8_t>(dest_cid.length()));
    append_bytes(buffer, dest_cid.data());

    // SCID
    buffer.push_back(static_cast<uint8_t>(src_cid.length()));
    append_bytes(buffer, src_cid.data());

    // Retry Token
    append_bytes(buffer, token);

    // Retry Integrity Tag
    buffer.insert(buffer.end(), integrity_tag.begin(), integrity_tag.end());

    return buffer;
}

auto packet_builder::build_short(
    const connection_id& dest_cid,
    uint64_t packet_num,
    bool key_phase,
    bool spin_bit) -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;

    size_t pn_len = packet_number::encoded_length(packet_num, 0);

    // First byte: Short header (0) + Fixed bit (1) + Spin + Reserved (00) + Key Phase + PN Length
    uint8_t first_byte = fixed_bit;  // Header form = 0
    if (spin_bit)
    {
        first_byte |= spin_bit_mask;
    }
    if (key_phase)
    {
        first_byte |= key_phase_mask;
    }
    first_byte |= static_cast<uint8_t>(pn_len - 1);
    buffer.push_back(first_byte);

    // DCID (no length field for short header)
    append_bytes(buffer, dest_cid.data());

    // Packet number
    auto [pn_bytes, _] = packet_number::encode(packet_num, 0);
    append_bytes(buffer, pn_bytes);

    return buffer;
}

auto packet_builder::build(const long_header& header) -> std::vector<uint8_t>
{
    auto ptype = header.type();
    switch (ptype)
    {
        case packet_type::initial:
            return build_initial(header.dest_conn_id, header.src_conn_id,
                                 header.token, header.packet_number, header.version);
        case packet_type::handshake:
            return build_handshake(header.dest_conn_id, header.src_conn_id,
                                   header.packet_number, header.version);
        case packet_type::zero_rtt:
            return build_zero_rtt(header.dest_conn_id, header.src_conn_id,
                                  header.packet_number, header.version);
        case packet_type::retry:
            return build_retry(header.dest_conn_id, header.src_conn_id,
                               header.token, header.retry_integrity_tag, header.version);
        default:
            return {};
    }
}

auto packet_builder::build(const short_header& header) -> std::vector<uint8_t>
{
    return build_short(header.dest_conn_id, header.packet_number,
                       header.key_phase(), header.spin_bit());
}

} // namespace kcenon::network::protocols::quic
