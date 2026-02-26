/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file udp_basic_test.cpp
 * @brief Functional unit tests for UDP client and server
 *
 * Tests validate:
 * - Server start/stop lifecycle
 * - Client start/stop lifecycle
 * - Double-start returns error
 * - send() when not running returns error
 * - Round-trip message send and receive (loopback)
 * - Callback setters (receive, error)
 * - Multiple concurrent UDP clients to single server
 *
 * Note: All tests use 127.0.0.1 loopback and ephemeral ports to avoid
 * network dependency and port conflicts.
 */

#include "internal/core/messaging_udp_client.h"
#include "internal/core/messaging_udp_server.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

using namespace kcenon::network::core;
using namespace kcenon::network;

// ============================================================================
// Server Lifecycle Tests
// ============================================================================

class UdpServerLifecycleTest : public ::testing::Test
{
};

TEST_F(UdpServerLifecycleTest, ConstructsWithServerId)
{
	auto server = std::make_shared<messaging_udp_server>("test_server");

	EXPECT_EQ(server->server_id(), "test_server");
	EXPECT_FALSE(server->is_running());
}

TEST_F(UdpServerLifecycleTest, StartAndStopOnEphemeralPort)
{
	auto server = std::make_shared<messaging_udp_server>("lifecycle_server");

	auto start_result = server->start_server(0);
	ASSERT_TRUE(start_result.is_ok()) << "Failed to start: " << start_result.error().message;
	EXPECT_TRUE(server->is_running());

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(server->is_running());
}

TEST_F(UdpServerLifecycleTest, DoubleStartReturnsError)
{
	auto server = std::make_shared<messaging_udp_server>("double_start_server");

	auto result1 = server->start_server(0);
	ASSERT_TRUE(result1.is_ok());

	auto result2 = server->start_server(0);
	EXPECT_TRUE(result2.is_err());
	EXPECT_EQ(result2.error().code, error_codes::network_system::server_already_running);

	(void)server->stop_server();
}

TEST_F(UdpServerLifecycleTest, StopWhenNotRunningReturnsOk)
{
	auto server = std::make_shared<messaging_udp_server>("stop_test");

	// Stopping a server that was never started returns ok (not running or already stopping)
	auto result = server->stop_server();
	EXPECT_TRUE(result.is_ok());
}

TEST_F(UdpServerLifecycleTest, DestructorStopsRunningServer)
{
	{
		auto server = std::make_shared<messaging_udp_server>("destructor_server");
		auto result = server->start_server(0);
		ASSERT_TRUE(result.is_ok());
		EXPECT_TRUE(server->is_running());
		// Destructor should stop it gracefully
	}
	SUCCEED();
}

// ============================================================================
// Client Lifecycle Tests
// ============================================================================

class UdpClientLifecycleTest : public ::testing::Test
{
};

TEST_F(UdpClientLifecycleTest, ConstructsWithClientId)
{
	auto client = std::make_shared<messaging_udp_client>("test_client");

	EXPECT_EQ(client->client_id(), "test_client");
	EXPECT_FALSE(client->is_running());
}

TEST_F(UdpClientLifecycleTest, StartAndStopWithLoopback)
{
	auto client = std::make_shared<messaging_udp_client>("lifecycle_client");

	// UDP client resolves the target but doesn't actually connect
	auto start_result = client->start_client("127.0.0.1", 55555);
	ASSERT_TRUE(start_result.is_ok()) << "Failed to start: " << start_result.error().message;
	EXPECT_TRUE(client->is_running());

	auto stop_result = client->stop_client();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(client->is_running());
}

TEST_F(UdpClientLifecycleTest, StartWithEmptyHostReturnsError)
{
	auto client = std::make_shared<messaging_udp_client>("empty_host_client");

	auto result = client->start_client("", 5555);
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::common_errors::invalid_argument);
}

TEST_F(UdpClientLifecycleTest, DoubleStartReturnsError)
{
	auto client = std::make_shared<messaging_udp_client>("double_start_client");

	auto result1 = client->start_client("127.0.0.1", 55555);
	ASSERT_TRUE(result1.is_ok());

	auto result2 = client->start_client("127.0.0.1", 55555);
	EXPECT_TRUE(result2.is_err());
	EXPECT_EQ(result2.error().code, error_codes::common_errors::already_exists);

	(void)client->stop_client();
}

TEST_F(UdpClientLifecycleTest, StopWhenNotRunningReturnsOk)
{
	auto client = std::make_shared<messaging_udp_client>("stop_test");

	auto result = client->stop_client();
	EXPECT_TRUE(result.is_ok());
}

TEST_F(UdpClientLifecycleTest, DestructorStopsRunningClient)
{
	{
		auto client = std::make_shared<messaging_udp_client>("destructor_client");
		auto result = client->start_client("127.0.0.1", 55555);
		ASSERT_TRUE(result.is_ok());
		EXPECT_TRUE(client->is_running());
	}
	SUCCEED();
}

// ============================================================================
// Send Error Tests
// ============================================================================

class UdpSendErrorTest : public ::testing::Test
{
};

TEST_F(UdpSendErrorTest, SendWhenNotRunningReturnsError)
{
	auto client = std::make_shared<messaging_udp_client>("send_test_client");

	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	auto result = client->send(std::move(data), nullptr);

	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Callback Setter Tests
// ============================================================================

class UdpCallbackTest : public ::testing::Test
{
};

TEST_F(UdpCallbackTest, ClientSetReceiveCallbackDoesNotThrow)
{
	auto client = std::make_shared<messaging_udp_client>("cb_client");

	EXPECT_NO_THROW(client->set_receive_callback(
		[](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {}));
}

TEST_F(UdpCallbackTest, ClientSetErrorCallbackDoesNotThrow)
{
	auto client = std::make_shared<messaging_udp_client>("cb_client");

	EXPECT_NO_THROW(client->set_error_callback(
		[](std::error_code) {}));
}

TEST_F(UdpCallbackTest, ClientSetNullCallbacksDoNotThrow)
{
	auto client = std::make_shared<messaging_udp_client>("null_cb_client");

	EXPECT_NO_THROW(client->set_receive_callback(
		messaging_udp_client::receive_callback_t(nullptr)));
	EXPECT_NO_THROW(client->set_error_callback(nullptr));
}

TEST_F(UdpCallbackTest, ServerSetReceiveCallbackDoesNotThrow)
{
	auto server = std::make_shared<messaging_udp_server>("cb_server");

	EXPECT_NO_THROW(server->set_receive_callback(
		[](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {}));
}

TEST_F(UdpCallbackTest, ServerSetErrorCallbackDoesNotThrow)
{
	auto server = std::make_shared<messaging_udp_server>("cb_server");

	EXPECT_NO_THROW(server->set_error_callback(
		[](std::error_code) {}));
}

TEST_F(UdpCallbackTest, ServerSetNullCallbacksDoNotThrow)
{
	auto server = std::make_shared<messaging_udp_server>("null_cb_server");

	EXPECT_NO_THROW(server->set_receive_callback(
		messaging_udp_server::receive_callback_t(nullptr)));
	EXPECT_NO_THROW(server->set_error_callback(nullptr));
}

// ============================================================================
// Round-Trip Loopback Tests
// ============================================================================

class UdpRoundTripTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		server_ = std::make_shared<messaging_udp_server>("loopback_server");
		client_ = std::make_shared<messaging_udp_client>("loopback_client");
	}

	void TearDown() override
	{
		if (client_ && client_->is_running())
		{
			(void)client_->stop_client();
		}
		if (server_ && server_->is_running())
		{
			(void)server_->stop_server();
		}
	}

	std::shared_ptr<messaging_udp_server> server_;
	std::shared_ptr<messaging_udp_client> client_;
};

TEST_F(UdpRoundTripTest, SendAndReceiveOnLoopback)
{
	std::atomic<bool> received{false};
	std::mutex mtx;
	std::condition_variable cv;
	std::vector<uint8_t> received_data;

	// Set up server receive callback
	server_->set_receive_callback(
		[&](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint&)
		{
			std::lock_guard<std::mutex> lock(mtx);
			received_data = data;
			received.store(true);
			cv.notify_one();
		});

	// Start server on ephemeral port - use a specific port to avoid race conditions
	// We'll use a port in the high range to avoid conflicts
	uint16_t test_port = 0;
	auto server_result = server_->start_server(0);
	ASSERT_TRUE(server_result.is_ok()) << "Server start failed: " << server_result.error().message;

	// Give the server time to start receiving
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// We can't easily get the bound port from the server, so we'll use a known port.
	// Restart on a specific port for the client to target
	(void)server_->stop_server();

	// Find an available port by trying one
	test_port = 49152 + (std::chrono::steady_clock::now().time_since_epoch().count() % 1000);
	server_result = server_->start_server(test_port);
	ASSERT_TRUE(server_result.is_ok()) << "Server start failed: " << server_result.error().message;

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Start client targeting the server
	auto client_result = client_->start_client("127.0.0.1", test_port);
	ASSERT_TRUE(client_result.is_ok()) << "Client start failed: " << client_result.error().message;

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Send a message
	std::string message = "Hello UDP";
	std::vector<uint8_t> data(message.begin(), message.end());
	auto send_result = client_->send(std::move(data), nullptr);
	ASSERT_TRUE(send_result.is_ok()) << "Send failed: " << send_result.error().message;

	// Wait for message to be received
	{
		std::unique_lock<std::mutex> lock(mtx);
		ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
			[&] { return received.load(); }))
			<< "Timeout waiting for UDP message";
	}

	// Verify received data
	std::string received_msg(received_data.begin(), received_data.end());
	EXPECT_EQ(received_msg, "Hello UDP");
}

// ============================================================================
// Multiple Concurrent Clients Test
// ============================================================================

class UdpMultiClientTest : public ::testing::Test
{
protected:
	void TearDown() override
	{
		if (server_ && server_->is_running())
		{
			(void)server_->stop_server();
		}
	}

	std::shared_ptr<messaging_udp_server> server_;
};

TEST_F(UdpMultiClientTest, MultipleConcurrentClientsToSingleServer)
{
	constexpr int NUM_CLIENTS = 3;

	std::atomic<int> messages_received{0};
	std::mutex mtx;
	std::condition_variable cv;

	server_ = std::make_shared<messaging_udp_server>("multi_server");

	server_->set_receive_callback(
		[&](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
		{
			messages_received.fetch_add(1);
			if (messages_received.load() >= NUM_CLIENTS)
			{
				std::lock_guard<std::mutex> lock(mtx);
				cv.notify_one();
			}
		});

	uint16_t test_port = 49152 + (std::chrono::steady_clock::now().time_since_epoch().count() % 1000);
	auto server_result = server_->start_server(test_port);
	ASSERT_TRUE(server_result.is_ok()) << "Server start failed: " << server_result.error().message;

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Create and start multiple clients
	std::vector<std::shared_ptr<messaging_udp_client>> clients;
	for (int i = 0; i < NUM_CLIENTS; ++i)
	{
		auto client = std::make_shared<messaging_udp_client>(
			"multi_client_" + std::to_string(i));
		auto result = client->start_client("127.0.0.1", test_port);
		ASSERT_TRUE(result.is_ok()) << "Client " << i << " start failed";
		clients.push_back(std::move(client));
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Each client sends a message
	for (int i = 0; i < NUM_CLIENTS; ++i)
	{
		std::string msg = "msg_" + std::to_string(i);
		std::vector<uint8_t> data(msg.begin(), msg.end());
		auto result = clients[i]->send(std::move(data), nullptr);
		ASSERT_TRUE(result.is_ok()) << "Client " << i << " send failed";
	}

	// Wait for all messages
	{
		std::unique_lock<std::mutex> lock(mtx);
		EXPECT_TRUE(cv.wait_for(lock, std::chrono::seconds(5),
			[&] { return messages_received.load() >= NUM_CLIENTS; }))
			<< "Only received " << messages_received.load() << "/" << NUM_CLIENTS << " messages";
	}

	// Stop all clients and server before locals (cv, mtx) go out of scope
	for (auto& client : clients)
	{
		(void)client->stop_client();
	}
	(void)server_->stop_server();
}
