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
#include <string>

namespace kcenon::network::protocols::quic
{

// ============================================================================
// Constants
// ============================================================================

//! AES-128-GCM key size in bytes
constexpr size_t aes_128_key_size = 16;

//! AES-256-GCM key size in bytes
constexpr size_t aes_256_key_size = 32;

//! AEAD IV/nonce size in bytes
constexpr size_t aead_iv_size = 12;

//! AEAD authentication tag size in bytes
constexpr size_t aead_tag_size = 16;

//! Traffic secret size (SHA-256 output)
constexpr size_t secret_size = 32;

//! Header protection key size for AES-128
constexpr size_t hp_key_size = 16;

//! Header protection sample size
constexpr size_t hp_sample_size = 16;

// ============================================================================
// Encryption Levels
// ============================================================================

/*!
 * \enum encryption_level
 * \brief QUIC encryption levels (RFC 9001 Section 4)
 *
 * QUIC uses four encryption levels, each with different key material:
 * - Initial: Keys derived from Destination Connection ID
 * - Handshake: Keys from TLS handshake
 * - Zero-RTT (0-RTT): Early data keys (optional)
 * - Application (1-RTT): Post-handshake keys
 */
enum class encryption_level : uint8_t
{
    initial = 0,      //!< Initial encryption (derived from DCID)
    handshake = 1,    //!< Handshake encryption
    zero_rtt = 2,     //!< 0-RTT early data encryption
    application = 3   //!< 1-RTT application data encryption
};

/*!
 * \brief Convert encryption level to string for debugging
 * \param level Encryption level to convert
 * \return String representation of the level
 */
[[nodiscard]] auto encryption_level_to_string(encryption_level level) -> std::string;

/*!
 * \brief Get the encryption level count
 * \return Number of encryption levels (4)
 */
[[nodiscard]] constexpr auto encryption_level_count() noexcept -> size_t
{
    return 4;
}

// ============================================================================
// Key Structures
// ============================================================================

/*!
 * \struct quic_keys
 * \brief QUIC encryption keys for a single encryption level (RFC 9001 Section 5)
 *
 * Contains all the cryptographic material needed for packet protection:
 * - Traffic secret: Used for key derivation and key updates
 * - AEAD key: Used for payload encryption (AES-128-GCM or AES-256-GCM)
 * - IV: Initialization vector XORed with packet number for nonce
 * - HP key: Header protection key for protecting header bytes
 */
struct quic_keys
{
    //! Traffic secret (used for key updates)
    std::array<uint8_t, secret_size> secret{};

    //! AEAD encryption key (AES-128-GCM by default)
    std::array<uint8_t, aes_128_key_size> key{};

    //! AEAD initialization vector
    std::array<uint8_t, aead_iv_size> iv{};

    //! Header protection key
    std::array<uint8_t, hp_key_size> hp_key{};

    /*!
     * \brief Check if keys are initialized (non-zero)
     * \return true if any key material is non-zero
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    /*!
     * \brief Clear all key material securely
     */
    void clear() noexcept;

    /*!
     * \brief Equality comparison
     * \param other Keys to compare with
     * \return true if all key material matches
     */
    [[nodiscard]] auto operator==(const quic_keys& other) const noexcept -> bool;

    /*!
     * \brief Inequality comparison
     */
    [[nodiscard]] auto operator!=(const quic_keys& other) const noexcept -> bool;
};

/*!
 * \struct key_pair
 * \brief A pair of read and write keys for bidirectional communication
 */
struct key_pair
{
    quic_keys read;   //!< Keys for decrypting received packets
    quic_keys write;  //!< Keys for encrypting outgoing packets

    /*!
     * \brief Check if both read and write keys are valid
     */
    [[nodiscard]] auto is_valid() const noexcept -> bool;

    /*!
     * \brief Clear all key material securely
     */
    void clear() noexcept;
};

} // namespace kcenon::network::protocols::quic
