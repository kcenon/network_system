/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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
#include "kcenon/network/interfaces/i_protocol_client.h"
#include "kcenon/network/interfaces/i_protocol_server.h"

#include <chrono>
#include <memory>
#include <stdexcept>
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

	EXPECT_THROW(facade_->create_client(config), std::invalid_argument);
}

TEST_F(TcpFacadeTest, CreateClientThrowsOnInvalidPortZero)
{
	tcp_facade::client_config config{
		.host = "127.0.0.1",
		.port = 0,
	};

	EXPECT_THROW(facade_->create_client(config), std::invalid_argument);
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

	EXPECT_THROW(facade_->create_client(config), std::invalid_argument);
}

// ============================================================================
// Server Configuration Validation Tests
// ============================================================================

TEST_F(TcpFacadeTest, CreateServerThrowsOnInvalidPortZero)
{
	tcp_facade::server_config config{
		.port = 0,
	};

	EXPECT_THROW(facade_->create_server(config), std::invalid_argument);
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
