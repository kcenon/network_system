// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.
#pragma once

#if !defined(NETWORK_ENABLE_TEST_INJECTION)
#error "QUIC client test access requires NETWORK_ENABLE_TEST_INJECTION"
#endif

#include "internal/experimental/quic_client.h"
#include "internal/quic_socket.h"

namespace kcenon::network::tests::support
{
class quic_client_test_access
{
public:
    // Attach a real socket for deterministic protocol-event tests. No result
    // values or handshake flags are assigned by this helper.
    static void attach(core::messaging_quic_client& client,
                       std::shared_ptr<internal::quic_socket> socket)
    {
        std::lock_guard<std::mutex> lock(client.socket_mutex_);
        client.socket_ = std::move(socket);
    }
};
} // namespace kcenon::network::tests::support
