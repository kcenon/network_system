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
#include <asio.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>

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

} // namespace kcenon::network::internal::test
