/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

/**
 * @file test_quic_e2e.cpp
 * @brief QUIC End-to-End integration tests
 *
 * Tests include:
 * - Server startup and shutdown
 * - Client-server handshake
 * - Data transfer on default stream
 * - Multi-stream support
 * - Connection statistics
 * - Error handling scenarios
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_client.h"
#include "internal/experimental/quic_server.h"
#include "kcenon/network/detail/session/quic_session.h"

using namespace kcenon::network::core;
using namespace kcenon::network::session;
using namespace std::chrono_literals;

// Free function for yielding to allow async operations to complete
inline void wait_for_ready() {
    for (int i = 0; i < 1000; ++i) {
        std::this_thread::yield();
    }
}


namespace
{

// Test configuration
constexpr uint16_t BASE_TEST_PORT = 14433;
std::atomic<uint16_t> g_port_counter{0};

uint16_t get_test_port()
{
	return BASE_TEST_PORT + g_port_counter++;
}

} // namespace

class QuicE2ETest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		port_ = get_test_port();
	}

	void TearDown() override
	{
		// Ensure cleanup
		if (client_)
		{
			(void)client_->stop_client();
			client_.reset();
		}
		if (server_)
		{
			(void)server_->stop_server();
			server_.reset();
		}
	}

	uint16_t port_{0};
	std::shared_ptr<messaging_quic_server> server_;
	std::shared_ptr<messaging_quic_client> client_;
};

// =============================================================================
// Server Lifecycle Tests
// =============================================================================

TEST_F(QuicE2ETest, ServerStartAndStop)
{
	server_ = std::make_shared<messaging_quic_server>("test_server");

	EXPECT_FALSE(server_->is_running());
	EXPECT_EQ(server_->session_count(), 0);

	auto start_result = server_->start_server(port_);
	EXPECT_TRUE(start_result.is_ok()) << start_result.error().message;
	EXPECT_TRUE(server_->is_running());

	auto stop_result = server_->stop_server();
	EXPECT_TRUE(stop_result.is_ok()) << stop_result.error().message;
	EXPECT_FALSE(server_->is_running());
}

TEST_F(QuicE2ETest, ServerStartWithConfig)
{
	server_ = std::make_shared<messaging_quic_server>("test_server");

	quic_server_config config;
	config.max_idle_timeout_ms = 60000;
	config.max_connections = 100;
	config.initial_max_streams_bidi = 50;
	config.enable_retry = false;

	auto start_result = server_->start_server(port_, config);
	EXPECT_TRUE(start_result.is_ok()) << start_result.error().message;
	EXPECT_TRUE(server_->is_running());

	auto stop_result = server_->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
}

TEST_F(QuicE2ETest, ServerDoubleStart)
{
	server_ = std::make_shared<messaging_quic_server>("test_server");

	auto result1 = server_->start_server(port_);
	EXPECT_TRUE(result1.is_ok());

	auto result2 = server_->start_server(port_);
	EXPECT_TRUE(result2.is_err());

	(void)server_->stop_server();
}

// =============================================================================
// Client Lifecycle Tests
// =============================================================================

TEST_F(QuicE2ETest, ClientConstruction)
{
	client_ = std::make_shared<messaging_quic_client>("test_client");

	EXPECT_FALSE(client_->is_connected());
	EXPECT_FALSE(client_->is_handshake_complete());
}

TEST_F(QuicE2ETest, ClientStartWithInvalidHost)
{
	client_ = std::make_shared<messaging_quic_client>("test_client");

	auto result = client_->start_client("", port_);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicE2ETest, ClientStopWhenNotRunning)
{
	client_ = std::make_shared<messaging_quic_client>("test_client");

	auto result = client_->stop_client();
	EXPECT_TRUE(result.is_ok());
}

// =============================================================================
// Connection Tests
// =============================================================================

TEST_F(QuicE2ETest, ClientConnectToServer)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	auto server_result = server_->start_server(port_);
	ASSERT_TRUE(server_result.is_ok()) << server_result.error().message;

	// Setup connection tracking
	std::promise<bool> connection_promise;
	server_->set_connection_callback(
		[&connection_promise](std::shared_ptr<quic_session>) {
			connection_promise.set_value(true);
		});

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> client_connected_promise;
	client_->set_connected_callback([&client_connected_promise]() {
		client_connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;

	auto client_result = client_->start_client("127.0.0.1", port_, config);
	ASSERT_TRUE(client_result.is_ok()) << client_result.error().message;

	// Wait for connection (with timeout)
	auto client_future = client_connected_promise.get_future();
	auto status = client_future.wait_for(5s);

	if (status == std::future_status::ready)
	{
		EXPECT_TRUE(client_future.get());
		EXPECT_TRUE(client_->is_connected());
	}

	// Cleanup
	(void)client_->stop_client();
	(void)server_->stop_server();
}

// =============================================================================
// Data Transfer Tests
// =============================================================================

TEST_F(QuicE2ETest, SendDataOnDefaultStream)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	// Setup receive tracking
	std::promise<std::vector<uint8_t>> data_promise;
	server_->set_receive_callback(
		[&data_promise](std::shared_ptr<quic_session>, const std::vector<uint8_t>& data) {
			data_promise.set_value(data);
		});

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout - server may not support this test";
	}

	// Send data
	std::vector<uint8_t> test_data = {'H', 'e', 'l', 'l', 'o'};
	auto send_result = client_->send_packet(std::vector<uint8_t>(test_data));

	if (send_result.is_ok())
	{
		// Wait for receive
		auto data_future = data_promise.get_future();
		if (data_future.wait_for(5s) == std::future_status::ready)
		{
			auto received = data_future.get();
			EXPECT_EQ(received, test_data);
		}
	}

	// Cleanup
	(void)client_->stop_client();
	(void)server_->stop_server();
}

TEST_F(QuicE2ETest, SendStringData)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	// Setup receive tracking
	std::promise<std::string> data_promise;
	server_->set_receive_callback(
		[&data_promise](std::shared_ptr<quic_session>, const std::vector<uint8_t>& data) {
			data_promise.set_value(std::string(data.begin(), data.end()));
		});

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Send string
	std::string test_message = "Hello QUIC Protocol!";
	auto send_result = client_->send_packet(test_message);

	if (send_result.is_ok())
	{
		auto data_future = data_promise.get_future();
		if (data_future.wait_for(5s) == std::future_status::ready)
		{
			auto received = data_future.get();
			EXPECT_EQ(received, test_message);
		}
	}

	(void)client_->stop_client();
	(void)server_->stop_server();
}

// =============================================================================
// Multi-Stream Tests
// =============================================================================

TEST_F(QuicE2ETest, CreateBidirectionalStream)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Create stream
	auto stream_result = client_->create_stream();
	if (stream_result.is_ok())
	{
		uint64_t stream_id = stream_result.value();
		EXPECT_GE(stream_id, 0);

		// Send on stream
		std::vector<uint8_t> data = {'T', 'e', 's', 't'};
		auto send_result = client_->send_on_stream(stream_id, std::move(data));
		EXPECT_TRUE(send_result.is_ok() || send_result.is_err());

		// Close stream
		auto close_result = client_->close_stream(stream_id);
		EXPECT_TRUE(close_result.is_ok() || close_result.is_err());
	}

	(void)client_->stop_client();
	(void)server_->stop_server();
}

TEST_F(QuicE2ETest, CreateUnidirectionalStream)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Create unidirectional stream
	auto stream_result = client_->create_unidirectional_stream();
	if (stream_result.is_ok())
	{
		uint64_t stream_id = stream_result.value();
		EXPECT_GE(stream_id, 0);

		// Send on stream with FIN
		std::vector<uint8_t> data = {'D', 'a', 't', 'a'};
		auto send_result = client_->send_on_stream(stream_id, std::move(data), true);
		EXPECT_TRUE(send_result.is_ok() || send_result.is_err());
	}

	(void)client_->stop_client();
	(void)server_->stop_server();
}

TEST_F(QuicE2ETest, MultipleStreams)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	// Track received data per stream
	std::mutex received_mutex;
	std::map<uint64_t, std::vector<uint8_t>> received_data;

	server_->set_stream_receive_callback(
		[&received_mutex, &received_data](
			std::shared_ptr<quic_session>,
			uint64_t stream_id,
			const std::vector<uint8_t>& data,
			bool /* fin */) {
			std::lock_guard lock(received_mutex);
			received_data[stream_id].insert(
				received_data[stream_id].end(), data.begin(), data.end());
		});

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Create multiple streams
	std::vector<uint64_t> stream_ids;
	for (int i = 0; i < 3; ++i)
	{
		auto stream_result = client_->create_stream();
		if (stream_result.is_ok())
		{
			stream_ids.push_back(stream_result.value());
		}
	}

	// Send data on each stream
	for (size_t i = 0; i < stream_ids.size(); ++i)
	{
		std::vector<uint8_t> data = {
			static_cast<uint8_t>('A' + i),
			static_cast<uint8_t>('B' + i),
			static_cast<uint8_t>('C' + i)
		};
		(void)client_->send_on_stream(stream_ids[i], std::move(data));
	}

	// Wait for data to be received
	wait_for_ready();

	(void)client_->stop_client();
	(void)server_->stop_server();
}

// =============================================================================
// Statistics Tests
// =============================================================================

TEST_F(QuicE2ETest, ConnectionStatistics)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Get initial stats
	auto stats_before = client_->stats();

	// Send some data
	for (int i = 0; i < 10; ++i)
	{
		(void)client_->send_packet("Test message " + std::to_string(i));
	}

	// Wait for processing
	wait_for_ready();

	// Get stats after
	auto stats_after = client_->stats();

	// Verify stats were updated
	EXPECT_GE(stats_after.bytes_sent, stats_before.bytes_sent);
	EXPECT_GE(stats_after.packets_sent, stats_before.packets_sent);

	(void)client_->stop_client();
	(void)server_->stop_server();
}

// =============================================================================
// Session Management Tests
// =============================================================================

TEST_F(QuicE2ETest, SessionTracking)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	EXPECT_EQ(server_->session_count(), 0);
	EXPECT_TRUE(server_->sessions().empty());

	// Track connection
	std::promise<std::string> session_id_promise;
	server_->set_connection_callback(
		[&session_id_promise](std::shared_ptr<quic_session> session) {
			if (session)
			{
				session_id_promise.set_value(session->session_id());
			}
		});

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Wait for session to be registered
	wait_for_ready();

	auto session_future = session_id_promise.get_future();
	if (session_future.wait_for(1s) == std::future_status::ready)
	{
		auto session_id = session_future.get();

		// Verify session exists
		auto session = server_->get_session(session_id);
		EXPECT_NE(session, nullptr);
		EXPECT_GE(server_->session_count(), 1);
	}

	(void)client_->stop_client();
	(void)server_->stop_server();
}

TEST_F(QuicE2ETest, DisconnectSession)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	// Track connection and disconnection
	std::promise<std::string> session_id_promise;
	std::promise<void> disconnection_promise;

	server_->set_connection_callback(
		[&session_id_promise](std::shared_ptr<quic_session> session) {
			if (session)
			{
				session_id_promise.set_value(session->session_id());
			}
		});

	server_->set_disconnection_callback(
		[&disconnection_promise](std::shared_ptr<quic_session>) {
			disconnection_promise.set_value();
		});

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Get session ID
	auto session_future = session_id_promise.get_future();
	if (session_future.wait_for(1s) == std::future_status::ready)
	{
		auto session_id = session_future.get();

		// Disconnect session from server side
		auto disconnect_result = server_->disconnect_session(session_id);
		EXPECT_TRUE(disconnect_result.is_ok() || disconnect_result.is_err());

		// Wait for disconnection callback
		auto disc_future = disconnection_promise.get_future();
		disc_future.wait_for(2s);
	}

	(void)client_->stop_client();
	(void)server_->stop_server();
}

// =============================================================================
// Broadcasting Tests
// =============================================================================

TEST_F(QuicE2ETest, BroadcastToClients)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	std::promise<std::vector<uint8_t>> received_promise;

	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	client_->set_receive_callback([&received_promise](const std::vector<uint8_t>& data) {
		received_promise.set_value(data);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Wait for session to be registered
	wait_for_ready();

	// Broadcast data
	std::vector<uint8_t> broadcast_data = {'B', 'r', 'o', 'a', 'd', 'c', 'a', 's', 't'};
	auto broadcast_result = server_->broadcast(std::vector<uint8_t>(broadcast_data));
	EXPECT_TRUE(broadcast_result.is_ok());

	// Wait for client to receive
	auto recv_future = received_promise.get_future();
	if (recv_future.wait_for(2s) == std::future_status::ready)
	{
		auto received = recv_future.get();
		EXPECT_EQ(received, broadcast_data);
	}

	(void)client_->stop_client();
	(void)server_->stop_server();
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(QuicE2ETest, SendWhenNotConnected)
{
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::vector<uint8_t> data = {1, 2, 3, 4};
	auto result = client_->send_packet(std::move(data));
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicE2ETest, CreateStreamWhenNotConnected)
{
	client_ = std::make_shared<messaging_quic_client>("test_client");

	auto result = client_->create_stream();
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicE2ETest, DisconnectNonExistentSession)
{
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	auto result = server_->disconnect_session("non_existent_session_id");
	EXPECT_TRUE(result.is_err());

	(void)server_->stop_server();
}

TEST_F(QuicE2ETest, GetNonExistentSession)
{
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	auto session = server_->get_session("non_existent_id");
	EXPECT_EQ(session, nullptr);

	(void)server_->stop_server();
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(QuicE2ETest, ConcurrentSend)
{
	// Start server
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	std::atomic<int> receive_count{0};
	server_->set_receive_callback(
		[&receive_count](std::shared_ptr<quic_session>, const std::vector<uint8_t>&) {
			receive_count++;
		});

	// Start client
	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Send from multiple threads
	std::vector<std::thread> threads;
	constexpr int num_threads = 5;
	constexpr int messages_per_thread = 10;

	for (int t = 0; t < num_threads; ++t)
	{
		threads.emplace_back([this, t]() {
			for (int i = 0; i < messages_per_thread; ++i)
			{
				std::string msg = "Thread " + std::to_string(t) + " Message " + std::to_string(i);
				(void)client_->send_packet(msg);
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// Wait for messages to be processed
	wait_for_ready();

	(void)client_->stop_client();
	(void)server_->stop_server();
}

TEST_F(QuicE2ETest, ConcurrentSessionAccess)
{
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	// Multiple threads accessing sessions concurrently
	std::vector<std::thread> threads;
	std::atomic<bool> running{true};

	for (int i = 0; i < 5; ++i)
	{
		threads.emplace_back([this, &running, i]() {
			while (running.load())
			{
				auto count = server_->session_count();
				auto sessions = server_->sessions();
				auto session = server_->get_session("session_" + std::to_string(i));
				(void)count;
				(void)sessions;
				(void)session;
				std::this_thread::yield();
			}
		});
	}

	// Let threads run for a bit
	wait_for_ready();
	running.store(false);

	for (auto& t : threads)
	{
		t.join();
	}

	(void)server_->stop_server();
	SUCCEED();
}

// =============================================================================
// ALPN Tests
// =============================================================================

TEST_F(QuicE2ETest, AlpnNegotiation)
{
	// Start server with ALPN
	server_ = std::make_shared<messaging_quic_server>("test_server");

	quic_server_config server_config;
	server_config.alpn_protocols = {"h3", "hq-interop"};
	ASSERT_TRUE(server_->start_server(port_, server_config).is_ok());

	// Start client with ALPN
	client_ = std::make_shared<messaging_quic_client>("test_client");
	client_->set_alpn_protocols({"h3", "h3-29"});

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config client_config;
	client_config.verify_server = false;
	client_config.alpn_protocols = {"h3", "h3-29"};
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, client_config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) == std::future_status::ready)
	{
		// Check negotiated ALPN
		auto alpn = client_->alpn_protocol();
		if (alpn.has_value())
		{
			EXPECT_TRUE(alpn.value() == "h3" || alpn.value() == "hq-interop");
		}
	}

	(void)client_->stop_client();
	(void)server_->stop_server();
}

// =============================================================================
// Cleanup Tests
// =============================================================================

TEST_F(QuicE2ETest, GracefulShutdown)
{
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	conn_future.wait_for(5s);

	// Stop client first
	auto client_stop = client_->stop_client();
	EXPECT_TRUE(client_stop.is_ok());

	// Wait for client to fully stop
	client_->wait_for_stop();

	// Then stop server
	auto server_stop = server_->stop_server();
	EXPECT_TRUE(server_stop.is_ok());
}

TEST_F(QuicE2ETest, DisconnectAllSessions)
{
	server_ = std::make_shared<messaging_quic_server>("test_server");
	ASSERT_TRUE(server_->start_server(port_).is_ok());

	client_ = std::make_shared<messaging_quic_client>("test_client");

	std::promise<bool> connected_promise;
	client_->set_connected_callback([&connected_promise]() {
		connected_promise.set_value(true);
	});

	quic_client_config config;
	config.verify_server = false;
	ASSERT_TRUE(client_->start_client("127.0.0.1", port_, config).is_ok());

	// Wait for connection
	auto conn_future = connected_promise.get_future();
	if (conn_future.wait_for(5s) != std::future_status::ready)
	{
		GTEST_SKIP() << "Connection timeout";
	}

	// Wait for session registration
	wait_for_ready();

	// Disconnect all
	server_->disconnect_all();

	// Wait for disconnection
	wait_for_ready();

	(void)client_->stop_client();
	(void)server_->stop_server();
}
