// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file quic_server_probe.h
 * @brief Friend-access probe for messaging_quic_server::handle_packet
 *        (Issue #1074 Phase 2D).
 *
 * The probe is a static-method forwarder: production code declares
 * @c quic_server_probe a friend of @c messaging_quic_server under the
 * @c NETWORK_ENABLE_TEST_INJECTION gate so tests can drive the previously
 * private @c handle_packet entry point without standing up a live UDP peer.
 */

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_server.h"

#include <asio/ip/udp.hpp>

#include <cstdint>
#include <span>

namespace kcenon::network::tests::support
{

class quic_server_probe
{
public:
    /**
     * @brief Forward to @c messaging_quic_server::handle_packet.
     */
    static auto invoke_handle_packet(
        kcenon::network::core::messaging_quic_server& srv,
        std::span<const std::uint8_t> bytes,
        const asio::ip::udp::endpoint& from) -> void;
};

} // namespace kcenon::network::tests::support
