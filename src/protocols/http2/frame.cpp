/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#include "internal/protocols/http2/frame.h"
#include <cstring>
#include <algorithm>

namespace kcenon::network::protocols::http2
{
    namespace
    {
        constexpr size_t frame_header_size = 9;
        constexpr uint32_t max_frame_size = (1 << 24) - 1;  // 16,777,215 bytes
        constexpr uint32_t stream_id_mask = 0x7FFFFFFF;     // 31-bit mask

        auto read_uint24_be(std::span<const uint8_t> data) -> uint32_t
        {
            return (static_cast<uint32_t>(data[0]) << 16) |
                   (static_cast<uint32_t>(data[1]) << 8) |
                   static_cast<uint32_t>(data[2]);
        }

        auto read_uint32_be(std::span<const uint8_t> data) -> uint32_t
        {
            return (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8) |
                   static_cast<uint32_t>(data[3]);
        }

        auto write_uint24_be(uint32_t value) -> std::array<uint8_t, 3>
        {
            return {
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF)
            };
        }

        auto write_uint32_be(uint32_t value) -> std::array<uint8_t, 4>
        {
            return {
                static_cast<uint8_t>((value >> 24) & 0xFF),
                static_cast<uint8_t>((value >> 16) & 0xFF),
                static_cast<uint8_t>((value >> 8) & 0xFF),
                static_cast<uint8_t>(value & 0xFF)
            };
        }
    }

    auto frame_header::parse(std::span<const uint8_t> data) -> Result<frame_header>
    {
        if (data.size() < frame_header_size)
        {
            return error_info(1, "Insufficient data for frame header", "http2");
        }

        frame_header header;
        header.length = read_uint24_be(data.subspan(0, 3));
        header.type = static_cast<frame_type>(data[3]);
        header.flags = data[4];
        header.stream_id = read_uint32_be(data.subspan(5, 4)) & stream_id_mask;

        if (header.length > max_frame_size)
        {
            return error_info(2, "Frame size exceeds maximum allowed", "http2");
        }

        return header;
    }

    auto frame_header::serialize() const -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result(frame_header_size);

        auto length_bytes = write_uint24_be(length);
        std::copy(length_bytes.begin(), length_bytes.end(), result.begin());

        result[3] = static_cast<uint8_t>(type);
        result[4] = flags;

        auto stream_id_bytes = write_uint32_be(stream_id & stream_id_mask);
        std::copy(stream_id_bytes.begin(), stream_id_bytes.end(), result.begin() + 5);

        return result;
    }

    frame::frame(const frame_header& hdr, std::vector<uint8_t> payload)
        : header_(hdr), payload_(std::move(payload))
    {
    }

    auto frame::parse(std::span<const uint8_t> data) -> Result<std::unique_ptr<frame>>
    {
        auto header_result = frame_header::parse(data);
        if (header_result.is_err())
        {
            return header_result.error();
        }

        const auto& header = header_result.value();

        if (data.size() < frame_header_size + header.length)
        {
            return error_info(3, "Insufficient data for frame payload", "http2");
        }

        auto payload_span = data.subspan(frame_header_size, header.length);
        std::vector<uint8_t> payload(payload_span.begin(), payload_span.end());

        std::unique_ptr<frame> result_frame;

        switch (header.type)
        {
            case frame_type::data:
            {
                auto parsed = data_frame::parse(header, payload);
                if (parsed.is_err()) return parsed.error();
                result_frame = std::move(parsed.value());
                break;
            }
            case frame_type::headers:
            {
                auto parsed = headers_frame::parse(header, payload);
                if (parsed.is_err()) return parsed.error();
                result_frame = std::move(parsed.value());
                break;
            }
            case frame_type::settings:
            {
                auto parsed = settings_frame::parse(header, payload);
                if (parsed.is_err()) return parsed.error();
                result_frame = std::move(parsed.value());
                break;
            }
            case frame_type::rst_stream:
            {
                auto parsed = rst_stream_frame::parse(header, payload);
                if (parsed.is_err()) return parsed.error();
                result_frame = std::move(parsed.value());
                break;
            }
            case frame_type::ping:
            {
                auto parsed = ping_frame::parse(header, payload);
                if (parsed.is_err()) return parsed.error();
                result_frame = std::move(parsed.value());
                break;
            }
            case frame_type::goaway:
            {
                auto parsed = goaway_frame::parse(header, payload);
                if (parsed.is_err()) return parsed.error();
                result_frame = std::move(parsed.value());
                break;
            }
            case frame_type::window_update:
            {
                auto parsed = window_update_frame::parse(header, payload);
                if (parsed.is_err()) return parsed.error();
                result_frame = std::move(parsed.value());
                break;
            }
            default:
                result_frame = std::make_unique<frame>(header, std::move(payload));
                break;
        }

        return std::move(result_frame);
    }

    auto frame::serialize() const -> std::vector<uint8_t>
    {
        auto result = header_.serialize();
        result.insert(result.end(), payload_.begin(), payload_.end());
        return result;
    }

    auto frame::header() const -> const frame_header&
    {
        return header_;
    }

    auto frame::payload() const -> std::span<const uint8_t>
    {
        return payload_;
    }

    data_frame::data_frame(uint32_t stream_id, std::vector<uint8_t> data,
                           bool end_stream, bool padded)
        : data_(std::move(data))
    {
        header_.stream_id = stream_id;
        header_.type = frame_type::data;
        header_.flags = frame_flags::none;

        if (end_stream)
        {
            header_.flags |= frame_flags::end_stream;
        }

        if (padded)
        {
            header_.flags |= frame_flags::padded;
            payload_.push_back(0);  // Pad length = 0 for now
        }

        payload_.insert(payload_.end(), data_.begin(), data_.end());
        header_.length = static_cast<uint32_t>(payload_.size());
    }

    auto data_frame::parse(const frame_header& hdr, std::span<const uint8_t> payload)
        -> Result<std::unique_ptr<data_frame>>
    {
        if (hdr.stream_id == 0)
        {
            return error_info(4, "DATA frame must have non-zero stream ID", "http2");
        }

        std::vector<uint8_t> data;
        size_t data_offset = 0;

        if (hdr.flags & frame_flags::padded)
        {
            if (payload.empty())
            {
                return error_info(5, "Padded DATA frame must have pad length", "http2");
            }

            uint8_t pad_length = payload[0];
            data_offset = 1;

            if (payload.size() < data_offset + pad_length)
            {
                return error_info(6, "Invalid padding in DATA frame", "http2");
            }

            data.assign(payload.begin() + data_offset,
                       payload.end() - pad_length);
        }
        else
        {
            data.assign(payload.begin(), payload.end());
        }

        auto frame = std::make_unique<data_frame>(
            hdr.stream_id,
            std::move(data),
            hdr.flags & frame_flags::end_stream,
            hdr.flags & frame_flags::padded
        );

        return frame;
    }

    auto data_frame::is_end_stream() const -> bool
    {
        return header_.flags & frame_flags::end_stream;
    }

    auto data_frame::is_padded() const -> bool
    {
        return header_.flags & frame_flags::padded;
    }

    auto data_frame::data() const -> std::span<const uint8_t>
    {
        return data_;
    }

    headers_frame::headers_frame(uint32_t stream_id, std::vector<uint8_t> header_block,
                                 bool end_stream, bool end_headers)
        : header_block_(std::move(header_block))
    {
        header_.stream_id = stream_id;
        header_.type = frame_type::headers;
        header_.flags = frame_flags::none;

        if (end_stream)
        {
            header_.flags |= frame_flags::end_stream;
        }

        if (end_headers)
        {
            header_.flags |= frame_flags::end_headers;
        }

        payload_ = header_block_;
        header_.length = static_cast<uint32_t>(payload_.size());
    }

    auto headers_frame::parse(const frame_header& hdr, std::span<const uint8_t> payload)
        -> Result<std::unique_ptr<headers_frame>>
    {
        if (hdr.stream_id == 0)
        {
            return error_info(7, "HEADERS frame must have non-zero stream ID", "http2");
        }

        std::vector<uint8_t> header_block;
        size_t offset = 0;

        if (hdr.flags & frame_flags::padded)
        {
            if (payload.empty())
            {
                return error_info(8, "Padded HEADERS frame must have pad length", "http2");
            }

            uint8_t pad_length = payload[0];
            offset = 1;

            if (payload.size() < offset + pad_length)
            {
                return error_info(9, "Invalid padding in HEADERS frame", "http2");
            }

            header_block.assign(payload.begin() + offset,
                              payload.end() - pad_length);
        }
        else
        {
            header_block.assign(payload.begin(), payload.end());
        }

        auto frame = std::make_unique<headers_frame>(
            hdr.stream_id,
            std::move(header_block),
            hdr.flags & frame_flags::end_stream,
            hdr.flags & frame_flags::end_headers
        );

        return frame;
    }

    auto headers_frame::is_end_stream() const -> bool
    {
        return header_.flags & frame_flags::end_stream;
    }

    auto headers_frame::is_end_headers() const -> bool
    {
        return header_.flags & frame_flags::end_headers;
    }

    auto headers_frame::header_block() const -> std::span<const uint8_t>
    {
        return header_block_;
    }

    settings_frame::settings_frame(std::vector<setting_parameter> settings, bool ack)
        : settings_(std::move(settings))
    {
        header_.stream_id = 0;
        header_.type = frame_type::settings;
        header_.flags = ack ? frame_flags::ack : frame_flags::none;

        if (!ack)
        {
            for (const auto& setting : settings_)
            {
                auto id_bytes = write_uint32_be(static_cast<uint32_t>(setting.identifier));
                payload_.insert(payload_.end(), id_bytes.begin() + 2, id_bytes.end());

                auto value_bytes = write_uint32_be(setting.value);
                payload_.insert(payload_.end(), value_bytes.begin(), value_bytes.end());
            }
        }

        header_.length = static_cast<uint32_t>(payload_.size());
    }

    auto settings_frame::parse(const frame_header& hdr, std::span<const uint8_t> payload)
        -> Result<std::unique_ptr<settings_frame>>
    {
        if (hdr.stream_id != 0)
        {
            return error_info(10, "SETTINGS frame must have zero stream ID", "http2");
        }

        if (hdr.flags & frame_flags::ack)
        {
            if (!payload.empty())
            {
                return error_info(11, "SETTINGS ACK must have empty payload", "http2");
            }

            return std::make_unique<settings_frame>(std::vector<setting_parameter>{}, true);
        }

        if (payload.size() % 6 != 0)
        {
            return error_info(12, "Invalid SETTINGS frame payload size", "http2");
        }

        std::vector<setting_parameter> settings;
        for (size_t i = 0; i < payload.size(); i += 6)
        {
            uint16_t identifier = (static_cast<uint16_t>(payload[i]) << 8) |
                                 static_cast<uint16_t>(payload[i + 1]);
            uint32_t value = read_uint32_be(payload.subspan(i + 2, 4));

            settings.push_back({identifier, value});
        }

        return std::make_unique<settings_frame>(std::move(settings), false);
    }

    auto settings_frame::settings() const -> const std::vector<setting_parameter>&
    {
        return settings_;
    }

    auto settings_frame::is_ack() const -> bool
    {
        return header_.flags & frame_flags::ack;
    }

    rst_stream_frame::rst_stream_frame(uint32_t stream_id, uint32_t error_code)
        : error_code_(error_code)
    {
        header_.stream_id = stream_id;
        header_.type = frame_type::rst_stream;
        header_.flags = frame_flags::none;
        header_.length = 4;

        auto error_bytes = write_uint32_be(error_code_);
        payload_.assign(error_bytes.begin(), error_bytes.end());
    }

    auto rst_stream_frame::parse(const frame_header& hdr, std::span<const uint8_t> payload)
        -> Result<std::unique_ptr<rst_stream_frame>>
    {
        if (hdr.stream_id == 0)
        {
            return error_info(13, "RST_STREAM frame must have non-zero stream ID", "http2");
        }

        if (payload.size() != 4)
        {
            return error_info(14, "RST_STREAM frame must have 4-byte payload", "http2");
        }

        uint32_t error_code = read_uint32_be(payload);
        return std::make_unique<rst_stream_frame>(hdr.stream_id, error_code);
    }

    auto rst_stream_frame::error_code() const -> uint32_t
    {
        return error_code_;
    }

    ping_frame::ping_frame(std::array<uint8_t, 8> opaque_data, bool ack)
        : opaque_data_(opaque_data)
    {
        header_.stream_id = 0;
        header_.type = frame_type::ping;
        header_.flags = ack ? frame_flags::ack : frame_flags::none;
        header_.length = 8;

        payload_.assign(opaque_data_.begin(), opaque_data_.end());
    }

    auto ping_frame::parse(const frame_header& hdr, std::span<const uint8_t> payload)
        -> Result<std::unique_ptr<ping_frame>>
    {
        if (hdr.stream_id != 0)
        {
            return error_info(15, "PING frame must have zero stream ID", "http2");
        }

        if (payload.size() != 8)
        {
            return error_info(16, "PING frame must have 8-byte payload", "http2");
        }

        std::array<uint8_t, 8> opaque_data;
        std::copy_n(payload.begin(), 8, opaque_data.begin());

        return std::make_unique<ping_frame>(opaque_data, hdr.flags & frame_flags::ack);
    }

    auto ping_frame::opaque_data() const -> const std::array<uint8_t, 8>&
    {
        return opaque_data_;
    }

    auto ping_frame::is_ack() const -> bool
    {
        return header_.flags & frame_flags::ack;
    }

    goaway_frame::goaway_frame(uint32_t last_stream_id, uint32_t error_code,
                               std::vector<uint8_t> additional_data)
        : last_stream_id_(last_stream_id),
          error_code_(error_code),
          additional_data_(std::move(additional_data))
    {
        header_.stream_id = 0;
        header_.type = frame_type::goaway;
        header_.flags = frame_flags::none;

        auto last_stream_bytes = write_uint32_be(last_stream_id_ & stream_id_mask);
        payload_.insert(payload_.end(), last_stream_bytes.begin(), last_stream_bytes.end());

        auto error_bytes = write_uint32_be(error_code_);
        payload_.insert(payload_.end(), error_bytes.begin(), error_bytes.end());

        payload_.insert(payload_.end(), additional_data_.begin(), additional_data_.end());

        header_.length = static_cast<uint32_t>(payload_.size());
    }

    auto goaway_frame::parse(const frame_header& hdr, std::span<const uint8_t> payload)
        -> Result<std::unique_ptr<goaway_frame>>
    {
        if (hdr.stream_id != 0)
        {
            return error_info(17, "GOAWAY frame must have zero stream ID", "http2");
        }

        if (payload.size() < 8)
        {
            return error_info(18, "GOAWAY frame must have at least 8-byte payload", "http2");
        }

        uint32_t last_stream_id = read_uint32_be(payload.subspan(0, 4)) & stream_id_mask;
        uint32_t error_code = read_uint32_be(payload.subspan(4, 4));

        std::vector<uint8_t> additional_data;
        if (payload.size() > 8)
        {
            additional_data.assign(payload.begin() + 8, payload.end());
        }

        return std::make_unique<goaway_frame>(last_stream_id, error_code,
                                             std::move(additional_data));
    }

    auto goaway_frame::last_stream_id() const -> uint32_t
    {
        return last_stream_id_;
    }

    auto goaway_frame::error_code() const -> uint32_t
    {
        return error_code_;
    }

    auto goaway_frame::additional_data() const -> std::span<const uint8_t>
    {
        return additional_data_;
    }

    window_update_frame::window_update_frame(uint32_t stream_id,
                                             uint32_t window_size_increment)
        : window_size_increment_(window_size_increment)
    {
        header_.stream_id = stream_id;
        header_.type = frame_type::window_update;
        header_.flags = frame_flags::none;
        header_.length = 4;

        auto increment_bytes = write_uint32_be(window_size_increment_ & stream_id_mask);
        payload_.assign(increment_bytes.begin(), increment_bytes.end());
    }

    auto window_update_frame::parse(const frame_header& hdr, std::span<const uint8_t> payload)
        -> Result<std::unique_ptr<window_update_frame>>
    {
        if (payload.size() != 4)
        {
            return error_info(19, "WINDOW_UPDATE frame must have 4-byte payload", "http2");
        }

        uint32_t window_size_increment = read_uint32_be(payload) & stream_id_mask;

        if (window_size_increment == 0)
        {
            return error_info(20, "WINDOW_UPDATE increment must be non-zero", "http2");
        }

        return std::make_unique<window_update_frame>(hdr.stream_id, window_size_increment);
    }

    auto window_update_frame::window_size_increment() const -> uint32_t
    {
        return window_size_increment_;
    }

} // namespace kcenon::network::protocols::http2
