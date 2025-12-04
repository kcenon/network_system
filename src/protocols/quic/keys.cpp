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

#include "kcenon/network/protocols/quic/keys.h"

#include <algorithm>
#include <cstring>

namespace network_system::protocols::quic
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

} // namespace network_system::protocols::quic
