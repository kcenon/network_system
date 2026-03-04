/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file udp_socket_test.cpp
 * @brief Unit tests for raw udp_socket class
 *
 * Tests validate:
 * - UDP socket construction and close lifecycle
 * - Datagram send/receive on loopback
 * - Multiple sequential datagrams
 * - Callback registration (receive, error)
 * - Close state transitions and double-close safety
 * - Stop receive lifecycle
 */

#include "internal/udp/udp_socket.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

using namespace kcenon::network::internal;

// ============================================================================
// Test Fixture
// ============================================================================

class UdpSocketTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		io_context_ = std::make_unique<asio::io_context>();
		work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
			io_context_->get_executor());

		io_thread_ = std::thread([this]() { io_context_->run(); });
	}

	void TearDown() override
	{
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

	auto create_bound_socket() -> std::pair<std::shared_ptr<udp_socket>, uint16_t>
	{
		asio::ip::udp::socket raw(*io_context_, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
		uint16_t port = raw.local_endpoint().port();
		auto sock = std::make_shared<udp_socket>(std::move(raw));
		return {sock, port};
	}

	std::unique_ptr<asio::io_context> io_context_;
	std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
	std::thread io_thread_;
};

// ============================================================================
// Construction and Close Lifecycle Tests
// ============================================================================

TEST_F(UdpSocketTest, ConstructionFromOpenSocket)
{
	auto [sock, port] = create_bound_socket();
	ASSERT_NE(sock, nullptr);
	EXPECT_FALSE(sock->is_closed());
	EXPECT_GT(port, 0);
}

TEST_F(UdpSocketTest, CloseTransitionsToClosedState)
{
	auto [sock, port] = create_bound_socket();
	ASSERT_NE(sock, nullptr);

	EXPECT_FALSE(sock->is_closed());
	sock->close();
	EXPECT_TRUE(sock->is_closed());
}

TEST_F(UdpSocketTest, DoubleCloseDoesNotCrash)
{
	auto [sock, port] = create_bound_socket();
	ASSERT_NE(sock, nullptr);

	EXPECT_NO_THROW(sock->close());
	EXPECT_NO_THROW(sock->close());
	EXPECT_TRUE(sock->is_closed());
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST_F(UdpSocketTest, SetReceiveCallbackDoesNotThrow)
{
	auto [sock, port] = create_bound_socket();

	EXPECT_NO_THROW(sock->set_receive_callback(
		[](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {}));
}

TEST_F(UdpSocketTest, SetErrorCallbackDoesNotThrow)
{
	auto [sock, port] = create_bound_socket();

	EXPECT_NO_THROW(sock->set_error_callback([](std::error_code) {}));
}

TEST_F(UdpSocketTest, SetNullCallbacksDoNotThrow)
{
	auto [sock, port] = create_bound_socket();

	EXPECT_NO_THROW(sock->set_receive_callback(nullptr));
	EXPECT_NO_THROW(sock->set_error_callback(nullptr));
}

// ============================================================================
// Datagram Send/Receive Tests
// ============================================================================

TEST_F(UdpSocketTest, SendAndReceiveOnLoopback)
{
	auto [receiver, rx_port] = create_bound_socket();
	auto [sender, tx_port] = create_bound_socket();
	ASSERT_NE(receiver, nullptr);
	ASSERT_NE(sender, nullptr);

	std::promise<std::vector<uint8_t>> rx_promise;
	auto rx_future = rx_promise.get_future();
	std::atomic<bool> rx_set{false};

	receiver->set_receive_callback(
		[&rx_promise, &rx_set](const std::vector<uint8_t>& data,
							   const asio::ip::udp::endpoint&)
		{
			bool expected = false;
			if (rx_set.compare_exchange_strong(expected, true))
			{
				rx_promise.set_value(data);
			}
		});

	receiver->start_receive();

	// Send datagram
	std::vector<uint8_t> test_data = {0xDE, 0xAD, 0xBE, 0xEF};
	asio::ip::udp::endpoint target(asio::ip::make_address("127.0.0.1"), rx_port);

	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();

	sender->async_send_to(std::move(test_data), target,
						  [&send_promise](std::error_code ec, std::size_t)
						  {
							  send_promise.set_value(!ec);
						  });

	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	ASSERT_TRUE(send_future.get()) << "Send failed";

	ASSERT_TRUE(rx_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto received = rx_future.get();

	EXPECT_EQ(received.size(), 4);
	EXPECT_EQ(received[0], 0xDE);
	EXPECT_EQ(received[3], 0xEF);

	receiver->stop_receive();
}

TEST_F(UdpSocketTest, MultipleSequentialDatagrams)
{
	auto [receiver, rx_port] = create_bound_socket();
	auto [sender, tx_port] = create_bound_socket();
	ASSERT_NE(receiver, nullptr);
	ASSERT_NE(sender, nullptr);

	constexpr int kNumDatagrams = 5;
	std::atomic<int> received_count{0};
	std::promise<void> done_promise;
	auto done_future = done_promise.get_future();
	std::atomic<bool> done_set{false};

	receiver->set_receive_callback(
		[&](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
		{
			if (received_count.fetch_add(1) + 1 >= kNumDatagrams)
			{
				bool expected = false;
				if (done_set.compare_exchange_strong(expected, true))
				{
					done_promise.set_value();
				}
			}
		});

	receiver->start_receive();

	asio::ip::udp::endpoint target(asio::ip::make_address("127.0.0.1"), rx_port);

	for (int i = 0; i < kNumDatagrams; ++i)
	{
		std::vector<uint8_t> data = {static_cast<uint8_t>(i), 0x00, 0xFF};
		std::promise<bool> send_promise;
		auto send_future = send_promise.get_future();

		sender->async_send_to(std::move(data), target,
							  [&send_promise](std::error_code ec, std::size_t)
							  {
								  send_promise.set_value(!ec);
							  });

		ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
		ASSERT_TRUE(send_future.get()) << "Send " << i << " failed";
	}

	ASSERT_TRUE(done_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	EXPECT_GE(received_count.load(), kNumDatagrams);

	receiver->stop_receive();
}

TEST_F(UdpSocketTest, DatagramPreservesMessageBoundaries)
{
	auto [receiver, rx_port] = create_bound_socket();
	auto [sender, tx_port] = create_bound_socket();
	ASSERT_NE(receiver, nullptr);
	ASSERT_NE(sender, nullptr);

	std::promise<std::vector<uint8_t>> first_promise;
	auto first_future = first_promise.get_future();
	std::atomic<bool> first_set{false};

	receiver->set_receive_callback(
		[&](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint&)
		{
			bool expected = false;
			if (first_set.compare_exchange_strong(expected, true))
			{
				first_promise.set_value(data);
			}
		});

	receiver->start_receive();

	// Send a 3-byte datagram followed by a 5-byte datagram
	asio::ip::udp::endpoint target(asio::ip::make_address("127.0.0.1"), rx_port);

	std::vector<uint8_t> small = {0x01, 0x02, 0x03};
	std::promise<bool> p1;
	auto f1 = p1.get_future();
	sender->async_send_to(std::move(small), target,
						  [&p1](std::error_code ec, std::size_t) { p1.set_value(!ec); });
	ASSERT_TRUE(f1.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	ASSERT_TRUE(f1.get());

	ASSERT_TRUE(first_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto first = first_future.get();

	// First datagram should be exactly 3 bytes (not merged with second)
	EXPECT_EQ(first.size(), 3);
	EXPECT_EQ(first[0], 0x01);

	receiver->stop_receive();
}

// ============================================================================
// Sender Endpoint Verification Tests
// ============================================================================

TEST_F(UdpSocketTest, ReceivedDatagramIncludesSenderEndpoint)
{
	auto [receiver, rx_port] = create_bound_socket();
	auto [sender, tx_port] = create_bound_socket();
	ASSERT_NE(receiver, nullptr);
	ASSERT_NE(sender, nullptr);

	std::promise<asio::ip::udp::endpoint> ep_promise;
	auto ep_future = ep_promise.get_future();
	std::atomic<bool> ep_set{false};

	receiver->set_receive_callback(
		[&ep_promise, &ep_set](const std::vector<uint8_t>&,
							   const asio::ip::udp::endpoint& sender_ep)
		{
			bool expected = false;
			if (ep_set.compare_exchange_strong(expected, true))
			{
				ep_promise.set_value(sender_ep);
			}
		});

	receiver->start_receive();

	std::vector<uint8_t> data = {0x42};
	asio::ip::udp::endpoint target(asio::ip::make_address("127.0.0.1"), rx_port);

	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();
	sender->async_send_to(std::move(data), target,
						  [&send_promise](std::error_code ec, std::size_t)
						  {
							  send_promise.set_value(!ec);
						  });

	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	ASSERT_TRUE(send_future.get());

	ASSERT_TRUE(ep_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto sender_ep = ep_future.get();

	// Sender endpoint should be loopback with the sender's port
	EXPECT_EQ(sender_ep.address().to_string(), "127.0.0.1");
	EXPECT_EQ(sender_ep.port(), tx_port);

	receiver->stop_receive();
}

// ============================================================================
// Start/Stop Receive Lifecycle Tests
// ============================================================================

TEST_F(UdpSocketTest, StopReceivePreventsCallback)
{
	auto [receiver, rx_port] = create_bound_socket();
	auto [sender, tx_port] = create_bound_socket();
	ASSERT_NE(receiver, nullptr);
	ASSERT_NE(sender, nullptr);

	std::atomic<int> receive_count{0};

	receiver->set_receive_callback(
		[&receive_count](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
		{
			receive_count.fetch_add(1);
		});

	receiver->start_receive();

	// Send first datagram
	asio::ip::udp::endpoint target(asio::ip::make_address("127.0.0.1"), rx_port);
	{
		std::vector<uint8_t> data = {0x01};
		std::promise<bool> p;
		auto f = p.get_future();
		sender->async_send_to(std::move(data), target,
							  [&p](std::error_code ec, std::size_t) { p.set_value(!ec); });
		ASSERT_TRUE(f.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
		ASSERT_TRUE(f.get());
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	int count_before = receive_count.load();
	EXPECT_GE(count_before, 1);

	receiver->stop_receive();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Send second datagram after stop — should not trigger callback
	{
		std::vector<uint8_t> data = {0x02};
		std::promise<bool> p;
		auto f = p.get_future();
		sender->async_send_to(std::move(data), target,
							  [&p](std::error_code ec, std::size_t) { p.set_value(!ec); });
		ASSERT_TRUE(f.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(receive_count.load(), count_before);
}

TEST_F(UdpSocketTest, RestartReceiveAfterStop)
{
	auto [receiver, rx_port] = create_bound_socket();
	auto [sender, tx_port] = create_bound_socket();
	ASSERT_NE(receiver, nullptr);
	ASSERT_NE(sender, nullptr);

	std::atomic<int> receive_count{0};

	receiver->set_receive_callback(
		[&receive_count](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
		{
			receive_count.fetch_add(1);
		});

	asio::ip::udp::endpoint target(asio::ip::make_address("127.0.0.1"), rx_port);

	// Start, send, stop
	receiver->start_receive();
	{
		std::vector<uint8_t> data = {0x01};
		std::promise<bool> p;
		auto f = p.get_future();
		sender->async_send_to(std::move(data), target,
							  [&p](std::error_code ec, std::size_t) { p.set_value(!ec); });
		ASSERT_TRUE(f.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	receiver->stop_receive();
	int after_first = receive_count.load();
	EXPECT_GE(after_first, 1);

	// Restart receive
	receiver->start_receive();
	{
		std::vector<uint8_t> data = {0x02};
		std::promise<bool> p;
		auto f = p.get_future();
		sender->async_send_to(std::move(data), target,
							  [&p](std::error_code ec, std::size_t) { p.set_value(!ec); });
		ASSERT_TRUE(f.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_GT(receive_count.load(), after_first);

	receiver->stop_receive();
}

// ============================================================================
// Raw Socket Access Tests
// ============================================================================

TEST_F(UdpSocketTest, SocketAccessReturnsOpenSocket)
{
	auto [sock, port] = create_bound_socket();
	ASSERT_NE(sock, nullptr);

	auto& raw = sock->socket();
	EXPECT_TRUE(raw.is_open());
	EXPECT_EQ(raw.local_endpoint().port(), port);
}
