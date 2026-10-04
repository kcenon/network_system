// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file mock_udp_peer.cpp
 * @brief Implementation of mock UDP peer helpers (Issue #1060)
 */

#include "mock_udp_peer.h"

#include <asio/buffer.hpp>
#include <asio/error.hpp>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <thread>

namespace kcenon::network::tests::support
{

std::vector<uint8_t> mock_udp_peer::receive(
    std::size_t max_size,
    std::chrono::milliseconds timeout)
{
    std::vector<uint8_t> buffer(max_size);
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline)
    {
        if (socket_.available() > 0)
        {
            std::error_code ec;
            const auto n = socket_.receive(asio::buffer(buffer), 0, ec);
            if (ec)
            {
                return {};
            }
            buffer.resize(n);
            return buffer;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return {};
}

std::vector<uint8_t> make_quic_initial_packet_stub(
    std::span<const uint8_t> dcid,
    std::span<const uint8_t> scid)
{
    // Long-header first byte:
    //   bit7 = 1 (long header)
    //   bit6 = 1 (fixed bit per RFC 9000 §17.2)
    //   bit5..4 = 00 (Initial packet type)
    //   bit3..2 = 00 (reserved)
    //   bit1..0 = 00 (smallest packet-number length)
    constexpr uint8_t long_header_initial = 0xC0;

    std::vector<uint8_t> packet;
    packet.reserve(64 + dcid.size() + scid.size());
    packet.push_back(long_header_initial);

    // Version: pick QUIC v1 (0x00000001) so version-negotiation paths are not
    // triggered. Tests that want negotiation can build their own bytes.
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x00);
    packet.push_back(0x01);

    // DCID length + bytes
    assert(dcid.size() <= 20);
    packet.push_back(static_cast<uint8_t>(dcid.size()));
    packet.insert(packet.end(), dcid.begin(), dcid.end());

    // SCID length + bytes
    assert(scid.size() <= 20);
    packet.push_back(static_cast<uint8_t>(scid.size()));
    packet.insert(packet.end(), scid.begin(), scid.end());

    // Token length (varint, single-byte 0).
    packet.push_back(0x00);

    // Length (varint, single-byte 4) + minimal payload (4 bytes).
    packet.push_back(0x04);
    packet.push_back(0x00); // packet number byte
    packet.push_back(0x00); // 3 bytes of garbage frame data; not parseable as
    packet.push_back(0x00); // any real frame, but exercises the entry point.
    packet.push_back(0x00);

    return packet;
}

} // namespace kcenon::network::tests::support
