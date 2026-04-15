/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file quic_session_test.cpp
 * @brief Unit tests for quic_session class
 *
 * Tests validate:
 * - Construction with session ID
 * - Session ID property
 * - i_session and i_quic_session interface compliance
 * - Callback registration
 * - State management (active/inactive)
 *
 * Note: Full data transfer tests require a running QUIC socket.
 * These tests focus on construction, state, and interface compliance.
 */

#include "kcenon/network/detail/session/quic_session.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace kcenon::network::session;

// ============================================================================
// Construction Tests
// ============================================================================

class QuicSessionTest : public ::testing::Test
{
};

TEST_F(QuicSessionTest, ConstructWithNullSocketAndId)
{
	// quic_session can be constructed with nullptr socket
	// Operations requiring the socket will fail gracefully
	auto session = std::make_shared<quic_session>(nullptr, "quic-session-1");

	EXPECT_EQ(session->session_id(), "quic-session-1");
	EXPECT_EQ(session->id(), "quic-session-1");
}

TEST_F(QuicSessionTest, EmptySessionId)
{
	auto session = std::make_shared<quic_session>(nullptr, "");

	EXPECT_TRUE(session->session_id().empty());
	EXPECT_TRUE(session->id().empty());
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_F(QuicSessionTest, SetReceiveCallbackDoesNotCrash)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	EXPECT_NO_FATAL_FAILURE(
		session->set_receive_callback([](const std::vector<uint8_t>&) {}));
}

TEST_F(QuicSessionTest, SetStreamReceiveCallbackDoesNotCrash)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	EXPECT_NO_FATAL_FAILURE(
		session->set_stream_receive_callback(
			[](uint64_t, const std::vector<uint8_t>&, bool) {}));
}

TEST_F(QuicSessionTest, SetCloseCallbackDoesNotCrash)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	EXPECT_NO_FATAL_FAILURE(
		session->set_close_callback([]() {}));
}

// ============================================================================
// Interface Compliance Tests
// ============================================================================

TEST_F(QuicSessionTest, SendWithoutSocketReturnsError)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	auto result = session->send(std::vector<uint8_t>{0x01, 0x02});
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicSessionTest, CreateStreamWithoutSocketReturnsError)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	auto result = session->create_stream();
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicSessionTest, CreateUnidirectionalStreamWithoutSocketReturnsError)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	auto result = session->create_unidirectional_stream();
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicSessionTest, SendOnStreamWithoutSocketReturnsError)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	auto result = session->send_on_stream(0, std::vector<uint8_t>{0x01}, false);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicSessionTest, CloseStreamWithoutSocketReturnsError)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	auto result = session->close_stream(0);
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicSessionTest, SendStringWithoutSocketReturnsError)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	auto result = session->send("hello");
	EXPECT_TRUE(result.is_err());
}

TEST_F(QuicSessionTest, CloseWithErrorCodeWithoutSocket)
{
	auto session = std::make_shared<quic_session>(nullptr, "session");

	auto result = session->close(0);
	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Destructor Tests
// ============================================================================

TEST_F(QuicSessionTest, DestructorWithNullSocket)
{
	{
		auto session = std::make_shared<quic_session>(nullptr, "session");
	}
	// No crash = success
}
