/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file socket_concepts_test.cpp
 * @brief Unit tests for socket concepts defined in socket_concepts.h
 *
 * These tests validate that the socket concepts correctly constrain
 * socket types at compile time. The tests verify:
 * - Concept satisfaction for each socket type
 * - RAII guards functionality
 * - Generic algorithm applicability
 */

#include "kcenon/network/detail/concepts/socket_concepts.h"
#include "internal/tcp/tcp_socket.h"
#include "internal/udp/udp_socket.h"
#include <gtest/gtest.h>

#include <asio.hpp>

using namespace kcenon::network::concepts;
using namespace kcenon::network::internal;

// ============================================================================
// Compile-Time Concept Verification Tests
// ============================================================================

/**
 * @brief Test that tcp_socket satisfies the Socket concept.
 */
TEST(SocketConceptsTest, TcpSocketSatisfiesSocketConcept)
{
	// This is a compile-time check
	static_assert(Socket<tcp_socket>, "tcp_socket must satisfy Socket concept");
	SUCCEED();
}

/**
 * @brief Test that tcp_socket satisfies the StreamSocket concept.
 */
TEST(SocketConceptsTest, TcpSocketSatisfiesStreamSocketConcept)
{
	static_assert(StreamSocket<tcp_socket>,
				  "tcp_socket must satisfy StreamSocket concept");
	SUCCEED();
}

/**
 * @brief Test that tcp_socket satisfies the BackpressureAwareSocket concept.
 */
TEST(SocketConceptsTest, TcpSocketSatisfiesBackpressureAwareSocketConcept)
{
	static_assert(BackpressureAwareSocket<tcp_socket>,
				  "tcp_socket must satisfy BackpressureAwareSocket concept");
	SUCCEED();
}

/**
 * @brief Test that tcp_socket satisfies the MetricsAwareSocket concept.
 */
TEST(SocketConceptsTest, TcpSocketSatisfiesMetricsAwareSocketConcept)
{
	static_assert(MetricsAwareSocket<tcp_socket>,
				  "tcp_socket must satisfy MetricsAwareSocket concept");
	SUCCEED();
}

/**
 * @brief Test that udp_socket satisfies the Socket concept.
 */
TEST(SocketConceptsTest, UdpSocketSatisfiesSocketConcept)
{
	static_assert(Socket<udp_socket>, "udp_socket must satisfy Socket concept");
	SUCCEED();
}

/**
 * @brief Test that udp_socket satisfies the DatagramSocket concept.
 */
TEST(SocketConceptsTest, UdpSocketSatisfiesDatagramSocketConcept)
{
	static_assert(DatagramSocket<udp_socket>,
				  "udp_socket must satisfy DatagramSocket concept");
	SUCCEED();
}

/**
 * @brief Test that udp_socket satisfies the DatagramSocketWithEndpoint concept.
 */
TEST(SocketConceptsTest, UdpSocketSatisfiesDatagramSocketWithEndpointConcept)
{
	static_assert(
		DatagramSocketWithEndpoint<udp_socket, asio::ip::udp::endpoint>,
		"udp_socket must satisfy DatagramSocketWithEndpoint concept");
	SUCCEED();
}

// ============================================================================
// Negative Compile-Time Tests (Concept Rejection)
// ============================================================================

/**
 * @brief Test that udp_socket does NOT satisfy StreamSocket concept.
 */
TEST(SocketConceptsTest, UdpSocketDoesNotSatisfyStreamSocketConcept)
{
	static_assert(!StreamSocket<udp_socket>,
				  "udp_socket must NOT satisfy StreamSocket concept");
	SUCCEED();
}

/**
 * @brief Test that tcp_socket does NOT satisfy DatagramSocket concept.
 */
TEST(SocketConceptsTest, TcpSocketDoesNotSatisfyDatagramSocketConcept)
{
	static_assert(!DatagramSocket<tcp_socket>,
				  "tcp_socket must NOT satisfy DatagramSocket concept");
	SUCCEED();
}

// ============================================================================
// Generic Algorithm Tests
// ============================================================================

namespace
{

// Generic function that works with any Socket
template <Socket S>
bool check_and_close(S& socket)
{
	if (!socket.is_closed())
	{
		socket.close();
		return true;
	}
	return false;
}

// Generic function that works with any StreamSocket
template <StreamSocket S>
void setup_stream_callbacks(
	S& socket, std::function<void(const std::vector<uint8_t>&)> recv_callback,
	std::function<void(std::error_code)> error_callback)
{
	socket.set_receive_callback(recv_callback);
	socket.set_error_callback(error_callback);
}

// Generic function that works with any DatagramSocket
template <DatagramSocket S>
void setup_datagram_callbacks(S& socket,
							  std::function<void(std::error_code)> error_callback)
{
	socket.set_error_callback(error_callback);
}

} // namespace

/**
 * @brief Test generic Socket algorithm with tcp_socket.
 */
TEST(SocketConceptsTest, GenericSocketAlgorithmWithTcpSocket)
{
	asio::io_context io_context;
	asio::ip::tcp::socket raw_socket(io_context);
	auto socket = std::make_shared<tcp_socket>(std::move(raw_socket));

	// check_and_close should work with tcp_socket
	bool result = check_and_close(*socket);
	EXPECT_TRUE(result);
	EXPECT_TRUE(socket->is_closed());
}

/**
 * @brief Test generic Socket algorithm with udp_socket.
 */
TEST(SocketConceptsTest, GenericSocketAlgorithmWithUdpSocket)
{
	asio::io_context io_context;
	asio::ip::udp::socket raw_socket(io_context);
	auto socket = std::make_shared<udp_socket>(std::move(raw_socket));

	// check_and_close should work with udp_socket
	bool result = check_and_close(*socket);
	EXPECT_TRUE(result);
	EXPECT_TRUE(socket->is_closed());
}

/**
 * @brief Test generic StreamSocket algorithm with tcp_socket.
 */
TEST(SocketConceptsTest, GenericStreamSocketAlgorithmWithTcpSocket)
{
	asio::io_context io_context;
	asio::ip::tcp::socket raw_socket(io_context);
	auto socket = std::make_shared<tcp_socket>(std::move(raw_socket));

	bool received = false;
	bool error_occurred = false;

	setup_stream_callbacks(
		*socket, [&received](const std::vector<uint8_t>&) { received = true; },
		[&error_occurred](std::error_code) { error_occurred = true; });

	// Callbacks are set (we can't easily verify without actual I/O)
	SUCCEED();
}

/**
 * @brief Test generic DatagramSocket algorithm with udp_socket.
 */
TEST(SocketConceptsTest, GenericDatagramSocketAlgorithmWithUdpSocket)
{
	asio::io_context io_context;
	asio::ip::udp::socket raw_socket(io_context);
	auto socket = std::make_shared<udp_socket>(std::move(raw_socket));

	bool error_occurred = false;

	setup_datagram_callbacks(
		*socket, [&error_occurred](std::error_code) { error_occurred = true; });

	// Callback is set (we can't easily verify without actual I/O)
	SUCCEED();
}

// ============================================================================
// Handler Concept Tests
// ============================================================================

TEST(SocketConceptsTest, AsyncCompletionHandlerConcept)
{
	auto lambda_handler = [](std::error_code, std::size_t) {};
	static_assert(AsyncCompletionHandler<decltype(lambda_handler)>,
				  "Lambda should satisfy AsyncCompletionHandler");

	std::function<void(std::error_code, std::size_t)> func_handler;
	static_assert(AsyncCompletionHandler<decltype(func_handler)>,
				  "std::function should satisfy AsyncCompletionHandler");

	SUCCEED();
}

TEST(SocketConceptsTest, ErrorCompletionHandlerConcept)
{
	auto lambda_handler = [](std::error_code) {};
	static_assert(ErrorCompletionHandler<decltype(lambda_handler)>,
				  "Lambda should satisfy ErrorCompletionHandler");

	std::function<void(std::error_code)> func_handler;
	static_assert(ErrorCompletionHandler<decltype(func_handler)>,
				  "std::function should satisfy ErrorCompletionHandler");

	SUCCEED();
}
