/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, network_system contributors
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

#include "kcenon/network/facade/tcp_facade.h"
#include "kcenon/network/facade/udp_facade.h"
#include "kcenon/network/facade/http_facade.h"
#include "kcenon/network/facade/websocket_facade.h"
#include "kcenon/network/facade/quic_facade.h"

#include <chrono>
#include <stdexcept>
#include <string>

using namespace kcenon::network::facade;

// ============================================================================
// TcpFacadeValidationTest
// ============================================================================

class TcpFacadeValidationTest : public ::testing::Test
{
protected:
	tcp_facade facade_;
};

TEST_F(TcpFacadeValidationTest, ClientConfigEmptyHost)
{
	tcp_facade::client_config config;
	config.host = "";
	config.port = 8080;
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, ClientConfigPortZero)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 0;
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, ClientConfigTimeoutZero)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 8080;
	config.timeout = std::chrono::milliseconds(0);
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, ClientConfigTimeoutNegative)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 8080;
	config.timeout = std::chrono::milliseconds(-1);
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, ClientConfigSslNotImplemented)
{
	tcp_facade::client_config config;
	config.host = "localhost";
	config.port = 8080;
	config.use_ssl = true;
	// SSL throws runtime_error, not invalid_argument
	EXPECT_THROW((void)facade_.create_client(config), std::runtime_error);
}

TEST_F(TcpFacadeValidationTest, ServerConfigPortZero)
{
	tcp_facade::server_config config;
	config.port = 0;
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, ServerConfigSslWithoutCertPath)
{
	tcp_facade::server_config config;
	config.port = 8080;
	config.use_ssl = true;
	// cert_path is std::nullopt by default
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, ServerConfigSslWithEmptyCertPath)
{
	tcp_facade::server_config config;
	config.port = 8080;
	config.use_ssl = true;
	config.cert_path = "";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, ServerConfigSslWithoutKeyPath)
{
	tcp_facade::server_config config;
	config.port = 8080;
	config.use_ssl = true;
	config.cert_path = "/path/to/cert.pem";
	// key_path is std::nullopt by default
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, ServerConfigSslWithEmptyKeyPath)
{
	tcp_facade::server_config config;
	config.port = 8080;
	config.use_ssl = true;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, ServerConfigSslNotImplemented)
{
	tcp_facade::server_config config;
	config.port = 8080;
	config.use_ssl = true;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	// Passes validation but SSL creation throws runtime_error
	EXPECT_THROW((void)facade_.create_server(config), std::runtime_error);
}

TEST_F(TcpFacadeValidationTest, PoolConfigEmptyHost)
{
	tcp_facade::pool_config config;
	config.host = "";
	config.port = 8080;
	config.pool_size = 5;
	EXPECT_THROW((void)facade_.create_connection_pool(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, PoolConfigPortZero)
{
	tcp_facade::pool_config config;
	config.host = "localhost";
	config.port = 0;
	config.pool_size = 5;
	EXPECT_THROW((void)facade_.create_connection_pool(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, PoolConfigSizeZero)
{
	tcp_facade::pool_config config;
	config.host = "localhost";
	config.port = 8080;
	config.pool_size = 0;
	EXPECT_THROW((void)facade_.create_connection_pool(config), std::invalid_argument);
}

TEST_F(TcpFacadeValidationTest, PoolConfigValidReturnsNonNull)
{
	tcp_facade::pool_config config;
	config.host = "localhost";
	config.port = 8080;
	config.pool_size = 5;
	// create_connection_pool only creates the pool object without connecting
	auto pool = facade_.create_connection_pool(config);
	EXPECT_NE(pool, nullptr);
}

// ============================================================================
// UdpFacadeValidationTest
// ============================================================================

class UdpFacadeValidationTest : public ::testing::Test
{
protected:
	udp_facade facade_;
};

TEST_F(UdpFacadeValidationTest, ClientConfigEmptyHost)
{
	udp_facade::client_config config;
	config.host = "";
	config.port = 5555;
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(UdpFacadeValidationTest, ClientConfigPortZero)
{
	udp_facade::client_config config;
	config.host = "localhost";
	config.port = 0;
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(UdpFacadeValidationTest, ServerConfigPortZero)
{
	udp_facade::server_config config;
	config.port = 0;
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

// ============================================================================
// HttpFacadeValidationTest
// ============================================================================

class HttpFacadeValidationTest : public ::testing::Test
{
protected:
	http_facade facade_;
};

TEST_F(HttpFacadeValidationTest, ClientConfigTimeoutZero)
{
	http_facade::client_config config;
	config.timeout = std::chrono::milliseconds(0);
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(HttpFacadeValidationTest, ClientConfigTimeoutNegative)
{
	http_facade::client_config config;
	config.timeout = std::chrono::milliseconds(-100);
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(HttpFacadeValidationTest, ServerConfigPortZero)
{
	http_facade::server_config config;
	config.port = 0;
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

// ============================================================================
// WebSocketFacadeValidationTest
// ============================================================================

class WebSocketFacadeValidationTest : public ::testing::Test
{
protected:
	websocket_facade facade_;
};

TEST_F(WebSocketFacadeValidationTest, ClientConfigPingIntervalZero)
{
	websocket_facade::client_config config;
	config.ping_interval = std::chrono::milliseconds(0);
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(WebSocketFacadeValidationTest, ClientConfigPingIntervalNegative)
{
	websocket_facade::client_config config;
	config.ping_interval = std::chrono::milliseconds(-1);
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(WebSocketFacadeValidationTest, ServerConfigPortZero)
{
	websocket_facade::server_config config;
	config.port = 0;
	config.path = "/ws";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(WebSocketFacadeValidationTest, ServerConfigPathEmpty)
{
	websocket_facade::server_config config;
	config.port = 8080;
	config.path = "";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(WebSocketFacadeValidationTest, ServerConfigPathWithoutLeadingSlash)
{
	websocket_facade::server_config config;
	config.port = 8080;
	config.path = "ws";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

// ============================================================================
// QuicFacadeValidationTest
// ============================================================================

class QuicFacadeValidationTest : public ::testing::Test
{
protected:
	quic_facade facade_;
};

TEST_F(QuicFacadeValidationTest, ClientConfigEmptyHost)
{
	quic_facade::client_config config;
	config.host = "";
	config.port = 4433;
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(QuicFacadeValidationTest, ClientConfigPortZero)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 0;
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(QuicFacadeValidationTest, ClientConfigMaxIdleTimeoutZero)
{
	quic_facade::client_config config;
	config.host = "localhost";
	config.port = 4433;
	config.max_idle_timeout_ms = 0;
	EXPECT_THROW((void)facade_.create_client(config), std::invalid_argument);
}

TEST_F(QuicFacadeValidationTest, ServerConfigPortZero)
{
	quic_facade::server_config config;
	config.port = 0;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(QuicFacadeValidationTest, ServerConfigEmptyCertPath)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "";
	config.key_path = "/path/to/key.pem";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(QuicFacadeValidationTest, ServerConfigEmptyKeyPath)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "";
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(QuicFacadeValidationTest, ServerConfigMaxIdleTimeoutZero)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.max_idle_timeout_ms = 0;
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

TEST_F(QuicFacadeValidationTest, ServerConfigMaxConnectionsZero)
{
	quic_facade::server_config config;
	config.port = 4433;
	config.cert_path = "/path/to/cert.pem";
	config.key_path = "/path/to/key.pem";
	config.max_connections = 0;
	EXPECT_THROW((void)facade_.create_server(config), std::invalid_argument);
}

// ============================================================================
// Config Default Values Tests
// ============================================================================

class FacadeConfigDefaultsTest : public ::testing::Test
{
};

TEST_F(FacadeConfigDefaultsTest, TcpClientDefaults)
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

TEST_F(FacadeConfigDefaultsTest, TcpServerDefaults)
{
	tcp_facade::server_config config;
	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.server_id.empty());
	EXPECT_FALSE(config.use_ssl);
	EXPECT_FALSE(config.cert_path.has_value());
	EXPECT_FALSE(config.key_path.has_value());
	EXPECT_FALSE(config.tls_version.has_value());
}

TEST_F(FacadeConfigDefaultsTest, TcpPoolDefaults)
{
	tcp_facade::pool_config config;
	EXPECT_TRUE(config.host.empty());
	EXPECT_EQ(config.port, 0);
	EXPECT_EQ(config.pool_size, 10);
}

TEST_F(FacadeConfigDefaultsTest, UdpClientDefaults)
{
	udp_facade::client_config config;
	EXPECT_TRUE(config.host.empty());
	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.client_id.empty());
}

TEST_F(FacadeConfigDefaultsTest, UdpServerDefaults)
{
	udp_facade::server_config config;
	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.server_id.empty());
}

TEST_F(FacadeConfigDefaultsTest, HttpClientDefaults)
{
	http_facade::client_config config;
	EXPECT_TRUE(config.client_id.empty());
	EXPECT_EQ(config.timeout, std::chrono::seconds(30));
	EXPECT_FALSE(config.use_ssl);
	EXPECT_EQ(config.path, "/");
}

TEST_F(FacadeConfigDefaultsTest, HttpServerDefaults)
{
	http_facade::server_config config;
	EXPECT_EQ(config.port, 0);
	EXPECT_TRUE(config.server_id.empty());
}

TEST_F(FacadeConfigDefaultsTest, WebSocketClientDefaults)
{
	websocket_facade::client_config config;
	EXPECT_TRUE(config.client_id.empty());
	EXPECT_EQ(config.ping_interval, std::chrono::seconds(30));
}

TEST_F(FacadeConfigDefaultsTest, WebSocketServerDefaults)
{
	websocket_facade::server_config config;
	EXPECT_EQ(config.port, 0);
	EXPECT_EQ(config.path, "/");
	EXPECT_TRUE(config.server_id.empty());
}

TEST_F(FacadeConfigDefaultsTest, QuicClientDefaults)
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

TEST_F(FacadeConfigDefaultsTest, QuicServerDefaults)
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
