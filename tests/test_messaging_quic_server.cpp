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

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <future>

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_server.h"
#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/session/quic_session.h"
#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::core::test
{

class MessagingQuicServerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}

	// Get an available port for testing
	static unsigned short get_available_port()
	{
		static std::atomic<unsigned short> port_counter{45000};
		return port_counter.fetch_add(1);
	}
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, BasicConstruction)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->session_count(), 0);
}

TEST_F(MessagingQuicServerTest, MultipleServerInstances)
{
	auto server1 = std::make_shared<messaging_quic_server>("server_1");
	auto server2 = std::make_shared<messaging_quic_server>("server_2");

	EXPECT_FALSE(server1->is_running());
	EXPECT_FALSE(server2->is_running());
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, DefaultConfig)
{
	quic_server_config config;

	EXPECT_TRUE(config.cert_file.empty());
	EXPECT_TRUE(config.key_file.empty());
	EXPECT_FALSE(config.ca_cert_file.has_value());
	EXPECT_FALSE(config.require_client_cert);
	EXPECT_TRUE(config.alpn_protocols.empty());
	EXPECT_EQ(config.max_idle_timeout_ms, 30000);
	EXPECT_EQ(config.initial_max_data, 1048576);
	EXPECT_EQ(config.initial_max_stream_data, 65536);
	EXPECT_EQ(config.initial_max_streams_bidi, 100);
	EXPECT_EQ(config.initial_max_streams_uni, 100);
	EXPECT_EQ(config.max_connections, 10000);
	EXPECT_TRUE(config.enable_retry);
	EXPECT_TRUE(config.retry_key.empty());
}

TEST_F(MessagingQuicServerTest, CustomConfig)
{
	quic_server_config config;
	config.cert_file = "/path/to/cert.pem";
	config.key_file = "/path/to/key.pem";
	config.ca_cert_file = "/path/to/ca.pem";
	config.require_client_cert = true;
	config.alpn_protocols = {"h3", "h3-29"};
	config.max_idle_timeout_ms = 60000;
	config.max_connections = 5000;

	EXPECT_EQ(config.cert_file, "/path/to/cert.pem");
	EXPECT_EQ(config.key_file, "/path/to/key.pem");
	EXPECT_TRUE(config.ca_cert_file.has_value());
	EXPECT_EQ(*config.ca_cert_file, "/path/to/ca.pem");
	EXPECT_TRUE(config.require_client_cert);
	EXPECT_EQ(config.alpn_protocols.size(), 2);
	EXPECT_EQ(config.max_idle_timeout_ms, 60000);
	EXPECT_EQ(config.max_connections, 5000);
}

// =============================================================================
// Callback Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, SetCallbacks)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");

	bool connection_called = false;
	bool disconnection_called = false;
	bool receive_called = false;
	bool stream_receive_called = false;
	bool error_called = false;

	server->set_connection_callback(
		[&](std::shared_ptr<session::quic_session>) {
			connection_called = true;
		});

	server->set_disconnection_callback(
		[&](std::shared_ptr<session::quic_session>) {
			disconnection_called = true;
		});

	server->set_receive_callback(
		[&](std::shared_ptr<session::quic_session>,
			const std::vector<uint8_t>&) {
			receive_called = true;
		});

	server->set_stream_receive_callback(
		[&](std::shared_ptr<session::quic_session>,
			uint64_t, const std::vector<uint8_t>&, bool) {
			stream_receive_called = true;
		});

	server->set_error_callback([&](std::error_code) {
		error_called = true;
	});

	// Callbacks should not be invoked just by setting them
	EXPECT_FALSE(connection_called);
	EXPECT_FALSE(disconnection_called);
	EXPECT_FALSE(receive_called);
	EXPECT_FALSE(stream_receive_called);
	EXPECT_FALSE(error_called);
}

// =============================================================================
// Server Lifecycle Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, StartAndStopServer)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	auto start_result = server->start_server(port);
	EXPECT_TRUE(start_result.is_ok());
	EXPECT_TRUE(server->is_running());

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(server->is_running());
}

TEST_F(MessagingQuicServerTest, DoubleStart)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	auto result1 = server->start_server(port);
	EXPECT_TRUE(result1.is_ok());

	auto result2 = server->start_server(port);
	EXPECT_TRUE(result2.is_err());
	EXPECT_EQ(result2.error().code,
		error_codes::network_system::server_already_running);

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
}

TEST_F(MessagingQuicServerTest, StopWhenNotRunning)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");

	auto result = server->stop_server();
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code,
		error_codes::network_system::server_not_started);
}

TEST_F(MessagingQuicServerTest, MultipleStop)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	auto start_result = server->start_server(port);
	EXPECT_TRUE(start_result.is_ok());

	auto result1 = server->stop_server();
	EXPECT_TRUE(result1.is_ok());

	auto result2 = server->stop_server();
	EXPECT_TRUE(result2.is_err());
	EXPECT_EQ(result2.error().code,
		error_codes::network_system::server_not_started);
}

TEST_F(MessagingQuicServerTest, DestructorStopsServer)
{
	auto port = get_available_port();

	{
		auto server = std::make_shared<messaging_quic_server>("test_server");
		auto result = server->start_server(port);
		EXPECT_TRUE(result.is_ok());
		// Destructor should handle cleanup gracefully
	}
	SUCCEED();
}

TEST_F(MessagingQuicServerTest, StartWithConfig)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	quic_server_config config;
	config.max_idle_timeout_ms = 60000;
	config.max_connections = 100;

	auto result = server->start_server(port, config);
	EXPECT_TRUE(result.is_ok());
	EXPECT_TRUE(server->is_running());

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
}

// =============================================================================
// Session Management Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, SessionsEmptyInitially)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");

	EXPECT_EQ(server->session_count(), 0);
	EXPECT_TRUE(server->sessions().empty());
}

TEST_F(MessagingQuicServerTest, GetNonExistentSession)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");

	auto session = server->get_session("non_existent_id");
	EXPECT_EQ(session, nullptr);
}

TEST_F(MessagingQuicServerTest, DisconnectNonExistentSession)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	auto start_result = server->start_server(port);
	EXPECT_TRUE(start_result.is_ok());

	auto result = server->disconnect_session("non_existent_id");
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::common_errors::not_found);

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
}

TEST_F(MessagingQuicServerTest, DisconnectAllWithNoSessions)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	auto start_result = server->start_server(port);
	EXPECT_TRUE(start_result.is_ok());

	// Should not crash when there are no sessions
	server->disconnect_all(0);
	EXPECT_EQ(server->session_count(), 0);

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
}

// =============================================================================
// Broadcast Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, BroadcastWithNoSessions)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	auto start_result = server->start_server(port);
	EXPECT_TRUE(start_result.is_ok());

	std::vector<uint8_t> data = {1, 2, 3, 4, 5};
	auto result = server->broadcast(std::move(data));
	EXPECT_TRUE(result.is_ok());

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
}

TEST_F(MessagingQuicServerTest, MulticastWithNoSessions)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	auto start_result = server->start_server(port);
	EXPECT_TRUE(start_result.is_ok());

	std::vector<std::string> session_ids = {"id1", "id2", "id3"};
	std::vector<uint8_t> data = {1, 2, 3, 4, 5};
	auto result = server->multicast(session_ids, std::move(data));
	EXPECT_TRUE(result.is_ok());

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, ConcurrentCallbackSetting)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");

	std::vector<std::thread> threads;
	std::atomic<int> callback_count{0};

	// Multiple threads setting callbacks concurrently
	for (int i = 0; i < 10; ++i)
	{
		threads.emplace_back([&server, &callback_count]() {
			server->set_connection_callback(
				[&callback_count](std::shared_ptr<session::quic_session>) {
					callback_count++;
				});
			server->set_receive_callback(
				[&callback_count](std::shared_ptr<session::quic_session>,
								  const std::vector<uint8_t>&) {
					callback_count++;
				});
			server->set_error_callback([&callback_count](std::error_code) {
				callback_count++;
			});
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// Should not crash
	SUCCEED();
}

TEST_F(MessagingQuicServerTest, ConcurrentSessionAccess)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	auto start_result = server->start_server(port);
	EXPECT_TRUE(start_result.is_ok());

	std::vector<std::thread> threads;

	// Multiple threads accessing sessions concurrently
	for (int i = 0; i < 10; ++i)
	{
		threads.emplace_back([&server, i]() {
			for (int j = 0; j < 100; ++j)
			{
				auto count = server->session_count();
				auto sessions = server->sessions();
				auto session = server->get_session("session_" + std::to_string(i));
				(void)count;
				(void)sessions;
				(void)session;
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
	SUCCEED();
}

// =============================================================================
// API Consistency Tests (with messaging_server)
// =============================================================================

TEST_F(MessagingQuicServerTest, ApiConsistencyWithTcpServer)
{
	auto server = std::make_shared<messaging_quic_server>("test_server");
	auto port = get_available_port();

	// These methods should exist and have similar signatures
	EXPECT_FALSE(server->is_running());

	auto start_result = server->start_server(port);
	EXPECT_TRUE(start_result.is_ok());
	EXPECT_TRUE(server->is_running());

	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(server->is_running());
}

// =============================================================================
// quic_session Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, QuicSessionDefaultStats)
{
	quic_connection_stats stats;

	EXPECT_EQ(stats.bytes_sent, 0);
	EXPECT_EQ(stats.bytes_received, 0);
	EXPECT_EQ(stats.packets_sent, 0);
	EXPECT_EQ(stats.packets_received, 0);
	EXPECT_EQ(stats.packets_lost, 0);
	EXPECT_EQ(stats.smoothed_rtt.count(), 0);
	EXPECT_EQ(stats.min_rtt.count(), 0);
	EXPECT_EQ(stats.cwnd, 0);
}

// =============================================================================
// Unified Pattern Type Alias Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, TypeAliasQuicServer)
{
	// Verify quic_server is an alias for messaging_quic_server
	static_assert(std::is_same_v<quic_server, messaging_quic_server>,
	              "quic_server should be an alias for messaging_quic_server");

	auto server = std::make_shared<quic_server>("alias_test");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), "alias_test");
}

TEST_F(MessagingQuicServerTest, TypeAliasSecureQuicServer)
{
	// Verify secure_quic_server is an alias for messaging_quic_server
	// QUIC always uses TLS 1.3, so secure_quic_server == quic_server
	static_assert(std::is_same_v<secure_quic_server, messaging_quic_server>,
	              "secure_quic_server should be an alias for messaging_quic_server");
	static_assert(std::is_same_v<quic_server, secure_quic_server>,
	              "quic_server and secure_quic_server should be the same type");

	auto server = std::make_shared<secure_quic_server>("secure_alias_test");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), "secure_alias_test");
}

} // namespace kcenon::network::core::test
