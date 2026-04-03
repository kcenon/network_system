// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file frame.h
 * @brief gRPC message framing and serialization.
 *
 */

#pragma once

/**
 * @file frame.h
 * @brief gRPC message framing (length-prefixed binary protocol)
 *
 * Implements gRPC's 5-byte header framing for encoding and decoding
 * length-prefixed messages over HTTP/2 streams.
 */

#include "kcenon/network/detail/utils/result_types.h"
#include <cstdint>
#include <span>
#include <vector>

namespace kcenon::network::protocols::grpc
{
    /*!
     * \brief gRPC message header size (5 bytes)
     *
     * gRPC messages are prefixed with a 5-byte header:
     * - 1 byte: Compressed flag (0 = uncompressed, 1 = compressed)
     * - 4 bytes: Message length (big-endian)
     */
    constexpr size_t grpc_header_size = 5;

    /*!
     * \brief Maximum gRPC message size (default 4MB)
     */
    constexpr size_t default_max_message_size = 4 * 1024 * 1024;

    /*!
     * \struct grpc_message
     * \brief gRPC message with compression flag and payload
     *
     * Represents a gRPC message as transmitted over HTTP/2.
     * Format: | Compressed-Flag (1 byte) | Message-Length (4 bytes) | Message |
     */
    struct grpc_message
    {
        bool compressed = false;      //!< Whether payload is compressed
        std::vector<uint8_t> data;    //!< Message payload

        /*!
         * \brief Default constructor
         */
        grpc_message() = default;

        /*!
         * \brief Construct with data
         * \param payload Message data
         * \param is_compressed Whether data is compressed
         */
        explicit grpc_message(std::vector<uint8_t> payload, bool is_compressed = false)
            : compressed(is_compressed), data(std::move(payload)) {}

        /*!
         * \brief Parse gRPC message from raw bytes
         * \param input Raw byte data (header + payload)
         * \return Result containing parsed message or error
         *
         * Parses a gRPC length-prefixed message from the input buffer.
         */
        static auto parse(std::span<const uint8_t> input) -> Result<grpc_message>;

        /*!
         * \brief Serialize message to bytes with length prefix
         * \return Byte vector containing header + payload
         *
         * Serializes the message with a 5-byte header prefix.
         */
        auto serialize() const -> std::vector<uint8_t>;

        /*!
         * \brief Get total serialized size including header
         * \return Total size in bytes
         */
        auto serialized_size() const -> size_t
        {
            return grpc_header_size + data.size();
        }

        /*!
         * \brief Check if message is empty
         * \return True if data is empty
         */
        auto empty() const -> bool { return data.empty(); }

        /*!
         * \brief Get message size (payload only)
         * \return Payload size in bytes
         */
        auto size() const -> size_t { return data.size(); }
    };

    /*!
     * \brief gRPC content-type header value
     */
    constexpr const char* grpc_content_type = "application/grpc";

    /*!
     * \brief gRPC content-type with proto encoding
     */
    constexpr const char* grpc_content_type_proto = "application/grpc+proto";

    /*!
     * \brief gRPC trailing header names
     */
    namespace trailer_names
    {
        constexpr const char* grpc_status = "grpc-status";
        constexpr const char* grpc_message = "grpc-message";
        constexpr const char* grpc_status_details = "grpc-status-details-bin";
    }

    /*!
     * \brief gRPC request header names
     */
    namespace header_names
    {
        constexpr const char* te = "te";
        constexpr const char* content_type = "content-type";
        constexpr const char* grpc_encoding = "grpc-encoding";
        constexpr const char* grpc_accept_encoding = "grpc-accept-encoding";
        constexpr const char* grpc_timeout = "grpc-timeout";
        constexpr const char* user_agent = "user-agent";
    }

    /*!
     * \brief gRPC compression algorithms
     */
    namespace compression
    {
        constexpr const char* identity = "identity";
        constexpr const char* deflate = "deflate";
        constexpr const char* gzip = "gzip";
    }

    /*!
     * \brief Parse timeout string (e.g., "10S", "100m", "1000u")
     * \param timeout_str Timeout string with unit suffix
     * \return Timeout in milliseconds, or 0 if invalid
     *
     * Supported units:
     * - H: Hours
     * - M: Minutes
     * - S: Seconds
     * - m: Milliseconds
     * - u: Microseconds
     * - n: Nanoseconds
     */
    auto parse_timeout(std::string_view timeout_str) -> uint64_t;

    /*!
     * \brief Format timeout as gRPC timeout string
     * \param timeout_ms Timeout in milliseconds
     * \return Formatted timeout string
     */
    auto format_timeout(uint64_t timeout_ms) -> std::string;

} // namespace kcenon::network::protocols::grpc
