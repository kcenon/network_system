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

#pragma once

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
