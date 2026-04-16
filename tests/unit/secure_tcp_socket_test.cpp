/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/tcp/secure_tcp_socket.h"
#include <gtest/gtest.h>

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <functional>
#include <memory>
#include <span>
#include <system_error>
#include <vector>

using namespace kcenon::network::internal;

/**
 * @file secure_tcp_socket_test.cpp
 * @brief Unit tests for secure_tcp_socket
 *
 * Tests validate:
 * - Construction with io_context and ssl_context
 * - Initial closed state
 * - Callback registration (receive, receive_view, error)
 * - stop_read() and close() operations
 * - is_closed() state tracking
 * - stream() and socket() accessors
 * - Multiple close() calls (idempotent)
 */

// ============================================================================
// Test Helpers
// ============================================================================

namespace
{

/**
 * @brief Creates a secure_tcp_socket for testing.
 *
 * Uses a real io_context and ssl_context but does NOT connect
 * the socket to any server. Tests only exercise non-network operations.
 */
class SecureTcpSocketTestFixture : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ssl_ctx_ = std::make_unique<asio::ssl::context>(
			asio::ssl::context::tlsv12_client);

		asio::ip::tcp::socket raw_socket(io_context_);
		socket_ = std::make_shared<secure_tcp_socket>(
			std::move(raw_socket), *ssl_ctx_);
	}

	asio::io_context io_context_;
	std::unique_ptr<asio::ssl::context> ssl_ctx_;
	std::shared_ptr<secure_tcp_socket> socket_;
};

} // namespace

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(SecureTcpSocketTestFixture, Construction)
{
	EXPECT_NE(socket_, nullptr);
}

TEST_F(SecureTcpSocketTestFixture, InitiallyNotClosed)
{
	EXPECT_FALSE(socket_->is_closed());
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST_F(SecureTcpSocketTestFixture, SetReceiveCallback)
{
	bool callback_registered = false;
	socket_->set_receive_callback(
		[&callback_registered](const std::vector<uint8_t>&) {
			callback_registered = true;
		});
	// Callback should be set without error; it is only invoked on data receipt
	EXPECT_FALSE(callback_registered);
}

TEST_F(SecureTcpSocketTestFixture, SetReceiveCallbackView)
{
	bool callback_registered = false;
	socket_->set_receive_callback_view(
		[&callback_registered](std::span<const uint8_t>) {
			callback_registered = true;
		});
	EXPECT_FALSE(callback_registered);
}

TEST_F(SecureTcpSocketTestFixture, SetErrorCallback)
{
	bool callback_registered = false;
	socket_->set_error_callback(
		[&callback_registered](std::error_code) {
			callback_registered = true;
		});
	EXPECT_FALSE(callback_registered);
}

TEST_F(SecureTcpSocketTestFixture, SetNullReceiveCallback)
{
	// Setting a null callback should not crash
	socket_->set_receive_callback(nullptr);
}

TEST_F(SecureTcpSocketTestFixture, SetNullErrorCallback)
{
	socket_->set_error_callback(nullptr);
}

// ============================================================================
// Close and State Tests
// ============================================================================

TEST_F(SecureTcpSocketTestFixture, CloseMarksAsClosed)
{
	socket_->close();
	EXPECT_TRUE(socket_->is_closed());
}

TEST_F(SecureTcpSocketTestFixture, DoubleCloseIsSafe)
{
	socket_->close();
	EXPECT_TRUE(socket_->is_closed());
	// Second close should be idempotent
	socket_->close();
	EXPECT_TRUE(socket_->is_closed());
}

TEST_F(SecureTcpSocketTestFixture, StopReadBeforeStart)
{
	// Stopping read before starting should be safe
	socket_->stop_read();
	EXPECT_FALSE(socket_->is_closed());
}

TEST_F(SecureTcpSocketTestFixture, StopReadThenClose)
{
	socket_->stop_read();
	socket_->close();
	EXPECT_TRUE(socket_->is_closed());
}

// ============================================================================
// Accessor Tests
// ============================================================================

TEST_F(SecureTcpSocketTestFixture, StreamAccessor)
{
	// stream() should return reference without crashing
	auto& stream = socket_->stream();
	(void)stream;
}

TEST_F(SecureTcpSocketTestFixture, SocketAccessor)
{
	// socket() should return lowest_layer reference without crashing
	auto& lowest = socket_->socket();
	(void)lowest;
}
