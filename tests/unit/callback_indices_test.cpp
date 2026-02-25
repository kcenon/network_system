/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/core/callback_indices.h"
#include <gtest/gtest.h>

using namespace kcenon::network;

/**
 * @file callback_indices_test.cpp
 * @brief Unit tests for callback_indices enums and to_index() helper
 *
 * Tests validate:
 * - tcp_client_callback enum values and count
 * - tcp_server_callback enum values and count
 * - udp_client_callback enum values and count
 * - secure_udp_client_callback enum values and count
 * - udp_server_callback enum values and count
 * - unified_udp_client_callback enum values and count
 * - unified_udp_server_callback enum values and count
 * - ws_client_callback enum values and count
 * - ws_server_callback enum values and count
 * - quic_client_callback enum values and count
 * - quic_server_callback enum values and count
 * - to_index() template helper for all enum types
 */

// ============================================================================
// TCP Client Callback Tests
// ============================================================================

class TcpClientCallbackTest : public ::testing::Test
{
};

TEST_F(TcpClientCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(tcp_client_callback::receive), 0);
	EXPECT_EQ(to_index(tcp_client_callback::connected), 1);
	EXPECT_EQ(to_index(tcp_client_callback::disconnected), 2);
	EXPECT_EQ(to_index(tcp_client_callback::error), 3);
}

TEST_F(TcpClientCallbackTest, EnumValuesAreDistinct)
{
	EXPECT_NE(tcp_client_callback::receive, tcp_client_callback::connected);
	EXPECT_NE(tcp_client_callback::connected, tcp_client_callback::disconnected);
	EXPECT_NE(tcp_client_callback::disconnected, tcp_client_callback::error);
}

// ============================================================================
// TCP Server Callback Tests
// ============================================================================

class TcpServerCallbackTest : public ::testing::Test
{
};

TEST_F(TcpServerCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(tcp_server_callback::connection), 0);
	EXPECT_EQ(to_index(tcp_server_callback::disconnection), 1);
	EXPECT_EQ(to_index(tcp_server_callback::receive), 2);
	EXPECT_EQ(to_index(tcp_server_callback::error), 3);
}

// ============================================================================
// UDP Client Callback Tests
// ============================================================================

class UdpClientCallbackTest : public ::testing::Test
{
};

TEST_F(UdpClientCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(udp_client_callback::receive), 0);
	EXPECT_EQ(to_index(udp_client_callback::error), 1);
}

// ============================================================================
// Secure UDP Client Callback Tests
// ============================================================================

class SecureUdpClientCallbackTest : public ::testing::Test
{
};

TEST_F(SecureUdpClientCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(secure_udp_client_callback::receive), 0);
	EXPECT_EQ(to_index(secure_udp_client_callback::connected), 1);
	EXPECT_EQ(to_index(secure_udp_client_callback::disconnected), 2);
	EXPECT_EQ(to_index(secure_udp_client_callback::error), 3);
}

// ============================================================================
// UDP Server Callback Tests
// ============================================================================

class UdpServerCallbackTest : public ::testing::Test
{
};

TEST_F(UdpServerCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(udp_server_callback::receive), 0);
	EXPECT_EQ(to_index(udp_server_callback::error), 1);
}

// ============================================================================
// Unified UDP Client Callback Tests
// ============================================================================

class UnifiedUdpClientCallbackTest : public ::testing::Test
{
};

TEST_F(UnifiedUdpClientCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(unified_udp_client_callback::receive), 0);
	EXPECT_EQ(to_index(unified_udp_client_callback::connected), 1);
	EXPECT_EQ(to_index(unified_udp_client_callback::disconnected), 2);
	EXPECT_EQ(to_index(unified_udp_client_callback::error), 3);
}

// ============================================================================
// Unified UDP Server Callback Tests
// ============================================================================

class UnifiedUdpServerCallbackTest : public ::testing::Test
{
};

TEST_F(UnifiedUdpServerCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(unified_udp_server_callback::receive), 0);
	EXPECT_EQ(to_index(unified_udp_server_callback::client_connected), 1);
	EXPECT_EQ(to_index(unified_udp_server_callback::client_disconnected), 2);
	EXPECT_EQ(to_index(unified_udp_server_callback::error), 3);
}

// ============================================================================
// WebSocket Client Callback Tests
// ============================================================================

class WsClientCallbackTest : public ::testing::Test
{
};

TEST_F(WsClientCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(ws_client_callback::message), 0);
	EXPECT_EQ(to_index(ws_client_callback::text_message), 1);
	EXPECT_EQ(to_index(ws_client_callback::binary_message), 2);
	EXPECT_EQ(to_index(ws_client_callback::connected), 3);
	EXPECT_EQ(to_index(ws_client_callback::disconnected), 4);
	EXPECT_EQ(to_index(ws_client_callback::error), 5);
}

// ============================================================================
// WebSocket Server Callback Tests
// ============================================================================

class WsServerCallbackTest : public ::testing::Test
{
};

TEST_F(WsServerCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(ws_server_callback::connection), 0);
	EXPECT_EQ(to_index(ws_server_callback::disconnection), 1);
	EXPECT_EQ(to_index(ws_server_callback::message), 2);
	EXPECT_EQ(to_index(ws_server_callback::text_message), 3);
	EXPECT_EQ(to_index(ws_server_callback::binary_message), 4);
	EXPECT_EQ(to_index(ws_server_callback::error), 5);
}

// ============================================================================
// QUIC Client Callback Tests
// ============================================================================

class QuicClientCallbackTest : public ::testing::Test
{
};

TEST_F(QuicClientCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(quic_client_callback::receive), 0);
	EXPECT_EQ(to_index(quic_client_callback::stream_receive), 1);
	EXPECT_EQ(to_index(quic_client_callback::connected), 2);
	EXPECT_EQ(to_index(quic_client_callback::disconnected), 3);
	EXPECT_EQ(to_index(quic_client_callback::error), 4);
}

// ============================================================================
// QUIC Server Callback Tests
// ============================================================================

class QuicServerCallbackTest : public ::testing::Test
{
};

TEST_F(QuicServerCallbackTest, EnumValuesAreSequential)
{
	EXPECT_EQ(to_index(quic_server_callback::connection), 0);
	EXPECT_EQ(to_index(quic_server_callback::disconnection), 1);
	EXPECT_EQ(to_index(quic_server_callback::receive), 2);
	EXPECT_EQ(to_index(quic_server_callback::stream_receive), 3);
	EXPECT_EQ(to_index(quic_server_callback::error), 4);
}

// ============================================================================
// to_index() Template Helper Tests
// ============================================================================

class ToIndexHelperTest : public ::testing::Test
{
};

TEST_F(ToIndexHelperTest, ReturnsCorrectSizeT)
{
	std::size_t index = to_index(tcp_client_callback::error);

	EXPECT_EQ(index, 3);
	static_assert(std::is_same_v<decltype(to_index(tcp_client_callback::error)), std::size_t>,
				  "to_index must return std::size_t");
}

TEST_F(ToIndexHelperTest, IsConstexpr)
{
	constexpr auto index = to_index(ws_client_callback::connected);

	static_assert(index == 3, "to_index should be usable in constexpr context");
	EXPECT_EQ(index, 3);
}

TEST_F(ToIndexHelperTest, WorksWithAllEnumTypes)
{
	// Verify to_index compiles and returns correct values for every enum type
	EXPECT_EQ(to_index(tcp_client_callback::receive), 0);
	EXPECT_EQ(to_index(tcp_server_callback::connection), 0);
	EXPECT_EQ(to_index(udp_client_callback::receive), 0);
	EXPECT_EQ(to_index(secure_udp_client_callback::receive), 0);
	EXPECT_EQ(to_index(udp_server_callback::receive), 0);
	EXPECT_EQ(to_index(unified_udp_client_callback::receive), 0);
	EXPECT_EQ(to_index(unified_udp_server_callback::receive), 0);
	EXPECT_EQ(to_index(ws_client_callback::message), 0);
	EXPECT_EQ(to_index(ws_server_callback::connection), 0);
	EXPECT_EQ(to_index(quic_client_callback::receive), 0);
	EXPECT_EQ(to_index(quic_server_callback::connection), 0);
}

TEST_F(ToIndexHelperTest, IsNoexcept)
{
	static_assert(noexcept(to_index(tcp_client_callback::receive)),
				  "to_index should be noexcept");
	SUCCEED();
}
