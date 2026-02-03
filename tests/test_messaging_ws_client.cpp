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

#include "internal/http/websocket_client.h"
#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::core::test
{

class MessagingWsClientTest : public ::testing::Test
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

TEST_F(MessagingWsClientTest, BasicConstruction)
{
	auto client = std::make_shared<messaging_ws_client>("test_client");
	EXPECT_FALSE(client->is_running());
	EXPECT_FALSE(client->is_connected());
	EXPECT_EQ(client->client_id(), "test_client");
}

// =============================================================================
// Unified Pattern Type Alias Tests
// =============================================================================

TEST_F(MessagingWsClientTest, TypeAliasWsClient)
{
	// Verify ws_client is an alias for messaging_ws_client
	static_assert(std::is_same_v<ws_client, messaging_ws_client>,
	              "ws_client should be an alias for messaging_ws_client");

	auto client = std::make_shared<ws_client>("alias_test");
	EXPECT_FALSE(client->is_running());
	EXPECT_EQ(client->client_id(), "alias_test");
}

TEST_F(MessagingWsClientTest, TypeAliasSecureWsClient)
{
	// Verify secure_ws_client is an alias for messaging_ws_client
	// WSS (WebSocket Secure) uses TLS, but shares the same base implementation
	static_assert(std::is_same_v<secure_ws_client, messaging_ws_client>,
	              "secure_ws_client should be an alias for messaging_ws_client");
	static_assert(std::is_same_v<ws_client, secure_ws_client>,
	              "ws_client and secure_ws_client should be the same type");

	auto client = std::make_shared<secure_ws_client>("secure_alias_test");
	EXPECT_FALSE(client->is_running());
	EXPECT_EQ(client->client_id(), "secure_alias_test");
}

} // namespace kcenon::network::core::test
