// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file mock_ws_handshake.cpp
 * @brief Implementation of WebSocket byte-level helpers (Issue #1060)
 */

#include "mock_ws_handshake.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace kcenon::network::tests::support
{

namespace
{

constexpr std::string_view kTestNonceB64 = "dGhlIHNhbXBsZSBub25jZQ==";

// Fixed mask used by tests so the masked payload bytes are deterministic and
// byte-comparable in assertions.
constexpr std::array<uint8_t, 4> kTestMask = {0x37, 0xFA, 0x21, 0x3D};

void append_payload_length(std::vector<uint8_t>& frame, std::size_t payload_len, bool masked)
{
    const uint8_t mask_bit = masked ? 0x80 : 0x00;
    if (payload_len < 126)
    {
        frame.push_back(static_cast<uint8_t>(mask_bit | payload_len));
    }
    else if (payload_len <= 0xFFFF)
    {
        frame.push_back(static_cast<uint8_t>(mask_bit | 126));
        frame.push_back(static_cast<uint8_t>((payload_len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payload_len & 0xFF));
    }
    else
    {
        frame.push_back(static_cast<uint8_t>(mask_bit | 127));
        for (int shift = 56; shift >= 0; shift -= 8)
        {
            frame.push_back(static_cast<uint8_t>((payload_len >> shift) & 0xFF));
        }
    }
}

void append_masked_payload(
    std::vector<uint8_t>& frame,
    std::span<const uint8_t> payload)
{
    frame.insert(frame.end(), kTestMask.begin(), kTestMask.end());
    for (std::size_t i = 0; i < payload.size(); ++i)
    {
        frame.push_back(static_cast<uint8_t>(payload[i] ^ kTestMask[i % 4]));
    }
}

[[nodiscard]] std::vector<uint8_t> make_data_frame(
    uint8_t opcode,
    std::span<const uint8_t> payload,
    bool masked)
{
    std::vector<uint8_t> frame;
    frame.reserve(payload.size() + 14);

    // FIN=1, RSV=0, opcode in low 4 bits
    frame.push_back(static_cast<uint8_t>(0x80 | (opcode & 0x0F)));
    append_payload_length(frame, payload.size(), masked);

    if (masked)
    {
        append_masked_payload(frame, payload);
    }
    else
    {
        frame.insert(frame.end(), payload.begin(), payload.end());
    }
    return frame;
}

} // namespace

std::string make_websocket_upgrade_request(
    std::string_view host,
    std::string_view resource)
{
    // Per RFC 6455 §4.1, the request must include Host, Upgrade, Connection,
    // Sec-WebSocket-Key, Sec-WebSocket-Version. Origin is optional but some
    // servers require it.
    std::string req;
    req.reserve(256);
    req.append("GET ").append(resource).append(" HTTP/1.1\r\n");
    req.append("Host: ").append(host).append("\r\n");
    req.append("Upgrade: websocket\r\n");
    req.append("Connection: Upgrade\r\n");
    req.append("Sec-WebSocket-Key: ").append(kTestNonceB64).append("\r\n");
    req.append("Sec-WebSocket-Version: 13\r\n");
    req.append("Origin: http://").append(host).append("\r\n");
    req.append("\r\n");
    return req;
}

std::vector<uint8_t> make_websocket_text_frame(
    std::string_view payload,
    bool masked)
{
    return make_data_frame(
        0x1, // text opcode
        std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(payload.data()), payload.size()),
        masked);
}

std::vector<uint8_t> make_websocket_binary_frame(
    std::span<const uint8_t> payload,
    bool masked)
{
    return make_data_frame(0x2 /* binary opcode */, payload, masked);
}

std::vector<uint8_t> make_websocket_close_frame(
    uint16_t code,
    std::string_view reason)
{
    std::vector<uint8_t> body;
    if (code != 0)
    {
        body.reserve(2 + reason.size());
        body.push_back(static_cast<uint8_t>((code >> 8) & 0xFF));
        body.push_back(static_cast<uint8_t>(code & 0xFF));
        body.insert(body.end(), reason.begin(), reason.end());
    }
    return make_data_frame(0x8 /* close opcode */, body, /*masked=*/true);
}

} // namespace kcenon::network::tests::support
