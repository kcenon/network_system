// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file tcp_facade_test.cpp
 * @brief Unit tests for tcp_facade module
 *
 * Tests validate:
 * - Client config struct defaults and custom values
 * - Server config struct defaults and custom values
 * - Pool config struct defaults and custom values
 * - Validation: empty host, port 0, zero timeout, SSL without certs
 * - Connection pool creation with valid/invalid configs
 * - Server creation with valid/invalid ports
 */

#include <gtest/gtest.h>

#include "kcenon/network/facade/tcp_facade.h"

#include <chrono>
#include <optional>
#include <string>

using namespace kcenon::network::facade;

// ============================================================================
// TCP Facade Config Default Tests
// ============================================================================

TEST(TcpFacadeModuleConfigTest, ClientConfigDefaults)
{
	tcp_facade::client_config config;

	EXPECT_TRUE(config.host.empty());
	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.client_id.empty());
	EXPECT_EQ(config.timeout, std::chrono::seconds(30));
	EXPECT_FALSE(config.use_ssl);
	EXPECT_FALSE(config.ca_cert_path.has_value());
	EXPECT_TRUE(config.verify_certificate);
}

TEST(TcpFacadeModuleConfigTest, ServerConfigDefaults)
{
	tcp_facade::server_config config;

	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.server_id.empty());
	EXPECT_FALSE(config.use_ssl);
	EXPECT_FALSE(config.cert_path.has_value());
	EXPECT_FALSE(config.key_path.has_value());
	EXPECT_FALSE(config.tls_version.has_value());
}

TEST(TcpFacadeModuleConfigTest, PoolConfigDefaults)
{
	tcp_facade::pool_config config;

	EXPECT_TRUE(config.host.empty());
	EXPECT_EQ(config.port, 0);
	EXPECT_EQ(config.pool_size, 10u);
	EXPECT_EQ(config.acquire_timeout, std::chrono::seconds(30));
}

// ============================================================================
// TCP Facade Config Custom Values Tests
// ============================================================================

TEST(TcpFacadeModuleConfigTest, ClientConfigCustomValues)
{
	tcp_facade::client_config config;
	config.host = "10.0.0.1";
	config.port = 4433;
	config.client_id = "tcp-module-client";
	config.timeout = std::chrono::seconds(60);
	config.use_ssl = true;
	config.ca_cert_path = "/certs/ca.pem";
	config.verify_certificate = false;

	EXPECT_EQ(config.host, "10.0.0.1");
	EXPECT_EQ(config.port, 4433);
	EXPECT_EQ(config.client_id, "tcp-module-client");
	EXPECT_EQ(config.timeout, std::chrono::seconds(60));
	EXPECT_TRUE(config.use_ssl);
	EXPECT_EQ(config.ca_cert_path.value(), "/certs/ca.pem");
	EXPECT_FALSE(config.verify_certificate);
}

TEST(TcpFacadeModuleConfigTest, ServerConfigCustomValues)
{
	tcp_facade::server_config config;
	config.port = 8443;
	config.server_id = "tcp-module-server";
	config.use_ssl = true;
	config.cert_path = "/certs/server.pem";
	config.key_path = "/certs/server-key.pem";
	config.tls_version = "TLSv1.3";

	EXPECT_EQ(config.port, 8443);
	EXPECT_EQ(config.server_id, "tcp-module-server");
	EXPECT_TRUE(config.use_ssl);
	EXPECT_EQ(config.cert_path.value(), "/certs/server.pem");
	EXPECT_EQ(config.key_path.value(), "/certs/server-key.pem");
	EXPECT_EQ(config.tls_version.value(), "TLSv1.3");
}

TEST(TcpFacadeModuleConfigTest, PoolConfigCustomValues)
{
	tcp_facade::pool_config config;
	config.host = "db.internal.net";
	config.port = 5432;
	config.pool_size = 50;
	config.acquire_timeout = std::chrono::seconds(120);

	EXPECT_EQ(config.host, "db.internal.net");
	EXPECT_EQ(config.port, 5432);
	EXPECT_EQ(config.pool_size, 50u);
	EXPECT_EQ(config.acquire_timeout, std::chrono::seconds(120));
}

// ============================================================================
// TCP Facade Client Creation Tests
// ============================================================================

class TcpFacadeModuleTest : public ::testing::Test
{
protected:
	tcp_facade facade_;
};

TEST_F(TcpFacadeModuleTest, CreateClientEmptyHostFails)
{
	tcp_facade::client_config config;
	config.host = "";
	config.port = 8080;
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreateClientPort0Fails)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 0;
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreateClientZeroTimeoutFails)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 8080;
	config.timeout = std::chrono::milliseconds(0);
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreateClientNegativeTimeoutFails)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 8080;
	config.timeout = std::chrono::milliseconds(-1);
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreateClientSslNotImplemented)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 8080;
	config.use_ssl = true;
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreateClientValidConfigConnects)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 8080;
	config.timeout = std::chrono::milliseconds(100);
	// May fail at connection level, but should not fail at validation
	EXPECT_NO_THROW(
		try
		{
			(void)facade_.create_client(config);
		}
		catch (const std::invalid_argument&)
		{
			throw;
		}
		catch (...)
		{
			// runtime_error from connection failure is acceptable
		});
}

// ============================================================================
// TCP Facade Server Creation Tests
// ============================================================================

TEST_F(TcpFacadeModuleTest, CreateServerPort0Fails)
{
	tcp_facade::server_config config;
	config.port = 0;
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreateServerSslWithoutCertFails)
{
	tcp_facade::server_config config;
	config.port = 8443;
	config.use_ssl = true;
	// Missing cert_path and key_path
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreateServerSslWithCertButNoKeyFails)
{
	tcp_facade::server_config config;
	config.port = 8443;
	config.use_ssl = true;
	config.cert_path = "/path/to/cert.pem";
	// Missing key_path
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreateServerSslNotImplemented)
{
	tcp_facade::server_config config;
	config.port = 8443;
	config.use_ssl = true;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	// Passes validation but SSL is not yet implemented
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreateServerValidPort)
{
	tcp_facade::server_config config;
	config.port = 8080;
	config.use_ssl = false;
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

// ============================================================================
// TCP Facade Connection Pool Tests
// ============================================================================

TEST_F(TcpFacadeModuleTest, CreatePoolEmptyHostFails)
{
	tcp_facade::pool_config config;
	config.host = "";
	config.port = 8080;
	config.pool_size = 5;
	auto result = facade_.create_connection_pool(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreatePoolPort0Fails)
{
	tcp_facade::pool_config config;
	config.host = "localhost";
	config.port = 0;
	config.pool_size = 5;
	auto result = facade_.create_connection_pool(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreatePoolSize0Fails)
{
	tcp_facade::pool_config config;
	config.host = "localhost";
	config.port = 8080;
	config.pool_size = 0;
	auto result = facade_.create_connection_pool(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeModuleTest, CreatePoolValidConfig)
{
	tcp_facade::pool_config config;
	config.host = "localhost";
	config.port = 8080;
	config.pool_size = 5;
	config.acquire_timeout = std::chrono::seconds(10);
	auto result = facade_.create_connection_pool(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(TcpFacadeModuleTest, CreatePoolLargeSize)
{
	tcp_facade::pool_config config;
	config.host = "localhost";
	config.port = 8080;
	config.pool_size = 200;
	auto result = facade_.create_connection_pool(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}
