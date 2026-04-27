// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file hermetic_transport_fixture.h
 * @brief Common GTest fixture base for hermetic transport tests (Issue #1060)
 *
 * Provides a shared @c asio::io_context driven by a worker thread plus utility
 * helpers for hermetic protocol tests. "Hermetic" here means: every socket is
 * bound to @c 127.0.0.1 with a kernel-assigned port, no external network
 * interface or DNS lookup is performed, and the fixture is parallel-safe.
 *
 * Subclasses are typically the existing @c *_branch_test fixtures that need
 * to drive private async/socket-bound methods on protocol clients and servers
 * without standing up a real listener. See:
 *  - @ref mock_tls_socket.h for HTTP/2 + gRPC client/server peers
 *  - @ref mock_udp_peer.h for QUIC client/server peers
 *  - @ref mock_ws_handshake.h for WebSocket server peers
 */

#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

namespace kcenon::network::tests::support
{

/**
 * @brief GTest fixture providing a shared io_context with a worker thread.
 *
 * Each test gets a fresh @c io_context running in a dedicated worker thread.
 * Subclasses can submit async work via @ref io() and synchronize with @ref
 * wait_for(). The worker thread is joined on TearDown.
 */
class hermetic_transport_fixture : public ::testing::Test
{
public:
    hermetic_transport_fixture() = default;
    ~hermetic_transport_fixture() override = default;

protected:
    void SetUp() override
    {
        work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
            io_.get_executor());
        worker_ = std::thread([this]() { io_.run(); });
    }

    void TearDown() override
    {
        if (work_guard_)
        {
            work_guard_->reset();
        }
        io_.stop();
        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    /**
     * @brief Access the shared io_context.
     */
    [[nodiscard]] auto io() -> asio::io_context& { return io_; }

    /**
     * @brief Spin until @p predicate returns true or @p timeout elapses.
     * @return true if the predicate became true, false on timeout.
     *
     * Polls every 5ms — adequate for unit-test scale without burning CPU.
     */
    [[nodiscard]] static auto wait_for(
        std::function<bool()> predicate,
        std::chrono::milliseconds timeout = std::chrono::seconds(2)) -> bool
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (predicate())
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return predicate();
    }

private:
    asio::io_context io_;
    std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
    std::thread worker_;
};

/**
 * @brief Construct a connected pair of @c asio::ip::tcp::socket objects on
 *        the loopback interface.
 *
 * Uses an ephemeral acceptor on @c 127.0.0.1:0 — the kernel picks a free port,
 * so concurrent test executions never collide. The acceptor is bound, the
 * client connects synchronously, and both halves are returned.
 *
 * @param io io_context to associate the sockets with.
 * @return std::pair where @c first is the "client" side and @c second is the
 *         "server" side (the socket returned by @c accept).
 */
[[nodiscard]] std::pair<asio::ip::tcp::socket, asio::ip::tcp::socket>
make_loopback_tcp_pair(asio::io_context& io);

/**
 * @brief Construct two @c asio::ip::udp::socket objects bound on the loopback
 *        interface and connected to each other's endpoint.
 *
 * Both sockets bind to @c 127.0.0.1:0; after binding, the resolved endpoints
 * are exchanged via @c connect() so each socket has a fixed remote.
 *
 * @param io io_context to associate the sockets with.
 * @return std::pair of two connected UDP sockets (caller decides client/server
 *         role assignment).
 */
[[nodiscard]] std::pair<asio::ip::udp::socket, asio::ip::udp::socket>
make_loopback_udp_pair(asio::io_context& io);

} // namespace kcenon::network::tests::support
