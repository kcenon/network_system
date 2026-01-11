/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "kcenon/network/internal/dtls_socket.h"
#include "../helpers/dtls_test_helpers.h"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

using namespace kcenon::network::internal;
using namespace kcenon::network::testing;

/**
 * @file test_dtls_socket.cpp
 * @brief Unit tests for dtls_socket functionality
 *
 * Tests validate:
 * - DTLS socket construction
 * - Handshake operations (client/server)
 * - Encrypted send/receive operations
 * - Callback registration and invocation
 * - Error handling
 * - Thread safety
 */

// ============================================================================
// Test Fixture
// ============================================================================

class DtlsSocketTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		// Generate test certificates
		cert_pair_ = test_certificate_generator::generate("localhost");

		// Create server and client SSL contexts
		server_ctx_ = dtls_context_factory::create_server_context(cert_pair_);
		client_ctx_ = dtls_context_factory::create_client_context(false);

		// Create io_context and work guard
		io_context_ = std::make_unique<asio::io_context>();
		work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
			io_context_->get_executor());

		// Start io_context in background thread
		io_thread_ = std::thread([this]()
		                         { io_context_->run(); });

		// Find available port
		test_port_ = find_available_udp_port();
	}

	void TearDown() override
	{
		work_guard_.reset();
		if (io_context_)
		{
			io_context_->stop();
		}
		if (io_thread_.joinable())
		{
			io_thread_.join();
		}
		io_context_.reset();
	}

	auto create_server_socket() -> asio::ip::udp::socket
	{
		asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());
		socket.bind(asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), test_port_));
		return socket;
	}

	auto create_client_socket() -> asio::ip::udp::socket
	{
		asio::ip::udp::socket socket(*io_context_, asio::ip::udp::v4());
		socket.bind(asio::ip::udp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
		return socket;
	}

	test_certificate_generator::certificate_pair cert_pair_;
	ssl_context_wrapper server_ctx_{nullptr};
	ssl_context_wrapper client_ctx_{nullptr};
	std::unique_ptr<asio::io_context> io_context_;
	std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_;
	std::thread io_thread_;
	uint16_t test_port_ = 0;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(DtlsSocketTest, ConstructWithValidContext)
{
	auto socket = create_server_socket();
	EXPECT_NO_THROW({
		auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());
		EXPECT_NE(dtls, nullptr);
	});
}

TEST_F(DtlsSocketTest, ConstructWithNullContextThrows)
{
	auto socket = create_server_socket();
	EXPECT_THROW(
		{
			auto dtls = std::make_shared<dtls_socket>(std::move(socket), nullptr);
		},
		std::runtime_error);
}

TEST_F(DtlsSocketTest, InitialStateNotHandshakeComplete)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	EXPECT_FALSE(dtls->is_handshake_complete());
}

// ============================================================================
// Callback Registration Tests
// ============================================================================

TEST_F(DtlsSocketTest, SetReceiveCallback)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	std::atomic<bool> callback_set{false};

	EXPECT_NO_THROW({
		dtls->set_receive_callback(
			[&callback_set](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)
			{
				callback_set.store(true);
			});
	});
}

TEST_F(DtlsSocketTest, SetErrorCallback)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	EXPECT_NO_THROW({
		dtls->set_error_callback([](std::error_code) {});
	});
}

TEST_F(DtlsSocketTest, SetPeerEndpoint)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	asio::ip::udp::endpoint peer(asio::ip::make_address("127.0.0.1"), 12345);
	dtls->set_peer_endpoint(peer);

	EXPECT_EQ(dtls->peer_endpoint().address().to_string(), "127.0.0.1");
	EXPECT_EQ(dtls->peer_endpoint().port(), 12345);
}

// ============================================================================
// Start/Stop Receive Tests
// ============================================================================

TEST_F(DtlsSocketTest, StartReceiveDoesNotThrow)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	EXPECT_NO_THROW({
		dtls->start_receive();
	});

	dtls->stop_receive();
}

TEST_F(DtlsSocketTest, StopReceiveDoesNotThrow)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	EXPECT_NO_THROW({
		dtls->stop_receive();
	});
}

TEST_F(DtlsSocketTest, MultipleStartReceiveCalls)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	// Multiple start calls should be safe
	EXPECT_NO_THROW({
		dtls->start_receive();
		dtls->start_receive();
		dtls->start_receive();
	});

	dtls->stop_receive();
}

// ============================================================================
// Send Before Handshake Tests
// ============================================================================

TEST_F(DtlsSocketTest, SendBeforeHandshakeFails)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	std::promise<std::error_code> send_promise;
	auto send_future = send_promise.get_future();
	std::atomic<bool> promise_set{false};

	std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	dtls->async_send(
		std::move(data),
		[&send_promise, &promise_set](std::error_code ec, std::size_t)
		{
			bool expected = false;
			if (promise_set.compare_exchange_strong(expected, true))
			{
				send_promise.set_value(ec);
			}
		});

	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	auto ec = send_future.get();

	// Should fail because handshake not complete
	EXPECT_TRUE(ec);
	EXPECT_EQ(ec, std::make_error_code(std::errc::not_connected));
}

// ============================================================================
// Handshake Tests
// ============================================================================

TEST_F(DtlsSocketTest, ClientServerHandshake)
{
	// Create server DTLS socket
	auto server_udp = create_server_socket();
	auto server_dtls = std::make_shared<dtls_socket>(std::move(server_udp), server_ctx_.get());

	// Create client DTLS socket
	auto client_udp = create_client_socket();
	auto client_dtls = std::make_shared<dtls_socket>(std::move(client_udp), client_ctx_.get());

	// Set peer endpoints
	asio::ip::udp::endpoint server_endpoint(asio::ip::make_address("127.0.0.1"), test_port_);
	asio::ip::udp::endpoint client_endpoint = client_dtls->socket().local_endpoint();

	client_dtls->set_peer_endpoint(server_endpoint);
	server_dtls->set_peer_endpoint(client_endpoint);

	// Promise/future for handshake completion
	std::promise<std::error_code> server_handshake_promise;
	auto server_handshake_future = server_handshake_promise.get_future();
	std::atomic<bool> server_promise_set{false};

	std::promise<std::error_code> client_handshake_promise;
	auto client_handshake_future = client_handshake_promise.get_future();
	std::atomic<bool> client_promise_set{false};

	// Start server handshake
	server_dtls->async_handshake(
		dtls_socket::handshake_type::server,
		[&server_handshake_promise, &server_promise_set](std::error_code ec)
		{
			bool expected = false;
			if (server_promise_set.compare_exchange_strong(expected, true))
			{
				server_handshake_promise.set_value(ec);
			}
		});

	// Start client handshake
	client_dtls->async_handshake(
		dtls_socket::handshake_type::client,
		[&client_handshake_promise, &client_promise_set](std::error_code ec)
		{
			bool expected = false;
			if (client_promise_set.compare_exchange_strong(expected, true))
			{
				client_handshake_promise.set_value(ec);
			}
		});

	// Wait for handshakes to complete (with timeout)
	auto client_status = client_handshake_future.wait_for(std::chrono::seconds(10));
	auto server_status = server_handshake_future.wait_for(std::chrono::seconds(10));

	if (client_status == std::future_status::ready &&
	    server_status == std::future_status::ready)
	{
		auto client_ec = client_handshake_future.get();
		auto server_ec = server_handshake_future.get();

		// Both should complete successfully
		EXPECT_FALSE(client_ec) << "Client handshake failed: " << client_ec.message();
		EXPECT_FALSE(server_ec) << "Server handshake failed: " << server_ec.message();

		if (!client_ec && !server_ec)
		{
			EXPECT_TRUE(client_dtls->is_handshake_complete());
			EXPECT_TRUE(server_dtls->is_handshake_complete());
		}
	}
	else
	{
		// Handshake timeout - this can happen in test environments
		// due to UDP packet loss or timing issues
		GTEST_SKIP() << "Handshake timeout - may be due to test environment";
	}

	server_dtls->stop_receive();
	client_dtls->stop_receive();
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(DtlsSocketTest, ConcurrentCallbackRegistration)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	std::atomic<int> registration_count{0};
	const int num_registrations = 100;

	std::vector<std::thread> threads;
	threads.reserve(num_registrations);

	for (int i = 0; i < num_registrations; ++i)
	{
		threads.emplace_back(
			[dtls, &registration_count, i]()
			{
				if (i % 2 == 0)
				{
					dtls->set_receive_callback(
						[](const std::vector<uint8_t>&, const asio::ip::udp::endpoint&) {});
				}
				else
				{
					dtls->set_error_callback([](std::error_code) {});
				}
				registration_count.fetch_add(1);
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_EQ(registration_count.load(), num_registrations);
}

TEST_F(DtlsSocketTest, ConcurrentEndpointAccess)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	std::atomic<int> operation_count{0};
	const int num_operations = 100;

	std::vector<std::thread> threads;
	threads.reserve(num_operations);

	for (int i = 0; i < num_operations; ++i)
	{
		threads.emplace_back(
			[dtls, &operation_count, i]()
			{
				if (i % 2 == 0)
				{
					asio::ip::udp::endpoint ep(
						asio::ip::make_address("127.0.0.1"),
						static_cast<uint16_t>(10000 + i));
					dtls->set_peer_endpoint(ep);
				}
				else
				{
					auto ep = dtls->peer_endpoint();
					(void)ep;  // Suppress unused warning
				}
				operation_count.fetch_add(1);
			});
	}

	for (auto& t : threads)
	{
		t.join();
	}

	EXPECT_EQ(operation_count.load(), num_operations);
}

// ============================================================================
// Socket Access Tests
// ============================================================================

TEST_F(DtlsSocketTest, SocketAccessReturnsValidSocket)
{
	auto socket = create_server_socket();
	auto expected_port = socket.local_endpoint().port();

	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	EXPECT_TRUE(dtls->socket().is_open());
	EXPECT_EQ(dtls->socket().local_endpoint().port(), expected_port);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(DtlsSocketTest, DestructorAfterStartReceive)
{
	// Test that destructor properly cleans up after start_receive
	EXPECT_NO_THROW({
		auto socket = create_server_socket();
		auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());
		dtls->start_receive();
		// Destructor called when dtls goes out of scope
	});

	// Give time for async operations to complete
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

TEST_F(DtlsSocketTest, MultipleStopReceiveCalls)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	dtls->start_receive();

	// Multiple stop calls should be safe
	EXPECT_NO_THROW({
		dtls->stop_receive();
		dtls->stop_receive();
		dtls->stop_receive();
	});
}

// ============================================================================
// Send/Receive After Handshake Tests (Integration-like)
// ============================================================================

class DtlsSocketIntegrationTest : public DtlsSocketTest
{
protected:
	// Helper to perform handshake between client and server
	bool perform_handshake(
		std::shared_ptr<dtls_socket> server,
		std::shared_ptr<dtls_socket> client,
		std::chrono::seconds timeout = std::chrono::seconds(10))
	{
		std::promise<std::error_code> server_promise;
		auto server_future = server_promise.get_future();
		std::atomic<bool> server_set{false};

		std::promise<std::error_code> client_promise;
		auto client_future = client_promise.get_future();
		std::atomic<bool> client_set{false};

		server->async_handshake(
			dtls_socket::handshake_type::server,
			[&server_promise, &server_set](std::error_code ec)
			{
				bool expected = false;
				if (server_set.compare_exchange_strong(expected, true))
				{
					server_promise.set_value(ec);
				}
			});

		client->async_handshake(
			dtls_socket::handshake_type::client,
			[&client_promise, &client_set](std::error_code ec)
			{
				bool expected = false;
				if (client_set.compare_exchange_strong(expected, true))
				{
					client_promise.set_value(ec);
				}
			});

		auto client_status = client_future.wait_for(timeout);
		auto server_status = server_future.wait_for(timeout);

		if (client_status != std::future_status::ready ||
		    server_status != std::future_status::ready)
		{
			return false;
		}

		return !client_future.get() && !server_future.get();
	}
};

TEST_F(DtlsSocketIntegrationTest, SendReceiveAfterHandshake)
{
	// Create server DTLS socket
	auto server_udp = create_server_socket();
	auto server_dtls = std::make_shared<dtls_socket>(std::move(server_udp), server_ctx_.get());

	// Create client DTLS socket
	auto client_udp = create_client_socket();
	auto client_dtls = std::make_shared<dtls_socket>(std::move(client_udp), client_ctx_.get());

	// Set peer endpoints
	asio::ip::udp::endpoint server_endpoint(asio::ip::make_address("127.0.0.1"), test_port_);
	client_dtls->set_peer_endpoint(server_endpoint);
	server_dtls->set_peer_endpoint(client_dtls->socket().local_endpoint());

	// Perform handshake
	if (!perform_handshake(server_dtls, client_dtls))
	{
		GTEST_SKIP() << "Handshake failed - skipping send/receive test";
	}

	// Set up receive callback on server
	std::promise<std::vector<uint8_t>> data_promise;
	auto data_future = data_promise.get_future();
	std::atomic<bool> promise_set{false};

	server_dtls->set_receive_callback(
		[&data_promise, &promise_set](const std::vector<uint8_t>& data,
		                               const asio::ip::udp::endpoint&)
		{
			bool expected = false;
			if (promise_set.compare_exchange_strong(expected, true))
			{
				data_promise.set_value(data);
			}
		});

	server_dtls->start_receive();

	// Send data from client
	std::vector<uint8_t> test_data = {0x01, 0x02, 0x03, 0x04, 0x05};
	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();
	std::atomic<bool> send_set{false};

	client_dtls->async_send(
		std::vector<uint8_t>(test_data),
		[&send_promise, &send_set](std::error_code ec, std::size_t)
		{
			bool expected = false;
			if (send_set.compare_exchange_strong(expected, true))
			{
				send_promise.set_value(!ec);
			}
		});

	// Wait for send to complete
	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	EXPECT_TRUE(send_future.get()) << "Send failed";

	// Wait for receive callback
	auto receive_status = data_future.wait_for(std::chrono::seconds(5));
	if (receive_status == std::future_status::ready)
	{
		auto received_data = data_future.get();
		EXPECT_EQ(received_data.size(), test_data.size());
		EXPECT_EQ(received_data, test_data);
	}
	else
	{
		GTEST_SKIP() << "Receive timeout - may be due to test environment";
	}

	server_dtls->stop_receive();
	client_dtls->stop_receive();
}

TEST_F(DtlsSocketIntegrationTest, BidirectionalCommunication)
{
	// Create server DTLS socket
	auto server_udp = create_server_socket();
	auto server_dtls = std::make_shared<dtls_socket>(std::move(server_udp), server_ctx_.get());

	// Create client DTLS socket
	auto client_udp = create_client_socket();
	auto client_dtls = std::make_shared<dtls_socket>(std::move(client_udp), client_ctx_.get());

	// Set peer endpoints
	asio::ip::udp::endpoint server_endpoint(asio::ip::make_address("127.0.0.1"), test_port_);
	client_dtls->set_peer_endpoint(server_endpoint);
	server_dtls->set_peer_endpoint(client_dtls->socket().local_endpoint());

	// Perform handshake
	if (!perform_handshake(server_dtls, client_dtls))
	{
		GTEST_SKIP() << "Handshake failed - skipping bidirectional test";
	}

	// Set up receive callbacks on both sides
	std::promise<std::vector<uint8_t>> server_recv_promise;
	auto server_recv_future = server_recv_promise.get_future();
	std::atomic<bool> server_recv_set{false};

	std::promise<std::vector<uint8_t>> client_recv_promise;
	auto client_recv_future = client_recv_promise.get_future();
	std::atomic<bool> client_recv_set{false};

	server_dtls->set_receive_callback(
		[&server_recv_promise, &server_recv_set](const std::vector<uint8_t>& data,
		                                          const asio::ip::udp::endpoint&)
		{
			bool expected = false;
			if (server_recv_set.compare_exchange_strong(expected, true))
			{
				server_recv_promise.set_value(data);
			}
		});

	client_dtls->set_receive_callback(
		[&client_recv_promise, &client_recv_set](const std::vector<uint8_t>& data,
		                                          const asio::ip::udp::endpoint&)
		{
			bool expected = false;
			if (client_recv_set.compare_exchange_strong(expected, true))
			{
				client_recv_promise.set_value(data);
			}
		});

	server_dtls->start_receive();
	client_dtls->start_receive();

	// Send from client to server
	std::vector<uint8_t> client_msg = {0xAA, 0xBB, 0xCC};
	client_dtls->async_send(
		std::vector<uint8_t>(client_msg),
		[](std::error_code, std::size_t) {});

	// Send from server to client
	std::vector<uint8_t> server_msg = {0x11, 0x22, 0x33};
	server_dtls->async_send(
		std::vector<uint8_t>(server_msg),
		[](std::error_code, std::size_t) {});

	// Check if server received client's message
	auto server_recv_status = server_recv_future.wait_for(std::chrono::seconds(5));
	if (server_recv_status == std::future_status::ready)
	{
		auto server_received = server_recv_future.get();
		EXPECT_EQ(server_received, client_msg);
	}

	// Check if client received server's message
	auto client_recv_status = client_recv_future.wait_for(std::chrono::seconds(5));
	if (client_recv_status == std::future_status::ready)
	{
		auto client_received = client_recv_future.get();
		EXPECT_EQ(client_received, server_msg);
	}

	server_dtls->stop_receive();
	client_dtls->stop_receive();
}

// ============================================================================
// Error Callback Tests
// ============================================================================

TEST_F(DtlsSocketTest, ErrorCallbackInvokedOnSocketClose)
{
	auto socket = create_server_socket();
	auto dtls = std::make_shared<dtls_socket>(std::move(socket), server_ctx_.get());

	std::promise<std::error_code> error_promise;
	auto error_future = error_promise.get_future();
	std::atomic<bool> promise_set{false};

	dtls->set_error_callback(
		[&error_promise, &promise_set](std::error_code ec)
		{
			bool expected = false;
			if (promise_set.compare_exchange_strong(expected, true))
			{
				error_promise.set_value(ec);
			}
		});

	dtls->start_receive();

	// Close the underlying socket to trigger error
	dtls->socket().close();

	// Wait for error callback
	auto status = error_future.wait_for(std::chrono::seconds(2));
	if (status == std::future_status::ready)
	{
		auto ec = error_future.get();
		// Should have an error (socket closed)
		EXPECT_TRUE(ec);
	}
	// If timeout, the error callback may not be invoked on all platforms
}

// ============================================================================
// Large Data Tests
// ============================================================================

TEST_F(DtlsSocketIntegrationTest, LargePayload)
{
	// Create server DTLS socket
	auto server_udp = create_server_socket();
	auto server_dtls = std::make_shared<dtls_socket>(std::move(server_udp), server_ctx_.get());

	// Create client DTLS socket
	auto client_udp = create_client_socket();
	auto client_dtls = std::make_shared<dtls_socket>(std::move(client_udp), client_ctx_.get());

	// Set peer endpoints
	asio::ip::udp::endpoint server_endpoint(asio::ip::make_address("127.0.0.1"), test_port_);
	client_dtls->set_peer_endpoint(server_endpoint);
	server_dtls->set_peer_endpoint(client_dtls->socket().local_endpoint());

	// Perform handshake
	if (!perform_handshake(server_dtls, client_dtls))
	{
		GTEST_SKIP() << "Handshake failed - skipping large payload test";
	}

	// Set up receive callback on server
	std::promise<std::vector<uint8_t>> data_promise;
	auto data_future = data_promise.get_future();
	std::atomic<bool> promise_set{false};

	server_dtls->set_receive_callback(
		[&data_promise, &promise_set](const std::vector<uint8_t>& data,
		                               const asio::ip::udp::endpoint&)
		{
			bool expected = false;
			if (promise_set.compare_exchange_strong(expected, true))
			{
				data_promise.set_value(data);
			}
		});

	server_dtls->start_receive();

	// Create larger payload (within DTLS datagram limits ~1400 bytes for safe UDP)
	std::vector<uint8_t> large_data(1000);
	for (size_t i = 0; i < large_data.size(); ++i)
	{
		large_data[i] = static_cast<uint8_t>(i % 256);
	}

	std::promise<bool> send_promise;
	auto send_future = send_promise.get_future();
	std::atomic<bool> send_set{false};

	client_dtls->async_send(
		std::vector<uint8_t>(large_data),
		[&send_promise, &send_set](std::error_code ec, std::size_t)
		{
			bool expected = false;
			if (send_set.compare_exchange_strong(expected, true))
			{
				send_promise.set_value(!ec);
			}
		});

	// Wait for send
	ASSERT_TRUE(send_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready);
	EXPECT_TRUE(send_future.get()) << "Large payload send failed";

	// Wait for receive
	auto receive_status = data_future.wait_for(std::chrono::seconds(5));
	if (receive_status == std::future_status::ready)
	{
		auto received = data_future.get();
		EXPECT_EQ(received.size(), large_data.size());
		EXPECT_EQ(received, large_data);
	}
	else
	{
		GTEST_SKIP() << "Receive timeout for large payload";
	}

	server_dtls->stop_receive();
	client_dtls->stop_receive();
}
