/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file http_facade_example.cpp
 * @brief Demonstrates HTTP facade for simplified server/client creation.
 * @example http_facade_example.cpp
 *
 * @par Category
 * Protocol - HTTP networking
 *
 * Demonstrates:
 * - Creating HTTP server with http_facade
 * - Creating HTTP client with http_facade
 * - Configuration via config structs
 *
 * @see kcenon::network::facade::http_facade
 */

#include <kcenon/network/facade/http_facade.h>

#include <iostream>
#include <string>

using namespace kcenon::network;

int main()
{
	std::cout << "=== HTTP Facade Example ===" << std::endl;

	// Server configuration
	facade::http_facade::server_config srv_config;
	srv_config.port = 8080;
	srv_config.server_id = "example-http-server";

	std::cout << "\n1. Creating HTTP server..." << std::endl;
	std::cout << "   Port: " << srv_config.port << std::endl;
	std::cout << "   Server ID: " << srv_config.server_id << std::endl;

	auto server = facade::http_facade::create_server(srv_config);
	if (server)
	{
		std::cout << "   Server created successfully" << std::endl;
	}
	else
	{
		std::cout << "   Server creation returned null (expected in demo)" << std::endl;
	}

	// Client configuration
	facade::http_facade::client_config cli_config;
	cli_config.client_id = "example-http-client";
	cli_config.timeout = std::chrono::seconds(30);
	cli_config.use_ssl = false;

	std::cout << "\n2. Creating HTTP client..." << std::endl;
	std::cout << "   Client ID: " << cli_config.client_id << std::endl;
	std::cout << "   Timeout: " << cli_config.timeout.count() << "ms" << std::endl;
	std::cout << "   SSL: " << (cli_config.use_ssl ? "enabled" : "disabled") << std::endl;

	auto client = facade::http_facade::create_client(cli_config);
	if (client)
	{
		std::cout << "   Client created successfully" << std::endl;
	}
	else
	{
		std::cout << "   Client creation returned null (expected in demo)" << std::endl;
	}

	std::cout << "\nDone." << std::endl;
	return 0;
}
