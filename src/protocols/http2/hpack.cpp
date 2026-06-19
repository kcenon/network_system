// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

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

        if (huffman)
        {
            // Only use Huffman coding when it actually shrinks the string;
            // RFC 7541 5.2 lets the encoder choose per string.
            auto encoded = huffman::encode(str);
            if (encoded.size() < str.size())
            {
                auto length_bytes = encode_integer(encoded.size(), 7);
                length_bytes[0] |= 0x80;  // Set H bit
                result.insert(result.end(), length_bytes.begin(), length_bytes.end());
                result.insert(result.end(), encoded.begin(), encoded.end());
                return result;
            }
        }

        // Raw literal: length with the H bit clear, then the octets verbatim.
        auto length_bytes = encode_integer(str.size(), 7);
        result.insert(result.end(), length_bytes.begin(), length_bytes.end());
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
            auto decoded = huffman::decode(data.subspan(0, length));
            if (decoded.is_err())
            {
                return decoded.error();
            }
            result = std::move(decoded.value());
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

    // Huffman coding (RFC 7541 Appendix B)
    namespace huffman
    {
        namespace
        {
            // RFC 7541 Appendix B static Huffman code: {code, bit length} per
            // symbol. Index 0..255 are octet values; index 256 is the EOS symbol.
            struct huffman_symbol
            {
                uint32_t code;
                uint8_t bits;
            };

            constexpr huffman_symbol kHuffmanTable[257] = {
                {0x1ff8u, 13},    {0x7fffd8u, 23},  {0xfffffe2u, 28}, {0xfffffe3u, 28},
                {0xfffffe4u, 28}, {0xfffffe5u, 28}, {0xfffffe6u, 28}, {0xfffffe7u, 28},
                {0xfffffe8u, 28}, {0xffffeau, 24},  {0x3ffffffcu, 30},{0xfffffe9u, 28},
                {0xfffffeau, 28}, {0x3ffffffdu, 30},{0xfffffebu, 28}, {0xfffffecu, 28},
                {0xfffffedu, 28}, {0xfffffeeu, 28}, {0xfffffefu, 28}, {0xffffff0u, 28},
                {0xffffff1u, 28}, {0xffffff2u, 28}, {0x3ffffffeu, 30},{0xffffff3u, 28},
                {0xffffff4u, 28}, {0xffffff5u, 28}, {0xffffff6u, 28}, {0xffffff7u, 28},
                {0xffffff8u, 28}, {0xffffff9u, 28}, {0xffffffau, 28}, {0xffffffbu, 28},
                {0x14u, 6},       {0x3f8u, 10},     {0x3f9u, 10},     {0xffau, 12},
                {0x1ff9u, 13},    {0x15u, 6},       {0xf8u, 8},       {0x7fau, 11},
                {0x3fau, 10},     {0x3fbu, 10},     {0xf9u, 8},       {0x7fbu, 11},
                {0xfau, 8},       {0x16u, 6},       {0x17u, 6},       {0x18u, 6},
                {0x0u, 5},        {0x1u, 5},        {0x2u, 5},        {0x19u, 6},
                {0x1au, 6},       {0x1bu, 6},       {0x1cu, 6},       {0x1du, 6},
                {0x1eu, 6},       {0x1fu, 6},       {0x5cu, 7},       {0xfbu, 8},
                {0x7ffcu, 15},    {0x20u, 6},       {0xffbu, 12},     {0x3fcu, 10},
                {0x1ffau, 13},    {0x21u, 6},       {0x5du, 7},       {0x5eu, 7},
                {0x5fu, 7},       {0x60u, 7},       {0x61u, 7},       {0x62u, 7},
                {0x63u, 7},       {0x64u, 7},       {0x65u, 7},       {0x66u, 7},
                {0x67u, 7},       {0x68u, 7},       {0x69u, 7},       {0x6au, 7},
                {0x6bu, 7},       {0x6cu, 7},       {0x6du, 7},       {0x6eu, 7},
                {0x6fu, 7},       {0x70u, 7},       {0x71u, 7},       {0x72u, 7},
                {0xfcu, 8},       {0x73u, 7},       {0xfdu, 8},       {0x1ffbu, 13},
                {0x7fff0u, 19},   {0x1ffcu, 13},    {0x3ffcu, 14},    {0x22u, 6},
                {0x7ffdu, 15},    {0x3u, 5},        {0x23u, 6},       {0x4u, 5},
                {0x24u, 6},       {0x5u, 5},        {0x25u, 6},       {0x26u, 6},
                {0x27u, 6},       {0x6u, 5},        {0x74u, 7},       {0x75u, 7},
                {0x28u, 6},       {0x29u, 6},       {0x2au, 6},       {0x7u, 5},
                {0x2bu, 6},       {0x76u, 7},       {0x2cu, 6},       {0x8u, 5},
                {0x9u, 5},        {0x2du, 6},       {0x77u, 7},       {0x78u, 7},
                {0x79u, 7},       {0x7au, 7},       {0x7bu, 7},       {0x7ffeu, 15},
                {0x7fcu, 11},     {0x3ffdu, 14},    {0x1ffdu, 13},    {0xffffffcu, 28},
                {0xfffe6u, 20},   {0x3fffd2u, 22},  {0xfffe7u, 20},   {0xfffe8u, 20},
                {0x3fffd3u, 22},  {0x3fffd4u, 22},  {0x3fffd5u, 22},  {0x7fffd9u, 23},
                {0x3fffd6u, 22},  {0x7fffdau, 23},  {0x7fffdbu, 23},  {0x7fffdcu, 23},
                {0x7fffddu, 23},  {0x7fffdeu, 23},  {0xffffebu, 24},  {0x7fffdfu, 23},
                {0xffffecu, 24},  {0xffffedu, 24},  {0x3fffd7u, 22},  {0x7fffe0u, 23},
                {0xffffeeu, 24},  {0x7fffe1u, 23},  {0x7fffe2u, 23},  {0x7fffe3u, 23},
                {0x7fffe4u, 23},  {0x1fffdcu, 21},  {0x3fffd8u, 22},  {0x7fffe5u, 23},
                {0x3fffd9u, 22},  {0x7fffe6u, 23},  {0x7fffe7u, 23},  {0xffffefu, 24},
                {0x3fffdau, 22},  {0x1fffddu, 21},  {0xfffe9u, 20},   {0x3fffdbu, 22},
                {0x3fffdcu, 22},  {0x7fffe8u, 23},  {0x7fffe9u, 23},  {0x1fffdeu, 21},
                {0x7fffeau, 23},  {0x3fffddu, 22},  {0x3fffdeu, 22},  {0xfffff0u, 24},
                {0x1fffdfu, 21},  {0x3fffdfu, 22},  {0x7fffebu, 23},  {0x7fffecu, 23},
                {0x1fffe0u, 21},  {0x1fffe1u, 21},  {0x3fffe0u, 22},  {0x1fffe2u, 21},
                {0x7fffedu, 23},  {0x3fffe1u, 22},  {0x7fffeeu, 23},  {0x7fffefu, 23},
                {0xfffeau, 20},   {0x3fffe2u, 22},  {0x3fffe3u, 22},  {0x3fffe4u, 22},
                {0x7ffff0u, 23},  {0x3fffe5u, 22},  {0x3fffe6u, 22},  {0x7ffff1u, 23},
                {0x3ffffe0u, 26}, {0x3ffffe1u, 26}, {0xfffebu, 20},   {0x7fff1u, 19},
                {0x3fffe7u, 22},  {0x7ffff2u, 23},  {0x3fffe8u, 22},  {0x1ffffecu, 25},
                {0x3ffffe2u, 26}, {0x3ffffe3u, 26}, {0x3ffffe4u, 26}, {0x7ffffdeu, 27},
                {0x7ffffdfu, 27}, {0x3ffffe5u, 26}, {0xfffff1u, 24},  {0x1ffffedu, 25},
                {0x7fff2u, 19},   {0x1fffe3u, 21},  {0x3ffffe6u, 26}, {0x7ffffe0u, 27},
                {0x7ffffe1u, 27}, {0x3ffffe7u, 26}, {0x7ffffe2u, 27}, {0xfffff2u, 24},
                {0x1fffe4u, 21},  {0x1fffe5u, 21},  {0x3ffffe8u, 26}, {0x3ffffe9u, 26},
                {0xffffffdu, 28}, {0x7ffffe3u, 27}, {0x7ffffe4u, 27}, {0x7ffffe5u, 27},
                {0xfffecu, 20},   {0xfffff3u, 24},  {0xfffedu, 20},   {0x1fffe6u, 21},
                {0x3fffe9u, 22},  {0x1fffe7u, 21},  {0x1fffe8u, 21},  {0x7ffff3u, 23},
                {0x3fffeau, 22},  {0x3fffebu, 22},  {0x1ffffeeu, 25}, {0x1ffffefu, 25},
                {0xfffff4u, 24},  {0xfffff5u, 24},  {0x3ffffeau, 26}, {0x7ffff4u, 23},
                {0x3ffffebu, 26}, {0x7ffffe6u, 27}, {0x3ffffecu, 26}, {0x3ffffedu, 26},
                {0x7ffffe7u, 27}, {0x7ffffe8u, 27}, {0x7ffffe9u, 27}, {0x7ffffeau, 27},
                {0x7ffffebu, 27}, {0xffffffeu, 28}, {0x7ffffecu, 27}, {0x7ffffedu, 27},
                {0x7ffffeeu, 27}, {0x7ffffefu, 27}, {0x7fffff0u, 27}, {0x3ffffeeu, 26},
                {0x3fffffffu, 30},  // 256: EOS
            };

            constexpr int kEosSymbol = 256;

            // Decoding trie node: child indices for bit 0 / bit 1 (-1 if absent)
            // and the decoded symbol at a leaf (-1 for internal nodes).
            struct decode_node
            {
                int children[2] = {-1, -1};
                int symbol = -1;
            };

            auto build_decode_tree() -> std::vector<decode_node>
            {
                std::vector<decode_node> nodes(1);  // root at index 0
                for (int sym = 0; sym < 257; ++sym)
                {
                    const auto& entry = kHuffmanTable[sym];
                    int cur = 0;
                    for (int b = static_cast<int>(entry.bits) - 1; b >= 0; --b)
                    {
                        int bit = static_cast<int>((entry.code >> b) & 1u);
                        int next = nodes[cur].children[bit];
                        if (next == -1)
                        {
                            nodes.push_back(decode_node{});
                            next = static_cast<int>(nodes.size()) - 1;
                            nodes[cur].children[bit] = next;
                        }
                        cur = next;
                    }
                    nodes[cur].symbol = sym;
                }
                return nodes;
            }
        }  // namespace

        auto encode(std::string_view input) -> std::vector<uint8_t>
        {
            std::vector<uint8_t> out;
            out.reserve(input.size());

            uint64_t buffer = 0;
            int buffer_bits = 0;

            for (unsigned char ch : input)
            {
                const auto& entry = kHuffmanTable[ch];
                buffer = (buffer << entry.bits) | entry.code;
                buffer_bits += entry.bits;

                while (buffer_bits >= 8)
                {
                    buffer_bits -= 8;
                    out.push_back(static_cast<uint8_t>(buffer >> buffer_bits));
                }
                // Keep only the still-pending low bits to avoid emitting stale data.
                buffer &= (1ull << buffer_bits) - 1;
            }

            if (buffer_bits > 0)
            {
                // Pad the final byte with the most significant bits of EOS (all 1s).
                int pad = 8 - buffer_bits;
                out.push_back(static_cast<uint8_t>((buffer << pad) | ((1u << pad) - 1)));
            }

            return out;
        }

        auto decode(std::span<const uint8_t> data) -> Result<std::string>
        {
            static const std::vector<decode_node> tree = build_decode_tree();

            std::string out;
            int node = 0;
            int partial_bits = 0;
            bool partial_all_ones = true;

            for (uint8_t byte : data)
            {
                for (int i = 7; i >= 0; --i)
                {
                    int bit = (byte >> i) & 1;
                    node = tree[node].children[bit];
                    if (node == -1)
                    {
                        return error_info(108, "Invalid Huffman code", "hpack");
                    }
                    ++partial_bits;
                    if (bit == 0)
                    {
                        partial_all_ones = false;
                    }

                    if (tree[node].symbol >= 0)
                    {
                        if (tree[node].symbol == kEosSymbol)
                        {
                            // RFC 7541 5.2: EOS in the encoded data is an error.
                            return error_info(108, "EOS symbol in Huffman-encoded data",
                                              "hpack");
                        }
                        out.push_back(static_cast<char>(tree[node].symbol));
                        node = 0;
                        partial_bits = 0;
                        partial_all_ones = true;
                    }
                }
            }

            // RFC 7541 5.2: any trailing bits must be valid EOS padding —
            // at most 7 bits, all set to 1.
            if (node != 0 && (partial_bits > 7 || !partial_all_ones))
            {
                return error_info(108, "Invalid Huffman padding", "hpack");
            }

            return out;
        }

        auto encoded_size(std::string_view input) -> size_t
        {
            size_t bits = 0;
            for (unsigned char ch : input)
            {
                bits += kHuffmanTable[ch].bits;
            }
            return (bits + 7) / 8;
        }
    }

} // namespace kcenon::network::protocols::http2
