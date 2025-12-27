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

using namespace kcenon::network::internal;

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

// ============================================================================
// Socket Configuration Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, SocketConfig_DefaultValues)
{
	socket_config config;

	// Default values should be backward compatible (unlimited)
	EXPECT_EQ(config.max_pending_bytes, 0);
	EXPECT_EQ(config.high_water_mark, 1024 * 1024);  // 1MB
	EXPECT_EQ(config.low_water_mark, 256 * 1024);    // 256KB
}

TEST_F(TcpSocketCallbackTest, SocketWithConfig_CustomValues)
{
	asio::ip::tcp::socket raw_socket(*io_context_);
	socket_config config;
	config.max_pending_bytes = 1024 * 1024;  // 1MB
	config.high_water_mark = 512 * 1024;     // 512KB
	config.low_water_mark = 128 * 1024;      // 128KB

	auto socket = std::make_shared<tcp_socket>(std::move(raw_socket), config);

	EXPECT_EQ(socket->config().max_pending_bytes, 1024 * 1024);
	EXPECT_EQ(socket->config().high_water_mark, 512 * 1024);
	EXPECT_EQ(socket->config().low_water_mark, 128 * 1024);
}

// ============================================================================
// Pending Bytes Tracking Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, PendingBytes_InitiallyZero)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	EXPECT_EQ(server->pending_bytes(), 0);
	EXPECT_EQ(client->pending_bytes(), 0);
}

TEST_F(TcpSocketCallbackTest, PendingBytes_TrackedDuringSend)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	// Create large buffer to ensure we can observe pending bytes
	std::vector<uint8_t> test_data(4096, 0xAA);
	std::size_t data_size = test_data.size();

	// Start reading to consume data
	server->start_read();

	client->async_send(std::move(test_data),
					   [&send_promise](std::error_code ec, std::size_t)
					   {
						   send_promise.set_value(!ec);
					   });

	// Wait for send to complete
	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	ASSERT_TRUE(send_future.get());

	// After completion, pending bytes should be 0
	// (Note: might need a small delay for completion handler to run)
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(client->pending_bytes(), 0);

	server->stop_read();
}

// ============================================================================
// Socket Metrics Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, Metrics_InitiallyZero)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);

	const auto& metrics = server->metrics();
	EXPECT_EQ(metrics.total_bytes_sent.load(), 0);
	EXPECT_EQ(metrics.total_bytes_received.load(), 0);
	EXPECT_EQ(metrics.current_pending_bytes.load(), 0);
	EXPECT_EQ(metrics.backpressure_events.load(), 0);
	EXPECT_EQ(metrics.rejected_sends.load(), 0);
	EXPECT_EQ(metrics.send_count.load(), 0);
}

TEST_F(TcpSocketCallbackTest, Metrics_TrackedAfterSend)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	// Start reading to consume data
	server->start_read();

	// Send some data
	std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	client->async_send(std::move(test_data),
					   [&send_promise](std::error_code ec, std::size_t)
					   {
						   send_promise.set_value(!ec);
					   });

	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	ASSERT_TRUE(send_future.get());

	// Give time for completion handler
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	const auto& metrics = client->metrics();
	EXPECT_EQ(metrics.total_bytes_sent.load(), 4);
	EXPECT_EQ(metrics.send_count.load(), 1);

	server->stop_read();
}

TEST_F(TcpSocketCallbackTest, Metrics_Reset)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	server->start_read();

	// Send some data
	std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04};
	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	client->async_send(std::move(test_data),
					   [&send_promise](std::error_code ec, std::size_t)
					   {
						   send_promise.set_value(!ec);
					   });

	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Reset metrics
	client->reset_metrics();

	const auto& metrics = client->metrics();
	EXPECT_EQ(metrics.total_bytes_sent.load(), 0);
	EXPECT_EQ(metrics.send_count.load(), 0);

	server->stop_read();
}

// ============================================================================
// try_send Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, TrySend_SucceedsWhenUnderLimit)
{
	asio::ip::tcp::socket raw_socket(*io_context_);
	std::error_code ec;
	raw_socket.connect(
		asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), test_port_), ec);

	// Create socket with limit
	socket_config config;
	config.max_pending_bytes = 10 * 1024;  // 10KB limit

	// Wait for server to accept
	std::promise<std::shared_ptr<tcp_socket>> server_promise;
	auto server_future = server_promise.get_future();
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

	auto client = std::make_shared<tcp_socket>(std::move(raw_socket), config);
	auto server = server_future.get();
	ASSERT_NE(server, nullptr);

	server->start_read();

	// try_send with data under limit should succeed
	std::vector<uint8_t> test_data(1024, 0xAA);  // 1KB, under 10KB limit
	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	bool result = client->try_send(std::move(test_data),
								   [&send_promise](std::error_code ec, std::size_t)
								   {
									   send_promise.set_value(!ec);
								   });

	EXPECT_TRUE(result);
	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	EXPECT_TRUE(send_future.get());

	server->stop_read();
}

// ============================================================================
// Backpressure Callback Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, BackpressureCallback_SetAndGet)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);

	std::atomic<bool> callback_invoked{false};
	std::atomic<bool> backpressure_value{false};

	server->set_backpressure_callback(
		[&callback_invoked, &backpressure_value](bool apply)
		{
			callback_invoked.store(true);
			backpressure_value.store(apply);
		});

	// Initially, backpressure should not be active
	EXPECT_FALSE(server->is_backpressure_active());
}

TEST_F(TcpSocketCallbackTest, BackpressureActive_InitiallyFalse)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);

	EXPECT_FALSE(server->is_backpressure_active());
	EXPECT_FALSE(client->is_backpressure_active());
}
