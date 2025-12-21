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

#include "kcenon/network/protocols/quic/connection_id.h"
#include "kcenon/network/protocols/quic/keys.h"
#include "kcenon/network/utils/result_types.h"

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

// Forward declarations for OpenSSL types
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;

namespace kcenon::network::protocols::quic
{

// ============================================================================
// Initial Secret Derivation
// ============================================================================

/*!
 * \brief QUIC version 1 initial salt (RFC 9001 Section 5.2)
 *
 * This salt is used to derive the initial secrets from the Destination
 * Connection ID. It is a fixed value defined in the RFC.
 */
constexpr std::array<uint8_t, 20> initial_salt_v1 = {
    0x38, 0x76, 0x2c, 0xf7, 0xf5, 0x59, 0x34, 0xb3, 0x4d, 0x17,
    0x9a, 0xe6, 0xa4, 0xc8, 0x0c, 0xad, 0xcc, 0xbb, 0x7f, 0x0a
};

/*!
 * \brief QUIC version 2 initial salt (RFC 9369)
 */
constexpr std::array<uint8_t, 20> initial_salt_v2 = {
    0x0d, 0xed, 0xe3, 0xde, 0xf7, 0x00, 0xa6, 0xdb, 0x81, 0x93,
    0x81, 0xbe, 0x6e, 0x26, 0x9d, 0xcb, 0xf9, 0xbd, 0x2e, 0xd9
};

// ============================================================================
// HKDF Functions
// ============================================================================

/*!
 * \class hkdf
 * \brief HKDF (HMAC-based Key Derivation Function) utilities (RFC 5869)
 *
 * Used for deriving QUIC keys from secrets.
 */
class hkdf
{
public:
    /*!
     * \brief HKDF-Extract function
     * \param salt Salt value (non-secret random value)
     * \param ikm Input keying material
     * \return Pseudorandom key (PRK) or error
     */
    [[nodiscard]] static auto extract(std::span<const uint8_t> salt,
                                       std::span<const uint8_t> ikm)
        -> Result<std::array<uint8_t, secret_size>>;

    /*!
     * \brief HKDF-Expand function
     * \param prk Pseudorandom key from Extract
     * \param info Context and application specific information
     * \param length Desired output length
     * \return Output keying material (OKM) or error
     */
    [[nodiscard]] static auto expand(std::span<const uint8_t> prk,
                                      std::span<const uint8_t> info,
                                      size_t length)
        -> Result<std::vector<uint8_t>>;

    /*!
     * \brief HKDF-Expand-Label function (TLS 1.3 style)
     * \param secret Secret to expand
     * \param label Label string (without "tls13 " prefix)
     * \param context Context data (usually empty for QUIC)
     * \param length Desired output length
     * \return Output keying material or error
     */
    [[nodiscard]] static auto expand_label(std::span<const uint8_t> secret,
                                            const std::string& label,
                                            std::span<const uint8_t> context,
                                            size_t length)
        -> Result<std::vector<uint8_t>>;
};

// ============================================================================
// Initial Keys
// ============================================================================

/*!
 * \class initial_keys
 * \brief Derives initial encryption keys from Destination Connection ID
 *
 * Initial keys are derived deterministically from the Destination Connection
 * ID, allowing peers to decrypt Initial packets without a prior handshake.
 */
class initial_keys
{
public:
    /*!
     * \brief Derive client and server initial keys
     * \param dest_cid Destination Connection ID (from client's perspective)
     * \param version QUIC version (affects salt selection)
     * \return Key pair for initial encryption level or error
     */
    [[nodiscard]] static auto derive(const connection_id& dest_cid,
                                      uint32_t version = 0x00000001)
        -> Result<key_pair>;

    /*!
     * \brief Derive keys from an initial secret
     * \param initial_secret The initial secret
     * \param is_client_keys true for client keys, false for server keys
     * \return QUIC keys or error
     */
    [[nodiscard]] static auto derive_keys(std::span<const uint8_t> initial_secret,
                                           bool is_client_keys)
        -> Result<quic_keys>;
};

// ============================================================================
// Packet Protection
// ============================================================================

/*!
 * \class packet_protection
 * \brief QUIC packet protection (encryption/decryption) (RFC 9001 Section 5)
 *
 * Provides AEAD encryption for packet payloads and header protection
 * to prevent linkability attacks.
 */
class packet_protection
{
public:
    /*!
     * \brief Protect (encrypt) a QUIC packet
     * \param keys Encryption keys for the current level
     * \param header Packet header (will be used as AAD)
     * \param payload Plaintext payload to encrypt
     * \param packet_number Packet number (used for nonce derivation)
     * \return Protected packet (header + encrypted payload + tag) or error
     */
    [[nodiscard]] static auto protect(const quic_keys& keys,
                                       std::span<const uint8_t> header,
                                       std::span<const uint8_t> payload,
                                       uint64_t packet_number)
        -> Result<std::vector<uint8_t>>;

    /*!
     * \brief Unprotect (decrypt) a QUIC packet
     * \param keys Decryption keys for the current level
     * \param packet Full packet data (header + encrypted payload + tag)
     * \param header_length Length of the header (including packet number)
     * \param packet_number Decoded packet number
     * \return Pair of (header, decrypted payload) or error
     */
    [[nodiscard]] static auto unprotect(
        const quic_keys& keys,
        std::span<const uint8_t> packet,
        size_t header_length,
        uint64_t packet_number)
        -> Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>;

    /*!
     * \brief Apply header protection
     * \param keys Keys containing the HP key
     * \param header Header bytes (modified in place)
     * \param pn_offset Offset of packet number in header
     * \param pn_length Length of packet number (1-4)
     * \param sample Sample from encrypted payload (16 bytes)
     * \return Success or error
     */
    [[nodiscard]] static auto protect_header(const quic_keys& keys,
                                              std::span<uint8_t> header,
                                              size_t pn_offset,
                                              size_t pn_length,
                                              std::span<const uint8_t> sample)
        -> VoidResult;

    /*!
     * \brief Remove header protection
     * \param keys Keys containing the HP key
     * \param header Header bytes (modified in place)
     * \param pn_offset Offset of packet number in header
     * \param sample Sample from encrypted payload (16 bytes)
     * \return Pair of (first_byte_unprotected, pn_length) or error
     */
    [[nodiscard]] static auto unprotect_header(const quic_keys& keys,
                                                std::span<uint8_t> header,
                                                size_t pn_offset,
                                                std::span<const uint8_t> sample)
        -> Result<std::pair<uint8_t, size_t>>;

    /*!
     * \brief Generate header protection mask using AES-ECB
     * \param hp_key Header protection key
     * \param sample 16-byte sample from ciphertext
     * \return 5-byte mask or error
     */
    [[nodiscard]] static auto generate_hp_mask(std::span<const uint8_t> hp_key,
                                                std::span<const uint8_t> sample)
        -> Result<std::array<uint8_t, 5>>;

private:
    /*!
     * \brief Construct nonce from IV and packet number
     * \param iv Initialization vector
     * \param packet_number Packet number
     * \return Nonce for AEAD
     */
    [[nodiscard]] static auto make_nonce(std::span<const uint8_t> iv,
                                          uint64_t packet_number)
        -> std::array<uint8_t, aead_iv_size>;
};

// ============================================================================
// QUIC Crypto Handler
// ============================================================================

/*!
 * \class quic_crypto
 * \brief QUIC-TLS integration handler (RFC 9001)
 *
 * Manages the TLS 1.3 handshake for QUIC, handling:
 * - Key derivation for each encryption level
 * - CRYPTO frame data processing
 * - Key updates after handshake
 *
 * This class wraps OpenSSL for cryptographic operations.
 */
class quic_crypto
{
public:
    /*!
     * \brief Default constructor
     */
    quic_crypto();

    /*!
     * \brief Destructor (cleans up OpenSSL resources)
     */
    ~quic_crypto();

    // Non-copyable
    quic_crypto(const quic_crypto&) = delete;
    quic_crypto& operator=(const quic_crypto&) = delete;

    // Movable
    quic_crypto(quic_crypto&& other) noexcept;
    quic_crypto& operator=(quic_crypto&& other) noexcept;

    /*!
     * \brief Initialize as client
     * \param server_name Server hostname (for SNI)
     * \return Success or error
     */
    [[nodiscard]] auto init_client(const std::string& server_name) -> VoidResult;

    /*!
     * \brief Initialize as server
     * \param cert_file Path to certificate file (PEM format)
     * \param key_file Path to private key file (PEM format)
     * \return Success or error
     */
    [[nodiscard]] auto init_server(const std::string& cert_file,
                                    const std::string& key_file) -> VoidResult;

    /*!
     * \brief Derive initial secrets from destination connection ID
     * \param dest_cid Destination Connection ID
     * \return Success or error
     */
    [[nodiscard]] auto derive_initial_secrets(const connection_id& dest_cid)
        -> VoidResult;

    /*!
     * \brief Process incoming CRYPTO frame data
     * \param level Encryption level of the data
     * \param data CRYPTO frame payload
     * \return Outgoing CRYPTO data to send (may be empty) or error
     */
    [[nodiscard]] auto process_crypto_data(encryption_level level,
                                            std::span<const uint8_t> data)
        -> Result<std::vector<uint8_t>>;

    /*!
     * \brief Start the handshake (generate initial CRYPTO data)
     * \return Initial CRYPTO data to send or error
     */
    [[nodiscard]] auto start_handshake() -> Result<std::vector<uint8_t>>;

    /*!
     * \brief Check if the handshake is complete
     * \return true if handshake has finished successfully
     */
    [[nodiscard]] auto is_handshake_complete() const noexcept -> bool;

    /*!
     * \brief Get current encryption level
     * \return Current encryption level
     */
    [[nodiscard]] auto current_level() const noexcept -> encryption_level;

    /*!
     * \brief Get write keys for an encryption level
     * \param level Desired encryption level
     * \return Keys or error if not available
     */
    [[nodiscard]] auto get_write_keys(encryption_level level) const
        -> Result<quic_keys>;

    /*!
     * \brief Get read keys for an encryption level
     * \param level Desired encryption level
     * \return Keys or error if not available
     */
    [[nodiscard]] auto get_read_keys(encryption_level level) const
        -> Result<quic_keys>;

    /*!
     * \brief Set keys for an encryption level (used during handshake)
     * \param level Encryption level
     * \param read_keys Keys for decryption
     * \param write_keys Keys for encryption
     */
    void set_keys(encryption_level level,
                  const quic_keys& read_keys,
                  const quic_keys& write_keys);

    /*!
     * \brief Perform a key update (1-RTT only)
     * \return Success or error
     */
    [[nodiscard]] auto update_keys() -> VoidResult;

    /*!
     * \brief Get the negotiated ALPN protocol
     * \return ALPN protocol string or empty if not negotiated
     */
    [[nodiscard]] auto get_alpn() const -> std::string;

    /*!
     * \brief Set ALPN protocols to offer/accept
     * \param protocols List of protocol names (e.g., {"h3", "hq-interop"})
     * \return Success or error
     */
    [[nodiscard]] auto set_alpn(const std::vector<std::string>& protocols)
        -> VoidResult;

    /*!
     * \brief Check if this is a server instance
     * \return true if server, false if client
     */
    [[nodiscard]] auto is_server() const noexcept -> bool;

    /*!
     * \brief Get current key phase (for key updates)
     * \return Current key phase (0 or 1)
     */
    [[nodiscard]] auto key_phase() const noexcept -> uint8_t;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace kcenon::network::protocols::quic
