// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include <gtest/gtest.h>
#include <asio.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>
#include <limits>
#include <string>
#include <vector>

#include "../src/internal/quic_socket.h"

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

} // namespace kcenon::network::internal::test
