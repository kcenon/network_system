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

#include "kcenon/network/protocols/quic/crypto.h"
#include "kcenon/network/protocols/quic/packet.h"
#include "kcenon/network/internal/openssl_compat.h"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include <algorithm>
#include <cstring>

namespace kcenon::network::protocols::quic
{

namespace
{

// Use the compatibility layer for OpenSSL error handling
using network_system::internal::get_openssl_error;

auto get_openssl_error_string() -> std::string
{
    return get_openssl_error();
}

constexpr std::array<uint8_t, 9> client_initial_label = {
    'c', 'l', 'i', 'e', 'n', 't', ' ', 'i', 'n'
};

constexpr std::array<uint8_t, 9> server_initial_label = {
    's', 'e', 'r', 'v', 'e', 'r', ' ', 'i', 'n'
};

constexpr std::array<uint8_t, 8> quic_key_label = {
    'q', 'u', 'i', 'c', ' ', 'k', 'e', 'y'
};

constexpr std::array<uint8_t, 7> quic_iv_label = {
    'q', 'u', 'i', 'c', ' ', 'i', 'v'
};

constexpr std::array<uint8_t, 7> quic_hp_label = {
    'q', 'u', 'i', 'c', ' ', 'h', 'p'
};

} // anonymous namespace

// ============================================================================
// HKDF Implementation
// ============================================================================

auto hkdf::extract(std::span<const uint8_t> salt,
                   std::span<const uint8_t> ikm)
    -> Result<std::array<uint8_t, secret_size>>
{
    std::array<uint8_t, secret_size> prk{};
    size_t prk_len = prk.size();

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx)
    {
        return error<std::array<uint8_t, secret_size>>(
            -1, "Failed to create HKDF context", "quic::hkdf");
    }

    int ret = EVP_PKEY_derive_init(pctx);
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::array<uint8_t, secret_size>>(
            -1, "HKDF derive init failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256());
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::array<uint8_t, secret_size>>(
            -1, "HKDF set md failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt.data(),
                                       static_cast<int>(salt.size()));
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::array<uint8_t, secret_size>>(
            -1, "HKDF set salt failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_CTX_set1_hkdf_key(pctx, ikm.data(),
                                      static_cast<int>(ikm.size()));
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::array<uint8_t, secret_size>>(
            -1, "HKDF set key failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_CTX_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY);
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::array<uint8_t, secret_size>>(
            -1, "HKDF set mode failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_derive(pctx, prk.data(), &prk_len);
    EVP_PKEY_CTX_free(pctx);

    if (ret <= 0)
    {
        return error<std::array<uint8_t, secret_size>>(
            -1, "HKDF extract failed", "quic::hkdf", get_openssl_error_string());
    }

    return ok(std::move(prk));
}

auto hkdf::expand(std::span<const uint8_t> prk,
                  std::span<const uint8_t> info,
                  size_t length)
    -> Result<std::vector<uint8_t>>
{
    std::vector<uint8_t> okm(length);

    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx)
    {
        return error<std::vector<uint8_t>>(
            -1, "Failed to create HKDF context", "quic::hkdf");
    }

    int ret = EVP_PKEY_derive_init(pctx);
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::vector<uint8_t>>(
            -1, "HKDF derive init failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256());
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::vector<uint8_t>>(
            -1, "HKDF set md failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_CTX_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXPAND_ONLY);
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::vector<uint8_t>>(
            -1, "HKDF set mode failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_CTX_set1_hkdf_key(pctx, prk.data(),
                                      static_cast<int>(prk.size()));
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::vector<uint8_t>>(
            -1, "HKDF set key failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_CTX_add1_hkdf_info(pctx, info.data(),
                                       static_cast<int>(info.size()));
    if (ret <= 0)
    {
        EVP_PKEY_CTX_free(pctx);
        return error<std::vector<uint8_t>>(
            -1, "HKDF set info failed", "quic::hkdf", get_openssl_error_string());
    }

    ret = EVP_PKEY_derive(pctx, okm.data(), &length);
    EVP_PKEY_CTX_free(pctx);

    if (ret <= 0)
    {
        return error<std::vector<uint8_t>>(
            -1, "HKDF expand failed", "quic::hkdf", get_openssl_error_string());
    }

    okm.resize(length);
    return ok(std::move(okm));
}

auto hkdf::expand_label(std::span<const uint8_t> secret,
                        const std::string& label,
                        std::span<const uint8_t> context,
                        size_t length)
    -> Result<std::vector<uint8_t>>
{
    // TLS 1.3 HKDF-Expand-Label structure:
    // struct {
    //     uint16 length;
    //     opaque label<7..255> = "tls13 " + Label;
    //     opaque context<0..255>;
    // } HkdfLabel;

    const std::string prefix = "tls13 ";
    std::vector<uint8_t> hkdf_label;
    hkdf_label.reserve(2 + 1 + prefix.size() + label.size() + 1 + context.size());

    // Length (2 bytes, big-endian)
    hkdf_label.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    hkdf_label.push_back(static_cast<uint8_t>(length & 0xFF));

    // Label length (1 byte) + "tls13 " + label
    size_t label_len = prefix.size() + label.size();
    hkdf_label.push_back(static_cast<uint8_t>(label_len));
    for (char c : prefix)
    {
        hkdf_label.push_back(static_cast<uint8_t>(c));
    }
    for (char c : label)
    {
        hkdf_label.push_back(static_cast<uint8_t>(c));
    }

    // Context length (1 byte) + context
    hkdf_label.push_back(static_cast<uint8_t>(context.size()));
    hkdf_label.insert(hkdf_label.end(), context.begin(), context.end());

    return expand(secret, hkdf_label, length);
}

// ============================================================================
// Initial Keys Implementation
// ============================================================================

auto initial_keys::derive(const connection_id& dest_cid, uint32_t version)
    -> Result<key_pair>
{
    // Select salt based on version
    std::span<const uint8_t> salt;
    if (version == quic_version::version_2)
    {
        salt = initial_salt_v2;
    }
    else
    {
        salt = initial_salt_v1;
    }

    // Extract initial secret from destination connection ID
    auto initial_secret_result = hkdf::extract(salt, dest_cid.data());
    if (initial_secret_result.is_err())
    {
        return error<key_pair>(
            initial_secret_result.error().code,
            "Failed to derive initial secret",
            "quic::initial_keys",
            initial_secret_result.error().message);
    }

    auto& initial_secret = initial_secret_result.value();

    // Derive client initial secret
    auto client_secret_result = hkdf::expand_label(
        initial_secret,
        std::string(reinterpret_cast<const char*>(client_initial_label.data()),
                    client_initial_label.size()),
        {},
        secret_size);
    if (client_secret_result.is_err())
    {
        return error<key_pair>(
            client_secret_result.error().code,
            "Failed to derive client initial secret",
            "quic::initial_keys",
            client_secret_result.error().message);
    }

    // Derive server initial secret
    auto server_secret_result = hkdf::expand_label(
        initial_secret,
        std::string(reinterpret_cast<const char*>(server_initial_label.data()),
                    server_initial_label.size()),
        {},
        secret_size);
    if (server_secret_result.is_err())
    {
        return error<key_pair>(
            server_secret_result.error().code,
            "Failed to derive server initial secret",
            "quic::initial_keys",
            server_secret_result.error().message);
    }

    // Derive client keys
    auto client_keys_result = derive_keys(client_secret_result.value(), true);
    if (client_keys_result.is_err())
    {
        return error<key_pair>(
            client_keys_result.error().code,
            "Failed to derive client keys",
            "quic::initial_keys",
            client_keys_result.error().message);
    }

    // Derive server keys
    auto server_keys_result = derive_keys(server_secret_result.value(), false);
    if (server_keys_result.is_err())
    {
        return error<key_pair>(
            server_keys_result.error().code,
            "Failed to derive server keys",
            "quic::initial_keys",
            server_keys_result.error().message);
    }

    // Copy secrets into keys
    auto& client_keys = client_keys_result.value();
    auto& server_keys = server_keys_result.value();

    std::copy(client_secret_result.value().begin(),
              client_secret_result.value().end(),
              client_keys.secret.begin());
    std::copy(server_secret_result.value().begin(),
              server_secret_result.value().end(),
              server_keys.secret.begin());

    // For a client: write = client keys, read = server keys
    // For a server: write = server keys, read = client keys
    // This function returns from client's perspective
    key_pair result;
    result.write = std::move(client_keys);
    result.read = std::move(server_keys);

    return ok(std::move(result));
}

auto initial_keys::derive_keys(std::span<const uint8_t> initial_secret,
                               bool is_client_keys)
    -> Result<quic_keys>
{
    (void)is_client_keys; // Not used but kept for API clarity

    quic_keys keys;

    // Derive AEAD key
    auto key_result = hkdf::expand_label(
        initial_secret,
        std::string(reinterpret_cast<const char*>(quic_key_label.data()),
                    quic_key_label.size()),
        {},
        aes_128_key_size);
    if (key_result.is_err())
    {
        return error<quic_keys>(
            key_result.error().code,
            "Failed to derive AEAD key",
            "quic::initial_keys",
            key_result.error().message);
    }
    std::copy(key_result.value().begin(), key_result.value().end(),
              keys.key.begin());

    // Derive IV
    auto iv_result = hkdf::expand_label(
        initial_secret,
        std::string(reinterpret_cast<const char*>(quic_iv_label.data()),
                    quic_iv_label.size()),
        {},
        aead_iv_size);
    if (iv_result.is_err())
    {
        return error<quic_keys>(
            iv_result.error().code,
            "Failed to derive IV",
            "quic::initial_keys",
            iv_result.error().message);
    }
    std::copy(iv_result.value().begin(), iv_result.value().end(),
              keys.iv.begin());

    // Derive header protection key
    auto hp_result = hkdf::expand_label(
        initial_secret,
        std::string(reinterpret_cast<const char*>(quic_hp_label.data()),
                    quic_hp_label.size()),
        {},
        hp_key_size);
    if (hp_result.is_err())
    {
        return error<quic_keys>(
            hp_result.error().code,
            "Failed to derive HP key",
            "quic::initial_keys",
            hp_result.error().message);
    }
    std::copy(hp_result.value().begin(), hp_result.value().end(),
              keys.hp_key.begin());

    return ok(std::move(keys));
}

// ============================================================================
// Packet Protection Implementation
// ============================================================================

auto packet_protection::make_nonce(std::span<const uint8_t> iv,
                                   uint64_t packet_number)
    -> std::array<uint8_t, aead_iv_size>
{
    std::array<uint8_t, aead_iv_size> nonce{};
    std::copy(iv.begin(), iv.end(), nonce.begin());

    // XOR packet number into the rightmost bytes of the IV
    for (size_t i = 0; i < 8; ++i)
    {
        nonce[aead_iv_size - 1 - i] ^=
            static_cast<uint8_t>((packet_number >> (i * 8)) & 0xFF);
    }

    return nonce;
}

auto packet_protection::protect(const quic_keys& keys,
                                std::span<const uint8_t> header,
                                std::span<const uint8_t> payload,
                                uint64_t packet_number)
    -> Result<std::vector<uint8_t>>
{
    auto nonce = make_nonce(keys.iv, packet_number);

    // Create output buffer: header + ciphertext + tag
    std::vector<uint8_t> output;
    output.reserve(header.size() + payload.size() + aead_tag_size);
    output.insert(output.end(), header.begin(), header.end());
    output.resize(output.size() + payload.size() + aead_tag_size);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        return error<std::vector<uint8_t>>(
            -1, "Failed to create cipher context", "quic::packet_protection");
    }

    int ret = EVP_EncryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr,
                                  keys.key.data(), nonce.data());
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::vector<uint8_t>>(
            -1, "AES-GCM encrypt init failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    // Set AAD (header)
    int len;
    ret = EVP_EncryptUpdate(ctx, nullptr, &len, header.data(),
                            static_cast<int>(header.size()));
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::vector<uint8_t>>(
            -1, "AES-GCM AAD update failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    // Encrypt payload
    ret = EVP_EncryptUpdate(ctx, output.data() + header.size(), &len,
                            payload.data(), static_cast<int>(payload.size()));
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::vector<uint8_t>>(
            -1, "AES-GCM encrypt failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    int ciphertext_len = len;

    ret = EVP_EncryptFinal_ex(ctx, output.data() + header.size() + len, &len);
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::vector<uint8_t>>(
            -1, "AES-GCM encrypt final failed", "quic::packet_protection",
            get_openssl_error_string());
    }
    ciphertext_len += len;

    // Get tag
    ret = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, aead_tag_size,
                              output.data() + header.size() + ciphertext_len);
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::vector<uint8_t>>(
            -1, "AES-GCM get tag failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    EVP_CIPHER_CTX_free(ctx);
    output.resize(header.size() + ciphertext_len + aead_tag_size);

    return ok(std::move(output));
}

auto packet_protection::unprotect(const quic_keys& keys,
                                  std::span<const uint8_t> packet,
                                  size_t header_length,
                                  uint64_t packet_number)
    -> Result<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>
{
    if (packet.size() < header_length + aead_tag_size)
    {
        return error<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>(
            -1, "Packet too short for decryption", "quic::packet_protection");
    }

    auto nonce = make_nonce(keys.iv, packet_number);

    auto header = packet.subspan(0, header_length);
    auto ciphertext = packet.subspan(header_length,
                                     packet.size() - header_length - aead_tag_size);
    auto tag = packet.subspan(packet.size() - aead_tag_size, aead_tag_size);

    std::vector<uint8_t> plaintext(ciphertext.size());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        return error<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>(
            -1, "Failed to create cipher context", "quic::packet_protection");
    }

    int ret = EVP_DecryptInit_ex(ctx, EVP_aes_128_gcm(), nullptr,
                                  keys.key.data(), nonce.data());
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>(
            -1, "AES-GCM decrypt init failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    // Set AAD (header)
    int len;
    ret = EVP_DecryptUpdate(ctx, nullptr, &len, header.data(),
                            static_cast<int>(header.size()));
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>(
            -1, "AES-GCM AAD update failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    // Decrypt ciphertext
    ret = EVP_DecryptUpdate(ctx, plaintext.data(), &len,
                            ciphertext.data(), static_cast<int>(ciphertext.size()));
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>(
            -1, "AES-GCM decrypt failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    int plaintext_len = len;

    // Set expected tag
    ret = EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, aead_tag_size,
                              const_cast<uint8_t*>(tag.data()));
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>(
            -1, "AES-GCM set tag failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (ret != 1)
    {
        return error<std::pair<std::vector<uint8_t>, std::vector<uint8_t>>>(
            -1, "AES-GCM authentication failed", "quic::packet_protection");
    }

    plaintext_len += len;
    plaintext.resize(plaintext_len);

    std::vector<uint8_t> header_copy(header.begin(), header.end());
    return ok(std::make_pair(std::move(header_copy), std::move(plaintext)));
}

auto packet_protection::generate_hp_mask(std::span<const uint8_t> hp_key,
                                         std::span<const uint8_t> sample)
    -> Result<std::array<uint8_t, 5>>
{
    if (sample.size() < hp_sample_size)
    {
        return error<std::array<uint8_t, 5>>(
            -1, "Sample too short for HP mask", "quic::packet_protection");
    }

    std::array<uint8_t, 16> mask_full{};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
    {
        return error<std::array<uint8_t, 5>>(
            -1, "Failed to create cipher context", "quic::packet_protection");
    }

    int ret = EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), nullptr,
                                  hp_key.data(), nullptr);
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::array<uint8_t, 5>>(
            -1, "AES-ECB init failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    EVP_CIPHER_CTX_set_padding(ctx, 0);

    int len;
    ret = EVP_EncryptUpdate(ctx, mask_full.data(), &len,
                            sample.data(), hp_sample_size);
    if (ret != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        return error<std::array<uint8_t, 5>>(
            -1, "AES-ECB encrypt failed", "quic::packet_protection",
            get_openssl_error_string());
    }

    EVP_CIPHER_CTX_free(ctx);

    // Return first 5 bytes of the mask
    std::array<uint8_t, 5> mask{};
    std::copy(mask_full.begin(), mask_full.begin() + 5, mask.begin());

    return ok(std::move(mask));
}

auto packet_protection::protect_header(const quic_keys& keys,
                                       std::span<uint8_t> header,
                                       size_t pn_offset,
                                       size_t pn_length,
                                       std::span<const uint8_t> sample)
    -> VoidResult
{
    auto mask_result = generate_hp_mask(keys.hp_key, sample);
    if (mask_result.is_err())
    {
        return error_void(mask_result.error().code,
                         mask_result.error().message,
                         get_error_source(mask_result.error()));
    }

    auto& mask = mask_result.value();

    // Apply mask to first byte
    if ((header[0] & 0x80) != 0)
    {
        // Long header: mask lower 4 bits
        header[0] ^= (mask[0] & 0x0F);
    }
    else
    {
        // Short header: mask lower 5 bits
        header[0] ^= (mask[0] & 0x1F);
    }

    // Apply mask to packet number
    for (size_t i = 0; i < pn_length; ++i)
    {
        header[pn_offset + i] ^= mask[1 + i];
    }

    return ok();
}

auto packet_protection::unprotect_header(const quic_keys& keys,
                                         std::span<uint8_t> header,
                                         size_t pn_offset,
                                         std::span<const uint8_t> sample)
    -> Result<std::pair<uint8_t, size_t>>
{
    auto mask_result = generate_hp_mask(keys.hp_key, sample);
    if (mask_result.is_err())
    {
        return error<std::pair<uint8_t, size_t>>(
            mask_result.error().code,
            mask_result.error().message,
            get_error_source(mask_result.error()));
    }

    auto& mask = mask_result.value();

    // Unmask first byte
    if ((header[0] & 0x80) != 0)
    {
        // Long header
        header[0] ^= (mask[0] & 0x0F);
    }
    else
    {
        // Short header
        header[0] ^= (mask[0] & 0x1F);
    }

    // Get packet number length from first byte
    size_t pn_length = (header[0] & 0x03) + 1;

    // Unmask packet number
    for (size_t i = 0; i < pn_length; ++i)
    {
        header[pn_offset + i] ^= mask[1 + i];
    }

    return ok(std::make_pair(header[0], pn_length));
}

// ============================================================================
// QUIC Crypto Handler Implementation
// ============================================================================

struct quic_crypto::impl
{
    SSL_CTX* ssl_ctx{nullptr};
    SSL* ssl{nullptr};
    BIO* rbio{nullptr};
    BIO* wbio{nullptr};

    bool is_server{false};
    bool handshake_complete{false};
    encryption_level current_level{encryption_level::initial};
    uint8_t key_phase{0};

    std::map<encryption_level, quic_keys> read_keys;
    std::map<encryption_level, quic_keys> write_keys;

    std::string alpn;
    std::vector<uint8_t> alpn_data;

    ~impl()
    {
        if (ssl)
        {
            SSL_free(ssl);
        }
        if (ssl_ctx)
        {
            SSL_CTX_free(ssl_ctx);
        }
        // BIOs are freed by SSL_free
    }
};

quic_crypto::quic_crypto()
    : impl_(std::make_unique<impl>())
{
}

quic_crypto::~quic_crypto() = default;

quic_crypto::quic_crypto(quic_crypto&& other) noexcept = default;
quic_crypto& quic_crypto::operator=(quic_crypto&& other) noexcept = default;

auto quic_crypto::init_client(const std::string& server_name) -> VoidResult
{
    impl_->is_server = false;

    impl_->ssl_ctx = SSL_CTX_new(TLS_client_method());
    if (!impl_->ssl_ctx)
    {
        return error_void(-1, "Failed to create SSL context", "quic::crypto",
                         get_openssl_error_string());
    }

    // Set TLS 1.3 only
    SSL_CTX_set_min_proto_version(impl_->ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(impl_->ssl_ctx, TLS1_3_VERSION);

    impl_->ssl = SSL_new(impl_->ssl_ctx);
    if (!impl_->ssl)
    {
        return error_void(-1, "Failed to create SSL object", "quic::crypto",
                         get_openssl_error_string());
    }

    // Set SNI
    if (!server_name.empty())
    {
        SSL_set_tlsext_host_name(impl_->ssl, server_name.c_str());
    }

    // Create memory BIOs
    impl_->rbio = BIO_new(BIO_s_mem());
    impl_->wbio = BIO_new(BIO_s_mem());
    if (!impl_->rbio || !impl_->wbio)
    {
        return error_void(-1, "Failed to create BIO objects", "quic::crypto");
    }

    BIO_set_nbio(impl_->rbio, 1);
    BIO_set_nbio(impl_->wbio, 1);

    SSL_set_bio(impl_->ssl, impl_->rbio, impl_->wbio);
    SSL_set_connect_state(impl_->ssl);

    return ok();
}

auto quic_crypto::init_server(const std::string& cert_file,
                              const std::string& key_file) -> VoidResult
{
    impl_->is_server = true;

    impl_->ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!impl_->ssl_ctx)
    {
        return error_void(-1, "Failed to create SSL context", "quic::crypto",
                         get_openssl_error_string());
    }

    // Set TLS 1.3 only
    SSL_CTX_set_min_proto_version(impl_->ssl_ctx, TLS1_3_VERSION);
    SSL_CTX_set_max_proto_version(impl_->ssl_ctx, TLS1_3_VERSION);

    // Load certificate
    if (SSL_CTX_use_certificate_file(impl_->ssl_ctx, cert_file.c_str(),
                                     SSL_FILETYPE_PEM) != 1)
    {
        return error_void(-1, "Failed to load certificate", "quic::crypto",
                         get_openssl_error_string());
    }

    // Load private key
    if (SSL_CTX_use_PrivateKey_file(impl_->ssl_ctx, key_file.c_str(),
                                    SSL_FILETYPE_PEM) != 1)
    {
        return error_void(-1, "Failed to load private key", "quic::crypto",
                         get_openssl_error_string());
    }

    impl_->ssl = SSL_new(impl_->ssl_ctx);
    if (!impl_->ssl)
    {
        return error_void(-1, "Failed to create SSL object", "quic::crypto",
                         get_openssl_error_string());
    }

    // Create memory BIOs
    impl_->rbio = BIO_new(BIO_s_mem());
    impl_->wbio = BIO_new(BIO_s_mem());
    if (!impl_->rbio || !impl_->wbio)
    {
        return error_void(-1, "Failed to create BIO objects", "quic::crypto");
    }

    BIO_set_nbio(impl_->rbio, 1);
    BIO_set_nbio(impl_->wbio, 1);

    SSL_set_bio(impl_->ssl, impl_->rbio, impl_->wbio);
    SSL_set_accept_state(impl_->ssl);

    return ok();
}

auto quic_crypto::derive_initial_secrets(const connection_id& dest_cid)
    -> VoidResult
{
    auto keys_result = initial_keys::derive(dest_cid);
    if (keys_result.is_err())
    {
        return error_void(keys_result.error().code,
                         keys_result.error().message,
                         get_error_source(keys_result.error()));
    }

    auto& keys = keys_result.value();

    if (impl_->is_server)
    {
        // Server: read with client keys, write with server keys
        impl_->read_keys[encryption_level::initial] = keys.write;  // Client's write = Server's read
        impl_->write_keys[encryption_level::initial] = keys.read;  // Server's write = Client's read
    }
    else
    {
        // Client: write with client keys, read with server keys
        impl_->write_keys[encryption_level::initial] = keys.write;
        impl_->read_keys[encryption_level::initial] = keys.read;
    }

    return ok();
}

auto quic_crypto::process_crypto_data(encryption_level level,
                                      std::span<const uint8_t> data)
    -> Result<std::vector<uint8_t>>
{
    (void)level; // For now, we don't differentiate by level

    // Write incoming data to read BIO
    int written = BIO_write(impl_->rbio, data.data(),
                            static_cast<int>(data.size()));
    if (written <= 0)
    {
        return error<std::vector<uint8_t>>(
            -1, "Failed to write to BIO", "quic::crypto");
    }

    // Continue handshake
    int result = SSL_do_handshake(impl_->ssl);
    if (result == 1)
    {
        impl_->handshake_complete = true;
        impl_->current_level = encryption_level::application;
    }
    else
    {
        int err = SSL_get_error(impl_->ssl, result);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
        {
            return error<std::vector<uint8_t>>(
                -1, "SSL handshake failed", "quic::crypto",
                get_openssl_error_string());
        }
    }

    // Read any output data from write BIO
    std::vector<uint8_t> output;
    int pending = BIO_ctrl_pending(impl_->wbio);
    if (pending > 0)
    {
        output.resize(static_cast<size_t>(pending));
        int read = BIO_read(impl_->wbio, output.data(), pending);
        if (read > 0)
        {
            output.resize(static_cast<size_t>(read));
        }
        else
        {
            output.clear();
        }
    }

    return ok(std::move(output));
}

auto quic_crypto::start_handshake() -> Result<std::vector<uint8_t>>
{
    int result = SSL_do_handshake(impl_->ssl);
    if (result == 1)
    {
        impl_->handshake_complete = true;
    }
    else
    {
        int err = SSL_get_error(impl_->ssl, result);
        if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE)
        {
            return error<std::vector<uint8_t>>(
                -1, "SSL handshake start failed", "quic::crypto",
                get_openssl_error_string());
        }
    }

    // Read output from write BIO
    std::vector<uint8_t> output;
    int pending = BIO_ctrl_pending(impl_->wbio);
    if (pending > 0)
    {
        output.resize(static_cast<size_t>(pending));
        int read = BIO_read(impl_->wbio, output.data(), pending);
        if (read > 0)
        {
            output.resize(static_cast<size_t>(read));
        }
        else
        {
            output.clear();
        }
    }

    return ok(std::move(output));
}

auto quic_crypto::is_handshake_complete() const noexcept -> bool
{
    return impl_->handshake_complete;
}

auto quic_crypto::current_level() const noexcept -> encryption_level
{
    return impl_->current_level;
}

auto quic_crypto::get_write_keys(encryption_level level) const
    -> Result<quic_keys>
{
    auto it = impl_->write_keys.find(level);
    if (it == impl_->write_keys.end())
    {
        return error<quic_keys>(
            -1, "Write keys not available for level", "quic::crypto",
            encryption_level_to_string(level));
    }
    return ok(quic_keys(it->second));
}

auto quic_crypto::get_read_keys(encryption_level level) const
    -> Result<quic_keys>
{
    auto it = impl_->read_keys.find(level);
    if (it == impl_->read_keys.end())
    {
        return error<quic_keys>(
            -1, "Read keys not available for level", "quic::crypto",
            encryption_level_to_string(level));
    }
    return ok(quic_keys(it->second));
}

void quic_crypto::set_keys(encryption_level level,
                           const quic_keys& read_keys,
                           const quic_keys& write_keys)
{
    impl_->read_keys[level] = read_keys;
    impl_->write_keys[level] = write_keys;

    if (level > impl_->current_level)
    {
        impl_->current_level = level;
    }
}

auto quic_crypto::update_keys() -> VoidResult
{
    if (!impl_->handshake_complete)
    {
        return error_void(-1, "Handshake not complete", "quic::crypto");
    }

    auto it = impl_->write_keys.find(encryption_level::application);
    if (it == impl_->write_keys.end())
    {
        return error_void(-1, "Application keys not available", "quic::crypto");
    }

    // Derive new secrets using "quic ku" label
    auto& old_secret = it->second.secret;
    auto new_secret_result = hkdf::expand_label(old_secret, "quic ku", {},
                                                 secret_size);
    if (new_secret_result.is_err())
    {
        return error_void(new_secret_result.error().code,
                         new_secret_result.error().message,
                         get_error_source(new_secret_result.error()));
    }

    // Derive new keys from new secret
    auto new_keys_result = initial_keys::derive_keys(new_secret_result.value(),
                                                      true);
    if (new_keys_result.is_err())
    {
        return error_void(new_keys_result.error().code,
                         new_keys_result.error().message,
                         get_error_source(new_keys_result.error()));
    }

    auto& new_keys = new_keys_result.value();
    std::copy(new_secret_result.value().begin(),
              new_secret_result.value().end(),
              new_keys.secret.begin());

    impl_->write_keys[encryption_level::application] = new_keys;
    impl_->key_phase = 1 - impl_->key_phase;

    return ok();
}

auto quic_crypto::get_alpn() const -> std::string
{
    return impl_->alpn;
}

auto quic_crypto::set_alpn(const std::vector<std::string>& protocols)
    -> VoidResult
{
    if (protocols.empty())
    {
        return ok();
    }

    // Build ALPN wire format: length-prefixed strings
    impl_->alpn_data.clear();
    for (const auto& proto : protocols)
    {
        if (proto.size() > 255)
        {
            return error_void(-1, "ALPN protocol too long", "quic::crypto");
        }
        impl_->alpn_data.push_back(static_cast<uint8_t>(proto.size()));
        impl_->alpn_data.insert(impl_->alpn_data.end(), proto.begin(), proto.end());
    }

    if (SSL_CTX_set_alpn_protos(impl_->ssl_ctx, impl_->alpn_data.data(),
                                static_cast<unsigned int>(impl_->alpn_data.size())) != 0)
    {
        return error_void(-1, "Failed to set ALPN protocols", "quic::crypto",
                         get_openssl_error_string());
    }

    return ok();
}

auto quic_crypto::is_server() const noexcept -> bool
{
    return impl_->is_server;
}

auto quic_crypto::key_phase() const noexcept -> uint8_t
{
    return impl_->key_phase;
}

} // namespace kcenon::network::protocols::quic
