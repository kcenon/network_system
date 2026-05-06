// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#include "quic_server_probe.h"

namespace kcenon::network::tests::support
{

auto quic_server_probe::invoke_handle_packet(
    kcenon::network::core::messaging_quic_server& srv,
    std::span<const std::uint8_t> bytes,
    const asio::ip::udp::endpoint& from) -> void
{
    srv.handle_packet(bytes, from);
}

} // namespace kcenon::network::tests::support
