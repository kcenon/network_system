/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/internal/tcp_socket.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <span>
#include <thread>
#include <vector>

using namespace network_system::internal;

/**
 * @file tcp_socket_test.cpp
 * @brief Unit tests for tcp_socket receive callback functionality
 *
 * Tests validate:
 * - Legacy vector callback registration and invocation
 * - New span-based callback registration and invocation
 * - Callback priority (span takes precedence over vector)
 * - Lock-free callback access
 */

// ============================================================================
// Callback Type Tests
// ============================================================================

class TcpSocketCallbackTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		io_context_ = std::make_unique<asio::io_context>();
		work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
			io_context_->get_executor());

		// Start io_context in background thread
		io_thread_ = std::thread([this]()
								 { io_context_->run(); });

		// Setup acceptor
		acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
			*io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
		test_port_ = acceptor_->local_endpoint().port();
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

	auto create_connected_socket_pair()
		-> std::pair<std::shared_ptr<tcp_socket>, std::shared_ptr<tcp_socket>>
	{
		std::promise<std::shared_ptr<tcp_socket>> server_promise;
		auto server_future = server_promise.get_future();

		// Accept connection asynchronously
		acceptor_->async_accept(
			[&server_promise](std::error_code ec, asio::ip::tcp::socket socket)
			{
				if (!ec)
				{
					server_promise.set_value(std::make_shared<tcp_socket>(std::move(socket)));
				}
				else
				{
					server_promise.set_value(nullptr);
				}
			});

		// Connect client synchronously (blocks until connected)
		asio::ip::tcp::socket client_raw_socket(*io_context_);
		std::error_code ec;
		client_raw_socket.connect(
			asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), test_port_), ec);

		if (ec)
		{
			return {nullptr, nullptr};
		}

		auto client_socket = std::make_shared<tcp_socket>(std::move(client_raw_socket));

		// Wait for server to accept
		auto server_socket = server_future.get();

		return {server_socket, client_socket};
	}

	std::unique_ptr<asio::io_context> io_context_;
	std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
	std::thread io_thread_;
	std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
	uint16_t test_port_ = 0;
};

// ============================================================================
// Legacy Vector Callback Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, SetReceiveCallback_LegacyVector)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr) << "Failed to create server socket";
	ASSERT_NE(client, nullptr) << "Failed to create client socket";

	std::promise<std::vector<uint8_t>> data_promise;
	auto data_future = data_promise.get_future();
	std::atomic<bool> promise_set{false};

	server->set_receive_callback(
		[&data_promise, &promise_set](const std::vector<uint8_t>& data)
		{
			bool expected = false;
			if (promise_set.compare_exchange_strong(expected, true))
			{
				data_promise.set_value(data);
			}
		});

	server->start_read();

	// Send data from client
	std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	client->async_send(std::move(test_data),
					   [&send_promise](std::error_code ec, std::size_t)
					   {
						   send_promise.set_value(!ec);
					   });

	// Wait for send to complete
	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	ASSERT_TRUE(send_future.get()) << "Send failed";

	// Wait for receive callback
	ASSERT_TRUE(data_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto received_data = data_future.get();

	EXPECT_EQ(received_data.size(), 4);
	EXPECT_EQ(received_data[0], 0x01);
	EXPECT_EQ(received_data[3], 0x04);

	server->stop_read();
}

// ============================================================================
// New Span Callback Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, SetReceiveCallbackView_Span)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr) << "Failed to create server socket";
	ASSERT_NE(client, nullptr) << "Failed to create client socket";

	std::promise<std::vector<uint8_t>> data_promise;
	auto data_future = data_promise.get_future();
	std::atomic<bool> promise_set{false};

	server->set_receive_callback_view(
		[&data_promise, &promise_set](std::span<const uint8_t> data)
		{
			bool expected = false;
			if (promise_set.compare_exchange_strong(expected, true))
			{
				// Copy data to vector (span is only valid during callback)
				data_promise.set_value(std::vector<uint8_t>(data.begin(), data.end()));
			}
		});

	server->start_read();

	// Send data from client
	std::vector<uint8_t> test_data = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	client->async_send(std::move(test_data),
					   [&send_promise](std::error_code ec, std::size_t)
					   {
						   send_promise.set_value(!ec);
					   });

	// Wait for send to complete
	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	ASSERT_TRUE(send_future.get()) << "Send failed";

	// Wait for receive callback
	ASSERT_TRUE(data_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto received_data = data_future.get();

	EXPECT_EQ(received_data.size(), 5);
	EXPECT_EQ(received_data[0], 0xAA);
	EXPECT_EQ(received_data[4], 0xEE);

	server->stop_read();
}

// ============================================================================
// Callback Priority Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, SpanCallbackTakesPrecedenceOverVector)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr) << "Failed to create server socket";
	ASSERT_NE(client, nullptr) << "Failed to create client socket";

	std::atomic<bool> vector_callback_invoked{false};
	std::promise<bool> span_promise;
	auto span_future = span_promise.get_future();
	std::atomic<bool> span_promise_set{false};

	// Set both callbacks - span should take precedence
	server->set_receive_callback(
		[&vector_callback_invoked](const std::vector<uint8_t>&)
		{
			vector_callback_invoked.store(true);
		});

	server->set_receive_callback_view(
		[&span_promise, &span_promise_set](std::span<const uint8_t>)
		{
			bool expected = false;
			if (span_promise_set.compare_exchange_strong(expected, true))
			{
				span_promise.set_value(true);
			}
		});

	server->start_read();

	// Send data from client
	std::vector<uint8_t> test_data = {0x01, 0x02};
	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	client->async_send(std::move(test_data),
					   [&send_promise](std::error_code ec, std::size_t)
					   {
						   send_promise.set_value(!ec);
					   });

	// Wait for send to complete
	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	ASSERT_TRUE(send_future.get()) << "Send failed";

	// Wait for span callback
	ASSERT_TRUE(span_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	EXPECT_TRUE(span_future.get());

	// Vector callback should NOT be invoked
	EXPECT_FALSE(vector_callback_invoked.load());

	server->stop_read();
}

// ============================================================================
// Error Callback Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, ErrorCallbackOnDisconnect)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr) << "Failed to create server socket";
	ASSERT_NE(client, nullptr) << "Failed to create client socket";

	std::promise<std::error_code> error_promise;
	auto error_future = error_promise.get_future();
	std::atomic<bool> error_promise_set{false};

	server->set_error_callback(
		[&error_promise, &error_promise_set](std::error_code ec)
		{
			bool expected = false;
			if (error_promise_set.compare_exchange_strong(expected, true))
			{
				error_promise.set_value(ec);
			}
		});

	server->start_read();

	// Close client socket to trigger error on server
	client->socket().close();

	// Wait for error callback
	ASSERT_TRUE(error_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto received_error = error_future.get();

	// Error should be EOF or connection reset
	EXPECT_TRUE(received_error == asio::error::eof ||
				received_error == asio::error::connection_reset ||
				received_error == asio::error::broken_pipe);
}

// ============================================================================
// Callback Registration Stress Test
// ============================================================================

TEST_F(TcpSocketCallbackTest, ConcurrentCallbackRegistration)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr) << "Failed to create server socket";
	ASSERT_NE(client, nullptr) << "Failed to create client socket";

	// Test that callback registration is thread-safe
	std::atomic<int> registration_count{0};
	const int num_registrations = 100;

	std::vector<std::thread> threads;
	threads.reserve(num_registrations);

	for (int i = 0; i < num_registrations; ++i)
	{
		threads.emplace_back(
			[&server, &registration_count, i]()
			{
				if (i % 3 == 0)
				{
					server->set_receive_callback([](const std::vector<uint8_t>&) {});
				}
				else if (i % 3 == 1)
				{
					server->set_receive_callback_view([](std::span<const uint8_t>) {});
				}
				else
				{
					server->set_error_callback([](std::error_code) {});
				}
				registration_count.fetch_add(1);
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_EQ(registration_count.load(), num_registrations);
}
