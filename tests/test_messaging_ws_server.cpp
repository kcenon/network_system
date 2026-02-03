/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "internal/http/websocket_server.h"
#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::core::test
{

class MessagingWsServerTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
	}

	void TearDown() override
	{
	}
};

// =============================================================================
// Basic Construction Tests
// =============================================================================

TEST_F(MessagingWsServerTest, BasicConstruction)
{
	auto server = std::make_shared<messaging_ws_server>("test_server");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), "test_server");
}

// =============================================================================
// Unified Pattern Type Alias Tests
// =============================================================================

TEST_F(MessagingWsServerTest, TypeAliasWsServer)
{
	// Verify ws_server is an alias for messaging_ws_server
	static_assert(std::is_same_v<ws_server, messaging_ws_server>,
	              "ws_server should be an alias for messaging_ws_server");

	auto server = std::make_shared<ws_server>("alias_test");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), "alias_test");
}

TEST_F(MessagingWsServerTest, TypeAliasSecureWsServer)
{
	// Verify secure_ws_server is an alias for messaging_ws_server
	// WSS (WebSocket Secure) uses TLS, but shares the same base implementation
	static_assert(std::is_same_v<secure_ws_server, messaging_ws_server>,
	              "secure_ws_server should be an alias for messaging_ws_server");
	static_assert(std::is_same_v<ws_server, secure_ws_server>,
	              "ws_server and secure_ws_server should be the same type");

	auto server = std::make_shared<secure_ws_server>("secure_alias_test");
	EXPECT_FALSE(server->is_running());
	EXPECT_EQ(server->server_id(), "secure_alias_test");
}

} // namespace kcenon::network::core::test
