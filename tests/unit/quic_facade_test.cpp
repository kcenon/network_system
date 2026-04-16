// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file quic_facade_test.cpp
 * @brief Unit tests for quic_facade module
 *
 * Tests validate:
 * - Client config struct defaults and custom values
 * - Server config struct defaults and custom values
 * - Client validation: empty host, port 0, zero max_idle_timeout_ms
 * - Server validation: port 0, empty cert/key paths, zero timeout, zero connections
 * - Client creation with ALPN, CA cert, client cert, verify_server options
 * - Server creation with all valid fields
 */

#include <gtest/gtest.h>

#include "kcenon/network/facade/quic_facade.h"

#include <optional>
#include <string>

using namespace kcenon::network::facade;

// ============================================================================
// QUIC Facade Config Default Tests
// ============================================================================

TEST(QuicFacadeModuleConfigTest, ClientConfigDefaults)
{
	quic_facade::client_config config;

	EXPECT_TRUE(config.host.empty());
	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.client_id.empty());
	EXPECT_FALSE(config.ca_cert_path.has_value());
	EXPECT_FALSE(config.client_cert_path.has_value());
	EXPECT_FALSE(config.client_key_path.has_value());
	EXPECT_TRUE(config.verify_server);
	EXPECT_TRUE(config.alpn.empty());
	EXPECT_EQ(config.max_idle_timeout_ms, 30000u);
	EXPECT_FALSE(config.enable_0rtt);
}

TEST(QuicFacadeModuleConfigTest, ServerConfigDefaults)
{
	quic_facade::server_config config;

	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.server_id.empty());
	EXPECT_TRUE(config.cert_path.empty());
	EXPECT_TRUE(config.key_path.empty());
	EXPECT_FALSE(config.ca_cert_path.has_value());
	EXPECT_FALSE(config.require_client_cert);
	EXPECT_TRUE(config.alpn.empty());
	EXPECT_EQ(config.max_idle_timeout_ms, 30000u);
	EXPECT_EQ(config.max_connections, 10000u);
}

// ============================================================================
// QUIC Facade Config Custom Values Tests
// ============================================================================

TEST(QuicFacadeModuleConfigTest, ClientConfigCustomValues)
{
	quic_facade::client_config config;
	config.host = "quic.example.org";
	config.port = 4433;
	config.client_id = "quic-module-client";
	config.ca_cert_path = "/certs/ca.pem";
	config.client_cert_path = "/certs/client.pem";
	config.client_key_path = "/certs/client-key.pem";
	config.verify_server = false;
	config.alpn = "h3";
	config.max_idle_timeout_ms = 60000;
	config.enable_0rtt = true;

	EXPECT_EQ(config.host, "quic.example.org");
	EXPECT_EQ(config.port, 4433);
	EXPECT_EQ(config.client_id, "quic-module-client");
	EXPECT_EQ(config.ca_cert_path.value(), "/certs/ca.pem");
	EXPECT_EQ(config.client_cert_path.value(), "/certs/client.pem");
	EXPECT_EQ(config.client_key_path.value(), "/certs/client-key.pem");
	EXPECT_FALSE(config.verify_server);
	EXPECT_EQ(config.alpn, "h3");
	EXPECT_EQ(config.max_idle_timeout_ms, 60000u);
	EXPECT_TRUE(config.enable_0rtt);
}

TEST(QuicFacadeModuleConfigTest, ServerConfigCustomValues)
{
	quic_facade::server_config config;
	config.port = 443;
	config.server_id = "quic-module-server";
	config.cert_path = "/certs/server.pem";
	config.key_path = "/certs/server-key.pem";
	config.ca_cert_path = "/certs/ca.pem";
	config.require_client_cert = true;
	config.alpn = "h3";
	config.max_idle_timeout_ms = 120000;
	config.max_connections = 50000;

	EXPECT_EQ(config.port, 443);
	EXPECT_EQ(config.server_id, "quic-module-server");
	EXPECT_EQ(config.cert_path, "/certs/server.pem");
	EXPECT_EQ(config.key_path, "/certs/server-key.pem");
	EXPECT_EQ(config.ca_cert_path.value(), "/certs/ca.pem");
	EXPECT_TRUE(config.require_client_cert);
	EXPECT_EQ(config.alpn, "h3");
	EXPECT_EQ(config.max_idle_timeout_ms, 120000u);
	EXPECT_EQ(config.max_connections, 50000u);
}

// ============================================================================
// QUIC Facade Client Creation Tests
// ============================================================================

class QuicFacadeModuleTest : public ::testing::Test
{
protected:
	quic_facade facade_;
};

TEST_F(QuicFacadeModuleTest, CreateClientEmptyHostFails)
{
	quic_facade::client_config config;
	config.host = "";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeModuleTest, CreateClientPort0Fails)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 0;
	config.max_idle_timeout_ms = 10000;
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeModuleTest, CreateClientZeroTimeoutFails)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 0;
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeModuleTest, CreateClientValidConfig)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(QuicFacadeModuleTest, CreateClientWithAlpn)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.alpn = "h3";
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(QuicFacadeModuleTest, CreateClientWithCaCert)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.ca_cert_path = "/path/to/ca.pem";
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(QuicFacadeModuleTest, CreateClientWithClientCerts)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.client_cert_path = "/path/to/client.pem";
	config.client_key_path = "/path/to/client-key.pem";
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(QuicFacadeModuleTest, CreateClientVerifyServerFalse)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.verify_server = false;
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(QuicFacadeModuleTest, CreateClientEnable0rtt)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.enable_0rtt = true;
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(QuicFacadeModuleTest, CreateClientWithCustomId)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.client_id = "my-quic-module-client";
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

// ============================================================================
// QUIC Facade Server Creation Tests
// ============================================================================

TEST_F(QuicFacadeModuleTest, CreateServerPort0Fails)
{
	quic_facade::server_config config;
	config.port = 0;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeModuleTest, CreateServerEmptyCertPathFails)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "";
	config.key_path = "/path/to/key.pem";
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeModuleTest, CreateServerEmptyKeyPathFails)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "";
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeModuleTest, CreateServerZeroTimeoutFails)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.max_idle_timeout_ms = 0;
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeModuleTest, CreateServerZeroMaxConnectionsFails)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.max_connections = 0;
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicFacadeModuleTest, CreateServerValidConfig)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.max_idle_timeout_ms = 30000;
	config.max_connections = 100;
	EXPECT_NO_THROW(
		try
		{
			(void)facade_.create_server(config);
		}
		catch (const std::invalid_argument&)
		{
			throw;
		}
		catch (...)
		{
			// Other errors acceptable (e.g., file not found)
		});
}

TEST_F(QuicFacadeModuleTest, CreateServerWithAlpn)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.alpn = "h3";
	EXPECT_NO_THROW(
		try
		{
			(void)facade_.create_server(config);
		}
		catch (const std::invalid_argument&)
		{
			throw;
		}
		catch (...)
		{
			// Other errors acceptable
		});
}

TEST_F(QuicFacadeModuleTest, CreateServerWithCaCertAndClientCert)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.ca_cert_path = "/path/to/ca.pem";
	config.require_client_cert = true;
	EXPECT_NO_THROW(
		try
		{
			(void)facade_.create_server(config);
		}
		catch (const std::invalid_argument&)
		{
			throw;
		}
		catch (...)
		{
			// Other errors acceptable
		});
}

TEST_F(QuicFacadeModuleTest, CreateServerWithCustomId)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.server_id = "my-quic-module-server";
	EXPECT_NO_THROW(
		try
		{
			(void)facade_.create_server(config);
		}
		catch (const std::invalid_argument&)
		{
			throw;
		}
		catch (...)
		{
			// Other errors acceptable
		});
}
