// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>

#include "internal/http/websocket_server.h"
#include "kcenon/network/detail/utils/result_types.h"

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
