/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/tcp/tcp_socket.h"
#include "kcenon/network-core/interfaces/socket_observer.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <numeric>
#include <span>
#include <thread>
#include <vector>

using namespace kcenon::network::internal;
using namespace kcenon::network_core::interfaces;

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

		// Setup acceptor BEFORE starting io_context thread to avoid data race
		// ThreadSanitizer detected race condition when acceptor is created
		// while io_context thread is already running
		acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
			*io_context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));
		test_port_ = acceptor_->local_endpoint().port();

		// Start io_context in background thread AFTER acceptor setup
		io_thread_ = std::thread([this]()
								 { io_context_->run(); });
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

// ============================================================================
// Close Lifecycle Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, CloseTransitionsToClosedState)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	EXPECT_FALSE(server->is_closed());
	EXPECT_FALSE(client->is_closed());

	server->close();
	EXPECT_TRUE(server->is_closed());

	client->close();
	EXPECT_TRUE(client->is_closed());
}

TEST_F(TcpSocketCallbackTest, DoubleCloseDoesNotCrash)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);

	EXPECT_NO_THROW(server->close());
	EXPECT_NO_THROW(server->close());
	EXPECT_TRUE(server->is_closed());
}

TEST_F(TcpSocketCallbackTest, StopReadAfterCloseDoesNotCrash)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);

	server->start_read();
	server->close();

	EXPECT_NO_THROW(server->stop_read());
}

// ============================================================================
// Multiple Sequential Sends Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, MultipleSequentialSends)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	std::atomic<size_t> total_received{0};
	std::promise<void> done_promise;
	auto done_future = done_promise.get_future();
	std::atomic<bool> done_set{false};

	constexpr size_t kNumSends = 5;
	constexpr size_t kDataSize = 64;
	constexpr size_t kExpectedTotal = kNumSends * kDataSize;

	server->set_receive_callback(
		[&](const std::vector<uint8_t>& data)
		{
			total_received.fetch_add(data.size());
			if (total_received.load() >= kExpectedTotal)
			{
				bool expected = false;
				if (done_set.compare_exchange_strong(expected, true))
				{
					done_promise.set_value();
				}
			}
		});

	server->start_read();

	for (size_t i = 0; i < kNumSends; ++i)
	{
		std::vector<uint8_t> data(kDataSize, static_cast<uint8_t>(i));
		std::promise<bool> send_promise;
		auto send_future = send_promise.get_future();

		client->async_send(std::move(data),
						   [&send_promise](std::error_code ec, std::size_t)
						   {
							   send_promise.set_value(!ec);
						   });

		ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
		ASSERT_TRUE(send_future.get()) << "Send " << i << " failed";
	}

	ASSERT_TRUE(done_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	EXPECT_GE(total_received.load(), kExpectedTotal);

	const auto& metrics = client->metrics();
	EXPECT_EQ(metrics.send_count.load(), kNumSends);

	server->stop_read();
}

// ============================================================================
// Bidirectional Communication Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, BidirectionalCommunication)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	std::promise<std::vector<uint8_t>> server_rx_promise;
	auto server_rx_future = server_rx_promise.get_future();
	std::atomic<bool> server_rx_set{false};

	std::promise<std::vector<uint8_t>> client_rx_promise;
	auto client_rx_future = client_rx_promise.get_future();
	std::atomic<bool> client_rx_set{false};

	server->set_receive_callback(
		[&](const std::vector<uint8_t>& data)
		{
			bool expected = false;
			if (server_rx_set.compare_exchange_strong(expected, true))
			{
				server_rx_promise.set_value(data);
			}
		});

	client->set_receive_callback(
		[&](const std::vector<uint8_t>& data)
		{
			bool expected = false;
			if (client_rx_set.compare_exchange_strong(expected, true))
			{
				client_rx_promise.set_value(data);
			}
		});

	server->start_read();
	client->start_read();

	// Client sends to server
	std::vector<uint8_t> client_msg = {0x10, 0x20, 0x30};
	{
		std::promise<bool> p;
		auto f = p.get_future();
		client->async_send(std::move(client_msg),
						   [&p](std::error_code ec, std::size_t) { p.set_value(!ec); });
		ASSERT_TRUE(f.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
		ASSERT_TRUE(f.get());
	}

	ASSERT_TRUE(server_rx_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto server_received = server_rx_future.get();
	EXPECT_EQ(server_received.size(), 3);
	EXPECT_EQ(server_received[0], 0x10);

	// Server sends back to client
	std::vector<uint8_t> server_msg = {0xA0, 0xB0};
	{
		std::promise<bool> p;
		auto f = p.get_future();
		server->async_send(std::move(server_msg),
						   [&p](std::error_code ec, std::size_t) { p.set_value(!ec); });
		ASSERT_TRUE(f.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
		ASSERT_TRUE(f.get());
	}

	ASSERT_TRUE(client_rx_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto client_received = client_rx_future.get();
	EXPECT_EQ(client_received.size(), 2);
	EXPECT_EQ(client_received[0], 0xA0);

	server->stop_read();
	client->stop_read();
}

// ============================================================================
// Large Data Transfer Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, LargeDataTransfer)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	constexpr size_t kLargeSize = 64 * 1024; // 64KB
	std::atomic<size_t> total_received{0};
	std::promise<void> done_promise;
	auto done_future = done_promise.get_future();
	std::atomic<bool> done_set{false};

	server->set_receive_callback(
		[&](const std::vector<uint8_t>& data)
		{
			total_received.fetch_add(data.size());
			if (total_received.load() >= kLargeSize)
			{
				bool expected = false;
				if (done_set.compare_exchange_strong(expected, true))
				{
					done_promise.set_value();
				}
			}
		});

	server->start_read();

	// Create large payload with pattern
	std::vector<uint8_t> large_data(kLargeSize);
	std::iota(large_data.begin(), large_data.end(), static_cast<uint8_t>(0));

	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	client->async_send(std::move(large_data),
					   [&send_promise](std::error_code ec, std::size_t)
					   {
						   send_promise.set_value(!ec);
					   });

	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	ASSERT_TRUE(send_future.get());

	ASSERT_TRUE(done_future.wait_for(std::chrono::seconds(10)) == std::future_status::ready);
	EXPECT_GE(total_received.load(), kLargeSize);

	server->stop_read();
}

// ============================================================================
// Observer Pattern Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, ObserverReceivesData)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	std::promise<std::vector<uint8_t>> rx_promise;
	auto rx_future = rx_promise.get_future();
	std::atomic<bool> rx_set{false};

	auto adapter = std::make_shared<socket_callback_adapter>();
	adapter->on_receive(
		[&](std::span<const uint8_t> data)
		{
			bool expected = false;
			if (rx_set.compare_exchange_strong(expected, true))
			{
				rx_promise.set_value(std::vector<uint8_t>(data.begin(), data.end()));
			}
		});

	server->attach_observer(adapter);
	server->start_read();

	std::vector<uint8_t> test_data = {0xDE, 0xAD, 0xBE, 0xEF};
	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	client->async_send(std::move(test_data),
					   [&send_promise](std::error_code ec, std::size_t)
					   {
						   send_promise.set_value(!ec);
					   });

	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	ASSERT_TRUE(send_future.get());

	ASSERT_TRUE(rx_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto received = rx_future.get();
	EXPECT_EQ(received.size(), 4);
	EXPECT_EQ(received[0], 0xDE);
	EXPECT_EQ(received[3], 0xEF);

	server->stop_read();
	server->detach_observer(adapter);
}

TEST_F(TcpSocketCallbackTest, ObserverReceivesError)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	std::promise<std::error_code> error_promise;
	auto error_future = error_promise.get_future();
	std::atomic<bool> error_set{false};

	auto adapter = std::make_shared<socket_callback_adapter>();
	adapter->on_error(
		[&](std::error_code ec)
		{
			bool expected = false;
			if (error_set.compare_exchange_strong(expected, true))
			{
				error_promise.set_value(ec);
			}
		});

	server->attach_observer(adapter);
	server->start_read();

	// Close client to trigger error on server
	client->socket().close();

	ASSERT_TRUE(error_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto ec = error_future.get();
	EXPECT_TRUE(ec == asio::error::eof ||
				ec == asio::error::connection_reset ||
				ec == asio::error::broken_pipe);

	server->detach_observer(adapter);
}

TEST_F(TcpSocketCallbackTest, DetachObserverStopsNotifications)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	std::atomic<int> receive_count{0};

	auto adapter = std::make_shared<socket_callback_adapter>();
	adapter->on_receive(
		[&](std::span<const uint8_t>)
		{
			receive_count.fetch_add(1);
		});

	server->attach_observer(adapter);
	server->start_read();

	// Send first message
	{
		std::vector<uint8_t> data = {0x01};
		std::promise<bool> p;
		auto f = p.get_future();
		client->async_send(std::move(data),
						   [&p](std::error_code ec, std::size_t) { p.set_value(!ec); });
		ASSERT_TRUE(f.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
		ASSERT_TRUE(f.get());
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	int count_before_detach = receive_count.load();
	EXPECT_GE(count_before_detach, 1);

	// Detach observer
	server->detach_observer(adapter);

	// Send second message — observer should NOT receive it
	{
		std::vector<uint8_t> data = {0x02};
		std::promise<bool> p;
		auto f = p.get_future();
		client->async_send(std::move(data),
						   [&p](std::error_code ec, std::size_t) { p.set_value(!ec); });
		ASSERT_TRUE(f.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(receive_count.load(), count_before_detach);

	server->stop_read();
}

// ============================================================================
// Error Path Tests
// ============================================================================

TEST_F(TcpSocketCallbackTest, ConnectToNonListeningPortFails)
{
	// Use a port that is very likely not listening
	asio::ip::tcp::socket raw_socket(*io_context_);
	std::error_code ec;
	raw_socket.connect(
		asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 1), ec);

	// Should fail with connection refused or similar
	EXPECT_TRUE(ec) << "Expected connection to port 1 to fail";
}

TEST_F(TcpSocketCallbackTest, SendAfterCloseTriggersError)
{
	auto [server, client] = create_connected_socket_pair();
	ASSERT_NE(server, nullptr);
	ASSERT_NE(client, nullptr);

	client->close();
	EXPECT_TRUE(client->is_closed());

	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();
	std::atomic<bool> promise_set{false};

	std::vector<uint8_t> data = {0x01, 0x02};
	client->async_send(std::move(data),
					   [&send_promise, &promise_set](std::error_code ec, std::size_t)
					   {
						   bool expected = false;
						   if (promise_set.compare_exchange_strong(expected, true))
						   {
							   send_promise.set_value(!ec);
						   }
					   });

	// Either the send fails immediately or the callback reports error
	if (send_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready)
	{
		// If callback was invoked, it should report failure
		EXPECT_FALSE(send_future.get());
	}
}
