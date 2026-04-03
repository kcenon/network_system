// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file connection_id.h
 * @brief QUIC connection identifier type.
 *
 */

#pragma once

/**
 * @file connection_id.h
 * @brief QUIC Connection ID implementation (RFC 9000 Section 5.1)
 *
 * Provides the connection_id class for generating, comparing, and
 * managing QUIC connection identifiers (0-20 bytes).
 */

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace kcenon::network::protocols::quic
{

/*!
 * \class connection_id
 * \brief QUIC Connection ID (RFC 9000 Section 5.1)
 *
 * A connection ID is used to identify a QUIC connection. Connection IDs are
 * used to route packets to the correct connection and to allow endpoints to
 * change network addresses without breaking the connection.
 *
 * Key properties (RFC 9000):
 * - Length: 0 to 20 bytes (max_length = 20)
 * - An endpoint generates connection IDs for its peer to use
 * - Zero-length connection IDs are valid but limit connection migration
 * - Connection IDs should be unpredictable to avoid linkability
 */
class connection_id
{
public:
    //! Maximum length of a connection ID (RFC 9000)
    static constexpr size_t max_length = 20;

    /*!
     * \brief Default constructor creates an empty connection ID
     */
    connection_id() = default;

    /*!
     * \brief Construct from raw bytes
     * \param data Span of bytes (max 20)
     * \note If data is longer than max_length, only the first max_length bytes
     *       are used
     */
    explicit connection_id(std::span<const uint8_t> data);

    /*!
     * \brief Generate a random connection ID
     * \param length Desired length (1-20, clamped to max_length)
     * \return Randomly generated connection ID
     */
    [[nodiscard]] static auto generate(size_t length = 8) -> connection_id;

    /*!
     * \brief Get the raw bytes of the connection ID
     * \return Span of connection ID bytes
     */
    [[nodiscard]] auto data() const -> std::span<const uint8_t>;

    /*!
     * \brief Get the length of the connection ID
     * \return Length in bytes (0-20)
     */
    [[nodiscard]] auto length() const noexcept -> size_t;

    /*!
     * \brief Check if the connection ID is empty
     * \return true if length is 0
     */
    [[nodiscard]] auto empty() const noexcept -> bool;

    /*!
     * \brief Equality comparison
     * \param other Connection ID to compare with
     * \return true if both IDs are equal
     */
    [[nodiscard]] auto operator==(const connection_id& other) const noexcept -> bool;

    /*!
     * \brief Inequality comparison
     * \param other Connection ID to compare with
     * \return true if IDs are not equal
     */
    [[nodiscard]] auto operator!=(const connection_id& other) const noexcept -> bool;

    /*!
     * \brief Less-than comparison for use in ordered containers
     * \param other Connection ID to compare with
     * \return true if this ID is less than other
     */
    [[nodiscard]] auto operator<(const connection_id& other) const noexcept -> bool;

    /*!
     * \brief Convert to hexadecimal string for debugging
     * \return Hexadecimal representation of the connection ID
     */
    [[nodiscard]] auto to_string() const -> std::string;

private:
    std::array<uint8_t, max_length> data_{};
    uint8_t length_{0};
};

} // namespace kcenon::network::protocols::quic
