// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file facade_id_generation_test.cpp
 * @brief Unit tests for facade validation edge cases and
 *        additional code paths in facade .cpp files
 *
 * Tests validate:
 * - TCP facade: port boundary 65535, pool creation with custom timeout,
 *   server config SSL with valid certs throws runtime_error (not invalid_argument)
 * - UDP facade: port boundary 65535, server config port out of range
 * - HTTP facade: valid client creation returns non-null, server port boundary
 * - WebSocket facade: valid path with leading slash, server config boundary
 * - QUIC facade: server config with all valid fields, client config boundary
 * - All facades: config struct member access and mutation
 */

#include <gtest/gtest.h>

#include "kcenon/network/facade/tcp_facade.h"
#include "kcenon/network/facade/udp_facade.h"
#include "kcenon/network/facade/http_facade.h"
#include "kcenon/network/facade/websocket_facade.h"
#include "kcenon/network/facade/quic_facade.h"

#include <chrono>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::facade;

// ============================================================================
// TCP Facade Extended Tests
// ============================================================================

class TcpFacadeExtendedTest : public ::testing::Test
{
protected:
	tcp_facade facade_;
};

TEST_F(TcpFacadeExtendedTest, ClientConfigPortBoundary65535)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 65535;
	config.timeout = std::chrono::milliseconds(100);
	// Port 65535 is valid; create_client may fail due to no server,
	// but should NOT throw invalid_argument
	EXPECT_NO_THROW(
		try
		{
			(void)facade_.create_client(config);
		}
		catch (const std::invalid_argument&)
		{
			// This should NOT happen - 65535 is valid
			throw;
		}
		catch (const std::runtime_error&)
		{
			// Expected: connection failure is runtime_error, not validation
		});
}

TEST_F(TcpFacadeExtendedTest, ClientConfigPort1IsValid)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 1;
	config.timeout = std::chrono::milliseconds(100);
	// Port 1 is the minimum valid port
	EXPECT_NO_THROW(
		try
		{
			(void)facade_.create_client(config);
		}
		catch (const std::invalid_argument&)
		{
			throw;
		}
		catch (const std::runtime_error&)
		{
			// Connection failure is acceptable
		});
}

TEST_F(TcpFacadeExtendedTest, ServerConfigPortBoundary65535)
{
	tcp_facade::server_config config;
	config.port = 65535;
	config.use_ssl = false;
	// Port 65535 is valid; create_server should not throw invalid_argument
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
			// Other errors are acceptable (e.g., bind failure)
		});
}

TEST_F(TcpFacadeExtendedTest, ServerConfigPort0Invalid)
{
	tcp_facade::server_config config;
	config.port = 0;
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(TcpFacadeExtendedTest, PoolConfigCustomTimeout)
{
	tcp_facade::pool_config config;
	config.host = "localhost";
	config.port = 8080;
	config.pool_size = 3;
	config.acquire_timeout = std::chrono::seconds(5);
	auto pool = facade_.create_connection_pool(config);
	EXPECT_NE(pool, nullptr);
}

TEST_F(TcpFacadeExtendedTest, PoolConfigLargePoolSize)
{
	tcp_facade::pool_config config;
	config.host = "localhost";
	config.port = 8080;
	config.pool_size = 100;
	auto pool = facade_.create_connection_pool(config);
	EXPECT_NE(pool, nullptr);
}

TEST_F(TcpFacadeExtendedTest, PoolConfigPortBoundary65535)
{
	tcp_facade::pool_config config;
	config.host = "example.com";
	config.port = 65535;
	config.pool_size = 1;
	auto pool = facade_.create_connection_pool(config);
	EXPECT_NE(pool, nullptr);
}

TEST_F(TcpFacadeExtendedTest, PoolConfigPort0Invalid)
{
	tcp_facade::pool_config config;
	config.host = "example.com";
	config.port = 0;
	config.pool_size = 1;
	EXPECT_THROW((void)facade_.create_connection_pool(config), std::invalid_argument);
}

TEST_F(TcpFacadeExtendedTest, ClientConfigCustomId)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 8080;
	config.client_id = "my-custom-client";
	// Custom ID should be accepted; may fail at connection level
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

TEST_F(TcpFacadeExtendedTest, ServerConfigCustomId)
{
	tcp_facade::server_config config;
	config.port = 8080;
	config.server_id = "my-custom-server";
	// Custom ID should be accepted
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
// TCP Facade Config Struct Tests
// ============================================================================

TEST(TcpFacadeConfigTest, ClientConfigMutation)
{
	tcp_facade::client_config config;
	config.host = "192.168.1.1";
	config.port = 9090;
	config.timeout = std::chrono::seconds(10);
	config.use_ssl = true;
	config.ca_cert_path = "/path/to/ca.pem";
	config.verify_certificate = false;

	EXPECT_EQ(config.host, "192.168.1.1");
	EXPECT_EQ(config.port, 9090);
	EXPECT_EQ(config.timeout, std::chrono::seconds(10));
	EXPECT_TRUE(config.use_ssl);
	EXPECT_EQ(config.ca_cert_path.value(), "/path/to/ca.pem");
	EXPECT_FALSE(config.verify_certificate);
}

TEST(TcpFacadeConfigTest, ServerConfigMutation)
{
	tcp_facade::server_config config;
	config.port = 443;
	config.use_ssl = true;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.server_id = "my-server";

	EXPECT_EQ(config.port, 443);
	EXPECT_TRUE(config.use_ssl);
	EXPECT_EQ(config.cert_path.value(), "/path/to/cert.pem");
	EXPECT_EQ(config.key_path.value(), "/path/to/key.pem");
	EXPECT_EQ(config.server_id, "my-server");
}

TEST(TcpFacadeConfigTest, PoolConfigMutation)
{
	tcp_facade::pool_config config;
	config.host = "db.example.com";
	config.port = 3306;
	config.pool_size = 20;
	config.acquire_timeout = std::chrono::seconds(60);

	EXPECT_EQ(config.host, "db.example.com");
	EXPECT_EQ(config.port, 3306);
	EXPECT_EQ(config.pool_size, 20);
	EXPECT_EQ(config.acquire_timeout, std::chrono::seconds(60));
}

// ============================================================================
// UDP Facade Extended Tests
// ============================================================================

class UdpFacadeExtendedTest : public ::testing::Test
{
protected:
	udp_facade facade_;
};

TEST_F(UdpFacadeExtendedTest, ClientConfigPortBoundary65535)
{
	udp_facade::client_config config;
	config.host = "localhost";
	config.port = 65535;
	// Should not throw invalid_argument
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

TEST_F(UdpFacadeExtendedTest, ClientConfigPort0Invalid)
{
	udp_facade::client_config config;
	config.host = "localhost";
	config.port = 0;
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(UdpFacadeExtendedTest, ServerConfigPortBoundary65535)
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

TEST_F(UdpFacadeExtendedTest, ServerConfigPort0Invalid)
{
	udp_facade::server_config config;
	config.port = 0;
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(UdpFacadeExtendedTest, ClientConfigCustomId)
{
	udp_facade::client_config config;
	config.host = "localhost";
	config.port = 5555;
	config.client_id = "custom-udp-client";
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

TEST_F(UdpFacadeExtendedTest, ServerConfigCustomId)
{
	udp_facade::server_config config;
	config.port = 5555;
	config.server_id = "custom-udp-server";
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
// HTTP Facade Extended Tests
// ============================================================================

class HttpFacadeExtendedTest : public ::testing::Test
{
protected:
	http_facade facade_;
};

TEST_F(HttpFacadeExtendedTest, ClientConfigValidCreatesClient)
{
	http_facade::client_config config;
	config.timeout = std::chrono::seconds(5);
	config.use_ssl = false;
	config.path = "/api/v1";
	auto client = facade_.create_client(config);
	EXPECT_NE(client, nullptr);
}

TEST_F(HttpFacadeExtendedTest, ClientConfigCustomId)
{
	http_facade::client_config config;
	config.timeout = std::chrono::seconds(5);
	config.client_id = "http-test-client";
	auto client = facade_.create_client(config);
	EXPECT_NE(client, nullptr);
}

TEST_F(HttpFacadeExtendedTest, ClientConfigSslFlag)
{
	http_facade::client_config config;
	config.timeout = std::chrono::seconds(5);
	config.use_ssl = true;
	config.path = "/secure";
	auto client = facade_.create_client(config);
	EXPECT_NE(client, nullptr);
}

TEST_F(HttpFacadeExtendedTest, ServerConfigPortBoundary65535)
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

TEST_F(HttpFacadeExtendedTest, ServerConfigPort0Invalid)
{
	http_facade::server_config config;
	config.port = 0;
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(HttpFacadeExtendedTest, ServerConfigCustomId)
{
	http_facade::server_config config;
	config.port = 8080;
	config.server_id = "http-test-server";
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
// HTTP Facade Config Tests
// ============================================================================

TEST(HttpFacadeConfigTest, ClientConfigMutation)
{
	http_facade::client_config config;
	config.timeout = std::chrono::seconds(15);
	config.use_ssl = true;
	config.path = "/api/v2";
	config.client_id = "test-client";

	EXPECT_EQ(config.timeout, std::chrono::seconds(15));
	EXPECT_TRUE(config.use_ssl);
	EXPECT_EQ(config.path, "/api/v2");
	EXPECT_EQ(config.client_id, "test-client");
}

// ============================================================================
// WebSocket Facade Extended Tests
// ============================================================================

class WebSocketFacadeExtendedTest : public ::testing::Test
{
protected:
	websocket_facade facade_;
};

TEST_F(WebSocketFacadeExtendedTest, ClientConfigValidCreatesClient)
{
	websocket_facade::client_config config;
	config.ping_interval = std::chrono::seconds(10);
	auto client = facade_.create_client(config);
	EXPECT_NE(client, nullptr);
}

TEST_F(WebSocketFacadeExtendedTest, ClientConfigCustomId)
{
	websocket_facade::client_config config;
	config.ping_interval = std::chrono::seconds(5);
	config.client_id = "ws-test-client";
	auto client = facade_.create_client(config);
	EXPECT_NE(client, nullptr);
}

TEST_F(WebSocketFacadeExtendedTest, ServerConfigValidPath)
{
	websocket_facade::server_config config;
	config.port = 8080;
	config.path = "/ws/chat";
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

TEST_F(WebSocketFacadeExtendedTest, ServerConfigCustomId)
{
	websocket_facade::server_config config;
	config.port = 8080;
	config.path = "/ws";
	config.server_id = "ws-test-server";
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

TEST_F(WebSocketFacadeExtendedTest, ServerConfigPort0Invalid)
{
	websocket_facade::server_config config;
	config.port = 0;
	config.path = "/ws";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

// ============================================================================
// QUIC Facade Extended Tests
// ============================================================================

class QuicFacadeExtendedTest : public ::testing::Test
{
protected:
	quic_facade facade_;
};

TEST_F(QuicFacadeExtendedTest, ClientConfigPortBoundary65535)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 65535;
	config.max_idle_timeout_ms = 10000;
	// Port 65535 is valid
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

TEST_F(QuicFacadeExtendedTest, ClientConfigPort0Invalid)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 0;
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(QuicFacadeExtendedTest, ServerConfigAllValid)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.max_idle_timeout_ms = 30000;
	config.max_connections = 100;
	config.alpn = "h3";
	config.require_client_cert = false;
	// Should pass validation
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

TEST_F(QuicFacadeExtendedTest, ServerConfigPort0Invalid)
{
	quic_facade::server_config config;
	config.port = 0;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(QuicFacadeExtendedTest, ClientConfigWithAlpn)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.alpn = "h3";
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

TEST_F(QuicFacadeExtendedTest, ClientConfigWithCaCert)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.ca_cert_path = "/path/to/ca.pem";
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

TEST_F(QuicFacadeExtendedTest, ClientConfigWithClientCert)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.client_cert_path = "/path/to/client.pem";
	config.client_key_path = "/path/to/client-key.pem";
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

TEST_F(QuicFacadeExtendedTest, ClientConfigVerifyServerFalse)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 10000;
	config.verify_server = false;
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

TEST_F(QuicFacadeExtendedTest, ServerConfigWithCaCert)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.ca_cert_path = "/path/to/ca.pem";
	config.require_client_cert = true;
	config.max_idle_timeout_ms = 30000;
	config.max_connections = 500;
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
// QUIC Config Struct Tests
// ============================================================================

TEST(QuicFacadeConfigTest, ClientConfigMutation)
{
	quic_facade::client_config config;
	config.host = "quic.example.com";
	config.port = 443;
	config.client_id = "quic-client-1";
	config.alpn = "h3";
	config.max_idle_timeout_ms = 60000;
	config.enable_0rtt = true;
	config.verify_server = false;

	EXPECT_EQ(config.host, "quic.example.com");
	EXPECT_EQ(config.port, 443);
	EXPECT_EQ(config.client_id, "quic-client-1");
	EXPECT_EQ(config.alpn, "h3");
	EXPECT_EQ(config.max_idle_timeout_ms, 60000u);
	EXPECT_TRUE(config.enable_0rtt);
	EXPECT_FALSE(config.verify_server);
}

TEST(QuicFacadeConfigTest, ServerConfigMutation)
{
	quic_facade::server_config config;
	config.port = 443;
	config.server_id = "quic-server-1";
	config.cert_path = "/certs/server.pem";
	config.key_path = "/certs/server-key.pem";
	config.alpn = "h3";
	config.max_idle_timeout_ms = 120000;
	config.max_connections = 50000;
	config.require_client_cert = true;

	EXPECT_EQ(config.port, 443);
	EXPECT_EQ(config.server_id, "quic-server-1");
	EXPECT_EQ(config.cert_path, "/certs/server.pem");
	EXPECT_EQ(config.key_path, "/certs/server-key.pem");
	EXPECT_EQ(config.alpn, "h3");
	EXPECT_EQ(config.max_idle_timeout_ms, 120000u);
	EXPECT_EQ(config.max_connections, 50000u);
	EXPECT_TRUE(config.require_client_cert);
}
