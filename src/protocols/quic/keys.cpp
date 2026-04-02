// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "internal/protocols/quic/keys.h"

#include <algorithm>
#include <cstring>

namespace kcenon::network::protocols::quic
{

auto encryption_level_to_string(encryption_level level) -> std::string
{
    switch (level)
    {
        case encryption_level::initial:
            return "Initial";
        case encryption_level::handshake:
            return "Handshake";
        case encryption_level::zero_rtt:
            return "0-RTT";
        case encryption_level::application:
            return "Application";
        default:
            return "Unknown";
    }
}

auto quic_keys::is_valid() const noexcept -> bool
{
    // Check if key is non-zero
    return std::any_of(key.begin(), key.end(), [](uint8_t b) { return b != 0; });
}

void quic_keys::clear() noexcept
{
    // Use volatile to prevent compiler optimization
    volatile uint8_t* p;

    p = secret.data();
    std::memset(const_cast<uint8_t*>(p), 0, secret.size());

    p = key.data();
    std::memset(const_cast<uint8_t*>(p), 0, key.size());

    p = iv.data();
    std::memset(const_cast<uint8_t*>(p), 0, iv.size());

    p = hp_key.data();
    std::memset(const_cast<uint8_t*>(p), 0, hp_key.size());
}

auto quic_keys::operator==(const quic_keys& other) const noexcept -> bool
{
    return secret == other.secret && key == other.key && iv == other.iv &&
           hp_key == other.hp_key;
}

auto quic_keys::operator!=(const quic_keys& other) const noexcept -> bool
{
    return !(*this == other);
}

auto key_pair::is_valid() const noexcept -> bool
{
    return read.is_valid() && write.is_valid();
}

void key_pair::clear() noexcept
{
    read.clear();
    write.clear();
}

} // namespace kcenon::network::protocols::quic
