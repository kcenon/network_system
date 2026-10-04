/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/quic_socket.h"
#include <gtest/gtest.h>

#include <asio.hpp>

namespace internal = kcenon::network::internal;

/**
 * @file quic_socket_construction_test.cpp
 * @brief Unit tests for quic_socket construction and initial state
 *
 * Tests validate:
 * - Construction with client and server role
 * - state() returns idle initially
 * - is_connected() returns false initially
 * - role() returns correct role
 * - Callback setters do not throw
 * - Move construction preserves role and connection ID
 * - Connection ID generation produces unique IDs
 * - Socket access
 * - Role-based restrictions (connect/accept)
 * - Close on idle socket
 */

// ============================================================================
// Test Fixture
// ============================================================================

class QuicSocketConstructionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        io_context_ = std::make_unique<asio::io_context>();
    }

    void TearDown() override
    {
        io_context_->stop();
        io_context_.reset();
    }

    std::unique_ptr<asio::io_context> io_context_;
};

// ============================================================================
// Client Construction Tests
// ============================================================================

TEST_F(QuicSocketConstructionTest, ClientRoleIsCorrect)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    EXPECT_EQ(quic->role(), internal::quic_role::client);
}

TEST_F(QuicSocketConstructionTest, ClientStateIsIdle)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    EXPECT_EQ(quic->state(), internal::quic_connection_state::idle);
}

TEST_F(QuicSocketConstructionTest, ClientIsNotConnected)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    EXPECT_FALSE(quic->is_connected());
}

TEST_F(QuicSocketConstructionTest, ClientHandshakeNotComplete)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    EXPECT_FALSE(quic->is_handshake_complete());
}

// ============================================================================
// Server Construction Tests
// ============================================================================

TEST_F(QuicSocketConstructionTest, ServerRoleIsCorrect)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::server);

    EXPECT_EQ(quic->role(), internal::quic_role::server);
}

TEST_F(QuicSocketConstructionTest, ServerStateIsIdle)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::server);

    EXPECT_EQ(quic->state(), internal::quic_connection_state::idle);
}

TEST_F(QuicSocketConstructionTest, ServerIsNotConnected)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::server);

    EXPECT_FALSE(quic->is_connected());
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST_F(QuicSocketConstructionTest, SetStreamDataCallbackNoThrow)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    EXPECT_NO_THROW(quic->set_stream_data_callback(
        [](uint64_t, std::span<const uint8_t>, bool) {}));
}

TEST_F(QuicSocketConstructionTest, SetConnectedCallbackNoThrow)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    EXPECT_NO_THROW(quic->set_connected_callback([]() {}));
}

TEST_F(QuicSocketConstructionTest, SetErrorCallbackNoThrow)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    EXPECT_NO_THROW(quic->set_error_callback([](std::error_code) {}));
}

TEST_F(QuicSocketConstructionTest, SetCloseCallbackNoThrow)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    EXPECT_NO_THROW(quic->set_close_callback([](uint64_t, const std::string&) {}));
}

// ============================================================================
// Move Construction Tests
// ============================================================================

TEST_F(QuicSocketConstructionTest, MoveConstructionPreservesRole)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic1 = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    auto cid1 = quic1->local_connection_id();

    internal::quic_socket quic2(std::move(*quic1));

    EXPECT_EQ(quic2.role(), internal::quic_role::client);
    EXPECT_EQ(quic2.local_connection_id().to_string(), cid1.to_string());
}

// ============================================================================
// Connection ID Generation Tests
// ============================================================================

TEST_F(QuicSocketConstructionTest, ConnectionIdIsNonEmpty)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    auto& cid = quic->local_connection_id();
    EXPECT_GT(cid.length(), 0u);
    EXPECT_LE(cid.length(), 20u);
}

TEST_F(QuicSocketConstructionTest, TwoSocketsHaveUniqueConnectionIds)
{
    asio::ip::udp::socket socket1(*io_context_, asio::ip::udp::v4());
    asio::ip::udp::socket socket2(*io_context_, asio::ip::udp::v4());

    auto quic1 = std::make_shared<internal::quic_socket>(
        std::move(socket1), internal::quic_role::client);
    auto quic2 = std::make_shared<internal::quic_socket>(
        std::move(socket2), internal::quic_role::client);

    EXPECT_NE(quic1->local_connection_id().to_string(),
              quic2->local_connection_id().to_string());
}

// ============================================================================
// Socket Access Tests
// ============================================================================

TEST_F(QuicSocketConstructionTest, UnderlyingSocketIsOpen)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    auto& sock = quic->socket();
    EXPECT_TRUE(sock.is_open());
}

// ============================================================================
// Role-Based Restriction Tests
// ============================================================================

TEST_F(QuicSocketConstructionTest, ServerCannotConnect)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::server);

    asio::ip::udp::endpoint endpoint(
        asio::ip::make_address("127.0.0.1"), 12345);

    auto result = quic->connect(endpoint);
    EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketConstructionTest, ClientCannotAccept)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    auto result = quic->accept("cert.pem", "key.pem");
    EXPECT_FALSE(result.is_ok());
}

// ============================================================================
// Close Tests
// ============================================================================

TEST_F(QuicSocketConstructionTest, CloseIdleSocket)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    auto result = quic->close(0, "test close");
    EXPECT_TRUE(result.is_ok());
}

TEST_F(QuicSocketConstructionTest, DoubleCloseSucceeds)
{
    asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

    auto quic = std::make_shared<internal::quic_socket>(
        std::move(socket), internal::quic_role::client);

    auto result1 = quic->close(0, "first close");
    auto result2 = quic->close(0, "second close");

    EXPECT_TRUE(result1.is_ok());
    EXPECT_TRUE(result2.is_ok());
}
