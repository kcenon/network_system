/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file messaging_session_test.cpp
 * @brief Unit tests for messaging_session class
 *
 * Tests validate:
 * - Construction with connected TCP socket
 * - Session ID property
 * - Connection state tracking
 * - Callback registration
 * - Session stop behavior
 * - Double-stop safety
 * - i_session interface compliance
 */

#include "kcenon/network/detail/session/messaging_session.h"

#include <gtest/gtest.h>

#include <asio.hpp>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::session;

// ============================================================================
// Test Fixture
// ============================================================================

class MessagingSessionTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		io_ctx_ = std::make_unique<asio::io_context>();

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
		// Close sockets first to prevent hanging
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
	std::unique_ptr<asio::ip::tcp::socket> server_socket_;
	std::unique_ptr<asio::ip::tcp::socket> client_socket_;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(MessagingSessionTest, ConstructWithConnectedSocket)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "test-server-1");

	EXPECT_EQ(session->server_id(), "test-server-1");
	EXPECT_EQ(session->id(), "test-server-1");
}

TEST_F(MessagingSessionTest, InitialStateIsNotStopped)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "server-id");

	EXPECT_FALSE(session->is_stopped());
	EXPECT_TRUE(session->is_connected());
}

// ============================================================================
// Session ID Tests
// ============================================================================

TEST_F(MessagingSessionTest, EmptyServerId)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "");

	EXPECT_TRUE(session->server_id().empty());
	EXPECT_TRUE(session->id().empty());
}

TEST_F(MessagingSessionTest, LongServerId)
{
	std::string long_id(256, 'x');
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), long_id);

	EXPECT_EQ(session->server_id(), long_id);
}

// ============================================================================
// Stop Session Tests
// ============================================================================

TEST_F(MessagingSessionTest, StopSession)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "server");

	session->stop_session();

	EXPECT_TRUE(session->is_stopped());
	EXPECT_FALSE(session->is_connected());
}

TEST_F(MessagingSessionTest, DoubleStopIsNoOp)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "server");

	session->stop_session();
	EXPECT_NO_FATAL_FAILURE(session->stop_session());
	EXPECT_TRUE(session->is_stopped());
}

TEST_F(MessagingSessionTest, CloseCallsStop)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "server");

	session->close();

	EXPECT_TRUE(session->is_stopped());
	EXPECT_FALSE(session->is_connected());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(MessagingSessionTest, SetReceiveCallbackDoesNotCrash)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "server");

	EXPECT_NO_FATAL_FAILURE(
		session->set_receive_callback([](const std::vector<uint8_t>&) {}));
}

TEST_F(MessagingSessionTest, SetDisconnectionCallbackDoesNotCrash)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "server");

	EXPECT_NO_FATAL_FAILURE(
		session->set_disconnection_callback([](const std::string&) {}));
}

TEST_F(MessagingSessionTest, SetErrorCallbackDoesNotCrash)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "server");

	EXPECT_NO_FATAL_FAILURE(
		session->set_error_callback([](std::error_code) {}));
}

TEST_F(MessagingSessionTest, SetNullCallbacksDoNotCrash)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "server");

	EXPECT_NO_FATAL_FAILURE(session->set_receive_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(session->set_disconnection_callback(nullptr));
	EXPECT_NO_FATAL_FAILURE(session->set_error_callback(nullptr));
}

// ============================================================================
// Send on Stopped Session Tests
// ============================================================================

TEST_F(MessagingSessionTest, SendOnStoppedSessionReturnsError)
{
	auto session = std::make_shared<messaging_session>(
		std::move(*server_socket_), "server");

	session->stop_session();

	auto result = session->send(std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(MessagingSessionTest, DestructorStopsSession)
{
	{
		auto session = std::make_shared<messaging_session>(
			std::move(*server_socket_), "server");
		// Let session go out of scope
	}
	// No crash = success; destructor calls stop_session internally
}
