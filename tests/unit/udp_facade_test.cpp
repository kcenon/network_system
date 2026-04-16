// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file udp_facade_test.cpp
 * @brief Unit tests for udp_facade module
 *
 * Tests validate:
 * - Client config struct defaults and custom values
 * - Server config struct defaults and custom values
 * - Client validation: empty host, port 0
 * - Server validation: port 0
 * - Error result types for invalid configs
 */

#include <gtest/gtest.h>

#include "kcenon/network/facade/udp_facade.h"

#include <string>

using namespace kcenon::network::facade;

// ============================================================================
// UDP Facade Config Default Tests
// ============================================================================

TEST(UdpFacadeModuleConfigTest, ClientConfigDefaults)
{
	udp_facade::client_config config;

	EXPECT_TRUE(config.host.empty());
	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.client_id.empty());
}

TEST(UdpFacadeModuleConfigTest, ServerConfigDefaults)
{
	udp_facade::server_config config;

	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.server_id.empty());
}

// ============================================================================
// UDP Facade Config Custom Values Tests
// ============================================================================

TEST(UdpFacadeModuleConfigTest, ClientConfigCustomValues)
{
	udp_facade::client_config config;
	config.host = "192.168.0.100";
	config.port = 5555;
	config.client_id = "udp-module-client";

	EXPECT_EQ(config.host, "192.168.0.100");
	EXPECT_EQ(config.port, 5555);
	EXPECT_EQ(config.client_id, "udp-module-client");
}

TEST(UdpFacadeModuleConfigTest, ServerConfigCustomValues)
{
	udp_facade::server_config config;
	config.port = 7777;
	config.server_id = "udp-module-server";

	EXPECT_EQ(config.port, 7777);
	EXPECT_EQ(config.server_id, "udp-module-server");
}

// ============================================================================
// UDP Facade Client Creation Tests
// ============================================================================

class UdpFacadeModuleTest : public ::testing::Test
{
protected:
	udp_facade facade_;
};

TEST_F(UdpFacadeModuleTest, CreateClientEmptyHostFails)
{
	udp_facade::client_config config;
	config.host = "";
	config.port = 5555;
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(UdpFacadeModuleTest, CreateClientPort0Fails)
{
	udp_facade::client_config config;
	config.host = "localhost";
	config.port = 0;
	auto result = facade_.create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(UdpFacadeModuleTest, CreateClientValidConfig)
{
	udp_facade::client_config config;
	config.host = "localhost";
	config.port = 5555;
	// May fail at connection level, but should not throw invalid_argument
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

TEST_F(UdpFacadeModuleTest, CreateClientPortBoundary65535)
{
	udp_facade::client_config config;
	config.host = "localhost";
	config.port = 65535;
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
			// Other errors acceptable
		});
}

TEST_F(UdpFacadeModuleTest, CreateClientWithCustomId)
{
	udp_facade::client_config config;
	config.host = "localhost";
	config.port = 5555;
	config.client_id = "my-udp-module-client";
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
			// runtime_error acceptable
		});
}

// ============================================================================
// UDP Facade Server Creation Tests
// ============================================================================

TEST_F(UdpFacadeModuleTest, CreateServerPort0Fails)
{
	udp_facade::server_config config;
	config.port = 0;
	auto result = facade_.create_server(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(UdpFacadeModuleTest, CreateServerValidPort)
{
	udp_facade::server_config config;
	config.port = 5555;
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

TEST_F(UdpFacadeModuleTest, CreateServerPortBoundary1)
{
	udp_facade::server_config config;
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

TEST_F(UdpFacadeModuleTest, CreateServerPortBoundary65535)
{
	udp_facade::server_config config;
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

TEST_F(UdpFacadeModuleTest, CreateServerWithCustomId)
{
	udp_facade::server_config config;
	config.port = 5555;
	config.server_id = "my-udp-module-server";
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
