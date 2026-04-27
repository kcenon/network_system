// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <limits>
#include <string>
#include <thread>
#include <vector>

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_server.h"
#include "kcenon/network/detail/utils/result_types.h"
#include "kcenon/network/detail/session/quic_session.h"
#include "kcenon/network/detail/utils/result_types.h"

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

// =============================================================================
// Constructor Variation Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, ConstructWithEmptyServerId)
{
	auto server = std::make_shared<messaging_quic_server>("");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), "");
	EXPECT_EQ(server->session_count(), 0);
	EXPECT_EQ(server->connection_count(), 0);
}

TEST_F(MessagingQuicServerTest, ConstructWithLongServerId)
{
	const std::string long_id(1024, 'x');
	auto server = std::make_shared<messaging_quic_server>(long_id);
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), long_id);
}

TEST_F(MessagingQuicServerTest, ConstructWithSpecialCharactersInId)
{
	auto server = std::make_shared<messaging_quic_server>("server-1.2_test/A:B");
	EXPECT_EQ(server->server_id(), "server-1.2_test/A:B");
	EXPECT_FALSE(server->is_running());
}

TEST_F(MessagingQuicServerTest, ConstructFromStringView)
{
	std::string id = "view_id_test";
	std::string_view sv(id);
	auto server = std::make_shared<messaging_quic_server>(sv);
	EXPECT_EQ(server->server_id(), "view_id_test");
}

// =============================================================================
// Server-not-running Guard Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, ConnectionCountIsZeroBeforeStart)
{
	auto server = std::make_shared<messaging_quic_server>("guard_conn_count");
	EXPECT_EQ(server->connection_count(), 0);
	EXPECT_EQ(server->session_count(), 0);
}

TEST_F(MessagingQuicServerTest, SessionsListEmptyBeforeStart)
{
	auto server = std::make_shared<messaging_quic_server>("guard_sessions");
	auto list = server->sessions();
	EXPECT_TRUE(list.empty());
}

TEST_F(MessagingQuicServerTest, GetSessionEmptyIdReturnsNull)
{
	auto server = std::make_shared<messaging_quic_server>("guard_empty_id");
	EXPECT_EQ(server->get_session(""), nullptr);
}

TEST_F(MessagingQuicServerTest, GetSessionLongIdReturnsNull)
{
	auto server = std::make_shared<messaging_quic_server>("guard_long_id");
	const std::string long_id(2048, 'q');
	EXPECT_EQ(server->get_session(long_id), nullptr);
}

TEST_F(MessagingQuicServerTest, DisconnectAllBeforeStartDoesNotThrow)
{
	auto server = std::make_shared<messaging_quic_server>("guard_disc_all");
	EXPECT_NO_THROW(server->disconnect_all(0));
	EXPECT_NO_THROW(server->disconnect_all(42));
	EXPECT_EQ(server->session_count(), 0);
}

// =============================================================================
// Interface (i_quic_server) Alias Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, StartStopAliasMethods)
{
	auto server = std::make_shared<messaging_quic_server>("alias_methods");
	auto port = get_available_port();

	auto start_result = server->start(port);
	EXPECT_TRUE(start_result.is_ok());
	EXPECT_TRUE(server->is_running());

	auto stop_result = server->stop();
	EXPECT_TRUE(stop_result.is_ok());
	EXPECT_FALSE(server->is_running());
}

TEST_F(MessagingQuicServerTest, StopAliasWhenNotRunning)
{
	auto server = std::make_shared<messaging_quic_server>("alias_stop_idle");
	auto result = server->stop();
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code,
		error_codes::network_system::server_not_started);
}

// =============================================================================
// Interface Callback Setter Tests (i_quic_server)
// =============================================================================

TEST_F(MessagingQuicServerTest, SetInterfaceConnectionCallback)
{
	auto server = std::make_shared<messaging_quic_server>("iface_conn_cb");
	bool invoked = false;
	interfaces::i_quic_server::connection_callback_t cb =
		[&](std::shared_ptr<interfaces::i_quic_session>) { invoked = true; };
	server->set_connection_callback(std::move(cb));
	EXPECT_FALSE(invoked);
}

TEST_F(MessagingQuicServerTest, SetInterfaceDisconnectionCallback)
{
	auto server = std::make_shared<messaging_quic_server>("iface_disc_cb");
	bool invoked = false;
	interfaces::i_quic_server::disconnection_callback_t cb =
		[&](std::string_view) { invoked = true; };
	server->set_disconnection_callback(std::move(cb));
	EXPECT_FALSE(invoked);
}

TEST_F(MessagingQuicServerTest, SetInterfaceReceiveCallback)
{
	auto server = std::make_shared<messaging_quic_server>("iface_recv_cb");
	bool invoked = false;
	interfaces::i_quic_server::receive_callback_t cb =
		[&](std::string_view, const std::vector<uint8_t>&) { invoked = true; };
	server->set_receive_callback(std::move(cb));
	EXPECT_FALSE(invoked);
}

TEST_F(MessagingQuicServerTest, SetInterfaceStreamCallback)
{
	auto server = std::make_shared<messaging_quic_server>("iface_stream_cb");
	bool invoked = false;
	interfaces::i_quic_server::stream_callback_t cb =
		[&](std::string_view, uint64_t,
			const std::vector<uint8_t>&, bool) { invoked = true; };
	server->set_stream_callback(std::move(cb));
	EXPECT_FALSE(invoked);
}

TEST_F(MessagingQuicServerTest, SetInterfaceErrorCallback)
{
	auto server = std::make_shared<messaging_quic_server>("iface_err_cb");
	bool invoked = false;
	interfaces::i_quic_server::error_callback_t cb =
		[&](std::string_view, std::error_code) { invoked = true; };
	server->set_error_callback(std::move(cb));
	EXPECT_FALSE(invoked);
}

TEST_F(MessagingQuicServerTest, NullInterfaceCallbacksAreSafe)
{
	auto server = std::make_shared<messaging_quic_server>("iface_null_cb");
	server->set_connection_callback(
		interfaces::i_quic_server::connection_callback_t{});
	server->set_disconnection_callback(
		interfaces::i_quic_server::disconnection_callback_t{});
	server->set_receive_callback(
		interfaces::i_quic_server::receive_callback_t{});
	server->set_stream_callback(
		interfaces::i_quic_server::stream_callback_t{});
	server->set_error_callback(
		interfaces::i_quic_server::error_callback_t{});
	SUCCEED();
}

TEST_F(MessagingQuicServerTest, NullLegacyCallbacksAreSafe)
{
	auto server = std::make_shared<messaging_quic_server>("legacy_null_cb");
	server->set_connection_callback(messaging_quic_server::connection_callback_t{});
	server->set_disconnection_callback(messaging_quic_server::disconnection_callback_t{});
	server->set_receive_callback(messaging_quic_server::receive_callback_t{});
	server->set_stream_receive_callback(messaging_quic_server::stream_receive_callback_t{});
	server->set_error_callback(messaging_quic_server::error_callback_t{});
	SUCCEED();
}

// =============================================================================
// Configuration Boundary Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, ConfigZeroMaxConnections)
{
	quic_server_config config;
	config.max_connections = 0;
	EXPECT_EQ(config.max_connections, 0u);

	auto server = std::make_shared<messaging_quic_server>("cfg_zero_max");
	auto port = get_available_port();
	auto start_result = server->start_server(port, config);
	EXPECT_TRUE(start_result.is_ok());
	EXPECT_EQ(server->session_count(), 0);
	auto stop_result = server->stop_server();
	EXPECT_TRUE(stop_result.is_ok());
}

TEST_F(MessagingQuicServerTest, ConfigMaxUint64Timeouts)
{
	quic_server_config config;
	config.max_idle_timeout_ms = std::numeric_limits<uint64_t>::max();
	config.initial_max_data = std::numeric_limits<uint64_t>::max();
	config.initial_max_stream_data = std::numeric_limits<uint64_t>::max();
	config.initial_max_streams_bidi = std::numeric_limits<uint64_t>::max();
	config.initial_max_streams_uni = std::numeric_limits<uint64_t>::max();

	EXPECT_EQ(config.max_idle_timeout_ms, std::numeric_limits<uint64_t>::max());
	EXPECT_EQ(config.initial_max_data, std::numeric_limits<uint64_t>::max());
	EXPECT_EQ(config.initial_max_stream_data, std::numeric_limits<uint64_t>::max());
}

TEST_F(MessagingQuicServerTest, ConfigZeroTimeouts)
{
	quic_server_config config;
	config.max_idle_timeout_ms = 0;
	config.initial_max_data = 0;
	config.initial_max_stream_data = 0;
	config.initial_max_streams_bidi = 0;
	config.initial_max_streams_uni = 0;

	EXPECT_EQ(config.max_idle_timeout_ms, 0u);
	EXPECT_EQ(config.initial_max_data, 0u);
	EXPECT_EQ(config.initial_max_streams_bidi, 0u);
}

TEST_F(MessagingQuicServerTest, ConfigManyAlpnProtocols)
{
	quic_server_config config;
	for (int i = 0; i < 32; ++i)
	{
		config.alpn_protocols.push_back("proto-" + std::to_string(i));
	}
	EXPECT_EQ(config.alpn_protocols.size(), 32u);
	EXPECT_EQ(config.alpn_protocols.front(), "proto-0");
	EXPECT_EQ(config.alpn_protocols.back(), "proto-31");
}

TEST_F(MessagingQuicServerTest, ConfigRetryDisabledWithKey)
{
	quic_server_config config;
	config.enable_retry = false;
	config.retry_key = std::vector<uint8_t>(32, 0xAB);
	EXPECT_FALSE(config.enable_retry);
	EXPECT_EQ(config.retry_key.size(), 32u);
	EXPECT_EQ(config.retry_key[0], 0xAB);
}

TEST_F(MessagingQuicServerTest, ConfigCopyAssignment)
{
	quic_server_config a;
	a.cert_file = "/etc/ssl/cert.pem";
	a.key_file = "/etc/ssl/key.pem";
	a.max_connections = 1234;
	a.alpn_protocols = {"h3"};

	quic_server_config b = a;
	EXPECT_EQ(b.cert_file, "/etc/ssl/cert.pem");
	EXPECT_EQ(b.key_file, "/etc/ssl/key.pem");
	EXPECT_EQ(b.max_connections, 1234u);
	ASSERT_EQ(b.alpn_protocols.size(), 1u);
	EXPECT_EQ(b.alpn_protocols[0], "h3");
}

TEST_F(MessagingQuicServerTest, ConfigCaCertOptionalEmptyAndPresent)
{
	quic_server_config a;
	EXPECT_FALSE(a.ca_cert_file.has_value());

	quic_server_config b;
	b.ca_cert_file = "";
	EXPECT_TRUE(b.ca_cert_file.has_value());
	EXPECT_TRUE(b.ca_cert_file->empty());

	quic_server_config c;
	c.ca_cert_file = "/path/to/ca.pem";
	EXPECT_TRUE(c.ca_cert_file.has_value());
	EXPECT_EQ(*c.ca_cert_file, "/path/to/ca.pem");
}

// =============================================================================
// Multi-Instance State Isolation Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, IndependentServerIds)
{
	auto a = std::make_shared<messaging_quic_server>("alpha");
	auto b = std::make_shared<messaging_quic_server>("beta");
	EXPECT_EQ(a->server_id(), "alpha");
	EXPECT_EQ(b->server_id(), "beta");
	EXPECT_NE(a->server_id(), b->server_id());
}

TEST_F(MessagingQuicServerTest, IndependentRunStates)
{
	auto a = std::make_shared<messaging_quic_server>("rs_a");
	auto b = std::make_shared<messaging_quic_server>("rs_b");
	auto port_a = get_available_port();

	auto start_a = a->start_server(port_a);
	EXPECT_TRUE(start_a.is_ok());
	EXPECT_TRUE(a->is_running());
	EXPECT_FALSE(b->is_running());

	auto stop_a = a->stop_server();
	EXPECT_TRUE(stop_a.is_ok());
	EXPECT_FALSE(a->is_running());
	EXPECT_FALSE(b->is_running());
}

TEST_F(MessagingQuicServerTest, ManyServersConstructable)
{
	std::vector<std::shared_ptr<messaging_quic_server>> servers;
	servers.reserve(50);
	for (int i = 0; i < 50; ++i)
	{
		servers.push_back(std::make_shared<messaging_quic_server>(
			"bulk_" + std::to_string(i)));
	}
	for (size_t i = 0; i < servers.size(); ++i)
	{
		EXPECT_FALSE(servers[i]->is_running());
		EXPECT_EQ(servers[i]->session_count(), 0);
	}
}

// =============================================================================
// Broadcast / Multicast Edge Cases (no sessions)
// =============================================================================

TEST_F(MessagingQuicServerTest, BroadcastEmptyDataNoSessions)
{
	auto server = std::make_shared<messaging_quic_server>("bcast_empty");
	auto port = get_available_port();
	ASSERT_TRUE(server->start_server(port).is_ok());

	std::vector<uint8_t> data;
	auto result = server->broadcast(std::move(data));
	EXPECT_TRUE(result.is_ok());

	ASSERT_TRUE(server->stop_server().is_ok());
}

TEST_F(MessagingQuicServerTest, MulticastEmptyIdListNoSessions)
{
	auto server = std::make_shared<messaging_quic_server>("mcast_empty_ids");
	auto port = get_available_port();
	ASSERT_TRUE(server->start_server(port).is_ok());

	std::vector<std::string> ids;
	std::vector<uint8_t> data{0xAA, 0xBB};
	auto result = server->multicast(ids, std::move(data));
	EXPECT_TRUE(result.is_ok());

	ASSERT_TRUE(server->stop_server().is_ok());
}

TEST_F(MessagingQuicServerTest, MulticastUnknownIdsNoSessions)
{
	auto server = std::make_shared<messaging_quic_server>("mcast_unknown");
	auto port = get_available_port();
	ASSERT_TRUE(server->start_server(port).is_ok());

	std::vector<std::string> ids{"missing-1", "missing-2", ""};
	std::vector<uint8_t> data(64, 0xCC);
	auto result = server->multicast(ids, std::move(data));
	EXPECT_TRUE(result.is_ok());

	ASSERT_TRUE(server->stop_server().is_ok());
}

TEST_F(MessagingQuicServerTest, BroadcastLargeDataNoSessions)
{
	auto server = std::make_shared<messaging_quic_server>("bcast_large");
	auto port = get_available_port();
	ASSERT_TRUE(server->start_server(port).is_ok());

	std::vector<uint8_t> data(64 * 1024, 0x55);
	auto result = server->broadcast(std::move(data));
	EXPECT_TRUE(result.is_ok());

	ASSERT_TRUE(server->stop_server().is_ok());
}

// =============================================================================
// Disconnect Edge Cases
// =============================================================================

TEST_F(MessagingQuicServerTest, DisconnectSessionWithCustomErrorCode)
{
	auto server = std::make_shared<messaging_quic_server>("disc_custom_err");
	auto port = get_available_port();
	ASSERT_TRUE(server->start_server(port).is_ok());

	auto result = server->disconnect_session("nope", 0xDEADBEEFull);
	EXPECT_TRUE(result.is_err());
	EXPECT_EQ(result.error().code, error_codes::common_errors::not_found);

	ASSERT_TRUE(server->stop_server().is_ok());
}

TEST_F(MessagingQuicServerTest, DisconnectAllNonZeroErrorCode)
{
	auto server = std::make_shared<messaging_quic_server>("disc_all_err");
	auto port = get_available_port();
	ASSERT_TRUE(server->start_server(port).is_ok());

	server->disconnect_all(123);
	EXPECT_EQ(server->session_count(), 0);

	ASSERT_TRUE(server->stop_server().is_ok());
}

// =============================================================================
// Restart Lifecycle Tests
// =============================================================================

TEST_F(MessagingQuicServerTest, RestartAfterStop)
{
	auto server = std::make_shared<messaging_quic_server>("restart_test");

	auto port1 = get_available_port();
	ASSERT_TRUE(server->start_server(port1).is_ok());
	ASSERT_TRUE(server->is_running());
	ASSERT_TRUE(server->stop_server().is_ok());
	ASSERT_FALSE(server->is_running());

	auto port2 = get_available_port();
	auto start2 = server->start_server(port2);
	EXPECT_TRUE(start2.is_ok());
	EXPECT_TRUE(server->is_running());

	ASSERT_TRUE(server->stop_server().is_ok());
}

TEST_F(MessagingQuicServerTest, ServerIdStableAcrossLifecycle)
{
	auto server = std::make_shared<messaging_quic_server>("stable_id");
	EXPECT_EQ(server->server_id(), "stable_id");

	auto port = get_available_port();
	ASSERT_TRUE(server->start_server(port).is_ok());
	EXPECT_EQ(server->server_id(), "stable_id");

	ASSERT_TRUE(server->stop_server().is_ok());
	EXPECT_EQ(server->server_id(), "stable_id");
}

// =============================================================================
// quic_connection_stats Edge Cases
// =============================================================================

TEST_F(MessagingQuicServerTest, QuicConnectionStatsAccumulators)
{
	quic_connection_stats stats;
	stats.bytes_sent = 1024;
	stats.bytes_received = 2048;
	stats.packets_sent = 16;
	stats.packets_received = 32;
	stats.packets_lost = 1;
	stats.cwnd = 65536;

	EXPECT_EQ(stats.bytes_sent, 1024u);
	EXPECT_EQ(stats.bytes_received, 2048u);
	EXPECT_EQ(stats.packets_sent, 16u);
	EXPECT_EQ(stats.packets_received, 32u);
	EXPECT_EQ(stats.packets_lost, 1u);
	EXPECT_EQ(stats.cwnd, 65536u);
}

TEST_F(MessagingQuicServerTest, QuicConnectionStatsRttFields)
{
	quic_connection_stats stats;
	stats.smoothed_rtt = std::chrono::microseconds(12345);
	stats.min_rtt = std::chrono::microseconds(987);
	EXPECT_EQ(stats.smoothed_rtt.count(), 12345);
	EXPECT_EQ(stats.min_rtt.count(), 987);
}

} // namespace kcenon::network::core::test
