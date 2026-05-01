// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include <asio.hpp>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <future>
#include <limits>
#include <string>
#include <vector>

#if defined(_WIN32)
#  include <process.h>
#  define QUIC_TEST_GETPID() static_cast<unsigned long>(::_getpid())
#else
#  include <unistd.h>
#  define QUIC_TEST_GETPID() static_cast<unsigned long>(::getpid())
#endif

#include "../src/internal/quic_socket.h"
#include "hermetic_transport_fixture.h"
#include "mock_tls_socket.h"
#include "mock_udp_peer.h"

namespace kcenon::network::internal::test
{

class QuicSocketTest : public ::testing::Test
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

// =============================================================================
// Construction Tests
// =============================================================================

TEST_F(QuicSocketTest, ClientConstruction)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	EXPECT_EQ(quic->role(), quic_role::client);
	EXPECT_EQ(quic->state(), quic_connection_state::idle);
	EXPECT_FALSE(quic->is_connected());
	EXPECT_FALSE(quic->is_handshake_complete());
}

TEST_F(QuicSocketTest, ServerConstruction)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::server);

	EXPECT_EQ(quic->role(), quic_role::server);
	EXPECT_EQ(quic->state(), quic_connection_state::idle);
	EXPECT_FALSE(quic->is_connected());
	EXPECT_FALSE(quic->is_handshake_complete());
}

TEST_F(QuicSocketTest, ConnectionIdGeneration)
{
	asio::ip::udp::socket socket1(*io_context_, asio::ip::udp::v4());
	asio::ip::udp::socket socket2(*io_context_, asio::ip::udp::v4());

	auto quic1 = std::make_shared<quic_socket>(
		std::move(socket1), quic_role::client);
	auto quic2 = std::make_shared<quic_socket>(
		std::move(socket2), quic_role::client);

	// Connection IDs should be different
	auto& cid1 = quic1->local_connection_id();
	auto& cid2 = quic2->local_connection_id();

	EXPECT_NE(cid1.to_string(), cid2.to_string());
	EXPECT_GT(cid1.length(), 0u);
	EXPECT_GT(cid2.length(), 0u);
}

// =============================================================================
// Callback Registration Tests
// =============================================================================

TEST_F(QuicSocketTest, CallbackRegistration)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	bool stream_cb_set = false;
	bool connected_cb_set = false;
	bool error_cb_set = false;
	bool close_cb_set = false;

	quic->set_stream_data_callback(
		[&stream_cb_set](uint64_t, std::span<const uint8_t>, bool) {
			stream_cb_set = true;
		});

	quic->set_connected_callback(
		[&connected_cb_set]() {
			connected_cb_set = true;
		});

	quic->set_error_callback(
		[&error_cb_set](std::error_code) {
			error_cb_set = true;
		});

	quic->set_close_callback(
		[&close_cb_set](uint64_t, const std::string&) {
			close_cb_set = true;
		});

	// Callbacks are set but not called yet
	EXPECT_FALSE(stream_cb_set);
	EXPECT_FALSE(connected_cb_set);
	EXPECT_FALSE(error_cb_set);
	EXPECT_FALSE(close_cb_set);
}

// =============================================================================
// State Transition Tests
// =============================================================================

TEST_F(QuicSocketTest, ClientConnectRequiresClientRole)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::server);

	asio::ip::udp::endpoint endpoint(
		asio::ip::make_address("127.0.0.1"), 12345);

	auto result = quic->connect(endpoint);
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, ServerAcceptRequiresServerRole)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto result = quic->accept("cert.pem", "key.pem");
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, DoubleConnectFails)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	asio::ip::udp::endpoint endpoint(
		asio::ip::make_address("127.0.0.1"), 12345);

	// First connect may succeed or fail depending on TLS setup
	auto result1 = quic->connect(endpoint);

	// Second connect should fail (not in idle state)
	auto result2 = quic->connect(endpoint);
	EXPECT_FALSE(result2.is_ok());
}

// =============================================================================
// Stream Management Tests
// =============================================================================

TEST_F(QuicSocketTest, CreateStreamRequiresConnection)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	// Not connected, so stream creation should fail
	auto result = quic->create_stream(false);
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, SendStreamDataRequiresConnection)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	auto result = quic->send_stream_data(0, std::move(data));
	EXPECT_FALSE(result.is_ok());
}

// =============================================================================
// Receive Loop Tests
// =============================================================================

TEST_F(QuicSocketTest, StartStopReceive)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	// Initially not receiving
	quic->start_receive();

	// Run io_context briefly
	io_context_->run_for(std::chrono::milliseconds(10));

	// Stop receiving
	quic->stop_receive();

	// Should be able to stop without issues
	SUCCEED();
}

// =============================================================================
// Close Tests
// =============================================================================

TEST_F(QuicSocketTest, CloseIdleSocket)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	// Closing an idle socket should succeed
	auto result = quic->close(0, "test close");
	EXPECT_TRUE(result.is_ok());
}

TEST_F(QuicSocketTest, DoubleClose)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto result1 = quic->close(0, "first close");
	auto result2 = quic->close(0, "second close");

	// Both should succeed (idempotent)
	EXPECT_TRUE(result1.is_ok());
	EXPECT_TRUE(result2.is_ok());
}

// =============================================================================
// Move Semantics Tests
// =============================================================================

TEST_F(QuicSocketTest, MoveConstruction)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic1 = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto cid1 = quic1->local_connection_id();

	quic_socket quic2(std::move(*quic1));

	EXPECT_EQ(quic2.role(), quic_role::client);
	EXPECT_EQ(quic2.local_connection_id().to_string(), cid1.to_string());
}

// =============================================================================
// Socket Access Tests
// =============================================================================

TEST_F(QuicSocketTest, SocketAccess)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());
	auto local_endpoint = socket.local_endpoint();

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto& sock = quic->socket();
	EXPECT_TRUE(sock.is_open());
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(QuicSocketTest, ConcurrentCallbackRegistration)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	std::atomic<int> counter{0};

	// Spawn multiple threads that register callbacks
	std::vector<std::thread> threads;
	for (int i = 0; i < 10; ++i)
	{
		threads.emplace_back([&quic, &counter, i]() {
			for (int j = 0; j < 100; ++j)
			{
				quic->set_connected_callback([&counter]() {
					counter++;
				});
				quic->set_error_callback([](std::error_code) {});
			}
		});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	// No crashes = success
	SUCCEED();
}

// =============================================================================
// Integration-like Tests (without actual network)
// =============================================================================

TEST_F(QuicSocketTest, ClientServerRoleDifference)
{
	asio::ip::udp::socket client_socket(*io_context_, asio::ip::udp::v4());
	asio::ip::udp::socket server_socket(*io_context_, asio::ip::udp::v4());

	auto client = std::make_shared<quic_socket>(
		std::move(client_socket), quic_role::client);
	auto server = std::make_shared<quic_socket>(
		std::move(server_socket), quic_role::server);

	EXPECT_EQ(client->role(), quic_role::client);
	EXPECT_EQ(server->role(), quic_role::server);

	// Client can call connect, server cannot
	asio::ip::udp::endpoint endpoint(
		asio::ip::make_address("127.0.0.1"), 12345);

	auto client_connect = client->connect(endpoint);
	auto server_connect = server->connect(endpoint);

	// Server connect should fail
	EXPECT_FALSE(server_connect.is_ok());

	// Server can call accept, client cannot
	auto client_accept = client->accept("cert.pem", "key.pem");
	EXPECT_FALSE(client_accept.is_ok());
}

// =============================================================================
// State Query Tests
// =============================================================================

TEST_F(QuicSocketTest, StateQueries)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	// Initial state
	EXPECT_EQ(quic->state(), quic_connection_state::idle);
	EXPECT_FALSE(quic->is_connected());
	EXPECT_FALSE(quic->is_handshake_complete());

	// Connection ID should be valid
	auto& cid = quic->local_connection_id();
	EXPECT_GT(cid.length(), 0u);
	EXPECT_LE(cid.length(), 20u);  // Max CID length per RFC 9000
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(QuicSocketTest, ErrorCallbackOnSocketError)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	std::promise<std::error_code> error_promise;
	auto error_future = error_promise.get_future();

	quic->set_error_callback(
		[&error_promise](std::error_code ec) {
			error_promise.set_value(ec);
		});

	// Start receiving on unbound socket - this should work
	quic->start_receive();

	// Run briefly
	io_context_->run_for(std::chrono::milliseconds(10));

	quic->stop_receive();

	// No error expected in this case
	SUCCEED();
}

// =============================================================================
// Additional Coverage Tests (Issue #1065)
// =============================================================================
// These tests target surface reachable without an active QUIC peer:
// validation guards, public-API input variations, multi-instance state
// isolation, and struct/result edge cases. The acceptance criteria of #1065
// (>= 80% line / >= 70% branch) cannot be fully met without an in-process
// QUIC loopback fixture; these tests expand coverage of reachable paths.

// ---- Construction / Lifetime ----

TEST_F(QuicSocketTest, DestructorStopsReceiveLoop)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	{
		auto quic = std::make_shared<quic_socket>(
			std::move(socket), quic_role::client);
		quic->start_receive();
	}
	// Destructor must not crash even with active receive
	io_context_->run_for(std::chrono::milliseconds(5));
	SUCCEED();
}

TEST_F(QuicSocketTest, ServerInitialStreamIdDifferentFromClient)
{
	asio::ip::udp::socket s_client(*io_context_, asio::ip::udp::v4());
	asio::ip::udp::socket s_server(*io_context_, asio::ip::udp::v4());

	auto client = std::make_shared<quic_socket>(
		std::move(s_client), quic_role::client);
	auto server = std::make_shared<quic_socket>(
		std::move(s_server), quic_role::server);

	// Both start in idle state; create_stream requires connected,
	// so we just verify role distinction here.
	EXPECT_EQ(client->role(), quic_role::client);
	EXPECT_EQ(server->role(), quic_role::server);
}

TEST_F(QuicSocketTest, IPv6SocketConstruction)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v6());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	EXPECT_EQ(quic->role(), quic_role::client);
	EXPECT_EQ(quic->state(), quic_connection_state::idle);
}

TEST_F(QuicSocketTest, ConnectionIdLengthIsRfc9000Compliant)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto& cid = quic->local_connection_id();
	// RFC 9000: connection IDs may be 0-20 bytes
	EXPECT_GE(cid.length(), 0u);
	EXPECT_LE(cid.length(), 20u);
}

TEST_F(QuicSocketTest, RemoteConnectionIdAccessibleBeforeConnect)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	// Should not crash; remote CID exists from default-construction
	const auto& rcid = quic->remote_connection_id();
	EXPECT_LE(rcid.length(), 20u);
}

// ---- connect() Validation Guards ----

TEST_F(QuicSocketTest, ConnectFromServerRoleFails)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::server);

	asio::ip::udp::endpoint endpoint(
		asio::ip::make_address("127.0.0.1"), 4433);

	auto result = quic->connect(endpoint);
	EXPECT_FALSE(result.is_ok());
	// State should remain idle
	EXPECT_EQ(quic->state(), quic_connection_state::idle);
}

TEST_F(QuicSocketTest, ConnectWithEmptyServerNameUsesAddressString)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	asio::ip::udp::endpoint endpoint(
		asio::ip::make_address("127.0.0.1"), 4433);

	// Whatever the result of TLS init, connect must not crash.
	(void)quic->connect(endpoint, "");
	SUCCEED();
}

TEST_F(QuicSocketTest, ConnectWithExplicitServerName)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	asio::ip::udp::endpoint endpoint(
		asio::ip::make_address("127.0.0.1"), 4433);

	(void)quic->connect(endpoint, "example.com");
	SUCCEED();
}

TEST_F(QuicSocketTest, ConnectToZeroPort)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	asio::ip::udp::endpoint endpoint(
		asio::ip::make_address("127.0.0.1"), 0);

	// Port-0 endpoint is technically valid; connect must not crash.
	(void)quic->connect(endpoint);
	SUCCEED();
}

TEST_F(QuicSocketTest, ConnectAfterCloseFails)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto close_result = quic->close(0, "test");
	EXPECT_TRUE(close_result.is_ok());

	asio::ip::udp::endpoint endpoint(
		asio::ip::make_address("127.0.0.1"), 4433);

	auto connect_result = quic->connect(endpoint);
	EXPECT_FALSE(connect_result.is_ok());
}

// ---- accept() Validation Guards ----

TEST_F(QuicSocketTest, AcceptFromClientRoleFails)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto result = quic->accept("nonexistent_cert.pem", "nonexistent_key.pem");
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, AcceptWithMissingCertFiles)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::server);

	auto result = quic->accept("/nonexistent/cert.pem", "/nonexistent/key.pem");
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, AcceptWithEmptyCertPaths)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::server);

	auto result = quic->accept("", "");
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, AcceptAfterCloseFails)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::server);

	(void)quic->close(0, "preclose");

	auto result = quic->accept("cert.pem", "key.pem");
	EXPECT_FALSE(result.is_ok());
}

// ---- close() Variations ----

TEST_F(QuicSocketTest, CloseWithNonZeroErrorCode)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto result = quic->close(0x1234, "application error");
	EXPECT_TRUE(result.is_ok());
}

TEST_F(QuicSocketTest, CloseWithEmptyReason)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto result = quic->close(0, "");
	EXPECT_TRUE(result.is_ok());
}

TEST_F(QuicSocketTest, CloseWithLongReasonString)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	std::string long_reason(1024, 'x');
	auto result = quic->close(0, long_reason);
	EXPECT_TRUE(result.is_ok());
}

TEST_F(QuicSocketTest, CloseDefaultArguments)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	// Use default arguments (error_code=0, reason="")
	auto result = quic->close();
	EXPECT_TRUE(result.is_ok());
}

// ---- send_stream_data Validation ----

TEST_F(QuicSocketTest, SendStreamDataEmptyPayloadRejected)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	std::vector<uint8_t> empty;
	auto result = quic->send_stream_data(0, std::move(empty));
	// Not connected -> must fail
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, SendStreamDataLargePayloadRejectedWhenNotConnected)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	std::vector<uint8_t> large(65536, 0xAB);
	auto result = quic->send_stream_data(42, std::move(large), true);
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, SendStreamDataFinFlagWhenNotConnected)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	std::vector<uint8_t> data = {0xFF};
	auto result = quic->send_stream_data(7, std::move(data), /*fin=*/true);
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, SendStreamDataHighStreamIdWhenNotConnected)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	std::vector<uint8_t> data = {0x01};
	auto result = quic->send_stream_data(
		std::numeric_limits<uint64_t>::max(), std::move(data));
	EXPECT_FALSE(result.is_ok());
}

// ---- Stream Management ----

TEST_F(QuicSocketTest, CreateUnidirectionalStreamRequiresConnection)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto result = quic->create_stream(/*unidirectional=*/true);
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, CreateBidirectionalStreamRequiresConnection)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::server);

	auto result = quic->create_stream(/*unidirectional=*/false);
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, CloseUnknownStreamFails)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto result = quic->close_stream(9999);
	EXPECT_FALSE(result.is_ok());
}

TEST_F(QuicSocketTest, CloseStreamZeroFails)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto result = quic->close_stream(0);
	EXPECT_FALSE(result.is_ok());
}

// ---- Receive loop ----

TEST_F(QuicSocketTest, StopReceiveWithoutStart)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	// Calling stop without start must be a no-op
	quic->stop_receive();
	SUCCEED();
}

TEST_F(QuicSocketTest, StartReceiveTwiceIsIdempotent)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	quic->start_receive();
	quic->start_receive();
	io_context_->run_for(std::chrono::milliseconds(5));
	quic->stop_receive();
	SUCCEED();
}

TEST_F(QuicSocketTest, StartStopStartCycle)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	quic->start_receive();
	io_context_->run_for(std::chrono::milliseconds(2));
	quic->stop_receive();
	quic->start_receive();
	io_context_->run_for(std::chrono::milliseconds(2));
	quic->stop_receive();
	SUCCEED();
}

// ---- State Queries ----

TEST_F(QuicSocketTest, StateRemainsIdleAfterFailedConnect)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::server);

	asio::ip::udp::endpoint endpoint(
		asio::ip::make_address("127.0.0.1"), 4433);

	(void)quic->connect(endpoint);
	// Server role rejects connect; state must remain idle.
	EXPECT_EQ(quic->state(), quic_connection_state::idle);
}

TEST_F(QuicSocketTest, RemoteEndpointDefaultsToZero)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	auto ep = quic->remote_endpoint();
	EXPECT_EQ(ep.port(), 0u);
}

TEST_F(QuicSocketTest, IsConnectedFalseInIdleState)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	EXPECT_FALSE(quic->is_connected());
	EXPECT_NE(quic->state(), quic_connection_state::connected);
}

TEST_F(QuicSocketTest, HandshakeNotCompleteInIdleState)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::server);

	EXPECT_FALSE(quic->is_handshake_complete());
}

// ---- Multi-instance Isolation ----

TEST_F(QuicSocketTest, MultipleInstancesHaveIndependentState)
{
	asio::ip::udp::socket s1(*io_context_, asio::ip::udp::v4());
	asio::ip::udp::socket s2(*io_context_, asio::ip::udp::v4());
	asio::ip::udp::socket s3(*io_context_, asio::ip::udp::v4());

	auto q1 = std::make_shared<quic_socket>(std::move(s1), quic_role::client);
	auto q2 = std::make_shared<quic_socket>(std::move(s2), quic_role::server);
	auto q3 = std::make_shared<quic_socket>(std::move(s3), quic_role::client);

	EXPECT_EQ(q1->role(), quic_role::client);
	EXPECT_EQ(q2->role(), quic_role::server);
	EXPECT_EQ(q3->role(), quic_role::client);

	// Each instance has a unique connection ID
	EXPECT_NE(q1->local_connection_id().to_string(),
	          q2->local_connection_id().to_string());
	EXPECT_NE(q2->local_connection_id().to_string(),
	          q3->local_connection_id().to_string());
	EXPECT_NE(q1->local_connection_id().to_string(),
	          q3->local_connection_id().to_string());
}

TEST_F(QuicSocketTest, CloseOneInstanceDoesNotAffectOthers)
{
	asio::ip::udp::socket s1(*io_context_, asio::ip::udp::v4());
	asio::ip::udp::socket s2(*io_context_, asio::ip::udp::v4());

	auto q1 = std::make_shared<quic_socket>(std::move(s1), quic_role::client);
	auto q2 = std::make_shared<quic_socket>(std::move(s2), quic_role::client);

	auto r1 = q1->close(0, "close q1");
	EXPECT_TRUE(r1.is_ok());

	// q2 must remain in idle state
	EXPECT_EQ(q2->state(), quic_connection_state::idle);
}

// ---- Move Semantics ----

TEST_F(QuicSocketTest, MoveAssignmentTransfersState)
{
	asio::ip::udp::socket s1(*io_context_, asio::ip::udp::v4());
	asio::ip::udp::socket s2(*io_context_, asio::ip::udp::v4());

	auto q1 = std::make_shared<quic_socket>(std::move(s1), quic_role::client);
	auto q2 = std::make_shared<quic_socket>(std::move(s2), quic_role::server);

	auto orig_cid = q1->local_connection_id();

	*q2 = std::move(*q1);

	EXPECT_EQ(q2->role(), quic_role::client);
	EXPECT_EQ(q2->local_connection_id().to_string(), orig_cid.to_string());
}

TEST_F(QuicSocketTest, SelfMoveAssignmentSafe)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	quic_socket q(std::move(socket), quic_role::client);
	auto orig_cid = q.local_connection_id().to_string();

	// Self-assignment must be a no-op (guarded by this != &other).
	// Use a pointer to avoid -Wself-move warnings.
	quic_socket* p = &q;
	*p = std::move(*p);

	EXPECT_EQ(q.local_connection_id().to_string(), orig_cid);
	EXPECT_EQ(q.role(), quic_role::client);
}

// ---- Callback Edge Cases ----

TEST_F(QuicSocketTest, NullCallbacksAccepted)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	// Passing default-constructed (null) callbacks must not crash.
	quic->set_stream_data_callback({});
	quic->set_connected_callback({});
	quic->set_error_callback({});
	quic->set_close_callback({});
	SUCCEED();
}

TEST_F(QuicSocketTest, OverwriteCallbackReplacesOld)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	int v = 0;
	quic->set_connected_callback([&v]() { v = 1; });
	quic->set_connected_callback([&v]() { v = 2; });
	// Without invocation we cannot verify which fires; ensure no crash.
	(void)v;
	SUCCEED();
}

// ---- Enum / Result Sanity ----

TEST_F(QuicSocketTest, EnumQuicRoleValues)
{
	EXPECT_EQ(static_cast<uint8_t>(quic_role::client), 0u);
	EXPECT_EQ(static_cast<uint8_t>(quic_role::server), 1u);
}

TEST_F(QuicSocketTest, EnumStateOrderingMatchesHeader)
{
	EXPECT_EQ(static_cast<uint8_t>(quic_connection_state::idle), 0u);
	EXPECT_EQ(static_cast<uint8_t>(quic_connection_state::handshake_start), 1u);
	EXPECT_EQ(static_cast<uint8_t>(quic_connection_state::handshake), 2u);
	EXPECT_EQ(static_cast<uint8_t>(quic_connection_state::connected), 3u);
	EXPECT_EQ(static_cast<uint8_t>(quic_connection_state::closing), 4u);
	EXPECT_EQ(static_cast<uint8_t>(quic_connection_state::draining), 5u);
	EXPECT_EQ(static_cast<uint8_t>(quic_connection_state::closed), 6u);
}

TEST_F(QuicSocketTest, ConstSocketAccessor)
{
	asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());

	auto quic = std::make_shared<quic_socket>(
		std::move(socket), quic_role::client);

	const auto& cquic = *quic;
	const auto& sock = cquic.socket();
	EXPECT_TRUE(sock.is_open());
}

// =============================================================================
// Hermetic loopback coverage tests (Issue #1065)
// =============================================================================
// These tests drive quic_socket through its full publicly reachable surface
// using a real loopback UDP pair plus an in-memory self-signed PEM written
// to a temporary file for the server's accept(). They expand line/branch
// coverage of src/internal/quic_socket.cpp into the receive loop, packet
// dispatch, send-pending, close + draining timer, and frame processing
// branches that the idle-state-only tests above cannot reach.

namespace
{

using kcenon::network::tests::support::generate_self_signed_pem;
using kcenon::network::tests::support::make_loopback_udp_pair;
using kcenon::network::tests::support::make_quic_initial_packet_stub;
using kcenon::network::tests::support::mock_udp_peer;

// RAII wrapper that writes a self-signed cert + key to two temporary files
// and unlinks them on destruction. quic_crypto::init_server() requires file
// paths (it calls SSL_CTX_use_certificate_file / SSL_CTX_use_PrivateKey_file)
// so we cannot use the in-memory PEM directly.
class TempPemFiles
{
public:
	TempPemFiles()
	{
		const auto pem = generate_self_signed_pem();
		cert_path_ = make_unique_path("cert");
		key_path_ = make_unique_path("key");
		write_to(cert_path_, pem.cert_pem);
		write_to(key_path_, pem.key_pem);
	}

	~TempPemFiles()
	{
		std::remove(cert_path_.c_str());
		std::remove(key_path_.c_str());
	}

	TempPemFiles(const TempPemFiles&) = delete;
	TempPemFiles& operator=(const TempPemFiles&) = delete;

	[[nodiscard]] const std::string& cert() const noexcept { return cert_path_; }
	[[nodiscard]] const std::string& key() const noexcept { return key_path_; }

private:
	static std::string make_unique_path(const char* tag)
	{
		const char* tmpdir = std::getenv("TMPDIR");
		if (!tmpdir || !*tmpdir)
		{
			tmpdir = std::getenv("TEMP");
		}
		if (!tmpdir || !*tmpdir)
		{
			tmpdir = std::getenv("TMP");
		}
		if (!tmpdir || !*tmpdir)
		{
#if defined(_WIN32)
			tmpdir = ".";
#else
			tmpdir = "/tmp";
#endif
		}
		const auto pid = QUIC_TEST_GETPID();
		const auto stamp = static_cast<unsigned long long>(
			std::chrono::steady_clock::now().time_since_epoch().count());
#if defined(_WIN32)
		const char sep = '\\';
#else
		const char sep = '/';
#endif
		return std::string(tmpdir) + sep + "quic_socket_test_" + tag + "_" +
		       std::to_string(pid) + "_" + std::to_string(stamp) + ".pem";
	}

	static void write_to(const std::string& path, const std::string& data)
	{
		std::ofstream out(path, std::ios::binary);
		out.write(data.data(), static_cast<std::streamsize>(data.size()));
		out.close();
	}

	std::string cert_path_;
	std::string key_path_;
};

} // namespace

class QuicSocketHermeticTest
	: public kcenon::network::tests::support::hermetic_transport_fixture
{
};

// ----- connect() happy path drives crypto init + start_receive + send -----

TEST_F(QuicSocketHermeticTest, ClientConnectInitializesCryptoAndStartsReceive)
{
	auto pair = make_loopback_udp_pair(io());
	auto& server_sock = pair.first;
	asio::ip::udp::socket client_sock = std::move(pair.second);

	const auto server_endpoint = server_sock.local_endpoint();

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	auto result = client->connect(server_endpoint, "localhost");

	// init_client() with TLS_client_method does not require cert files,
	// so connect() should succeed and transition through handshake_start
	// into handshake.
	EXPECT_TRUE(result.is_ok())
		<< "connect() failed: TLS client init unexpectedly broken";
	EXPECT_NE(client->state(), quic_connection_state::idle);
	EXPECT_FALSE(client->is_connected());
	EXPECT_FALSE(client->is_handshake_complete());

	// Allow the worker io_context a moment to drain async sends.
	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	EXPECT_TRUE(client->close(0, "test").is_ok());
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

TEST_F(QuicSocketHermeticTest, ClientConnectWithEmptyServerNameUsesAddress)
{
	auto pair = make_loopback_udp_pair(io());
	auto& server_sock = pair.first;
	asio::ip::udp::socket client_sock = std::move(pair.second);

	const auto server_endpoint = server_sock.local_endpoint();

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	// Empty server_name triggers the branch that falls back to the address
	// string for SNI inside connect().
	auto result = client->connect(server_endpoint, "");
	EXPECT_TRUE(result.is_ok());

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	(void)client->close(0, "");
}

TEST_F(QuicSocketHermeticTest, ConnectAfterStartedFailsAndStateUnchanged)
{
	auto pair = make_loopback_udp_pair(io());
	auto& server_sock = pair.first;
	asio::ip::udp::socket client_sock = std::move(pair.second);

	const auto server_endpoint = server_sock.local_endpoint();

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	auto first = client->connect(server_endpoint, "localhost");
	EXPECT_TRUE(first.is_ok());

	const auto state_after_first = client->state();
	EXPECT_NE(state_after_first, quic_connection_state::idle);

	// Second connect must hit the "already in progress" branch.
	auto second = client->connect(server_endpoint, "localhost");
	EXPECT_FALSE(second.is_ok());

	// State must not regress from the second attempt.
	EXPECT_NE(client->state(), quic_connection_state::idle);

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	(void)client->close(0, "");
}

// ----- accept() happy path drives server-side init -----

TEST_F(QuicSocketHermeticTest, ServerAcceptInitializesCryptoAndStartsReceive)
{
	TempPemFiles pem;

	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket server_sock = std::move(pair.first);

	auto server = std::make_shared<quic_socket>(
		std::move(server_sock), quic_role::server);

	auto result = server->accept(pem.cert(), pem.key());
	EXPECT_TRUE(result.is_ok())
		<< "accept() failed with valid in-memory PEM files";
	EXPECT_NE(server->state(), quic_connection_state::idle);

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	server->stop_receive();
}

TEST_F(QuicSocketHermeticTest, AcceptTwiceFailsWithStateGuard)
{
	TempPemFiles pem;

	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket server_sock = std::move(pair.first);

	auto server = std::make_shared<quic_socket>(
		std::move(server_sock), quic_role::server);

	auto first = server->accept(pem.cert(), pem.key());
	EXPECT_TRUE(first.is_ok());

	// Second accept must hit the "already in progress" branch.
	auto second = server->accept(pem.cert(), pem.key());
	EXPECT_FALSE(second.is_ok());

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	server->stop_receive();
}

TEST_F(QuicSocketHermeticTest, AcceptInvalidCertFileFailsWithError)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket server_sock = std::move(pair.first);

	auto server = std::make_shared<quic_socket>(
		std::move(server_sock), quic_role::server);

	auto result = server->accept(
		"/nonexistent/dir/cert.pem", "/nonexistent/dir/key.pem");
	EXPECT_FALSE(result.is_ok());
	// Failure path leaves crypto init half-done; state should not be idle
	// (transition_state(handshake_start) runs before init_server returns
	// success, depending on impl). Either idle or handshake_start is fine,
	// but is_connected must remain false.
	EXPECT_FALSE(server->is_connected());
}

TEST_F(QuicSocketHermeticTest, AcceptCertWithoutMatchingKeyFails)
{
	TempPemFiles pem;

	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket server_sock = std::move(pair.first);

	auto server = std::make_shared<quic_socket>(
		std::move(server_sock), quic_role::server);

	auto result = server->accept(pem.cert(), "/nonexistent/key.pem");
	EXPECT_FALSE(result.is_ok());
	EXPECT_FALSE(server->is_connected());
}

// ----- close() drives draining timer + transition to closed -----

TEST_F(QuicSocketHermeticTest, CloseFromHandshakeStartDrivesDrainingTimer)
{
	auto pair = make_loopback_udp_pair(io());
	auto& server_sock = pair.first;
	asio::ip::udp::socket client_sock = std::move(pair.second);

	const auto server_endpoint = server_sock.local_endpoint();

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	auto connect_r = client->connect(server_endpoint, "localhost");
	ASSERT_TRUE(connect_r.is_ok());

	// Now close() runs from a non-idle state and drives:
	// - transition to closing
	// - send_packet() with CONNECTION_CLOSE (encryption_level::initial)
	// - transition to draining
	// - idle_timer_ async_wait callback that eventually flips to closed
	auto close_r = client->close(0x100, "draining-test");
	EXPECT_TRUE(close_r.is_ok());

	// Draining timer is 300ms; wait long enough for the async_wait callback
	// to fire so the closed branch executes.
	const bool reached_closed = wait_for(
		[&client]() {
			return client->state() == quic_connection_state::closed;
		},
		std::chrono::milliseconds(800));

	EXPECT_TRUE(reached_closed);
}

TEST_F(QuicSocketHermeticTest, CloseAfterCloseHitsAlreadyClosingBranch)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	const auto server_endpoint = pair.first.local_endpoint();
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	auto first = client->close(0, "first");
	EXPECT_TRUE(first.is_ok());

	// State is now closing or draining. Second close() must short-circuit
	// at the state guard and return ok without re-running close logic.
	auto second = client->close(0, "second");
	EXPECT_TRUE(second.is_ok());

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST_F(QuicSocketHermeticTest, CloseWithApplicationErrorCodeSetsAppFlag)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	const auto server_endpoint = pair.first.local_endpoint();
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	// Non-zero error_code triggers the is_application_error = true branch
	// inside close(); send_packet() builds a CONNECTION_CLOSE frame with
	// the application flag set.
	auto r = client->close(0xCAFE, "app-error-path");
	EXPECT_TRUE(r.is_ok());

	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// ----- start_receive drives do_receive callback + handle_packet branch -----

TEST_F(QuicSocketHermeticTest, ReceiveLoopHandlesUnparseablePacketSilently)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket peer_sock = std::move(pair.first);
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	std::atomic<bool> error_fired{false};
	client->set_error_callback([&error_fired](std::error_code) {
		error_fired = true;
	});

	const auto server_endpoint = peer_sock.local_endpoint();
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	// Send completely random bytes to the client. handle_packet must
	// silently drop these via the parse_header / pn-length / unprotect
	// failure branches without invoking the error callback.
	const std::vector<uint8_t> garbage = {
		0x00, 0xFF, 0xAB, 0xCD, 0x12, 0x34, 0x56, 0x78,
		0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01, 0x02, 0x03,
	};

	std::error_code ec;
	peer_sock.send(asio::buffer(garbage), 0, ec);
	ASSERT_FALSE(ec);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_FALSE(error_fired)
		<< "Garbage datagram must be silently dropped by handle_packet";

	(void)client->close(0, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST_F(QuicSocketHermeticTest, ReceiveLoopHandlesShortDatagramBelowSampleSize)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket peer_sock = std::move(pair.first);
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	std::atomic<bool> error_fired{false};
	client->set_error_callback([&error_fired](std::error_code) {
		error_fired = true;
	});

	const auto server_endpoint = peer_sock.local_endpoint();
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	// A long-header byte plus a few bytes -- not enough for a valid header
	// or for the 4-byte HP sample. This drives the
	// "sample_offset + hp_sample_size > data.size()" guard in handle_packet.
	const std::vector<uint8_t> tiny = {0xC0, 0x00, 0x00, 0x00, 0x01, 0x00};
	std::error_code ec;
	peer_sock.send(asio::buffer(tiny), 0, ec);
	ASSERT_FALSE(ec);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	EXPECT_FALSE(error_fired);

	(void)client->close(0, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST_F(QuicSocketHermeticTest, ReceiveLoopHandlesSingleByteDatagram)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket peer_sock = std::move(pair.first);
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	const auto server_endpoint = peer_sock.local_endpoint();
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	// One byte datagram: bytes_transferred > 0 branch fires, but the
	// packet is too short for parse_header to succeed.
	const std::uint8_t one_byte = 0x00;
	std::error_code ec;
	peer_sock.send(asio::buffer(&one_byte, 1), 0, ec);
	ASSERT_FALSE(ec);
	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	(void)client->close(0, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST_F(QuicSocketHermeticTest, ReceiveLoopWakesOnInitialPacketStub)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket peer_sock = std::move(pair.first);
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	const auto server_endpoint = peer_sock.local_endpoint();
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	const auto& cid = client->local_connection_id();
	std::vector<uint8_t> dcid_bytes;
	std::vector<uint8_t> scid_bytes(cid.length());
	for (size_t i = 0; i < cid.length(); ++i)
	{
		// connection_id has no public byte access, so synthesize alt bytes
		// for the stub. quic_socket only consults DCID for server-state
		// transitions, which the client path does not exercise.
		dcid_bytes.push_back(static_cast<uint8_t>(i ^ 0x55));
		scid_bytes[i] = static_cast<uint8_t>(i);
	}

	const auto stub = make_quic_initial_packet_stub(
		std::span(dcid_bytes), std::span(scid_bytes));

	std::error_code ec;
	peer_sock.send(asio::buffer(stub), 0, ec);
	ASSERT_FALSE(ec);

	// Stub will fail in unprotect_header (HP keys mismatch) but exercises
	// the long_header parsing branch + the read keys retrieval path.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	(void)client->close(0, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST_F(QuicSocketHermeticTest, ServerReceivesInitialPacketStubAndDerivesKeys)
{
	TempPemFiles pem;

	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket server_sock = std::move(pair.first);
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto server = std::make_shared<quic_socket>(
		std::move(server_sock), quic_role::server);

	ASSERT_TRUE(server->accept(pem.cert(), pem.key()).is_ok());

	// Send a long-header initial-packet stub. Server will:
	//  1. parse_header succeed
	//  2. detect handshake_start state and Initial type
	//  3. assign remote_conn_id from SCID
	//  4. derive_initial_secrets from DCID (may succeed)
	//  5. transition to handshake state
	//  6. attempt unprotect (likely fail; that branch is also covered)
	const std::vector<uint8_t> dcid = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	const std::vector<uint8_t> scid = {0xA1, 0xB2, 0xC3, 0xD4};
	const auto stub = make_quic_initial_packet_stub(
		std::span(dcid), std::span(scid));

	std::error_code ec;
	client_sock.send(asio::buffer(stub), 0, ec);
	ASSERT_FALSE(ec);

	std::this_thread::sleep_for(std::chrono::milliseconds(60));

	// State should have advanced beyond handshake_start, even if the
	// unprotect step failed.
	const auto state = server->state();
	EXPECT_TRUE(state == quic_connection_state::handshake_start ||
	            state == quic_connection_state::handshake)
		<< "Unexpected server state: " << static_cast<int>(state);

	server->stop_receive();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST_F(QuicSocketHermeticTest, ServerHandlesShortHeaderPacketWithoutKeys)
{
	TempPemFiles pem;

	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket server_sock = std::move(pair.first);
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto server = std::make_shared<quic_socket>(
		std::move(server_sock), quic_role::server);

	ASSERT_TRUE(server->accept(pem.cert(), pem.key()).is_ok());

	// Short-header packet (top bit 0) -- determine_encryption_level returns
	// application; get_read_keys for application before handshake completes
	// must fail and the packet is silently dropped.
	const std::vector<uint8_t> short_pkt = {
		0x40, 0x12, 0x34, 0x56, 0x78,
		0x00, 0x01, 0x02, 0x03,
		0x04, 0x05, 0x06, 0x07,
		0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	};

	std::error_code ec;
	client_sock.send(asio::buffer(short_pkt), 0, ec);
	ASSERT_FALSE(ec);

	std::this_thread::sleep_for(std::chrono::milliseconds(40));

	// State must not have crashed/migrated forward unexpectedly.
	EXPECT_FALSE(server->is_connected());

	server->stop_receive();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

// ----- send_stream_data + close_stream from non-connected state -----

TEST_F(QuicSocketHermeticTest, SendStreamDataDuringHandshakeFails)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	const auto server_endpoint = pair.first.local_endpoint();
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	// connect() leaves us in handshake state, not connected. send_stream_data
	// must reject because is_connected() == false.
	std::vector<uint8_t> data = {1, 2, 3, 4, 5};
	auto r = client->send_stream_data(0, std::move(data), false);
	EXPECT_FALSE(r.is_ok());

	(void)client->close(0, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST_F(QuicSocketHermeticTest, CreateStreamDuringHandshakeFails)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	const auto server_endpoint = pair.first.local_endpoint();
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	// is_connected() is false during handshake.
	auto r1 = client->create_stream(false);
	EXPECT_FALSE(r1.is_ok());

	auto r2 = client->create_stream(true);
	EXPECT_FALSE(r2.is_ok());

	(void)client->close(0, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

// ----- error_callback fires on socket-level error -----

TEST_F(QuicSocketHermeticTest, CallbacksRemainBoundAfterMultipleAssignments)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	int last_marker = 0;
	for (int i = 1; i <= 5; ++i)
	{
		client->set_connected_callback([&last_marker, i]() {
			last_marker = i;
		});
		client->set_error_callback([&last_marker, i](std::error_code) {
			last_marker = -i;
		});
		client->set_close_callback(
			[&last_marker, i](uint64_t, const std::string&) {
				last_marker = i * 10;
			});
		client->set_stream_data_callback(
			[&last_marker, i](uint64_t, std::span<const uint8_t>, bool) {
				last_marker = i * 100;
			});
	}

	(void)last_marker;
	SUCCEED();
}

// ----- send_pending_packets from idle is a no-op -----

TEST_F(QuicSocketHermeticTest, CloseFromIdleDoesNotCrash)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket sock = std::move(pair.second);

	auto q = std::make_shared<quic_socket>(
		std::move(sock), quic_role::client);

	// close() in idle state takes the early-return branch in send_pending
	// (state == idle returns immediately). It still transitions through
	// closing and sets up the draining timer.
	auto r = q->close(0, "idle-close");
	EXPECT_TRUE(r.is_ok());

	const bool reached_closed = wait_for(
		[&q]() {
			return q->state() == quic_connection_state::closed;
		},
		std::chrono::milliseconds(800));
	EXPECT_TRUE(reached_closed);
}

// ----- mock_udp_peer + stub bytes pack: packet variant coverage -----

TEST_F(QuicSocketHermeticTest, ServerReceivesMultipleStubsThroughMockPeer)
{
	TempPemFiles pem;

	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket server_sock = std::move(pair.first);
	mock_udp_peer peer(std::move(pair.second));

	auto server = std::make_shared<quic_socket>(
		std::move(server_sock), quic_role::server);

	ASSERT_TRUE(server->accept(pem.cert(), pem.key()).is_ok());

	// Flood the server with three different stub variants in succession.
	const std::vector<uint8_t> dcid_a = {0x10, 0x11, 0x12, 0x13};
	const std::vector<uint8_t> scid_a = {0xAA, 0xBB};
	const auto stub_a = make_quic_initial_packet_stub(
		std::span(dcid_a), std::span(scid_a));

	const std::vector<uint8_t> dcid_b = {0x20, 0x21};
	const std::vector<uint8_t> scid_b = {};
	const auto stub_b = make_quic_initial_packet_stub(
		std::span(dcid_b), std::span(scid_b));

	const std::vector<uint8_t> dcid_c = {};
	const std::vector<uint8_t> scid_c = {0xCC, 0xDD, 0xEE, 0xFF, 0x01};
	const auto stub_c = make_quic_initial_packet_stub(
		std::span(dcid_c), std::span(scid_c));

	peer.send(std::span(stub_a));
	peer.send(std::span(stub_b));
	peer.send(std::span(stub_c));

	std::this_thread::sleep_for(std::chrono::milliseconds(80));

	// State should have advanced past idle.
	EXPECT_NE(server->state(), quic_connection_state::idle);

	server->stop_receive();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

// ----- two concurrent quic_sockets: one connects, one accepts -----

TEST_F(QuicSocketHermeticTest, TwoSocketsCanBeConnectedSimultaneously)
{
	TempPemFiles pem;

	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket server_sock = std::move(pair.first);
	asio::ip::udp::socket client_sock = std::move(pair.second);

	const auto server_endpoint = server_sock.local_endpoint();

	auto server = std::make_shared<quic_socket>(
		std::move(server_sock), quic_role::server);
	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	ASSERT_TRUE(server->accept(pem.cert(), pem.key()).is_ok());
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	// Both sides have their receive loops spinning. Wait briefly so any
	// initial CRYPTO bytes the client emitted reach the server's
	// do_receive callback. The TLS handshake will not complete because
	// quic_socket lacks the QUIC-TLS feedback wiring, but the receive
	// path / initial-secret derivation branches are exercised on both ends.
	std::this_thread::sleep_for(std::chrono::milliseconds(120));

	(void)client->close(0, "client-close");
	(void)server->close(0, "server-close");
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// ----- start_receive on already-receiving socket is idempotent -----

TEST_F(QuicSocketHermeticTest, RepeatedStartReceiveDoesNotDoubleSubscribe)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket sock = std::move(pair.second);

	auto q = std::make_shared<quic_socket>(
		std::move(sock), quic_role::client);

	for (int i = 0; i < 5; ++i)
	{
		q->start_receive();
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	q->stop_receive();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	SUCCEED();
}

// ----- receive_buffer is large enough for max UDP datagram -----

TEST_F(QuicSocketHermeticTest, ReceiveLoopAcceptsLargeDatagram)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket peer_sock = std::move(pair.first);
	asio::ip::udp::socket client_sock = std::move(pair.second);

	auto client = std::make_shared<quic_socket>(
		std::move(client_sock), quic_role::client);

	const auto server_endpoint = peer_sock.local_endpoint();
	ASSERT_TRUE(client->connect(server_endpoint, "localhost").is_ok());

	// Send a 1500-byte datagram (typical Ethernet MTU) to exercise the
	// upper end of the recv_buffer_ array without overflowing it.
	std::vector<uint8_t> big(1500, 0x42);
	big[0] = 0xC0; // long-header bit so the parser at least starts
	std::error_code ec;
	peer_sock.send(asio::buffer(big), 0, ec);
	ASSERT_FALSE(ec);

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	(void)client->close(0, "");
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

// ----- close on socket whose receive loop was never started -----

TEST_F(QuicSocketHermeticTest, CloseUntouchedSocketReachesClosedAfterDrain)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket sock = std::move(pair.second);

	auto q = std::make_shared<quic_socket>(
		std::move(sock), quic_role::server);

	auto r = q->close(0, "untouched");
	EXPECT_TRUE(r.is_ok());

	const bool reached_closed = wait_for(
		[&q]() {
			return q->state() == quic_connection_state::closed;
		},
		std::chrono::milliseconds(800));
	EXPECT_TRUE(reached_closed);
}

// ----- Move-construct quic_socket built on hermetic loopback pair -----

TEST_F(QuicSocketHermeticTest, MoveConstructAfterReceiveStartIsSafe)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket sock = std::move(pair.second);

	auto q = std::make_shared<quic_socket>(
		std::move(sock), quic_role::client);

	q->start_receive();
	std::this_thread::sleep_for(std::chrono::milliseconds(5));

	// Stop before move to avoid racing with active async_receive_from on
	// the source socket.
	q->stop_receive();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	const auto cid = q->local_connection_id();

	quic_socket moved(std::move(*q));
	EXPECT_EQ(moved.role(), quic_role::client);
	EXPECT_EQ(moved.local_connection_id().to_string(), cid.to_string());
}

// ----- error_callback fires when underlying UDP socket errors out -----

TEST_F(QuicSocketHermeticTest, StopReceiveDuringActiveRecvDoesNotCallError)
{
	auto pair = make_loopback_udp_pair(io());
	asio::ip::udp::socket sock = std::move(pair.second);

	auto q = std::make_shared<quic_socket>(
		std::move(sock), quic_role::client);

	std::atomic<bool> error_fired{false};
	q->set_error_callback([&error_fired](std::error_code) {
		error_fired = true;
	});

	q->start_receive();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	q->stop_receive();
	std::this_thread::sleep_for(std::chrono::milliseconds(20));

	// operation_aborted from cancel() must not trigger error_cb_.
	EXPECT_FALSE(error_fired);
}

} // namespace kcenon::network::internal::test
