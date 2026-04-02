// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>

#include "internal/http/websocket_client.h"
#include "kcenon/network/detail/utils/result_types.h"

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
