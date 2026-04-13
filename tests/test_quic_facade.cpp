// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>

#include "kcenon/network/facade/quic_facade.h"
#include "kcenon/network/interfaces/i_protocol_client.h"
#include "kcenon/network/interfaces/i_protocol_server.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::facade;
using namespace kcenon::network::interfaces;

class QuicFacadeTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		facade_ = std::make_unique<quic_facade>();
	}

	void TearDown() override
	{
		if (client_)
		{
			(void)client_->stop();
		}
		if (server_)
		{
			(void)server_->stop();
		}
	}

	std::unique_ptr<quic_facade> facade_;
	std::shared_ptr<i_protocol_client> client_;
	std::shared_ptr<i_protocol_server> server_;
};

// ============================================================================
// Client Configuration Validation Tests
// ============================================================================

TEST_F(QuicFacadeTest, CreateClientThrowsOnEmptyHost)
{
	quic_facade::client_config config{
		.host = "",
		.port = 4433,
	};

	auto result = facade_->create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeTest, CreateClientThrowsOnInvalidPortZero)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 0,
	};

	auto result = facade_->create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeTest, CreateClientThrowsOnZeroMaxIdleTimeout)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
		.max_idle_timeout_ms = 0,
	};

	auto result = facade_->create_client(config);
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Server Configuration Validation Tests
// ============================================================================

TEST_F(QuicFacadeTest, CreateServerThrowsOnInvalidPortZero)
{
	quic_facade::server_config config{
		.port = 0,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeTest, CreateServerThrowsOnEmptyCertPath)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeTest, CreateServerThrowsOnEmptyKeyPath)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "",
	};

	auto result = facade_->create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeTest, CreateServerThrowsOnZeroMaxIdleTimeout)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
		.max_idle_timeout_ms = 0,
	};

	auto result = facade_->create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeTest, CreateServerThrowsOnZeroMaxConnections)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
		.max_connections = 0,
	};

	auto result = facade_->create_server(config);
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Basic Client Creation Tests
// ============================================================================

TEST_F(QuicFacadeTest, CreateClientReturnsNonNull)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateClientWithCustomId)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
		.client_id = "my-quic-client",
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateClientWithAlpn)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
		.alpn = "h3",
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateClientWithCaCertPath)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
		.ca_cert_path = "/path/to/ca.pem",
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateClientWithMutualTls)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
		.client_cert_path = "/path/to/client.pem",
		.client_key_path = "/path/to/client-key.pem",
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateClientWithVerifyServerDisabled)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
		.verify_server = false,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateClientWithCustomMaxIdleTimeout)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
		.max_idle_timeout_ms = 60000,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateClientWithEnable0Rtt)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
		.enable_0rtt = true,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateMultipleClients)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
	};

	auto result1 = facade_->create_client(config);
	auto result2 = facade_->create_client(config);

	ASSERT_TRUE(result1.is_ok());
	ASSERT_TRUE(result2.is_ok());
	auto client1 = result1.value();
	auto client2 = result2.value();

	ASSERT_NE(client1, nullptr);
	ASSERT_NE(client2, nullptr);
	EXPECT_NE(client1, client2);

	// Cleanup
	(void)client1->stop();
	(void)client2->stop();
}

// ============================================================================
// Basic Server Creation Tests
// ============================================================================

TEST_F(QuicFacadeTest, CreateServerReturnsNonNull)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();

	ASSERT_NE(server_, nullptr);
}

TEST_F(QuicFacadeTest, CreateServerWithCustomId)
{
	quic_facade::server_config config{
		.port = 4433,
		.server_id = "my-quic-server",
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();

	ASSERT_NE(server_, nullptr);
}

TEST_F(QuicFacadeTest, CreateServerWithAutoGeneratedId)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();

	ASSERT_NE(server_, nullptr);
}

TEST_F(QuicFacadeTest, CreateServerWithAlpn)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
		.alpn = "h3",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();

	ASSERT_NE(server_, nullptr);
}

TEST_F(QuicFacadeTest, CreateServerWithMutualTls)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
		.ca_cert_path = "/path/to/ca.pem",
		.require_client_cert = true,
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();

	ASSERT_NE(server_, nullptr);
}

TEST_F(QuicFacadeTest, CreateServerWithCustomMaxConnections)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
		.max_connections = 500,
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();

	ASSERT_NE(server_, nullptr);
}

TEST_F(QuicFacadeTest, CreateMultipleServersWithUniqueAutoIds)
{
	quic_facade::server_config config1{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	quic_facade::server_config config2{
		.port = 4434,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result1 = facade_->create_server(config1);
	auto result2 = facade_->create_server(config2);

	ASSERT_TRUE(result1.is_ok());
	ASSERT_TRUE(result2.is_ok());
	auto server1 = result1.value();
	auto server2 = result2.value();

	ASSERT_NE(server1, nullptr);
	ASSERT_NE(server2, nullptr);
	EXPECT_NE(server1, server2);

	// Cleanup
	(void)server1->stop();
	(void)server2->stop();
}

// ============================================================================
// Interface Conformance Tests
// ============================================================================

TEST_F(QuicFacadeTest, ClientReturnsProtocolInterface)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	auto client = result.value();

	// Verify it implements i_protocol_client interface
	ASSERT_NE(client, nullptr);
	// Client is not connected before start() is called
	EXPECT_FALSE(client->is_connected());
}

TEST_F(QuicFacadeTest, ServerReturnsProtocolInterface)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	auto server = result.value();

	// Verify it implements i_protocol_server interface
	ASSERT_NE(server, nullptr);
	// Server has no connections before start()
	EXPECT_EQ(server->connection_count(), 0u);
}

// ============================================================================
// Client Guard Path Tests (unconnected operations)
// ============================================================================

TEST_F(QuicFacadeTest, SendOnUnconnectedClientReturnsError)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();
	ASSERT_NE(client_, nullptr);

	// Sending data when not connected should fail
	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	auto send_result = client_->send(std::move(data));
	EXPECT_TRUE(send_result.is_err());
}

TEST_F(QuicFacadeTest, IsConnectedReturnsFalseBeforeStart)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();
	ASSERT_NE(client_, nullptr);

	EXPECT_FALSE(client_->is_connected());
}

TEST_F(QuicFacadeTest, StopClientBeforeStartIsIdempotent)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();
	ASSERT_NE(client_, nullptr);

	// Stop without start should not crash or throw
	auto stop_result = client_->stop();
	// Either ok or graceful error - just shouldn't crash
	(void)stop_result;
	SUCCEED();
}

// ============================================================================
// Server Guard Path Tests (pre-start operations)
// ============================================================================

TEST_F(QuicFacadeTest, ConnectionCountZeroBeforeStart)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();
	ASSERT_NE(server_, nullptr);

	EXPECT_EQ(server_->connection_count(), 0u);
}

TEST_F(QuicFacadeTest, StopServerBeforeStartIsIdempotent)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();
	ASSERT_NE(server_, nullptr);

	// Stop without start should succeed gracefully
	auto stop_result = server_->stop();
	(void)stop_result;
	SUCCEED();
}

// ============================================================================
// Edge Cases and Boundary Tests
// ============================================================================

TEST_F(QuicFacadeTest, CreateClientWithMinimalValidPort)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 1,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateClientWithMaximalValidPort)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 65535,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateServerWithMinimalValidPort)
{
	quic_facade::server_config config{
		.port = 1,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();

	ASSERT_NE(server_, nullptr);
}

TEST_F(QuicFacadeTest, CreateServerWithMaximalValidPort)
{
	quic_facade::server_config config{
		.port = 65535,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();

	ASSERT_NE(server_, nullptr);
}

TEST_F(QuicFacadeTest, CreateClientWithMinimalIdleTimeout)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
		.max_idle_timeout_ms = 1,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();

	ASSERT_NE(client_, nullptr);
}

TEST_F(QuicFacadeTest, CreateServerWithMinimalMaxConnections)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
		.max_connections = 1,
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();

	ASSERT_NE(server_, nullptr);
}

// ============================================================================
// Callback Setup Tests (verify callbacks can be set without crashing)
// ============================================================================

TEST_F(QuicFacadeTest, ClientSetCallbacksBeforeStart)
{
	quic_facade::client_config config{
		.host = "127.0.0.1",
		.port = 4433,
	};

	auto result = facade_->create_client(config);
	ASSERT_TRUE(result.is_ok());
	client_ = result.value();
	ASSERT_NE(client_, nullptr);

	// Setting callbacks before start should not crash
	client_->set_receive_callback([](const std::vector<uint8_t>&) {});
	client_->set_connected_callback([]() {});
	client_->set_disconnected_callback([]() {});
	client_->set_error_callback([](std::error_code) {});

	SUCCEED();
}

TEST_F(QuicFacadeTest, ServerSetCallbacksBeforeStart)
{
	quic_facade::server_config config{
		.port = 4433,
		.cert_path = "/path/to/cert.pem",
		.key_path = "/path/to/key.pem",
	};

	auto result = facade_->create_server(config);
	ASSERT_TRUE(result.is_ok());
	server_ = result.value();
	ASSERT_NE(server_, nullptr);

	// Setting callbacks before start should not crash
	server_->set_connection_callback([](std::shared_ptr<i_session>) {});
	server_->set_disconnection_callback([](std::string_view) {});
	server_->set_receive_callback([](std::string_view, const std::vector<uint8_t>&) {});
	server_->set_error_callback([](std::string_view, std::error_code) {});

	SUCCEED();
}
