// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/network/detail/protocols/grpc/frame.h"

#include <charconv>
#include <cstring>
#include <sstream>

namespace kcenon::network::protocols::grpc
{

auto grpc_message::parse(std::span<const uint8_t> input) -> Result<grpc_message>
{
    // Check minimum header size
    if (input.size() < grpc_header_size)
    {
        return error<grpc_message>(
            error_codes::common_errors::invalid_argument,
            "Input too small for gRPC message header",
            "grpc::frame",
            "Expected at least 5 bytes, got " + std::to_string(input.size()));
    }

    // Parse compressed flag (1 byte)
    bool compressed = (input[0] != 0);

    // Parse message length (4 bytes, big-endian)
    uint32_t length = (static_cast<uint32_t>(input[1]) << 24) |
                      (static_cast<uint32_t>(input[2]) << 16) |
                      (static_cast<uint32_t>(input[3]) << 8) |
                      (static_cast<uint32_t>(input[4]));

    // Validate message length
    if (length > default_max_message_size)
    {
        return error<grpc_message>(
            error_codes::common_errors::invalid_argument,
            "gRPC message exceeds maximum size",
            "grpc::frame",
            "Max: " + std::to_string(default_max_message_size) +
            ", Got: " + std::to_string(length));
    }

    // Check if we have enough data
    size_t total_size = grpc_header_size + length;
    if (input.size() < total_size)
    {
        return error<grpc_message>(
            error_codes::common_errors::invalid_argument,
            "Input too small for gRPC message payload",
            "grpc::frame",
            "Expected " + std::to_string(total_size) +
            " bytes, got " + std::to_string(input.size()));
    }

    // Extract message data
    grpc_message msg;
    msg.compressed = compressed;
    msg.data.assign(input.begin() + grpc_header_size,
                    input.begin() + grpc_header_size + length);

    return ok(std::move(msg));
}

auto grpc_message::serialize() const -> std::vector<uint8_t>
{
    std::vector<uint8_t> result;
    result.reserve(grpc_header_size + data.size());

    // Compressed flag (1 byte)
    result.push_back(compressed ? 1 : 0);

    // Message length (4 bytes, big-endian)
    uint32_t length = static_cast<uint32_t>(data.size());
    result.push_back(static_cast<uint8_t>((length >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((length >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(length & 0xFF));

    // Message data
    result.insert(result.end(), data.begin(), data.end());

    return result;
}

auto parse_timeout(std::string_view timeout_str) -> uint64_t
{
    if (timeout_str.empty())
    {
        return 0;
    }

    // Get the unit suffix
    char unit = timeout_str.back();

    // Parse the numeric value
    std::string_view num_str = timeout_str.substr(0, timeout_str.size() - 1);
    uint64_t value = 0;

    auto result = std::from_chars(num_str.data(),
                                  num_str.data() + num_str.size(),
                                  value);

    if (result.ec != std::errc{})
    {
        return 0;
    }

    // Convert to milliseconds based on unit
    switch (unit)
    {
        case 'H': // Hours
            return value * 3600000;
        case 'M': // Minutes
            return value * 60000;
        case 'S': // Seconds
            return value * 1000;
        case 'm': // Milliseconds
            return value;
        case 'u': // Microseconds
            return value / 1000;
        case 'n': // Nanoseconds
            return value / 1000000;
        default:
            return 0;
    }
}

auto format_timeout(uint64_t timeout_ms) -> std::string
{
    std::ostringstream oss;

    if (timeout_ms == 0)
    {
        return "0m";
    }

    // Use appropriate unit for best precision
    if (timeout_ms >= 3600000 && timeout_ms % 3600000 == 0)
    {
        oss << (timeout_ms / 3600000) << "H";
    }
    else if (timeout_ms >= 60000 && timeout_ms % 60000 == 0)
    {
        oss << (timeout_ms / 60000) << "M";
    }
    else if (timeout_ms >= 1000 && timeout_ms % 1000 == 0)
    {
        oss << (timeout_ms / 1000) << "S";
    }
    else
    {
        oss << timeout_ms << "m";
    }

    return oss.str();
}

} // namespace kcenon::network::protocols::grpc
