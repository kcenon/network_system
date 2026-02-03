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

#include "internal/protocols/http2/hpack.h"
#include <algorithm>
#include <cstring>

namespace kcenon::network::protocols::http2
{
    namespace
    {
        // HPACK static table (RFC 7541 Appendix A)
        const http_header static_table_entries[] = {
            {"", ""},                                        // Index 0 (unused)
            {":authority", ""},                              // 1
            {":method", "GET"},                              // 2
            {":method", "POST"},                             // 3
            {":path", "/"},                                  // 4
            {":path", "/index.html"},                        // 5
            {":scheme", "http"},                             // 6
            {":scheme", "https"},                            // 7
            {":status", "200"},                              // 8
            {":status", "204"},                              // 9
            {":status", "206"},                              // 10
            {":status", "304"},                              // 11
            {":status", "400"},                              // 12
            {":status", "404"},                              // 13
            {":status", "500"},                              // 14
            {"accept-charset", ""},                          // 15
            {"accept-encoding", "gzip, deflate"},            // 16
            {"accept-language", ""},                         // 17
            {"accept-ranges", ""},                           // 18
            {"accept", ""},                                  // 19
            {"access-control-allow-origin", ""},             // 20
            {"age", ""},                                     // 21
            {"allow", ""},                                   // 22
            {"authorization", ""},                           // 23
            {"cache-control", ""},                           // 24
            {"content-disposition", ""},                     // 25
            {"content-encoding", ""},                        // 26
            {"content-language", ""},                        // 27
            {"content-length", ""},                          // 28
            {"content-location", ""},                        // 29
            {"content-range", ""},                           // 30
            {"content-type", ""},                            // 31
            {"cookie", ""},                                  // 32
            {"date", ""},                                    // 33
            {"etag", ""},                                    // 34
            {"expect", ""},                                  // 35
            {"expires", ""},                                 // 36
            {"from", ""},                                    // 37
            {"host", ""},                                    // 38
            {"if-match", ""},                                // 39
            {"if-modified-since", ""},                       // 40
            {"if-none-match", ""},                           // 41
            {"if-range", ""},                                // 42
            {"if-unmodified-since", ""},                     // 43
            {"last-modified", ""},                           // 44
            {"link", ""},                                    // 45
            {"location", ""},                                // 46
            {"max-forwards", ""},                            // 47
            {"proxy-authenticate", ""},                      // 48
            {"proxy-authorization", ""},                     // 49
            {"range", ""},                                   // 50
            {"referer", ""},                                 // 51
            {"refresh", ""},                                 // 52
            {"retry-after", ""},                             // 53
            {"server", ""},                                  // 54
            {"set-cookie", ""},                              // 55
            {"strict-transport-security", ""},               // 56
            {"transfer-encoding", ""},                       // 57
            {"user-agent", ""},                              // 58
            {"vary", ""},                                    // 59
            {"via", ""},                                     // 60
            {"www-authenticate", ""}                         // 61
        };

        constexpr size_t static_table_size = 61;
    }

    // Static table implementation
    auto static_table::get(size_t index) -> std::optional<http_header>
    {
        if (index == 0 || index > static_table_size)
        {
            return std::nullopt;
        }
        return static_table_entries[index];
    }

    auto static_table::find(std::string_view name, std::string_view value)
        -> size_t
    {
        for (size_t i = 1; i <= static_table_size; ++i)
        {
            const auto& entry = static_table_entries[i];
            if (entry.name == name)
            {
                if (value.empty() || entry.value == value)
                {
                    return i;
                }
            }
        }
        return 0;
    }

    // Dynamic table implementation
    dynamic_table::dynamic_table(size_t max_size)
        : max_size_(max_size)
    {
    }

    auto dynamic_table::insert(std::string_view name, std::string_view value) -> void
    {
        http_header header{std::string(name), std::string(value)};
        size_t entry_size = header.size();

        // Evict entries if needed
        evict_to_size(max_size_ - entry_size);

        // Insert at beginning
        entries_.push_front(std::move(header));
        current_size_ += entry_size;
    }

    auto dynamic_table::get(size_t index) const -> std::optional<http_header>
    {
        if (index >= entries_.size())
        {
            return std::nullopt;
        }
        return entries_[index];
    }

    auto dynamic_table::find(std::string_view name, std::string_view value) const
        -> std::optional<size_t>
    {
        for (size_t i = 0; i < entries_.size(); ++i)
        {
            const auto& entry = entries_[i];
            if (entry.name == name)
            {
                if (value.empty() || entry.value == value)
                {
                    return i;
                }
            }
        }
        return std::nullopt;
    }

    auto dynamic_table::set_max_size(size_t size) -> void
    {
        max_size_ = size;
        evict_to_size(max_size_);
    }

    auto dynamic_table::current_size() const -> size_t
    {
        return current_size_;
    }

    auto dynamic_table::max_size() const -> size_t
    {
        return max_size_;
    }

    auto dynamic_table::entry_count() const -> size_t
    {
        return entries_.size();
    }

    auto dynamic_table::clear() -> void
    {
        entries_.clear();
        current_size_ = 0;
    }

    auto dynamic_table::evict_to_size(size_t target_size) -> void
    {
        while (current_size_ > target_size && !entries_.empty())
        {
            current_size_ -= entries_.back().size();
            entries_.pop_back();
        }
    }

    // HPACK encoder implementation
    hpack_encoder::hpack_encoder(size_t max_table_size)
        : table_(max_table_size)
    {
    }

    auto hpack_encoder::encode(const std::vector<http_header>& headers)
        -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result;

        for (const auto& header : headers)
        {
            // Try to find in static table first
            size_t static_index = static_table::find(header.name, header.value);
            if (static_index > 0)
            {
                // Indexed header field representation
                auto encoded = encode_indexed(static_index);
                result.insert(result.end(), encoded.begin(), encoded.end());
                continue;
            }

            // Try to find in dynamic table
            auto dynamic_index = table_.find(header.name, header.value);
            if (dynamic_index.has_value())
            {
                // Indexed header field representation
                size_t index = static_table::size() + 1 + dynamic_index.value();
                auto encoded = encode_indexed(index);
                result.insert(result.end(), encoded.begin(), encoded.end());
                continue;
            }

            // Check if name is in static table
            size_t name_index = static_table::find(header.name, "");
            if (name_index > 0)
            {
                // Literal with incremental indexing - indexed name
                auto encoded = encode_literal_with_indexing(name_index, header.value);
                result.insert(result.end(), encoded.begin(), encoded.end());
                table_.insert(header.name, header.value);
            }
            else
            {
                // Check if name is in dynamic table
                auto dynamic_name_index = table_.find(header.name, "");
                if (dynamic_name_index.has_value())
                {
                    size_t index = static_table::size() + 1 + dynamic_name_index.value();
                    auto encoded = encode_literal_with_indexing(index, header.value);
                    result.insert(result.end(), encoded.begin(), encoded.end());
                    table_.insert(header.name, header.value);
                }
                else
                {
                    // Literal with incremental indexing - new name
                    auto encoded = encode_literal_with_indexing(header.name, header.value);
                    result.insert(result.end(), encoded.begin(), encoded.end());
                    table_.insert(header.name, header.value);
                }
            }
        }

        return result;
    }

    auto hpack_encoder::set_max_table_size(size_t size) -> void
    {
        table_.set_max_size(size);
    }

    auto hpack_encoder::table_size() const -> size_t
    {
        return table_.current_size();
    }

    auto hpack_encoder::encode_integer(uint64_t value, uint8_t prefix_bits)
        -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result;
        uint8_t max_prefix = (1 << prefix_bits) - 1;

        if (value < max_prefix)
        {
            result.push_back(static_cast<uint8_t>(value));
        }
        else
        {
            result.push_back(max_prefix);
            value -= max_prefix;

            while (value >= 128)
            {
                result.push_back(static_cast<uint8_t>((value % 128) + 128));
                value /= 128;
            }
            result.push_back(static_cast<uint8_t>(value));
        }

        return result;
    }

    auto hpack_encoder::encode_string(std::string_view str, bool huffman)
        -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result;

        // String length (with Huffman bit)
        auto length_bytes = encode_integer(str.size(), 7);
        if (!huffman)
        {
            result.insert(result.end(), length_bytes.begin(), length_bytes.end());
        }
        else
        {
            // Huffman encoding not implemented yet
            // Set H bit in first byte
            length_bytes[0] |= 0x80;
            result.insert(result.end(), length_bytes.begin(), length_bytes.end());
        }

        // String data
        result.insert(result.end(), str.begin(), str.end());

        return result;
    }

    auto hpack_encoder::encode_indexed(size_t index) -> std::vector<uint8_t>
    {
        auto result = encode_integer(index, 7);
        result[0] |= 0x80;  // Set indexed bit
        return result;
    }

    auto hpack_encoder::encode_literal_with_indexing(std::string_view name,
                                                      std::string_view value)
        -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result;

        // First byte: 01xxxxxx (literal with incremental indexing, new name)
        result.push_back(0x40);

        // Encode name
        auto name_bytes = encode_string(name);
        result.insert(result.end(), name_bytes.begin(), name_bytes.end());

        // Encode value
        auto value_bytes = encode_string(value);
        result.insert(result.end(), value_bytes.begin(), value_bytes.end());

        return result;
    }

    auto hpack_encoder::encode_literal_with_indexing(size_t name_index,
                                                      std::string_view value)
        -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result;

        // Encode name index with 6-bit prefix
        auto index_bytes = encode_integer(name_index, 6);
        index_bytes[0] |= 0x40;  // Set literal with indexing bit
        result.insert(result.end(), index_bytes.begin(), index_bytes.end());

        // Encode value
        auto value_bytes = encode_string(value);
        result.insert(result.end(), value_bytes.begin(), value_bytes.end());

        return result;
    }

    auto hpack_encoder::encode_literal_without_indexing(std::string_view name,
                                                         std::string_view value)
        -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result;

        // First byte: 0000xxxx (literal without indexing, new name)
        result.push_back(0x00);

        // Encode name
        auto name_bytes = encode_string(name);
        result.insert(result.end(), name_bytes.begin(), name_bytes.end());

        // Encode value
        auto value_bytes = encode_string(value);
        result.insert(result.end(), value_bytes.begin(), value_bytes.end());

        return result;
    }

    auto hpack_encoder::encode_literal_without_indexing(size_t name_index,
                                                         std::string_view value)
        -> std::vector<uint8_t>
    {
        std::vector<uint8_t> result;

        // Encode name index with 4-bit prefix
        auto index_bytes = encode_integer(name_index, 4);
        // First byte already has 0000 prefix for literal without indexing
        result.insert(result.end(), index_bytes.begin(), index_bytes.end());

        // Encode value
        auto value_bytes = encode_string(value);
        result.insert(result.end(), value_bytes.begin(), value_bytes.end());

        return result;
    }

    // HPACK decoder implementation
    hpack_decoder::hpack_decoder(size_t max_table_size)
        : table_(max_table_size)
    {
    }

    auto hpack_decoder::decode(std::span<const uint8_t> data)
        -> Result<std::vector<http_header>>
    {
        std::vector<http_header> headers;
        auto remaining = data;

        while (!remaining.empty())
        {
            uint8_t first_byte = remaining[0];

            if (first_byte & 0x80)
            {
                // Indexed header field representation
                auto index_result = decode_integer(remaining, 7);
                if (index_result.is_err())
                {
                    return index_result.error();
                }
                size_t index = index_result.value();

                auto header_result = get_indexed_header(index);
                if (header_result.is_err())
                {
                    return header_result.error();
                }

                headers.push_back(header_result.value());
            }
            else if (first_byte & 0x40)
            {
                // Literal with incremental indexing
                auto name_index_result = decode_integer(remaining, 6);
                if (name_index_result.is_err())
                {
                    return name_index_result.error();
                }
                size_t name_index = name_index_result.value();

                std::string name;
                if (name_index == 0)
                {
                    // New name
                    auto name_result = decode_string(remaining);
                    if (name_result.is_err())
                    {
                        return name_result.error();
                    }
                    name = name_result.value();
                }
                else
                {
                    // Indexed name
                    auto header_result = get_indexed_header(name_index);
                    if (header_result.is_err())
                    {
                        return header_result.error();
                    }
                    name = header_result.value().name;
                }

                // Decode value
                auto value_result = decode_string(remaining);
                if (value_result.is_err())
                {
                    return value_result.error();
                }
                std::string value = value_result.value();

                headers.emplace_back(name, value);
                table_.insert(name, value);
            }
            else
            {
                // Literal without indexing or never indexed
                uint8_t prefix_bits = (first_byte & 0x10) ? 4 : 4;

                auto name_index_result = decode_integer(remaining, prefix_bits);
                if (name_index_result.is_err())
                {
                    return name_index_result.error();
                }
                size_t name_index = name_index_result.value();

                std::string name;
                if (name_index == 0)
                {
                    // New name
                    auto name_result = decode_string(remaining);
                    if (name_result.is_err())
                    {
                        return name_result.error();
                    }
                    name = name_result.value();
                }
                else
                {
                    // Indexed name
                    auto header_result = get_indexed_header(name_index);
                    if (header_result.is_err())
                    {
                        return header_result.error();
                    }
                    name = header_result.value().name;
                }

                // Decode value
                auto value_result = decode_string(remaining);
                if (value_result.is_err())
                {
                    return value_result.error();
                }

                headers.emplace_back(name, value_result.value());
            }
        }

        return headers;
    }

    auto hpack_decoder::set_max_table_size(size_t size) -> void
    {
        table_.set_max_size(size);
    }

    auto hpack_decoder::table_size() const -> size_t
    {
        return table_.current_size();
    }

    auto hpack_decoder::decode_integer(std::span<const uint8_t>& data,
                                        uint8_t prefix_bits)
        -> Result<uint64_t>
    {
        if (data.empty())
        {
            return error_info(100, "Insufficient data for integer", "hpack");
        }

        uint8_t prefix_mask = (1 << prefix_bits) - 1;
        uint64_t value = data[0] & prefix_mask;
        data = data.subspan(1);

        if (value < prefix_mask)
        {
            return value;
        }

        // Multi-byte integer
        uint64_t m = 0;
        do
        {
            if (data.empty())
            {
                return error_info(101, "Incomplete integer encoding", "hpack");
            }

            uint8_t byte = data[0];
            data = data.subspan(1);

            value += (byte & 0x7F) << m;
            m += 7;

            if (m >= 64)
            {
                return error_info(102, "Integer overflow", "hpack");
            }

            if ((byte & 0x80) == 0)
            {
                break;
            }
        } while (true);

        return value;
    }

    auto hpack_decoder::decode_string(std::span<const uint8_t>& data)
        -> Result<std::string>
    {
        if (data.empty())
        {
            return error_info(103, "Insufficient data for string", "hpack");
        }

        bool huffman = (data[0] & 0x80) != 0;

        auto length_result = decode_integer(data, 7);
        if (length_result.is_err())
        {
            return length_result.error();
        }

        size_t length = length_result.value();

        if (data.size() < length)
        {
            return error_info(104, "Insufficient data for string value", "hpack");
        }

        std::string result;
        if (huffman)
        {
            // Huffman decoding not implemented yet - just return raw for now
            result.assign(data.begin(), data.begin() + length);
        }
        else
        {
            result.assign(data.begin(), data.begin() + length);
        }

        data = data.subspan(length);
        return result;
    }

    auto hpack_decoder::get_indexed_header(size_t index) const
        -> Result<http_header>
    {
        if (index == 0)
        {
            return error_info(105, "Invalid index 0", "hpack");
        }

        // Check static table
        if (index <= static_table::size())
        {
            auto header = static_table::get(index);
            if (header.has_value())
            {
                return std::move(header.value());
            }
            return error_info(106, "Invalid static table index", "hpack");
        }

        // Check dynamic table
        size_t dynamic_index = index - static_table::size() - 1;
        auto header = table_.get(dynamic_index);
        if (header.has_value())
        {
            return std::move(header.value());
        }

        return error_info(107, "Invalid dynamic table index", "hpack");
    }

    // Huffman coding (basic stub implementation)
    namespace huffman
    {
        auto encode(std::string_view input) -> std::vector<uint8_t>
        {
            // Stub: just return the input as-is
            return std::vector<uint8_t>(input.begin(), input.end());
        }

        auto decode(std::span<const uint8_t> data) -> Result<std::string>
        {
            // Stub: just return the data as-is
            return std::string(data.begin(), data.end());
        }

        auto encoded_size(std::string_view input) -> size_t
        {
            // Stub: return input size
            return input.size();
        }
    }

} // namespace kcenon::network::protocols::http2
