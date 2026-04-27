// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file hermetic_transport_fixture.cpp
 * @brief Implementation of hermetic transport fixture helpers (Issue #1060)
 */

#include "hermetic_transport_fixture.h"

#include <asio/connect.hpp>
#include <asio/ip/address_v4.hpp>

#include <stdexcept>
#include <utility>

namespace kcenon::network::tests::support
{

std::pair<asio::ip::tcp::socket, asio::ip::tcp::socket>
make_loopback_tcp_pair(asio::io_context& io)
{
    using asio::ip::tcp;

    // Listen on a kernel-assigned port on the loopback interface so concurrent
    // tests never collide on a fixed port.
    tcp::acceptor acceptor(io, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
    const auto endpoint = acceptor.local_endpoint();

    tcp::socket client_side(io);
    tcp::socket accepted_side(io);

    // Asynchronously accept; the connect() call below drives the io_context
    // worker that the fixture owns.
    std::error_code accept_ec;
    acceptor.async_accept(
        accepted_side,
        [&accept_ec](const std::error_code& ec) { accept_ec = ec; });

    // Synchronous connect from the client side; the matching async_accept
    // completes on the worker thread.
    std::error_code connect_ec;
    client_side.connect(endpoint, connect_ec);
    if (connect_ec)
    {
        throw std::runtime_error("loopback tcp connect failed: " + connect_ec.message());
    }

    // Spin briefly waiting for the accept handler to run.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!accepted_side.is_open() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    if (!accepted_side.is_open())
    {
        throw std::runtime_error("loopback tcp accept did not complete");
    }
    if (accept_ec)
    {
        throw std::runtime_error("loopback tcp accept failed: " + accept_ec.message());
    }

    return {std::move(client_side), std::move(accepted_side)};
}

std::pair<asio::ip::udp::socket, asio::ip::udp::socket>
make_loopback_udp_pair(asio::io_context& io)
{
    using asio::ip::udp;

    udp::socket a(io, udp::endpoint(asio::ip::address_v4::loopback(), 0));
    udp::socket b(io, udp::endpoint(asio::ip::address_v4::loopback(), 0));

    // Connect each socket to the other's bound endpoint so subsequent
    // send()/receive() calls have a fixed peer. Tests can still use
    // send_to()/receive_from() if they need explicit endpoint semantics.
    std::error_code ec_a;
    a.connect(b.local_endpoint(), ec_a);
    if (ec_a)
    {
        throw std::runtime_error("loopback udp connect (a -> b) failed: " + ec_a.message());
    }

    std::error_code ec_b;
    b.connect(a.local_endpoint(), ec_b);
    if (ec_b)
    {
        throw std::runtime_error("loopback udp connect (b -> a) failed: " + ec_b.message());
    }

    return {std::move(a), std::move(b)};
}

} // namespace kcenon::network::tests::support
