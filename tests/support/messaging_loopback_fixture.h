// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file messaging_loopback_fixture.h
 * @brief GTest fixture base for plain-TCP messaging_server / messaging_client
 *        tests (Issue #1088).
 *
 * Provides ephemeral-port allocation so concurrent @c ctest -j N runs do not
 * collide on a fixed port.
 *
 * Distinct from @ref hermetic_transport_fixture, which is for HTTP/2, gRPC,
 * QUIC, and WebSocket peers that require TLS configuration and async I/O
 * driving via a fixture-owned worker thread. This fixture is for the simple
 * @c kcenon::network::core::messaging_server / @c messaging_client TCP flow
 * which manages its own io_context internally.
 */

#include <thread>

#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/tcp.hpp>

#include <gtest/gtest.h>

namespace kcenon::network::tests::support
{

/**
 * @brief Acquire a free TCP port on the loopback interface.
 *
 * Opens a temporary acceptor bound to @c 127.0.0.1:0, reads the
 * kernel-assigned port, then closes the acceptor. There is a small race
 * window where another process could grab the same port; for parallel
 * @c ctest -j N on a single host this is acceptable in practice (and is
 * equivalent to what @ref hermetic_transport_fixture relies on for non-TCP
 * protocols).
 *
 * @return A free TCP port number.
 */
[[nodiscard]] inline unsigned short pick_ephemeral_port()
{
    using asio::ip::tcp;

    asio::io_context probe_io;
    tcp::acceptor probe(probe_io,
                        tcp::endpoint(asio::ip::address_v4::loopback(), 0));
    const auto endpoint = probe.local_endpoint();
    probe.close();
    return endpoint.port();
}

/**
 * @brief Yield repeatedly to allow async server / client setup.
 *
 * Equivalent to the @c WaitForServerReady helper used in the legacy
 * monolithic @c tests/unit_tests.cpp fixture. Kept for behavioural parity so
 * each split test reproduces the original timing.
 */
inline void wait_for_ready()
{
    for (int i = 0; i < 1000; ++i)
    {
        std::this_thread::yield();
    }
}

/**
 * @brief Detect whether the test binary is running under a sanitizer build.
 *
 * Mirrors the legacy free function in @c tests/unit_tests.cpp so individual
 * split files can keep their @c GTEST_SKIP() guards verbatim.
 */
[[nodiscard]] bool is_sanitizer_run() noexcept;

/**
 * @brief Trivial GTest fixture for plain-TCP messaging tests.
 *
 * The fixture body is intentionally empty: each test allocates its own
 * ephemeral port via @ref pick_ephemeral_port() and constructs the server /
 * client locally. This keeps negative tests (port-in-use, double-start,
 * etc.) trivial to express without fighting fixture-owned state.
 */
class messaging_loopback_fixture : public ::testing::Test
{
};

} // namespace kcenon::network::tests::support
