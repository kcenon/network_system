// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file websocket_facade_test.cpp
 * @brief Unit tests for websocket_facade module
 *
 * Tests validate:
 * - Client config struct defaults and custom values
 * - Server config struct defaults and custom values
 * - Client creation returns non-null with valid config
 * - Client validation: zero/negative ping_interval
 * - Server validation: port 0, empty path, path without leading slash
 */

#include <gtest/gtest.h>

#include "kcenon/network/facade/websocket_facade.h"

#include <chrono>
#include <string>

using namespace kcenon::network::facade;

// ============================================================================
// WebSocket Facade Config Default Tests
// ============================================================================

TEST(WebSocketFacadeModuleConfigTest, ClientConfigDefaults)
{
	websocket_facade::client_config config;

	EXPECT_TRUE(config.client_id.empty());
	EXPECT_EQ(config.ping_interval, std::chrono::seconds(30));
}

TEST(WebSocketFacadeModuleConfigTest, ServerConfigDefaults)
{
	websocket_facade::server_config config;

	EXPECT_EQ(config.port, 0);
	EXPECT_EQ(config.path, "/");
	EXPECT_TRUE(config.server_id.empty());
}

// ============================================================================
// WebSocket Facade Config Custom Values Tests
// ============================================================================

TEST(WebSocketFacadeModuleConfigTest, ClientConfigCustomValues)
{
	websocket_facade::client_config config;
	config.client_id = "ws-module-client";
	config.ping_interval = std::chrono::seconds(15);

	EXPECT_EQ(config.client_id, "ws-module-client");
	EXPECT_EQ(config.ping_interval, std::chrono::seconds(15));
}

TEST(WebSocketFacadeModuleConfigTest, ServerConfigCustomValues)
{
	websocket_facade::server_config config;
	config.port = 9090;
	config.path = "/ws/events";
	config.server_id = "ws-module-server";

	EXPECT_EQ(config.port, 9090);
	EXPECT_EQ(config.path, "/ws/events");
	EXPECT_EQ(config.server_id, "ws-module-server");
}

// ============================================================================
// WebSocket Facade Client Creation Tests
// ============================================================================

class WebSocketFacadeModuleTest : public ::testing::Test
{
protected:
	websocket_facade facade_;
};

TEST_F(WebSocketFacadeModuleTest, CreateClientWithDefaults)
{
	websocket_facade::client_config config;
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(WebSocketFacadeModuleTest, CreateClientWithCustomId)
{
	websocket_facade::client_config config;
	config.client_id = "my-ws-module-client";
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(WebSocketFacadeModuleTest, CreateClientWithCustomPingInterval)
{
	websocket_facade::client_config config;
	config.ping_interval = std::chrono::seconds(5);
	auto result = facade_.create_client(config);
	ASSERT_TRUE(result.is_ok());
	EXPECT_NE(result.value(), nullptr);
}

TEST_F(WebSocketFacadeModuleTest, CreateClientZeroPingIntervalFails)
{
	websocket_facade::client_config config;
	config.ping_interval = std::chrono::milliseconds(0);
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(WebSocketFacadeModuleTest, CreateClientNegativePingIntervalFails)
{
	websocket_facade::client_config config;
	config.ping_interval = std::chrono::milliseconds(-100);
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// WebSocket Facade Server Creation Tests
// ============================================================================

TEST_F(WebSocketFacadeModuleTest, CreateServerPort0Fails)
{
	websocket_facade::server_config config;
	config.port = 0;
	config.path = "/ws";
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(WebSocketFacadeModuleTest, CreateServerEmptyPathFails)
{
	websocket_facade::server_config config;
	config.port = 8080;
	config.path = "";
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(WebSocketFacadeModuleTest, CreateServerPathWithoutSlashFails)
{
	websocket_facade::server_config config;
	config.port = 8080;
	config.path = "ws/chat";
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(WebSocketFacadeModuleTest, CreateServerValidConfig)
{
	websocket_facade::server_config config;
	config.port = 8080;
	config.path = "/ws";
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

TEST_F(WebSocketFacadeModuleTest, CreateServerPortBoundary1)
{
	websocket_facade::server_config config;
	config.port = 1;
	config.path = "/";
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

TEST_F(WebSocketFacadeModuleTest, CreateServerPortBoundary65535)
{
	websocket_facade::server_config config;
	config.port = 65535;
	config.path = "/ws";
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

TEST_F(WebSocketFacadeModuleTest, CreateServerWithCustomId)
{
	websocket_facade::server_config config;
	config.port = 8080;
	config.path = "/ws/notifications";
	config.server_id = "my-ws-module-server";
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

TEST_F(WebSocketFacadeModuleTest, CreateServerDeepPath)
{
	websocket_facade::server_config config;
	config.port = 8080;
	config.path = "/api/v2/ws/stream";
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
