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

#include "kcenon/network/protocols/quic/frame.h"
#include "kcenon/network/protocols/quic/varint.h"

namespace kcenon::network::protocols::quic
{

namespace
{
    constexpr const char* source = "quic::frame";

    // Error helper
    template<typename T>
    auto make_error(int code, const std::string& message,
                    const std::string& details = "") -> Result<T>
    {
        return error<T>(code, message, source, details);
    }
} // namespace

// ============================================================================
// frame_types.h implementations
// ============================================================================

auto get_frame_type(const frame& f) -> frame_type
{
    return std::visit([](const auto& fr) -> frame_type {
        using T = std::decay_t<decltype(fr)>;
        if constexpr (std::is_same_v<T, padding_frame>)
            return frame_type::padding;
        else if constexpr (std::is_same_v<T, ping_frame>)
            return frame_type::ping;
        else if constexpr (std::is_same_v<T, ack_frame>)
            return fr.ecn ? frame_type::ack_ecn : frame_type::ack;
        else if constexpr (std::is_same_v<T, reset_stream_frame>)
            return frame_type::reset_stream;
        else if constexpr (std::is_same_v<T, stop_sending_frame>)
            return frame_type::stop_sending;
        else if constexpr (std::is_same_v<T, crypto_frame>)
            return frame_type::crypto;
        else if constexpr (std::is_same_v<T, new_token_frame>)
            return frame_type::new_token;
        else if constexpr (std::is_same_v<T, stream_frame>)
            return frame_type::stream_base;
        else if constexpr (std::is_same_v<T, max_data_frame>)
            return frame_type::max_data;
        else if constexpr (std::is_same_v<T, max_stream_data_frame>)
            return frame_type::max_stream_data;
        else if constexpr (std::is_same_v<T, max_streams_frame>)
            return fr.bidirectional ? frame_type::max_streams_bidi
                                    : frame_type::max_streams_uni;
        else if constexpr (std::is_same_v<T, data_blocked_frame>)
            return frame_type::data_blocked;
        else if constexpr (std::is_same_v<T, stream_data_blocked_frame>)
            return frame_type::stream_data_blocked;
        else if constexpr (std::is_same_v<T, streams_blocked_frame>)
            return fr.bidirectional ? frame_type::streams_blocked_bidi
                                    : frame_type::streams_blocked_uni;
        else if constexpr (std::is_same_v<T, new_connection_id_frame>)
            return frame_type::new_connection_id;
        else if constexpr (std::is_same_v<T, retire_connection_id_frame>)
            return frame_type::retire_connection_id;
        else if constexpr (std::is_same_v<T, path_challenge_frame>)
            return frame_type::path_challenge;
        else if constexpr (std::is_same_v<T, path_response_frame>)
            return frame_type::path_response;
        else if constexpr (std::is_same_v<T, connection_close_frame>)
            return fr.is_application_error ? frame_type::connection_close_app
                                           : frame_type::connection_close;
        else if constexpr (std::is_same_v<T, handshake_done_frame>)
            return frame_type::handshake_done;
        else
            return frame_type::padding;
    }, f);
}

auto frame_type_to_string(frame_type type) -> std::string
{
    switch (type)
    {
        case frame_type::padding: return "PADDING";
        case frame_type::ping: return "PING";
        case frame_type::ack: return "ACK";
        case frame_type::ack_ecn: return "ACK_ECN";
        case frame_type::reset_stream: return "RESET_STREAM";
        case frame_type::stop_sending: return "STOP_SENDING";
        case frame_type::crypto: return "CRYPTO";
        case frame_type::new_token: return "NEW_TOKEN";
        case frame_type::stream_base: return "STREAM";
        case frame_type::max_data: return "MAX_DATA";
        case frame_type::max_stream_data: return "MAX_STREAM_DATA";
        case frame_type::max_streams_bidi: return "MAX_STREAMS_BIDI";
        case frame_type::max_streams_uni: return "MAX_STREAMS_UNI";
        case frame_type::data_blocked: return "DATA_BLOCKED";
        case frame_type::stream_data_blocked: return "STREAM_DATA_BLOCKED";
        case frame_type::streams_blocked_bidi: return "STREAMS_BLOCKED_BIDI";
        case frame_type::streams_blocked_uni: return "STREAMS_BLOCKED_UNI";
        case frame_type::new_connection_id: return "NEW_CONNECTION_ID";
        case frame_type::retire_connection_id: return "RETIRE_CONNECTION_ID";
        case frame_type::path_challenge: return "PATH_CHALLENGE";
        case frame_type::path_response: return "PATH_RESPONSE";
        case frame_type::connection_close: return "CONNECTION_CLOSE";
        case frame_type::connection_close_app: return "CONNECTION_CLOSE_APP";
        case frame_type::handshake_done: return "HANDSHAKE_DONE";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// frame_parser implementations
// ============================================================================

auto frame_parser::peek_type(std::span<const uint8_t> data)
    -> Result<std::pair<uint64_t, size_t>>
{
    return varint::decode(data);
}

auto frame_parser::parse(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    if (data.empty())
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Empty frame data");
    }

    // Parse frame type
    auto type_result = varint::decode(data);
    if (type_result.is_err())
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument,
            "Failed to decode frame type");
    }

    auto [type_value, type_len] = type_result.value();
    auto remaining = data.subspan(type_len);

    // Check for STREAM frames (0x08-0x0f)
    if (is_stream_frame(type_value))
    {
        auto result = parse_stream(remaining, get_stream_flags(type_value));
        if (result.is_err()) return result;
        auto& [f, consumed] = result.value();
        return ok(std::make_pair(std::move(f), type_len + consumed));
    }

    // Dispatch based on frame type
    switch (static_cast<frame_type>(type_value))
    {
        case frame_type::padding:
            return parse_padding(data);

        case frame_type::ping:
        {
            auto result = parse_ping(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::ack:
        {
            auto result = parse_ack(remaining, false);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::ack_ecn:
        {
            auto result = parse_ack(remaining, true);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::reset_stream:
        {
            auto result = parse_reset_stream(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::stop_sending:
        {
            auto result = parse_stop_sending(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::crypto:
        {
            auto result = parse_crypto(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::new_token:
        {
            auto result = parse_new_token(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::max_data:
        {
            auto result = parse_max_data(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::max_stream_data:
        {
            auto result = parse_max_stream_data(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::max_streams_bidi:
        {
            auto result = parse_max_streams(remaining, true);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::max_streams_uni:
        {
            auto result = parse_max_streams(remaining, false);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::data_blocked:
        {
            auto result = parse_data_blocked(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::stream_data_blocked:
        {
            auto result = parse_stream_data_blocked(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::streams_blocked_bidi:
        {
            auto result = parse_streams_blocked(remaining, true);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::streams_blocked_uni:
        {
            auto result = parse_streams_blocked(remaining, false);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::new_connection_id:
        {
            auto result = parse_new_connection_id(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::retire_connection_id:
        {
            auto result = parse_retire_connection_id(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::path_challenge:
        {
            auto result = parse_path_challenge(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::path_response:
        {
            auto result = parse_path_response(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::connection_close:
        {
            auto result = parse_connection_close(remaining, false);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::connection_close_app:
        {
            auto result = parse_connection_close(remaining, true);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        case frame_type::handshake_done:
        {
            auto result = parse_handshake_done(remaining);
            if (result.is_err()) return result;
            auto& [f, consumed] = result.value();
            return ok(std::make_pair(std::move(f), type_len + consumed));
        }

        default:
            return make_error<std::pair<frame, size_t>>(
                error_codes::common_errors::invalid_argument,
                "Unknown frame type",
                "type=" + std::to_string(type_value));
    }
}

auto frame_parser::parse_all(std::span<const uint8_t> data)
    -> Result<std::vector<frame>>
{
    std::vector<frame> frames;
    size_t offset = 0;

    while (offset < data.size())
    {
        auto result = parse(data.subspan(offset));
        if (result.is_err())
        {
            return make_error<std::vector<frame>>(
                error_codes::common_errors::invalid_argument,
                "Failed to parse frame at offset " + std::to_string(offset));
        }

        auto& [f, consumed] = result.value();
        frames.push_back(std::move(f));
        offset += consumed;
    }

    return ok(std::move(frames));
}

auto frame_parser::parse_padding(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    // Count consecutive padding bytes
    size_t count = 0;
    while (count < data.size() && data[count] == 0x00)
    {
        ++count;
    }

    if (count == 0)
    {
        count = 1; // At least one padding byte was consumed (the type)
    }

    padding_frame f;
    f.count = count;
    return ok(std::make_pair(frame{f}, count));
}

auto frame_parser::parse_ping(std::span<const uint8_t> /*data*/)
    -> Result<std::pair<frame, size_t>>
{
    // PING frame has no payload after type
    return ok(std::make_pair(frame{ping_frame{}}, size_t{0}));
}

auto frame_parser::parse_ack(std::span<const uint8_t> data, bool has_ecn)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    ack_frame f;

    // Largest Acknowledged
    auto largest = varint::decode(data.subspan(offset));
    if (largest.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse largest acknowledged");
    f.largest_acknowledged = largest.value().first;
    offset += largest.value().second;

    // ACK Delay
    auto delay = varint::decode(data.subspan(offset));
    if (delay.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse ack delay");
    f.ack_delay = delay.value().first;
    offset += delay.value().second;

    // ACK Range Count
    auto range_count = varint::decode(data.subspan(offset));
    if (range_count.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse ack range count");
    offset += range_count.value().second;

    // First ACK Range
    auto first_range = varint::decode(data.subspan(offset));
    if (first_range.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse first ack range");
    offset += first_range.value().second;

    // The first range implicitly starts at largest_acknowledged
    // Additional ranges
    for (uint64_t i = 0; i < range_count.value().first; ++i)
    {
        ack_range range;

        auto gap = varint::decode(data.subspan(offset));
        if (gap.is_err()) return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Failed to parse ack gap");
        range.gap = gap.value().first;
        offset += gap.value().second;

        auto length = varint::decode(data.subspan(offset));
        if (length.is_err()) return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Failed to parse ack range length");
        range.length = length.value().first;
        offset += length.value().second;

        f.ranges.push_back(range);
    }

    // ECN Counts (if present)
    if (has_ecn)
    {
        ecn_counts ecn;

        auto ect0 = varint::decode(data.subspan(offset));
        if (ect0.is_err()) return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Failed to parse ECT(0) count");
        ecn.ect0 = ect0.value().first;
        offset += ect0.value().second;

        auto ect1 = varint::decode(data.subspan(offset));
        if (ect1.is_err()) return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Failed to parse ECT(1) count");
        ecn.ect1 = ect1.value().first;
        offset += ect1.value().second;

        auto ecn_ce = varint::decode(data.subspan(offset));
        if (ecn_ce.is_err()) return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Failed to parse ECN-CE count");
        ecn.ecn_ce = ecn_ce.value().first;
        offset += ecn_ce.value().second;

        f.ecn = ecn;
    }

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_reset_stream(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    reset_stream_frame f;

    auto stream_id = varint::decode(data.subspan(offset));
    if (stream_id.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse stream id");
    f.stream_id = stream_id.value().first;
    offset += stream_id.value().second;

    auto error_code = varint::decode(data.subspan(offset));
    if (error_code.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse error code");
    f.application_error_code = error_code.value().first;
    offset += error_code.value().second;

    auto final_size = varint::decode(data.subspan(offset));
    if (final_size.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse final size");
    f.final_size = final_size.value().first;
    offset += final_size.value().second;

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_stop_sending(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    stop_sending_frame f;

    auto stream_id = varint::decode(data.subspan(offset));
    if (stream_id.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse stream id");
    f.stream_id = stream_id.value().first;
    offset += stream_id.value().second;

    auto error_code = varint::decode(data.subspan(offset));
    if (error_code.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse error code");
    f.application_error_code = error_code.value().first;
    offset += error_code.value().second;

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_crypto(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    crypto_frame f;

    auto crypto_offset = varint::decode(data.subspan(offset));
    if (crypto_offset.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse crypto offset");
    f.offset = crypto_offset.value().first;
    offset += crypto_offset.value().second;

    auto length = varint::decode(data.subspan(offset));
    if (length.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse crypto length");
    offset += length.value().second;

    if (data.size() - offset < length.value().first)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Insufficient crypto data");
    }

    f.data.assign(data.begin() + offset, data.begin() + offset + length.value().first);
    offset += length.value().first;

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_new_token(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    new_token_frame f;

    auto length = varint::decode(data.subspan(offset));
    if (length.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse token length");
    offset += length.value().second;

    if (data.size() - offset < length.value().first)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Insufficient token data");
    }

    f.token.assign(data.begin() + offset, data.begin() + offset + length.value().first);
    offset += length.value().first;

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_stream(std::span<const uint8_t> data, uint8_t flags)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    stream_frame f;

    // Stream ID
    auto stream_id = varint::decode(data.subspan(offset));
    if (stream_id.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse stream id");
    f.stream_id = stream_id.value().first;
    offset += stream_id.value().second;

    // Offset (if OFF bit set)
    if (flags & stream_flags::off)
    {
        auto stream_offset = varint::decode(data.subspan(offset));
        if (stream_offset.is_err()) return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Failed to parse stream offset");
        f.offset = stream_offset.value().first;
        offset += stream_offset.value().second;
    }

    // Length (if LEN bit set)
    uint64_t length = 0;
    if (flags & stream_flags::len)
    {
        auto len_result = varint::decode(data.subspan(offset));
        if (len_result.is_err()) return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Failed to parse stream length");
        length = len_result.value().first;
        offset += len_result.value().second;
    }
    else
    {
        // Without LEN, data extends to end of packet
        length = data.size() - offset;
    }

    // FIN bit
    f.fin = (flags & stream_flags::fin) != 0;

    // Data
    if (data.size() - offset < length)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Insufficient stream data");
    }

    f.data.assign(data.begin() + offset, data.begin() + offset + length);
    offset += length;

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_max_data(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    max_data_frame f;

    auto max_data = varint::decode(data);
    if (max_data.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse max data");
    f.maximum_data = max_data.value().first;

    return ok(std::make_pair(frame{f}, max_data.value().second));
}

auto frame_parser::parse_max_stream_data(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    max_stream_data_frame f;

    auto stream_id = varint::decode(data.subspan(offset));
    if (stream_id.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse stream id");
    f.stream_id = stream_id.value().first;
    offset += stream_id.value().second;

    auto max_data = varint::decode(data.subspan(offset));
    if (max_data.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse max stream data");
    f.maximum_stream_data = max_data.value().first;
    offset += max_data.value().second;

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_max_streams(std::span<const uint8_t> data, bool bidi)
    -> Result<std::pair<frame, size_t>>
{
    max_streams_frame f;
    f.bidirectional = bidi;

    auto max_streams = varint::decode(data);
    if (max_streams.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse max streams");
    f.maximum_streams = max_streams.value().first;

    return ok(std::make_pair(frame{f}, max_streams.value().second));
}

auto frame_parser::parse_data_blocked(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    data_blocked_frame f;

    auto max_data = varint::decode(data);
    if (max_data.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse max data");
    f.maximum_data = max_data.value().first;

    return ok(std::make_pair(frame{f}, max_data.value().second));
}

auto frame_parser::parse_stream_data_blocked(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    stream_data_blocked_frame f;

    auto stream_id = varint::decode(data.subspan(offset));
    if (stream_id.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse stream id");
    f.stream_id = stream_id.value().first;
    offset += stream_id.value().second;

    auto max_data = varint::decode(data.subspan(offset));
    if (max_data.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse max stream data");
    f.maximum_stream_data = max_data.value().first;
    offset += max_data.value().second;

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_streams_blocked(std::span<const uint8_t> data, bool bidi)
    -> Result<std::pair<frame, size_t>>
{
    streams_blocked_frame f;
    f.bidirectional = bidi;

    auto max_streams = varint::decode(data);
    if (max_streams.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse max streams");
    f.maximum_streams = max_streams.value().first;

    return ok(std::make_pair(frame{f}, max_streams.value().second));
}

auto frame_parser::parse_new_connection_id(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    new_connection_id_frame f;

    // Sequence Number
    auto seq = varint::decode(data.subspan(offset));
    if (seq.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse sequence number");
    f.sequence_number = seq.value().first;
    offset += seq.value().second;

    // Retire Prior To
    auto retire = varint::decode(data.subspan(offset));
    if (retire.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse retire prior to");
    f.retire_prior_to = retire.value().first;
    offset += retire.value().second;

    // Connection ID Length (1 byte, not varint)
    if (data.size() <= offset)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Missing connection id length");
    }
    uint8_t cid_len = data[offset++];
    if (cid_len > 20)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Connection ID too long");
    }

    // Connection ID
    if (data.size() - offset < cid_len)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Insufficient connection id data");
    }
    f.connection_id.assign(data.begin() + offset, data.begin() + offset + cid_len);
    offset += cid_len;

    // Stateless Reset Token (16 bytes)
    if (data.size() - offset < 16)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Insufficient reset token data");
    }
    std::copy(data.begin() + offset, data.begin() + offset + 16,
              f.stateless_reset_token.begin());
    offset += 16;

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_retire_connection_id(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    retire_connection_id_frame f;

    auto seq = varint::decode(data);
    if (seq.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse sequence number");
    f.sequence_number = seq.value().first;

    return ok(std::make_pair(frame{f}, seq.value().second));
}

auto frame_parser::parse_path_challenge(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    if (data.size() < 8)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Insufficient path challenge data");
    }

    path_challenge_frame f;
    std::copy(data.begin(), data.begin() + 8, f.data.begin());

    return ok(std::make_pair(frame{f}, size_t{8}));
}

auto frame_parser::parse_path_response(std::span<const uint8_t> data)
    -> Result<std::pair<frame, size_t>>
{
    if (data.size() < 8)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Insufficient path response data");
    }

    path_response_frame f;
    std::copy(data.begin(), data.begin() + 8, f.data.begin());

    return ok(std::make_pair(frame{f}, size_t{8}));
}

auto frame_parser::parse_connection_close(std::span<const uint8_t> data, bool is_app)
    -> Result<std::pair<frame, size_t>>
{
    size_t offset = 0;
    connection_close_frame f;
    f.is_application_error = is_app;

    // Error Code
    auto error_code = varint::decode(data.subspan(offset));
    if (error_code.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse error code");
    f.error_code = error_code.value().first;
    offset += error_code.value().second;

    // Frame Type (transport close only)
    if (!is_app)
    {
        auto frame_type_val = varint::decode(data.subspan(offset));
        if (frame_type_val.is_err()) return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Failed to parse frame type");
        f.frame_type = frame_type_val.value().first;
        offset += frame_type_val.value().second;
    }

    // Reason Phrase Length
    auto reason_len = varint::decode(data.subspan(offset));
    if (reason_len.is_err()) return make_error<std::pair<frame, size_t>>(
        error_codes::common_errors::invalid_argument, "Failed to parse reason phrase length");
    offset += reason_len.value().second;

    // Reason Phrase
    if (data.size() - offset < reason_len.value().first)
    {
        return make_error<std::pair<frame, size_t>>(
            error_codes::common_errors::invalid_argument, "Insufficient reason phrase data");
    }

    f.reason_phrase.assign(reinterpret_cast<const char*>(data.data() + offset),
                           reason_len.value().first);
    offset += reason_len.value().first;

    return ok(std::make_pair(frame{f}, offset));
}

auto frame_parser::parse_handshake_done(std::span<const uint8_t> /*data*/)
    -> Result<std::pair<frame, size_t>>
{
    return ok(std::make_pair(frame{handshake_done_frame{}}, size_t{0}));
}

// ============================================================================
// frame_builder implementations
// ============================================================================

void frame_builder::append_varint(std::vector<uint8_t>& buffer, uint64_t value)
{
    auto encoded = varint::encode(value);
    buffer.insert(buffer.end(), encoded.begin(), encoded.end());
}

void frame_builder::append_bytes(std::vector<uint8_t>& buffer,
                                  std::span<const uint8_t> data)
{
    buffer.insert(buffer.end(), data.begin(), data.end());
}

auto frame_builder::build(const frame& f) -> std::vector<uint8_t>
{
    return std::visit([](const auto& fr) -> std::vector<uint8_t> {
        using T = std::decay_t<decltype(fr)>;
        if constexpr (std::is_same_v<T, padding_frame>)
            return build_padding(fr.count);
        else if constexpr (std::is_same_v<T, ping_frame>)
            return build_ping();
        else if constexpr (std::is_same_v<T, ack_frame>)
            return build_ack(fr);
        else if constexpr (std::is_same_v<T, reset_stream_frame>)
            return build_reset_stream(fr);
        else if constexpr (std::is_same_v<T, stop_sending_frame>)
            return build_stop_sending(fr);
        else if constexpr (std::is_same_v<T, crypto_frame>)
            return build_crypto(fr);
        else if constexpr (std::is_same_v<T, new_token_frame>)
            return build_new_token(fr);
        else if constexpr (std::is_same_v<T, stream_frame>)
            return build_stream(fr);
        else if constexpr (std::is_same_v<T, max_data_frame>)
            return build_max_data(fr);
        else if constexpr (std::is_same_v<T, max_stream_data_frame>)
            return build_max_stream_data(fr);
        else if constexpr (std::is_same_v<T, max_streams_frame>)
            return build_max_streams(fr);
        else if constexpr (std::is_same_v<T, data_blocked_frame>)
            return build_data_blocked(fr);
        else if constexpr (std::is_same_v<T, stream_data_blocked_frame>)
            return build_stream_data_blocked(fr);
        else if constexpr (std::is_same_v<T, streams_blocked_frame>)
            return build_streams_blocked(fr);
        else if constexpr (std::is_same_v<T, new_connection_id_frame>)
            return build_new_connection_id(fr);
        else if constexpr (std::is_same_v<T, retire_connection_id_frame>)
            return build_retire_connection_id(fr);
        else if constexpr (std::is_same_v<T, path_challenge_frame>)
            return build_path_challenge(fr);
        else if constexpr (std::is_same_v<T, path_response_frame>)
            return build_path_response(fr);
        else if constexpr (std::is_same_v<T, connection_close_frame>)
            return build_connection_close(fr);
        else if constexpr (std::is_same_v<T, handshake_done_frame>)
            return build_handshake_done();
        else
            return {};
    }, f);
}

auto frame_builder::build_padding(size_t count) -> std::vector<uint8_t>
{
    return std::vector<uint8_t>(count, 0x00);
}

auto frame_builder::build_ping() -> std::vector<uint8_t>
{
    return {0x01};
}

auto frame_builder::build_ack(const ack_frame& f) -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;

    // Frame type
    append_varint(buffer, f.ecn ? static_cast<uint64_t>(frame_type::ack_ecn)
                                : static_cast<uint64_t>(frame_type::ack));

    // Largest Acknowledged
    append_varint(buffer, f.largest_acknowledged);

    // ACK Delay
    append_varint(buffer, f.ack_delay);

    // ACK Range Count
    append_varint(buffer, f.ranges.size());

    // First ACK Range (largest_ack - smallest_ack in first range)
    // For simplicity, we'll use 0 if no ranges are specified
    uint64_t first_range = 0;
    if (!f.ranges.empty())
    {
        first_range = f.ranges[0].length;
    }
    append_varint(buffer, first_range);

    // Additional ranges (skip first)
    for (size_t i = 1; i < f.ranges.size(); ++i)
    {
        append_varint(buffer, f.ranges[i].gap);
        append_varint(buffer, f.ranges[i].length);
    }

    // ECN Counts
    if (f.ecn)
    {
        append_varint(buffer, f.ecn->ect0);
        append_varint(buffer, f.ecn->ect1);
        append_varint(buffer, f.ecn->ecn_ce);
    }

    return buffer;
}

auto frame_builder::build_reset_stream(const reset_stream_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::reset_stream));
    append_varint(buffer, f.stream_id);
    append_varint(buffer, f.application_error_code);
    append_varint(buffer, f.final_size);
    return buffer;
}

auto frame_builder::build_stop_sending(const stop_sending_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::stop_sending));
    append_varint(buffer, f.stream_id);
    append_varint(buffer, f.application_error_code);
    return buffer;
}

auto frame_builder::build_crypto(const crypto_frame& f) -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::crypto));
    append_varint(buffer, f.offset);
    append_varint(buffer, f.data.size());
    append_bytes(buffer, f.data);
    return buffer;
}

auto frame_builder::build_new_token(const new_token_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::new_token));
    append_varint(buffer, f.token.size());
    append_bytes(buffer, f.token);
    return buffer;
}

auto frame_builder::build_stream(const stream_frame& f, bool include_length)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;

    // Build frame type with flags
    uint8_t type = make_stream_type(f.fin, include_length, f.offset > 0);
    buffer.push_back(type);

    // Stream ID
    append_varint(buffer, f.stream_id);

    // Offset (if non-zero)
    if (f.offset > 0)
    {
        append_varint(buffer, f.offset);
    }

    // Length (if requested)
    if (include_length)
    {
        append_varint(buffer, f.data.size());
    }

    // Data
    append_bytes(buffer, f.data);

    return buffer;
}

auto frame_builder::build_max_data(const max_data_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::max_data));
    append_varint(buffer, f.maximum_data);
    return buffer;
}

auto frame_builder::build_max_stream_data(const max_stream_data_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::max_stream_data));
    append_varint(buffer, f.stream_id);
    append_varint(buffer, f.maximum_stream_data);
    return buffer;
}

auto frame_builder::build_max_streams(const max_streams_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, f.bidirectional
        ? static_cast<uint64_t>(frame_type::max_streams_bidi)
        : static_cast<uint64_t>(frame_type::max_streams_uni));
    append_varint(buffer, f.maximum_streams);
    return buffer;
}

auto frame_builder::build_data_blocked(const data_blocked_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::data_blocked));
    append_varint(buffer, f.maximum_data);
    return buffer;
}

auto frame_builder::build_stream_data_blocked(const stream_data_blocked_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::stream_data_blocked));
    append_varint(buffer, f.stream_id);
    append_varint(buffer, f.maximum_stream_data);
    return buffer;
}

auto frame_builder::build_streams_blocked(const streams_blocked_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, f.bidirectional
        ? static_cast<uint64_t>(frame_type::streams_blocked_bidi)
        : static_cast<uint64_t>(frame_type::streams_blocked_uni));
    append_varint(buffer, f.maximum_streams);
    return buffer;
}

auto frame_builder::build_new_connection_id(const new_connection_id_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::new_connection_id));
    append_varint(buffer, f.sequence_number);
    append_varint(buffer, f.retire_prior_to);

    // Connection ID length (1 byte, not varint)
    buffer.push_back(static_cast<uint8_t>(f.connection_id.size()));

    // Connection ID
    append_bytes(buffer, f.connection_id);

    // Stateless Reset Token
    buffer.insert(buffer.end(), f.stateless_reset_token.begin(),
                  f.stateless_reset_token.end());

    return buffer;
}

auto frame_builder::build_retire_connection_id(const retire_connection_id_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::retire_connection_id));
    append_varint(buffer, f.sequence_number);
    return buffer;
}

auto frame_builder::build_path_challenge(const path_challenge_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::path_challenge));
    buffer.insert(buffer.end(), f.data.begin(), f.data.end());
    return buffer;
}

auto frame_builder::build_path_response(const path_response_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, static_cast<uint64_t>(frame_type::path_response));
    buffer.insert(buffer.end(), f.data.begin(), f.data.end());
    return buffer;
}

auto frame_builder::build_connection_close(const connection_close_frame& f)
    -> std::vector<uint8_t>
{
    std::vector<uint8_t> buffer;
    append_varint(buffer, f.is_application_error
        ? static_cast<uint64_t>(frame_type::connection_close_app)
        : static_cast<uint64_t>(frame_type::connection_close));

    append_varint(buffer, f.error_code);

    // Frame Type (transport close only)
    if (!f.is_application_error)
    {
        append_varint(buffer, f.frame_type);
    }

    // Reason Phrase Length
    append_varint(buffer, f.reason_phrase.size());

    // Reason Phrase
    buffer.insert(buffer.end(), f.reason_phrase.begin(), f.reason_phrase.end());

    return buffer;
}

auto frame_builder::build_handshake_done() -> std::vector<uint8_t>
{
    return {static_cast<uint8_t>(frame_type::handshake_done)};
}

} // namespace kcenon::network::protocols::quic
