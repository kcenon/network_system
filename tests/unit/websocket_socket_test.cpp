/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/websocket/websocket_socket.h"
#include "internal/tcp/tcp_socket.h"
#include "kcenon/network-core/interfaces/socket_observer.h"
#include <gtest/gtest.h>

#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::internal;

/**
 * @file websocket_socket_test.cpp
 * @brief Unit tests for websocket_socket
 *
 * Tests validate:
 * - ws_state enum values
 * - ws_close_code enum values
 * - ws_message structure
 * - websocket_socket construction (client and server mode)
 * - Initial state (connecting, not open)
 * - Callback registration (message, ping, pong, close, error)
 */

// ============================================================================
// ws_state Enum Tests
// ============================================================================

class WsStateEnumTest : public ::testing::Test
{
};

TEST_F(WsStateEnumTest, StateValues)
{
	EXPECT_NE(ws_state::connecting, ws_state::open);
	EXPECT_NE(ws_state::open, ws_state::closing);
	EXPECT_NE(ws_state::closing, ws_state::closed);
}

// ============================================================================
// ws_close_code Enum Tests
// ============================================================================

class WsCloseCodeTest : public ::testing::Test
{
};

TEST_F(WsCloseCodeTest, NormalClose)
{
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::normal), 1000u);
}

TEST_F(WsCloseCodeTest, GoingAway)
{
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::going_away), 1001u);
}

TEST_F(WsCloseCodeTest, ProtocolError)
{
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::protocol_error), 1002u);
}

TEST_F(WsCloseCodeTest, UnsupportedData)
{
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::unsupported_data), 1003u);
}

TEST_F(WsCloseCodeTest, InvalidFrame)
{
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::invalid_frame), 1007u);
}

TEST_F(WsCloseCodeTest, PolicyViolation)
{
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::policy_violation), 1008u);
}

TEST_F(WsCloseCodeTest, MessageTooBig)
{
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::message_too_big), 1009u);
}

TEST_F(WsCloseCodeTest, InternalError)
{
	EXPECT_EQ(static_cast<uint16_t>(ws_close_code::internal_error), 1011u);
}

// ============================================================================
// ws_message_type Enum Tests
// ============================================================================

class WsMessageTypeTest : public ::testing::Test
{
};

TEST_F(WsMessageTypeTest, TypeValues)
{
	EXPECT_NE(ws_message_type::text, ws_message_type::binary);
}

// ============================================================================
// websocket_socket Construction Tests
// ============================================================================

class WebsocketSocketConstructionTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		io_context_ = std::make_unique<asio::io_context>();
		work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
			io_context_->get_executor());

		// Create a simple TCP socket pair for testing
		acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
			*io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
		test_port_ = acceptor_->local_endpoint().port();

		io_thread_ = std::thread([this]() { io_context_->run(); });
	}

	void TearDown() override
	{
		if (acceptor_ && acceptor_->is_open())
		{
			acceptor_->close();
		}
		acceptor_.reset();

		work_guard_.reset();
		if (io_context_)
		{
			io_context_->stop();
		}
		if (io_thread_.joinable())
		{
			io_thread_.join();
		}
		io_context_.reset();
	}

	auto create_connected_tcp_pair()
		-> std::pair<std::shared_ptr<tcp_socket>, std::shared_ptr<tcp_socket>>
	{
		std::promise<asio::ip::tcp::socket> accept_promise;
		auto accept_future = accept_promise.get_future();

		acceptor_->async_accept(
			[&accept_promise](std::error_code ec, asio::ip::tcp::socket sock) {
				if (!ec)
				{
					accept_promise.set_value(std::move(sock));
				}
			});

		asio::ip::tcp::socket client_sock(*io_context_);
		client_sock.connect(
			asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), test_port_));

		auto server_sock = accept_future.get();

		auto client_tcp = std::make_shared<tcp_socket>(std::move(client_sock));
		auto server_tcp = std::make_shared<tcp_socket>(std::move(server_sock));

		return {client_tcp, server_tcp};
	}

	std::unique_ptr<asio::io_context> io_context_;
	std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
	std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
	uint16_t test_port_ = 0;
	std::thread io_thread_;
};

TEST_F(WebsocketSocketConstructionTest, ClientModeConstruction)
{
	auto [client_tcp, server_tcp] = create_connected_tcp_pair();

	auto ws_socket = std::make_shared<websocket_socket>(client_tcp, true);

	EXPECT_EQ(ws_socket->state(), ws_state::connecting);
	EXPECT_FALSE(ws_socket->is_open());
}

TEST_F(WebsocketSocketConstructionTest, ServerModeConstruction)
{
	auto [client_tcp, server_tcp] = create_connected_tcp_pair();

	auto ws_socket = std::make_shared<websocket_socket>(server_tcp, false);

	EXPECT_EQ(ws_socket->state(), ws_state::connecting);
	EXPECT_FALSE(ws_socket->is_open());
}

// ============================================================================
// websocket_socket Callback Registration Tests
// ============================================================================

class WebsocketSocketCallbackTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		io_context_ = std::make_unique<asio::io_context>();
		work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
			io_context_->get_executor());

		acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
			*io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
		test_port_ = acceptor_->local_endpoint().port();

		io_thread_ = std::thread([this]() { io_context_->run(); });

		// Create a connected TCP pair
		std::promise<asio::ip::tcp::socket> accept_promise;
		auto accept_future = accept_promise.get_future();

		acceptor_->async_accept(
			[&accept_promise](std::error_code ec, asio::ip::tcp::socket sock) {
				if (!ec)
				{
					accept_promise.set_value(std::move(sock));
				}
			});

		asio::ip::tcp::socket client_sock(*io_context_);
		client_sock.connect(
			asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), test_port_));

		auto server_sock = accept_future.get();

		auto tcp = std::make_shared<tcp_socket>(std::move(client_sock));
		ws_socket_ = std::make_shared<websocket_socket>(tcp, true);
	}

	void TearDown() override
	{
		ws_socket_.reset();
		if (acceptor_ && acceptor_->is_open())
		{
			acceptor_->close();
		}
		acceptor_.reset();
		work_guard_.reset();
		if (io_context_)
		{
			io_context_->stop();
		}
		if (io_thread_.joinable())
		{
			io_thread_.join();
		}
		io_context_.reset();
	}

	std::unique_ptr<asio::io_context> io_context_;
	std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
	std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
	uint16_t test_port_ = 0;
	std::thread io_thread_;
	std::shared_ptr<websocket_socket> ws_socket_;
};

TEST_F(WebsocketSocketCallbackTest, SetMessageCallback)
{
	bool callback_set = false;

	EXPECT_NO_THROW(ws_socket_->set_message_callback(
		[&callback_set](const ws_message& msg) {
			callback_set = true;
		}));
}

TEST_F(WebsocketSocketCallbackTest, SetPingCallback)
{
	EXPECT_NO_THROW(ws_socket_->set_ping_callback(
		[](const std::vector<uint8_t>& payload) {}));
}

TEST_F(WebsocketSocketCallbackTest, SetPongCallback)
{
	EXPECT_NO_THROW(ws_socket_->set_pong_callback(
		[](const std::vector<uint8_t>& payload) {}));
}

TEST_F(WebsocketSocketCallbackTest, SetCloseCallback)
{
	EXPECT_NO_THROW(ws_socket_->set_close_callback(
		[](ws_close_code code, const std::string& reason) {}));
}

TEST_F(WebsocketSocketCallbackTest, SetErrorCallback)
{
	EXPECT_NO_THROW(ws_socket_->set_error_callback(
		[](std::error_code ec) {}));
}
