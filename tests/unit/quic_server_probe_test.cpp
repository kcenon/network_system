// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_server_probe_test.cpp
 * @brief Demo test for friend-test injection on messaging_quic_server
 *        (Issue #1074 Phase 2D).
 *
 * Verifies the previously-private @c handle_packet entry point is reachable
 * from tests via @c quic_server_probe under the NETWORK_ENABLE_TEST_INJECTION
 * gate. The empty-data branch returns early without touching async state, so
 * no live io_context or UDP peer is required.
 */

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_server.h"

#include "quic_server_probe.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <span>

using kcenon::network::core::messaging_quic_server;
using kcenon::network::tests::support::quic_server_probe;

TEST(QuicServerProbeTest, HandlePacketEmptyBufferDoesNotCrash)
{
    messaging_quic_server server("quic-probe-empty");

    std::array<std::uint8_t, 0> empty{};
    asio::ip::udp::endpoint from{};

    // Empty data path: handle_packet must early-return without touching
    // async state on a never-started server.
    EXPECT_NO_FATAL_FAILURE(quic_server_probe::invoke_handle_packet(
        server, std::span<const std::uint8_t>(empty.data(), empty.size()), from));

    EXPECT_FALSE(server.is_running());
}

TEST(QuicServerProbeTest, HandlePacketGarbageBufferDoesNotCrash)
{
    messaging_quic_server server("quic-probe-garbage");

    // Zero-filled buffer is rejected by the QUIC packet parser (parse_header
    // returns is_err()); handle_packet logs and returns without dispatching.
    std::array<std::uint8_t, 32> garbage{};
    asio::ip::udp::endpoint from{};

    EXPECT_NO_FATAL_FAILURE(quic_server_probe::invoke_handle_packet(
        server, std::span<const std::uint8_t>(garbage.data(), garbage.size()), from));

    EXPECT_FALSE(server.is_running());
}
