// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>

#include "kcenon/network/facade/tcp_facade.h"
#include "kcenon/network/interfaces/i_protocol_client.h"
#include "kcenon/network/interfaces/i_protocol_server.h"

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace kcenon::network::facade;
using namespace kcenon::network::interfaces;

class TcpFacadeTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		facade_ = std::make_unique<tcp_facade>();
	}

	void TearDown() override
	{
		// Cleanup any running clients
		// Note: server_ cleanup will be added after messaging_server implements IProtocolServer
		if (client_)
		{
			// Safe to call stop even if not running
			client_->stop();
		}
	}

	std::unique_ptr<tcp_facade> facade_;
	std::shared_ptr<i_protocol_server> server_;
	std::shared_ptr<i_protocol_client> client_;
};

// ============================================================================
// Client Configuration Validation Tests
// ============================================================================

TEST_F(TcpFacadeTest, CreateClientThrowsOnEmptyHost)
{
	tcp_facade::client_config config{
		.host = "",
		.port = 8080,
	};

	auto result = facade_->create_client(config);
	EXPECT_TRUE(result.is_err());
}

TEST_F(TcpFacadeTest, CreateClientThrowsOnInvalidPortZero)
{
	tcp_facade::client_config config{
		.host = "127.0.0.1",
		.port = 0,
	};

	auto result = facade_->create_client(config);
	EXPECT_TRUE(result.is_err());
}

// Temporarily disabled: uint16_t conversion makes port validation difficult to test
// Will be re-enabled with proper uint32_t port type or separate validation logic
// TEST_F(TcpFacadeTest, CreateClientThrowsOnInvalidPortTooLarge)

TEST_F(TcpFacadeTest, CreateClientThrowsOnNegativeTimeout)
{
	tcp_facade::client_config config{
		.host = "127.0.0.1",
		.port = 8080,
		.timeout = std::chrono::milliseconds(-1000),
	};

	auto result = facade_->create_client(config);
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Server Configuration Validation Tests
// ============================================================================

TEST_F(TcpFacadeTest, CreateServerThrowsOnInvalidPortZero)
{
	tcp_facade::server_config config{
		.port = 0,
	};

	auto result = facade_->create_server(config);
	EXPECT_TRUE(result.is_err());
}

// Temporarily disabled: Server implementation not ready
// TEST_F(TcpFacadeTest, CreateServerThrowsOnInvalidPortTooLarge)

// Server configuration validation tests are temporarily disabled
// Will be re-enabled after messaging_server implements IProtocolServer

// TEST_F(TcpFacadeTest, CreateServerThrowsOnSSLWithoutCert)
// TEST_F(TcpFacadeTest, CreateServerThrowsOnSSLWithoutKey)

// ============================================================================
// Basic Client Creation Tests
// ============================================================================

// Basic client creation tests are temporarily disabled due to lack of server implementation
// Will be re-enabled after messaging_server implements IProtocolServer
// These tests require a running server to connect to

// TEST_F(TcpFacadeTest, CreatePlainClientReturnsNonNull)

// Remaining test implementations will be added after:
// 1. messaging_server implements IProtocolServer interface
// 2. secure_messaging_client implements IProtocolClient interface
//
// Tests that require server functionality are temporarily disabled
