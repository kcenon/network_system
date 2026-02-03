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
#include "kcenon/network/experimental/quic_client.h"
#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::core::test
{

class MessagingQuicClientTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, BasicConstruction)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");
	EXPECT_FALSE(client->is_connected());
	EXPECT_FALSE(client->is_handshake_complete());
}

TEST_F(MessagingQuicClientTest, MultipleClientInstances)
{
	auto client1 = std::make_shared<messaging_quic_client>("client_1");
	auto client2 = std::make_shared<messaging_quic_client>("client_2");

	EXPECT_FALSE(client1->is_connected());
	EXPECT_FALSE(client2->is_connected());
}

// =============================================================================
// Configuration Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, DefaultConfig)
{
	quic_client_config config;

	EXPECT_FALSE(config.ca_cert_file.has_value());
	EXPECT_FALSE(config.client_cert_file.has_value());
	EXPECT_FALSE(config.client_key_file.has_value());
	EXPECT_TRUE(config.verify_server);
	EXPECT_TRUE(config.alpn_protocols.empty());
	EXPECT_EQ(config.max_idle_timeout_ms, 30000);
	EXPECT_EQ(config.initial_max_data, 1048576);
	EXPECT_EQ(config.initial_max_stream_data, 65536);
	EXPECT_EQ(config.initial_max_streams_bidi, 100);
	EXPECT_EQ(config.initial_max_streams_uni, 100);
	EXPECT_FALSE(config.enable_early_data);
	EXPECT_FALSE(config.session_ticket.has_value());
}

TEST_F(MessagingQuicClientTest, CustomConfig)
{
	quic_client_config config;
	config.ca_cert_file = "/path/to/ca.pem";
	config.verify_server = false;
	config.alpn_protocols = {"h3", "h3-29"};
	config.max_idle_timeout_ms = 60000;

	EXPECT_TRUE(config.ca_cert_file.has_value());
	EXPECT_EQ(*config.ca_cert_file, "/path/to/ca.pem");
	EXPECT_FALSE(config.verify_server);
	EXPECT_EQ(config.alpn_protocols.size(), 2);
	EXPECT_EQ(config.max_idle_timeout_ms, 60000);
}

// =============================================================================
// Connection Stats Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, DefaultStats)
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
// Callback Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, SetCallbacks)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	bool receive_called = false;
	bool stream_receive_called = false;
	bool connected_called = false;
	bool disconnected_called = false;
	bool error_called = false;

	client->set_receive_callback([&](const std::vector<uint8_t>&) {
		receive_called = true;
	});

	client->set_stream_receive_callback(
		[&](uint64_t, const std::vector<uint8_t>&, bool) {
			stream_receive_called = true;
		});

	client->set_connected_callback([&]() {
		connected_called = true;
	});

	client->set_disconnected_callback([&]() {
		disconnected_called = true;
	});

	client->set_error_callback([&](std::error_code) {
		error_called = true;
	});

	// Callbacks should not be invoked just by setting them
	EXPECT_FALSE(receive_called);
	EXPECT_FALSE(stream_receive_called);
	EXPECT_FALSE(connected_called);
	EXPECT_FALSE(disconnected_called);
	EXPECT_FALSE(error_called);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, StartWithEmptyHost)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	auto result = client->start_client("", 443);
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::common_errors::invalid_argument);
}

TEST_F(MessagingQuicClientTest, SendPacketWhenNotConnected)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	std::vector<uint8_t> data = {1, 2, 3, 4};
	auto result = client->send_packet(std::move(data));
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::network_system::connection_closed);
}

TEST_F(MessagingQuicClientTest, SendEmptyPacket)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	// Even if not connected, empty data should fail with invalid_argument
	std::vector<uint8_t> data;
	auto result = client->send_packet(std::move(data));
	EXPECT_TRUE(result.is_err());
	// The error could be either invalid_argument (if checked first) or connection_closed
}

TEST_F(MessagingQuicClientTest, SendStringPacketWhenNotConnected)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	auto result = client->send_packet("Hello QUIC");
	EXPECT_TRUE(result.is_err());
}

TEST_F(MessagingQuicClientTest, CreateStreamWhenNotConnected)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	auto result = client->create_stream();
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::network_system::connection_closed);
}

TEST_F(MessagingQuicClientTest, CreateUnidirectionalStreamWhenNotConnected)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	auto result = client->create_unidirectional_stream();
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::network_system::connection_closed);
}

TEST_F(MessagingQuicClientTest, SendOnStreamWhenNotConnected)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	std::vector<uint8_t> data = {1, 2, 3, 4};
	auto result = client->send_on_stream(0, std::move(data));
	EXPECT_TRUE(result.is_err());
}

TEST_F(MessagingQuicClientTest, CloseStreamWhenNotConnected)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	auto result = client->close_stream(0);
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::network_system::connection_closed);
}

// =============================================================================
// ALPN Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, SetAlpnProtocols)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	std::vector<std::string> protocols = {"h3", "h3-29", "hq-interop"};
	client->set_alpn_protocols(protocols);

	// ALPN protocol is only available after connection
	EXPECT_FALSE(client->alpn_protocol().has_value());
}

// =============================================================================
// Lifecycle Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, DoubleStart)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	// First start (will fail to connect but still marks as running)
	auto result1 = client->start_client("127.0.0.1", 12345);

	if (result1.is_ok())
	{
		// Second start should fail with already_exists
		auto result2 = client->start_client("127.0.0.1", 12345);
		EXPECT_TRUE(result2.is_err());
		EXPECT_EQ(result2.error().code, error_codes::common_errors::already_exists);

		auto stop_result = client->stop_client();
		EXPECT_TRUE(stop_result.is_ok());
	}
}

TEST_F(MessagingQuicClientTest, StopWhenNotRunning)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	// Stop should succeed even if not running
	auto result = client->stop_client();
	EXPECT_TRUE(result.is_ok());
}

TEST_F(MessagingQuicClientTest, MultipleStop)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	// Multiple stops should all succeed
	auto result1 = client->stop_client();
	auto result2 = client->stop_client();
	auto result3 = client->stop_client();

	EXPECT_TRUE(result1.is_ok());
	EXPECT_TRUE(result2.is_ok());
	EXPECT_TRUE(result3.is_ok());
}

TEST_F(MessagingQuicClientTest, DestructorStopsClient)
{
	// Create and destroy client - should not crash
	{
		auto client = std::make_shared<messaging_quic_client>("test_client");
		// Destructor should handle cleanup gracefully
	}
	SUCCEED();
}

// =============================================================================
// Stats Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, StatsWhenNotConnected)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	auto stats = client->stats();
	EXPECT_EQ(stats.bytes_sent, 0);
	EXPECT_EQ(stats.bytes_received, 0);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, ConcurrentCallbackSetting)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	std::vector<std::thread> threads;
	std::atomic<int> callback_count{0};

	// Multiple threads setting callbacks concurrently
	for (int i = 0; i < 10; ++i)
	{
		threads.emplace_back([&client, &callback_count, i]() {
			client->set_receive_callback([&callback_count](const std::vector<uint8_t>&) {
				callback_count++;
			});
			client->set_connected_callback([&callback_count]() {
				callback_count++;
			});
			client->set_error_callback([&callback_count](std::error_code) {
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

// =============================================================================
// API Consistency Tests (with messaging_client)
// =============================================================================

TEST_F(MessagingQuicClientTest, ApiConsistencyWithTcpClient)
{
	auto client = std::make_shared<messaging_quic_client>("test_client");

	// These methods should exist and have similar signatures
	EXPECT_FALSE(client->is_connected());

	std::vector<uint8_t> data = {1, 2, 3};
	auto send_result = client->send_packet(std::move(data));
	// Expect error since not connected
	EXPECT_TRUE(send_result.is_err());

	auto stop_result = client->stop_client();
	EXPECT_TRUE(stop_result.is_ok());
}

// =============================================================================
// Unified Pattern Type Alias Tests
// =============================================================================

TEST_F(MessagingQuicClientTest, TypeAliasQuicClient)
{
	// Verify quic_client is an alias for messaging_quic_client
	static_assert(std::is_same_v<quic_client, messaging_quic_client>,
	              "quic_client should be an alias for messaging_quic_client");

	auto client = std::make_shared<quic_client>("alias_test");
	EXPECT_FALSE(client->is_connected());
	EXPECT_EQ(client->client_id(), "alias_test");
}

TEST_F(MessagingQuicClientTest, TypeAliasSecureQuicClient)
{
	// Verify secure_quic_client is an alias for messaging_quic_client
	// QUIC always uses TLS 1.3, so secure_quic_client == quic_client
	static_assert(std::is_same_v<secure_quic_client, messaging_quic_client>,
	              "secure_quic_client should be an alias for messaging_quic_client");
	static_assert(std::is_same_v<quic_client, secure_quic_client>,
	              "quic_client and secure_quic_client should be the same type");

	auto client = std::make_shared<secure_quic_client>("secure_alias_test");
	EXPECT_FALSE(client->is_connected());
	EXPECT_EQ(client->client_id(), "secure_alias_test");
}

} // namespace kcenon::network::core::test
