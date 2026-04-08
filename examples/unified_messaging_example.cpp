/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file unified_messaging_example.cpp
 * @brief Demonstrates the unified transport interface abstraction.
 * @example unified_messaging_example.cpp
 *
 * @par Category
 * Core - Unified transport interface
 *
 * Demonstrates:
 * - endpoint_info configuration
 * - connection_callbacks setup
 * - Unified i_transport, i_connection, i_listener interfaces
 *
 * @see kcenon::network::unified::i_transport
 * @see kcenon::network::unified::i_connection
 * @see kcenon::network::unified::i_listener
 */

#include <kcenon/network/unified/unified.h>

#include <iostream>
#include <string>

using namespace kcenon::network;

int main()
{
	std::cout << "=== Unified Messaging Interface Example ===" << std::endl;

	// 1. Endpoint configuration
	std::cout << "\n1. Endpoint configuration:" << std::endl;
	unified::endpoint_info server_endpoint;
	server_endpoint.host = "0.0.0.0";
	server_endpoint.port = 9090;

	unified::endpoint_info client_endpoint;
	client_endpoint.host = "localhost";
	client_endpoint.port = 9090;

	std::cout << "   Server: " << server_endpoint.host << ":" << server_endpoint.port << std::endl;
	std::cout << "   Client: " << client_endpoint.host << ":" << client_endpoint.port << std::endl;

	// 2. Connection callbacks
	std::cout << "\n2. Setting up connection callbacks:" << std::endl;
	unified::connection_callbacks callbacks;

	callbacks.on_connected = []()
	{ std::cout << "   [Callback] Connected!" << std::endl; };

	callbacks.on_data = [](std::span<const std::byte> data)
	{ std::cout << "   [Callback] Received " << data.size() << " bytes" << std::endl; };

	callbacks.on_disconnected = [](std::optional<std::string_view> reason)
	{
		std::cout << "   [Callback] Disconnected";
		if (reason)
		{
			std::cout << ": " << *reason;
		}
		std::cout << std::endl;
	};

	callbacks.on_error = [](std::error_code ec)
	{ std::cout << "   [Callback] Error: " << ec.message() << std::endl; };

	std::cout << "   All 4 callbacks configured (on_connected, on_data, "
			  << "on_disconnected, on_error)" << std::endl;

	// 3. Interface overview
	std::cout << "\n3. Unified interface hierarchy:" << std::endl;
	std::cout << "   i_transport    - send(), is_connected(), remote_endpoint()" << std::endl;
	std::cout << "   i_connection   - connect(), close(), set_callbacks()" << std::endl;
	std::cout << "   i_listener     - start(), stop(), set_accept_callback()" << std::endl;
	std::cout << "\n   Use with protocol tags:" << std::endl;
	std::cout << "   unified_messaging_client<tcp_protocol, no_tls>" << std::endl;
	std::cout << "   unified_messaging_server<tcp_protocol, tls_enabled>" << std::endl;

	std::cout << "\nDone." << std::endl;
	return 0;
}
