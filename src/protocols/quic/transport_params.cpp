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

#include "internal/protocols/quic/transport_params.h"
#include "internal/protocols/quic/varint.h"

#include <algorithm>
#include <set>

namespace kcenon::network::protocols::quic
{

namespace
{

void append_varint(std::vector<uint8_t>& buffer, uint64_t value)
{
    auto encoded = varint::encode(value);
    buffer.insert(buffer.end(), encoded.begin(), encoded.end());
}

void append_bytes(std::vector<uint8_t>& buffer, std::span<const uint8_t> data)
{
    buffer.insert(buffer.end(), data.begin(), data.end());
}

void append_parameter(std::vector<uint8_t>& buffer, uint64_t param_id,
                      std::span<const uint8_t> value)
{
    append_varint(buffer, param_id);
    append_varint(buffer, value.size());
    append_bytes(buffer, value);
}

void append_varint_parameter(std::vector<uint8_t>& buffer, uint64_t param_id,
                              uint64_t value)
{
    auto encoded_value = varint::encode(value);
    append_parameter(buffer, param_id,
                     std::span<const uint8_t>(encoded_value.data(), encoded_value.size()));
}

void append_empty_parameter(std::vector<uint8_t>& buffer, uint64_t param_id)
{
    append_varint(buffer, param_id);
    append_varint(buffer, 0);
}

auto read_varint_from_span(std::span<const uint8_t>& data)
    -> Result<uint64_t>
{
    auto result = varint::decode(data);
    if (result.is_err())
    {
        return error<uint64_t>(transport_param_error::decode_error,
                               "Failed to decode varint",
                               "transport_params");
    }
    auto [value, consumed] = result.value();
    data = data.subspan(consumed);
    return ok(std::move(value));
}

} // namespace

auto transport_parameters::encode() const -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    buffer.reserve(256);

    // Original Destination Connection ID (server only)
    if (original_destination_connection_id)
    {
        append_parameter(buffer, transport_param_id::original_destination_connection_id,
                         original_destination_connection_id->data());
    }

    // Initial Source Connection ID
    if (initial_source_connection_id)
    {
        append_parameter(buffer, transport_param_id::initial_source_connection_id,
                         initial_source_connection_id->data());
    }

    // Retry Source Connection ID (server only)
    if (retry_source_connection_id)
    {
        append_parameter(buffer, transport_param_id::retry_source_connection_id,
                         retry_source_connection_id->data());
    }

    // Stateless Reset Token (server only)
    if (stateless_reset_token)
    {
        append_parameter(buffer, transport_param_id::stateless_reset_token,
                         std::span<const uint8_t>(stateless_reset_token->data(),
                                                   stateless_reset_token->size()));
    }

    // Timing parameters
    if (max_idle_timeout != 0)
    {
        append_varint_parameter(buffer, transport_param_id::max_idle_timeout,
                                 max_idle_timeout);
    }

    if (ack_delay_exponent != 3)
    {
        append_varint_parameter(buffer, transport_param_id::ack_delay_exponent,
                                 ack_delay_exponent);
    }

    if (max_ack_delay != 25)
    {
        append_varint_parameter(buffer, transport_param_id::max_ack_delay,
                                 max_ack_delay);
    }

    // Flow control parameters
    if (max_udp_payload_size != 65527)
    {
        append_varint_parameter(buffer, transport_param_id::max_udp_payload_size,
                                 max_udp_payload_size);
    }

    if (initial_max_data != 0)
    {
        append_varint_parameter(buffer, transport_param_id::initial_max_data,
                                 initial_max_data);
    }

    if (initial_max_stream_data_bidi_local != 0)
    {
        append_varint_parameter(buffer,
                                 transport_param_id::initial_max_stream_data_bidi_local,
                                 initial_max_stream_data_bidi_local);
    }

    if (initial_max_stream_data_bidi_remote != 0)
    {
        append_varint_parameter(buffer,
                                 transport_param_id::initial_max_stream_data_bidi_remote,
                                 initial_max_stream_data_bidi_remote);
    }

    if (initial_max_stream_data_uni != 0)
    {
        append_varint_parameter(buffer, transport_param_id::initial_max_stream_data_uni,
                                 initial_max_stream_data_uni);
    }

    // Stream limits
    if (initial_max_streams_bidi != 0)
    {
        append_varint_parameter(buffer, transport_param_id::initial_max_streams_bidi,
                                 initial_max_streams_bidi);
    }

    if (initial_max_streams_uni != 0)
    {
        append_varint_parameter(buffer, transport_param_id::initial_max_streams_uni,
                                 initial_max_streams_uni);
    }

    // Connection options
    if (disable_active_migration)
    {
        append_empty_parameter(buffer, transport_param_id::disable_active_migration);
    }

    if (active_connection_id_limit != 2)
    {
        append_varint_parameter(buffer, transport_param_id::active_connection_id_limit,
                                 active_connection_id_limit);
    }

    // Preferred address (server only, complex encoding)
    if (preferred_address)
    {
        std::vector<uint8_t> addr_data;
        addr_data.reserve(64);

        // IPv4 address (4 bytes) + port (2 bytes)
        append_bytes(addr_data,
                     std::span<const uint8_t>(preferred_address->ipv4_address.data(), 4));
        addr_data.push_back(static_cast<uint8_t>(preferred_address->ipv4_port >> 8));
        addr_data.push_back(static_cast<uint8_t>(preferred_address->ipv4_port & 0xFF));

        // IPv6 address (16 bytes) + port (2 bytes)
        append_bytes(addr_data,
                     std::span<const uint8_t>(preferred_address->ipv6_address.data(), 16));
        addr_data.push_back(static_cast<uint8_t>(preferred_address->ipv6_port >> 8));
        addr_data.push_back(static_cast<uint8_t>(preferred_address->ipv6_port & 0xFF));

        // Connection ID length + Connection ID
        addr_data.push_back(static_cast<uint8_t>(preferred_address->connection_id.length()));
        append_bytes(addr_data, preferred_address->connection_id.data());

        // Stateless reset token (16 bytes)
        append_bytes(addr_data,
                     std::span<const uint8_t>(preferred_address->stateless_reset_token.data(),
                                               16));

        append_parameter(buffer, transport_param_id::preferred_address,
                         std::span<const uint8_t>(addr_data.data(), addr_data.size()));
    }

    return buffer;
}

auto transport_parameters::decode(std::span<const uint8_t> data)
    -> Result<transport_parameters>
{
    transport_parameters params;
    std::set<uint64_t> seen_params;

    while (!data.empty())
    {
        // Read parameter ID
        auto id_result = read_varint_from_span(data);
        if (id_result.is_err())
        {
            return error<transport_parameters>(transport_param_error::decode_error,
                                                "Failed to decode parameter ID",
                                                "transport_params");
        }
        uint64_t param_id = id_result.value();

        // Check for duplicates
        if (seen_params.contains(param_id))
        {
            return error<transport_parameters>(transport_param_error::duplicate_parameter,
                                                "Duplicate transport parameter",
                                                "transport_params");
        }
        seen_params.insert(param_id);

        // Read parameter length
        auto len_result = read_varint_from_span(data);
        if (len_result.is_err())
        {
            return error<transport_parameters>(transport_param_error::decode_error,
                                                "Failed to decode parameter length",
                                                "transport_params");
        }
        uint64_t param_len = len_result.value();

        if (param_len > data.size())
        {
            return error<transport_parameters>(transport_param_error::decode_error,
                                                "Parameter length exceeds buffer",
                                                "transport_params");
        }

        std::span<const uint8_t> param_data = data.subspan(0, param_len);
        data = data.subspan(param_len);

        // Parse parameter value
        switch (param_id)
        {
        case transport_param_id::original_destination_connection_id:
            if (param_len > 20)
            {
                return error<transport_parameters>(transport_param_error::invalid_value,
                                                    "Connection ID too long",
                                                    "transport_params");
            }
            params.original_destination_connection_id =
                connection_id(param_data);
            break;

        case transport_param_id::initial_source_connection_id:
            if (param_len > 20)
            {
                return error<transport_parameters>(transport_param_error::invalid_value,
                                                    "Connection ID too long",
                                                    "transport_params");
            }
            params.initial_source_connection_id =
                connection_id(param_data);
            break;

        case transport_param_id::retry_source_connection_id:
            if (param_len > 20)
            {
                return error<transport_parameters>(transport_param_error::invalid_value,
                                                    "Connection ID too long",
                                                    "transport_params");
            }
            params.retry_source_connection_id =
                connection_id(param_data);
            break;

        case transport_param_id::stateless_reset_token:
            if (param_len != 16)
            {
                return error<transport_parameters>(transport_param_error::invalid_value,
                                                    "Stateless reset token must be 16 bytes",
                                                    "transport_params");
            }
            {
                std::array<uint8_t, 16> token{};
                std::copy_n(param_data.data(), 16, token.data());
                params.stateless_reset_token = token;
            }
            break;

        case transport_param_id::max_idle_timeout:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(transport_param_error::decode_error,
                                                    "Failed to decode max_idle_timeout",
                                                    "transport_params");
            }
            params.max_idle_timeout = result.value().first;
        }
        break;

        case transport_param_id::ack_delay_exponent:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(transport_param_error::decode_error,
                                                    "Failed to decode ack_delay_exponent",
                                                    "transport_params");
            }
            if (result.value().first > 20)
            {
                return error<transport_parameters>(transport_param_error::invalid_value,
                                                    "ack_delay_exponent exceeds maximum (20)",
                                                    "transport_params");
            }
            params.ack_delay_exponent = result.value().first;
        }
        break;

        case transport_param_id::max_ack_delay:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(transport_param_error::decode_error,
                                                    "Failed to decode max_ack_delay",
                                                    "transport_params");
            }
            if (result.value().first > 16383)
            {
                return error<transport_parameters>(transport_param_error::invalid_value,
                                                    "max_ack_delay exceeds maximum (16383)",
                                                    "transport_params");
            }
            params.max_ack_delay = result.value().first;
        }
        break;

        case transport_param_id::max_udp_payload_size:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(transport_param_error::decode_error,
                                                    "Failed to decode max_udp_payload_size",
                                                    "transport_params");
            }
            if (result.value().first < 1200)
            {
                return error<transport_parameters>(transport_param_error::invalid_value,
                                                    "max_udp_payload_size below minimum (1200)",
                                                    "transport_params");
            }
            params.max_udp_payload_size = result.value().first;
        }
        break;

        case transport_param_id::initial_max_data:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(transport_param_error::decode_error,
                                                    "Failed to decode initial_max_data",
                                                    "transport_params");
            }
            params.initial_max_data = result.value().first;
        }
        break;

        case transport_param_id::initial_max_stream_data_bidi_local:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(
                    transport_param_error::decode_error,
                    "Failed to decode initial_max_stream_data_bidi_local",
                    "transport_params");
            }
            params.initial_max_stream_data_bidi_local = result.value().first;
        }
        break;

        case transport_param_id::initial_max_stream_data_bidi_remote:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(
                    transport_param_error::decode_error,
                    "Failed to decode initial_max_stream_data_bidi_remote",
                    "transport_params");
            }
            params.initial_max_stream_data_bidi_remote = result.value().first;
        }
        break;

        case transport_param_id::initial_max_stream_data_uni:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(
                    transport_param_error::decode_error,
                    "Failed to decode initial_max_stream_data_uni",
                    "transport_params");
            }
            params.initial_max_stream_data_uni = result.value().first;
        }
        break;

        case transport_param_id::initial_max_streams_bidi:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(
                    transport_param_error::decode_error,
                    "Failed to decode initial_max_streams_bidi",
                    "transport_params");
            }
            params.initial_max_streams_bidi = result.value().first;
        }
        break;

        case transport_param_id::initial_max_streams_uni:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(
                    transport_param_error::decode_error,
                    "Failed to decode initial_max_streams_uni",
                    "transport_params");
            }
            params.initial_max_streams_uni = result.value().first;
        }
        break;

        case transport_param_id::disable_active_migration:
            if (param_len != 0)
            {
                return error<transport_parameters>(
                    transport_param_error::invalid_value,
                    "disable_active_migration must have zero-length value",
                    "transport_params");
            }
            params.disable_active_migration = true;
            break;

        case transport_param_id::active_connection_id_limit:
        {
            auto result = varint::decode(param_data);
            if (result.is_err())
            {
                return error<transport_parameters>(
                    transport_param_error::decode_error,
                    "Failed to decode active_connection_id_limit",
                    "transport_params");
            }
            if (result.value().first < 2)
            {
                return error<transport_parameters>(
                    transport_param_error::invalid_value,
                    "active_connection_id_limit must be at least 2",
                    "transport_params");
            }
            params.active_connection_id_limit = result.value().first;
        }
        break;

        case transport_param_id::preferred_address:
        {
            // Minimum: 4 + 2 + 16 + 2 + 1 + 0 + 16 = 41 bytes
            if (param_len < 41)
            {
                return error<transport_parameters>(transport_param_error::invalid_value,
                                                    "Preferred address too short",
                                                    "transport_params");
            }
            preferred_address_info addr;
            size_t offset = 0;

            // IPv4 address
            std::copy_n(param_data.data() + offset, 4, addr.ipv4_address.data());
            offset += 4;

            // IPv4 port
            addr.ipv4_port = static_cast<uint16_t>(
                (param_data[offset] << 8) | param_data[offset + 1]);
            offset += 2;

            // IPv6 address
            std::copy_n(param_data.data() + offset, 16, addr.ipv6_address.data());
            offset += 16;

            // IPv6 port
            addr.ipv6_port = static_cast<uint16_t>(
                (param_data[offset] << 8) | param_data[offset + 1]);
            offset += 2;

            // Connection ID
            uint8_t cid_len = param_data[offset++];
            if (cid_len > 20 || offset + cid_len + 16 > param_len)
            {
                return error<transport_parameters>(transport_param_error::invalid_value,
                                                    "Invalid preferred address connection ID",
                                                    "transport_params");
            }
            addr.connection_id = connection_id(
                std::span<const uint8_t>(param_data.data() + offset, cid_len));
            offset += cid_len;

            // Stateless reset token
            std::copy_n(param_data.data() + offset, 16, addr.stateless_reset_token.data());

            params.preferred_address = addr;
        }
        break;

        default:
            // Unknown parameters are ignored (RFC 9000 Section 18.1)
            break;
        }
    }

    return ok(std::move(params));
}

auto transport_parameters::validate(bool is_server) const -> VoidResult
{
    // Validate ack_delay_exponent
    if (ack_delay_exponent > 20)
    {
        return error_void(transport_param_error::invalid_value,
                          "ack_delay_exponent must not exceed 20",
                          "transport_params");
    }

    // Validate max_ack_delay
    if (max_ack_delay > 16383)
    {
        return error_void(transport_param_error::invalid_value,
                          "max_ack_delay must not exceed 16383",
                          "transport_params");
    }

    // Validate max_udp_payload_size
    if (max_udp_payload_size < 1200)
    {
        return error_void(transport_param_error::invalid_value,
                          "max_udp_payload_size must be at least 1200",
                          "transport_params");
    }

    // Validate active_connection_id_limit
    if (active_connection_id_limit < 2)
    {
        return error_void(transport_param_error::invalid_value,
                          "active_connection_id_limit must be at least 2",
                          "transport_params");
    }

    // Server-only parameters should not be set by client
    if (!is_server)
    {
        if (original_destination_connection_id)
        {
            return error_void(transport_param_error::invalid_parameter,
                              "Client must not send original_destination_connection_id",
                              "transport_params");
        }
        if (retry_source_connection_id)
        {
            return error_void(transport_param_error::invalid_parameter,
                              "Client must not send retry_source_connection_id",
                              "transport_params");
        }
        if (stateless_reset_token)
        {
            return error_void(transport_param_error::invalid_parameter,
                              "Client must not send stateless_reset_token",
                              "transport_params");
        }
        if (preferred_address)
        {
            return error_void(transport_param_error::invalid_parameter,
                              "Client must not send preferred_address",
                              "transport_params");
        }
    }

    return ok();
}

void transport_parameters::apply_defaults()
{
    // Most defaults are already set in the struct definition
    // This method can be used to apply context-specific defaults
    if (max_udp_payload_size == 0)
    {
        max_udp_payload_size = 65527;
    }
    if (ack_delay_exponent == 0)
    {
        ack_delay_exponent = 3;
    }
    if (max_ack_delay == 0)
    {
        max_ack_delay = 25;
    }
    if (active_connection_id_limit == 0)
    {
        active_connection_id_limit = 2;
    }
}

auto make_default_client_params() -> transport_parameters
{
    transport_parameters params;

    // Set reasonable defaults for a client
    params.max_idle_timeout = 30000;  // 30 seconds
    params.max_udp_payload_size = 65527;
    params.initial_max_data = 1048576;  // 1 MB
    params.initial_max_stream_data_bidi_local = 262144;  // 256 KB
    params.initial_max_stream_data_bidi_remote = 262144;
    params.initial_max_stream_data_uni = 262144;
    params.initial_max_streams_bidi = 100;
    params.initial_max_streams_uni = 100;
    params.ack_delay_exponent = 3;
    params.max_ack_delay = 25;
    params.active_connection_id_limit = 8;

    return params;
}

auto make_default_server_params() -> transport_parameters
{
    transport_parameters params;

    // Set reasonable defaults for a server
    params.max_idle_timeout = 30000;  // 30 seconds
    params.max_udp_payload_size = 65527;
    params.initial_max_data = 1048576;  // 1 MB
    params.initial_max_stream_data_bidi_local = 262144;  // 256 KB
    params.initial_max_stream_data_bidi_remote = 262144;
    params.initial_max_stream_data_uni = 262144;
    params.initial_max_streams_bidi = 100;
    params.initial_max_streams_uni = 100;
    params.ack_delay_exponent = 3;
    params.max_ack_delay = 25;
    params.active_connection_id_limit = 8;

    return params;
}

} // namespace kcenon::network::protocols::quic
