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

#include "kcenon/network/protocols/quic/connection_id.h"

#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>

namespace network_system::protocols::quic
{

connection_id::connection_id(std::span<const uint8_t> data)
{
    size_t copy_len = std::min(data.size(), max_length);
    std::copy_n(data.begin(), copy_len, data_.begin());
    length_ = static_cast<uint8_t>(copy_len);
}

auto connection_id::generate(size_t length) -> connection_id
{
    connection_id cid;

    // Clamp length to valid range
    size_t actual_length = std::min(length, max_length);
    if (actual_length == 0)
    {
        actual_length = 1;  // Minimum 1 byte for non-empty CID
    }

    // Use random_device for cryptographically secure random bytes
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 255);

    for (size_t i = 0; i < actual_length; ++i)
    {
        cid.data_[i] = static_cast<uint8_t>(dist(gen));
    }
    cid.length_ = static_cast<uint8_t>(actual_length);

    return cid;
}

auto connection_id::data() const -> std::span<const uint8_t>
{
    return std::span<const uint8_t>(data_.data(), length_);
}

auto connection_id::length() const noexcept -> size_t
{
    return length_;
}

auto connection_id::empty() const noexcept -> bool
{
    return length_ == 0;
}

auto connection_id::operator==(const connection_id& other) const noexcept -> bool
{
    if (length_ != other.length_)
    {
        return false;
    }
    return std::equal(data_.begin(), data_.begin() + length_,
                      other.data_.begin());
}

auto connection_id::operator!=(const connection_id& other) const noexcept -> bool
{
    return !(*this == other);
}

auto connection_id::operator<(const connection_id& other) const noexcept -> bool
{
    // Compare by length first, then by content
    if (length_ != other.length_)
    {
        return length_ < other.length_;
    }
    return std::lexicographical_compare(
        data_.begin(), data_.begin() + length_,
        other.data_.begin(), other.data_.begin() + other.length_);
}

auto connection_id::to_string() const -> std::string
{
    if (length_ == 0)
    {
        return "<empty>";
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < length_; ++i)
    {
        oss << std::setw(2) << static_cast<unsigned>(data_[i]);
    }
    return oss.str();
}

} // namespace network_system::protocols::quic
