// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file message_validator.h
 * @brief Network message validation for buffer overflow prevention
 *
 * @details Provides input validation utilities to prevent buffer overflow attacks
 * and other input-based vulnerabilities (CWE-119, CWE-20).
 *
 * Security features:
 * - Message size limits to prevent memory exhaustion
 * - Safe buffer copy operations
 * - HTTP header validation
 * - WebSocket frame validation
 * - NULL byte injection prevention
 *
 * @example
 * @code
 * // Validate incoming message size
 * if (!message_validator::validate_size(data.size())) {
 *     return error::message_too_large;
 * }
 *
 * // Safe buffer copy
 * std::vector<uint8_t> buffer(1024);
 * message_validator::safe_copy(buffer.data(), buffer.size(), source, source_len);
 * @endcode
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <stdexcept>
#include <algorithm>

namespace kcenon::network {

/**
 * @brief Configurable message size limits
 *
 * These limits can be adjusted based on deployment requirements.
 * Default values are set for general-purpose network applications.
 */
struct message_limits {
    /// Maximum allowed message size (default: 16MB)
    static constexpr size_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;

    /// Maximum HTTP header size (default: 8KB - Apache default)
    static constexpr size_t MAX_HEADER_SIZE = 8192;

    /// Maximum WebSocket frame payload (default: 1MB)
    static constexpr size_t MAX_WEBSOCKET_FRAME = 1 * 1024 * 1024;

    /// Maximum HTTP request line length (default: 8KB)
    static constexpr size_t MAX_HTTP_LINE = 8192;

    /// Maximum number of HTTP headers (default: 100)
    static constexpr size_t MAX_HEADER_COUNT = 100;

    /// Maximum URL length (default: 2KB)
    static constexpr size_t MAX_URL_LENGTH = 2048;

    /// Maximum cookie size (default: 4KB)
    static constexpr size_t MAX_COOKIE_SIZE = 4096;
};

/**
 * @brief Result type for validation operations
 */
enum class validation_result {
    ok,                     ///< Validation passed
    size_exceeded,          ///< Size limit exceeded
    null_byte_detected,     ///< NULL byte found in string
    invalid_format,         ///< Invalid data format
    invalid_character,      ///< Invalid character detected
    header_count_exceeded   ///< Too many headers
};

/**
 * @brief Convert validation result to string
 */
inline const char* to_string(validation_result result) {
    switch (result) {
        case validation_result::ok:
            return "ok";
        case validation_result::size_exceeded:
            return "size_exceeded";
        case validation_result::null_byte_detected:
            return "null_byte_detected";
        case validation_result::invalid_format:
            return "invalid_format";
        case validation_result::invalid_character:
            return "invalid_character";
        case validation_result::header_count_exceeded:
            return "header_count_exceeded";
        default:
            return "unknown";
    }
}

/**
 * @brief Message validator for network input validation
 *
 * @details Provides static methods for validating network input to prevent
 * buffer overflow and other input-related vulnerabilities.
 *
 * ### Thread Safety
 * All methods are stateless and thread-safe.
 *
 * ### Security Considerations
 * - Always validate input size before processing
 * - Use safe_copy for buffer operations
 * - Check for NULL bytes in strings from untrusted sources
 */
class message_validator {
public:
    /**
     * @brief Validate message size against limit
     *
     * @param size Size to validate
     * @param max_size Maximum allowed size (default: MAX_MESSAGE_SIZE)
     * @return true if size is within limit, false otherwise
     */
    [[nodiscard]] static bool validate_size(
            size_t size,
            size_t max_size = message_limits::MAX_MESSAGE_SIZE) noexcept {
        return size <= max_size;
    }

    /**
     * @brief Validate and throw if size exceeds limit
     *
     * @param size Size to validate
     * @param max_size Maximum allowed size
     * @throws std::length_error if size exceeds limit
     */
    static void validate_size_or_throw(
            size_t size,
            size_t max_size = message_limits::MAX_MESSAGE_SIZE) {
        if (size > max_size) {
            throw std::length_error(
                "Message size " + std::to_string(size) +
                " exceeds limit " + std::to_string(max_size));
        }
    }

    /**
     * @brief Safe buffer copy with size validation
     *
     * Copies data from source to destination, ensuring no buffer overflow.
     * Copies the minimum of dest_size and src_size bytes.
     *
     * @param dest Destination buffer
     * @param dest_size Size of destination buffer
     * @param src Source buffer
     * @param src_size Size of source data
     * @return Number of bytes actually copied
     */
    [[nodiscard]] static size_t safe_copy(
            void* dest,
            size_t dest_size,
            const void* src,
            size_t src_size) noexcept {
        if (!dest || !src || dest_size == 0 || src_size == 0) {
            return 0;
        }

        size_t copy_size = std::min(dest_size, src_size);
        std::memcpy(dest, src, copy_size);
        return copy_size;
    }

    /**
     * @brief Safe string copy with null termination
     *
     * @param dest Destination buffer
     * @param dest_size Size of destination buffer
     * @param src Source string
     * @return Number of characters copied (excluding null terminator)
     */
    [[nodiscard]] static size_t safe_strcpy(
            char* dest,
            size_t dest_size,
            const char* src) noexcept {
        if (!dest || dest_size == 0 || !src) {
            return 0;
        }

        size_t src_len = std::strlen(src);
        size_t copy_len = std::min(dest_size - 1, src_len);

        std::memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';

        return copy_len;
    }

    /**
     * @brief Validate HTTP header
     *
     * Checks for:
     * - Size within limits
     * - No NULL bytes
     * - Valid header format
     *
     * @param header Header string to validate
     * @return validation_result indicating success or failure type
     */
    [[nodiscard]] static validation_result validate_http_header(
            std::string_view header) noexcept {
        // Check size
        if (header.size() > message_limits::MAX_HEADER_SIZE) {
            return validation_result::size_exceeded;
        }

        // Check for NULL bytes (NULL byte injection attack)
        if (header.find('\0') != std::string_view::npos) {
            return validation_result::null_byte_detected;
        }

        // Check for invalid control characters (except \r\n\t)
        for (char c : header) {
            if (c < 0x20 && c != '\r' && c != '\n' && c != '\t') {
                return validation_result::invalid_character;
            }
        }

        return validation_result::ok;
    }

    /**
     * @brief Validate HTTP header count
     *
     * @param count Number of headers
     * @return true if within limit
     */
    [[nodiscard]] static bool validate_header_count(size_t count) noexcept {
        return count <= message_limits::MAX_HEADER_COUNT;
    }

    /**
     * @brief Validate WebSocket frame payload size
     *
     * @param payload_length Payload length to validate
     * @param max_size Maximum allowed size (default: MAX_WEBSOCKET_FRAME)
     * @return true if within limit
     */
    [[nodiscard]] static bool validate_websocket_frame(
            size_t payload_length,
            size_t max_size = message_limits::MAX_WEBSOCKET_FRAME) noexcept {
        return payload_length <= max_size;
    }

    /**
     * @brief Validate URL length
     *
     * @param url URL to validate
     * @return validation_result
     */
    [[nodiscard]] static validation_result validate_url(
            std::string_view url) noexcept {
        if (url.size() > message_limits::MAX_URL_LENGTH) {
            return validation_result::size_exceeded;
        }

        if (url.find('\0') != std::string_view::npos) {
            return validation_result::null_byte_detected;
        }

        return validation_result::ok;
    }

    /**
     * @brief Check if data contains potential injection patterns
     *
     * Basic check for common injection patterns. Should be used in
     * conjunction with proper input sanitization.
     *
     * @param data Data to check
     * @return true if suspicious pattern detected
     */
    [[nodiscard]] static bool contains_suspicious_pattern(
            std::string_view data) noexcept {
        // Check for NULL byte injection
        if (data.find('\0') != std::string_view::npos) {
            return true;
        }

        // Check for HTTP response splitting
        if (data.find("\r\n\r\n") != std::string_view::npos) {
            return true;
        }

        return false;
    }

    /**
     * @brief Sanitize string by removing control characters
     *
     * @param input String to sanitize
     * @return Sanitized string with control characters removed
     */
    [[nodiscard]] static std::string sanitize_string(std::string_view input) {
        std::string result;
        result.reserve(input.size());

        for (char c : input) {
            // Keep printable ASCII and common whitespace
            if (c >= 0x20 || c == '\t' || c == '\n' || c == '\r') {
                result += c;
            }
        }

        return result;
    }

    /**
     * @brief Calculate safe buffer size for operations
     *
     * Returns a buffer size that is safe to use, capped at max_size.
     *
     * @param requested_size Requested buffer size
     * @param max_size Maximum allowed size
     * @return Safe buffer size
     */
    [[nodiscard]] static size_t safe_buffer_size(
            size_t requested_size,
            size_t max_size = message_limits::MAX_MESSAGE_SIZE) noexcept {
        return std::min(requested_size, max_size);
    }
};

} // namespace kcenon::network
