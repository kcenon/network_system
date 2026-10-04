/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file secure_session_test.cpp
 * @brief Unit tests for secure_session class
 *
 * Tests validate:
 * - Construction with connected TCP socket and SSL context
 * - Server ID property
 * - Connection state tracking (is_stopped)
 * - Callback registration
 * - Stop session behavior
 * - Double-stop safety
 * - Destructor cleanup
 */

#include "kcenon/network/detail/session/secure_session.h"

#include <gtest/gtest.h>

#include <asio.hpp>
#include <asio/ssl.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::session;

// ============================================================================
// Test Fixture
// ============================================================================

class SecureSessionTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		io_ctx_ = std::make_unique<asio::io_context>();
		ssl_ctx_ = std::make_unique<asio::ssl::context>(asio::ssl::context::tls);

		// Create a connected TCP socket pair using loopback
		asio::ip::tcp::acceptor acceptor(
			*io_ctx_,
			asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0));
		auto port = acceptor.local_endpoint().port();

		asio::ip::tcp::socket client_socket(*io_ctx_);
		client_socket.connect(
			asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port));

		server_socket_ = std::make_unique<asio::ip::tcp::socket>(
			acceptor.accept());
		client_socket_ = std::make_unique<asio::ip::tcp::socket>(
			std::move(client_socket));
	}

	void TearDown() override
	{
		if (client_socket_ && client_socket_->is_open())
		{
			asio::error_code ec;
			client_socket_->close(ec);
		}
		if (server_socket_ && server_socket_->is_open())
		{
			asio::error_code ec;
			server_socket_->close(ec);
		}
		io_ctx_->stop();
	}

	std::unique_ptr<asio::io_context> io_ctx_;
	std::unique_ptr<asio::ssl::context> ssl_ctx_;
	std::unique_ptr<asio::ip::tcp::socket> server_socket_;
	std::unique_ptr<asio::ip::tcp::socket> client_socket_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(SecureSessionTest, ConstructWithConnectedSocket)
{
	auto session = std::make_shared<secure_session>(
		std::move(*server_socket_), *ssl_ctx_, "secure-server-1");

	EXPECT_EQ(session->server_id(), "secure-server-1");
}

TEST_F(SecureSessionTest, InitialStateIsNotStopped)
{
	auto session = std::make_shared<secure_session>(
		std::move(*server_socket_), *ssl_ctx_, "server-id");

	EXPECT_FALSE(session->is_stopped());
}

// ============================================================================
// Server ID Tests
// ============================================================================

TEST_F(SecureSessionTest, EmptyServerId)
{
	auto session = std::make_shared<secure_session>(
		std::move(*server_socket_), *ssl_ctx_, "");

	EXPECT_TRUE(session->server_id().empty());
}

// ============================================================================
// Stop Session Tests
// ============================================================================

TEST_F(SecureSessionTest, StopSession)
{
	auto session = std::make_shared<secure_session>(
		std::move(*server_socket_), *ssl_ctx_, "server");

	session->stop_session();

	EXPECT_TRUE(session->is_stopped());
}

TEST_F(SecureSessionTest, DoubleStopIsNoOp)
{
	auto session = std::make_shared<secure_session>(
		std::move(*server_socket_), *ssl_ctx_, "server");

	session->stop_session();
	EXPECT_NO_FATAL_FAILURE(session->stop_session());
	EXPECT_TRUE(session->is_stopped());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(SecureSessionTest, SetReceiveCallbackDoesNotCrash)
{
	auto session = std::make_shared<secure_session>(
		std::move(*server_socket_), *ssl_ctx_, "server");

	EXPECT_NO_FATAL_FAILURE(
		session->set_receive_callback([](const std::vector<uint8_t>&) {}));
}

TEST_F(SecureSessionTest, SetDisconnectionCallbackDoesNotCrash)
{
	auto session = std::make_shared<secure_session>(
		std::move(*server_socket_), *ssl_ctx_, "server");

	EXPECT_NO_FATAL_FAILURE(
		session->set_disconnection_callback([](const std::string&) {}));
}

TEST_F(SecureSessionTest, SetErrorCallbackDoesNotCrash)
{
	auto session = std::make_shared<secure_session>(
		std::move(*server_socket_), *ssl_ctx_, "server");

	EXPECT_NO_FATAL_FAILURE(
		session->set_error_callback([](std::error_code) {}));
}

TEST_F(SecureSessionTest, SetNullCallbacksDoNotCrash)
{
	auto session = std::make_shared<secure_session>(
		std::move(*server_socket_), *ssl_ctx_, "server");

	EXPECT_NO_FATAL_FAILURE(session->set_receive_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(session->set_disconnection_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(session->set_error_callback(nullptr));
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(SecureSessionTest, DestructorStopsSession)
{
	{
		auto session = std::make_shared<secure_session>(
			std::move(*server_socket_), *ssl_ctx_, "server");
		// Let session go out of scope
	}
	// No crash = success; destructor calls stop_session internally
}
