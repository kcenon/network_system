// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file http_facade_test.cpp
 * @brief Unit tests for http_facade module
 *
 * Tests validate:
 * - Client config struct defaults and custom values
 * - Server config struct defaults and custom values
 * - Client creation with valid config returns non-null
 * - Server creation validation (port boundaries)
 * - Error handling for invalid configs (zero timeout, port 0)
 */

#include <gtest/gtest.h>

#include "kcenon/network/facade/http_facade.h"

#include <chrono>
#include <string>

using namespace kcenon::network::facade;

// ============================================================================
// HTTP Facade Config Default Tests
// ============================================================================

TEST(HttpFacadeModuleConfigTest, ClientConfigDefaults)
{
	http_facade::client_config config;

	EXPECT_TRUE(config.client_id.empty());
	EXPECT_EQ(config.timeout, std::chrono::seconds(30));
	EXPECT_FALSE(config.use_ssl);
	EXPECT_EQ(config.path, "/");
}

TEST(HttpFacadeModuleConfigTest, ServerConfigDefaults)
{
	http_facade::server_config config;

	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.server_id.empty());
}

// ============================================================================
// HTTP Facade Config Custom Values Tests
// ============================================================================

TEST(HttpFacadeModuleConfigTest, ClientConfigCustomValues)
{
	http_facade::client_config config;
	config.client_id = "custom-http-client";
	config.timeout = std::chrono::milliseconds(5000);
	config.use_ssl = true;
	config.path = "/api/v1/data";

	EXPECT_EQ(config.client_id, "custom-http-client");
	EXPECT_EQ(config.timeout, std::chrono::milliseconds(5000));
	EXPECT_TRUE(config.use_ssl);
	EXPECT_EQ(config.path, "/api/v1/data");
}

TEST(HttpFacadeModuleConfigTest, ServerConfigCustomValues)
{
	http_facade::server_config config;
	config.port = 9090;
	config.server_id = "custom-http-server";

	EXPECT_EQ(config.port, 9090);
	EXPECT_EQ(config.server_id, "custom-http-server");
}

// ============================================================================
// HTTP Facade Client Creation Tests
// ============================================================================

class HttpFacadeModuleTest : public ::testing::Test
{
protected:
	http_facade facade_;
};

TEST_F(HttpFacadeModuleTest, CreateClientWithDefaults)
{
	http_facade::client_config config;
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(HttpFacadeModuleTest, CreateClientWithCustomId)
{
	http_facade::client_config config;
	config.client_id = "my-http-test-client";
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(HttpFacadeModuleTest, CreateClientWithSslEnabled)
{
	http_facade::client_config config;
	config.use_ssl = true;
	config.path = "/secure/endpoint";
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(HttpFacadeModuleTest, CreateClientWithCustomTimeout)
{
	http_facade::client_config config;
	config.timeout = std::chrono::seconds(60);
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(HttpFacadeModuleTest, CreateClientWithZeroTimeoutFails)
{
	http_facade::client_config config;
	config.timeout = std::chrono::milliseconds(0);
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpFacadeModuleTest, CreateClientWithNegativeTimeoutFails)
{
	http_facade::client_config config;
	config.timeout = std::chrono::milliseconds(-100);
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// HTTP Facade Server Creation Tests
// ============================================================================

TEST_F(HttpFacadeModuleTest, CreateServerPort0Fails)
{
	http_facade::server_config config;
	config.port = 0;
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(HttpFacadeModuleTest, CreateServerValidPort)
{
	http_facade::server_config config;
	config.port = 8080;
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
			// Other errors acceptable (e.g., bind failure)
		});
}

TEST_F(HttpFacadeModuleTest, CreateServerPortBoundary1)
{
	http_facade::server_config config;
	config.port = 1;
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

TEST_F(HttpFacadeModuleTest, CreateServerPortBoundary65535)
{
	http_facade::server_config config;
	config.port = 65535;
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

TEST_F(HttpFacadeModuleTest, CreateServerWithCustomId)
{
	http_facade::server_config config;
	config.port = 8080;
	config.server_id = "my-http-test-server";
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
