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

#pragma once

#include "kcenon/network/utils/result_types.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <optional>
#include <span>
#include <map>

namespace kcenon::network::protocols::http2
{
    /*!
     * \struct http_header
     * \brief HTTP header name-value pair
     */
    struct http_header
    {
        std::string name;   //!< Header name
        std::string value;  //!< Header value

        http_header() = default;
        http_header(std::string n, std::string v)
            : name(std::move(n)), value(std::move(v)) {}

        auto size() const -> size_t
        {
            return name.size() + value.size() + 32;  // RFC 7541: 32 byte overhead
        }

        bool operator==(const http_header& other) const
        {
            return name == other.name && value == other.value;
        }
    };

    /*!
     * \class static_table
     * \brief HPACK static table (RFC 7541 Appendix A)
     *
     * The static table consists of 61 predefined common header fields.
     */
    class static_table
    {
    public:
        /*!
         * \brief Get static table entry by index
         * \param index Index (1-61, 0 is invalid)
         * \return Header entry if found
         */
        static auto get(size_t index) -> std::optional<http_header>;

        /*!
         * \brief Find index of header in static table
         * \param name Header name
         * \param value Header value (empty to match name only)
         * \return Index if found, 0 if not found
         */
        static auto find(std::string_view name, std::string_view value = "")
            -> size_t;

        /*!
         * \brief Get static table size
         * \return Number of entries (61)
         */
        static constexpr auto size() -> size_t { return 61; }
    };

    /*!
     * \class dynamic_table
     * \brief HPACK dynamic table for header compression
     *
     * The dynamic table maintains recently used headers for compression.
     * Entries are added at the beginning and evicted from the end when
     * the table exceeds max_size.
     */
    class dynamic_table
    {
    public:
        /*!
         * \brief Construct dynamic table with max size
         * \param max_size Maximum table size in bytes (default 4096)
         */
        explicit dynamic_table(size_t max_size = 4096);

        /*!
         * \brief Insert header at beginning of table
         * \param name Header name
         * \param value Header value
         */
        auto insert(std::string_view name, std::string_view value) -> void;

        /*!
         * \brief Get header by dynamic table index
         * \param index Dynamic table index (0-based)
         * \return Header if found
         */
        auto get(size_t index) const -> std::optional<http_header>;

        /*!
         * \brief Find header in dynamic table
         * \param name Header name
         * \param value Header value (empty to match name only)
         * \return Dynamic table index if found, or empty
         */
        auto find(std::string_view name, std::string_view value = "") const
            -> std::optional<size_t>;

        /*!
         * \brief Set maximum table size
         * \param size New maximum size in bytes
         */
        auto set_max_size(size_t size) -> void;

        /*!
         * \brief Get current table size
         * \return Current size in bytes
         */
        auto current_size() const -> size_t;

        /*!
         * \brief Get maximum table size
         * \return Maximum size in bytes
         */
        auto max_size() const -> size_t;

        /*!
         * \brief Get number of entries
         * \return Entry count
         */
        auto entry_count() const -> size_t;

        /*!
         * \brief Clear all entries
         */
        auto clear() -> void;

    private:
        auto evict_to_size(size_t target_size) -> void;

        std::deque<http_header> entries_;
        size_t current_size_ = 0;
        size_t max_size_;
    };

    /*!
     * \class hpack_encoder
     * \brief HPACK header encoder (RFC 7541)
     *
     * Encodes HTTP headers using HPACK compression with dynamic table.
     */
    class hpack_encoder
    {
    public:
        /*!
         * \brief Construct encoder with max table size
         * \param max_table_size Maximum dynamic table size (default 4096)
         */
        explicit hpack_encoder(size_t max_table_size = 4096);

        /*!
         * \brief Encode headers to HPACK binary format
         * \param headers Headers to encode
         * \return Encoded bytes
         */
        auto encode(const std::vector<http_header>& headers) -> std::vector<uint8_t>;

        /*!
         * \brief Set maximum dynamic table size
         * \param size New maximum size
         */
        auto set_max_table_size(size_t size) -> void;

        /*!
         * \brief Get current dynamic table size
         * \return Current size in bytes
         */
        auto table_size() const -> size_t;

    private:
        auto encode_integer(uint64_t value, uint8_t prefix_bits) -> std::vector<uint8_t>;
        auto encode_string(std::string_view str, bool huffman = false) -> std::vector<uint8_t>;
        auto encode_indexed(size_t index) -> std::vector<uint8_t>;
        auto encode_literal_with_indexing(std::string_view name, std::string_view value)
            -> std::vector<uint8_t>;
        auto encode_literal_with_indexing(size_t name_index, std::string_view value)
            -> std::vector<uint8_t>;
        auto encode_literal_without_indexing(std::string_view name, std::string_view value)
            -> std::vector<uint8_t>;
        auto encode_literal_without_indexing(size_t name_index, std::string_view value)
            -> std::vector<uint8_t>;

        dynamic_table table_;
    };

    /*!
     * \class hpack_decoder
     * \brief HPACK header decoder (RFC 7541)
     *
     * Decodes HPACK compressed headers with dynamic table.
     */
    class hpack_decoder
    {
    public:
        /*!
         * \brief Construct decoder with max table size
         * \param max_table_size Maximum dynamic table size (default 4096)
         */
        explicit hpack_decoder(size_t max_table_size = 4096);

        /*!
         * \brief Decode HPACK binary to headers
         * \param data Encoded HPACK data
         * \return Decoded headers or error
         */
        auto decode(std::span<const uint8_t> data) -> Result<std::vector<http_header>>;

        /*!
         * \brief Set maximum dynamic table size
         * \param size New maximum size
         */
        auto set_max_table_size(size_t size) -> void;

        /*!
         * \brief Get current dynamic table size
         * \return Current size in bytes
         */
        auto table_size() const -> size_t;

    private:
        auto decode_integer(std::span<const uint8_t>& data, uint8_t prefix_bits)
            -> Result<uint64_t>;
        auto decode_string(std::span<const uint8_t>& data) -> Result<std::string>;
        auto get_indexed_header(size_t index) const -> Result<http_header>;

        dynamic_table table_;
    };

    /*!
     * \namespace huffman
     * \brief Huffman coding for HPACK string compression
     *
     * Implements the static Huffman code defined in RFC 7541 Appendix B.
     */
    namespace huffman
    {
        /*!
         * \brief Encode string using Huffman coding
         * \param input String to encode
         * \return Encoded bytes
         */
        auto encode(std::string_view input) -> std::vector<uint8_t>;

        /*!
         * \brief Decode Huffman encoded string
         * \param data Encoded bytes
         * \return Decoded string or error
         */
        auto decode(std::span<const uint8_t> data) -> Result<std::string>;

        /*!
         * \brief Get encoded size for string
         * \param input String to measure
         * \return Size in bytes if Huffman encoded
         */
        auto encoded_size(std::string_view input) -> size_t;
    }

} // namespace kcenon::network::protocols::http2
