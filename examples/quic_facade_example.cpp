/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file quic_facade_example.cpp
 * @brief Demonstrates QUIC facade for modern transport with built-in TLS.
 * @example quic_facade_example.cpp
 *
 * @par Category
 * Protocol - QUIC networking
 *
 * Demonstrates:
 * - QUIC client/server configuration
 * - TLS certificate configuration
 * - 0-RTT and ALPN settings
 *
 * @see kcenon::network::facade::quic_facade
 */

#include <kcenon/network/facade/quic_facade.h>

#include <iostream>
#include <string>

using namespace kcenon::network;

int main()
{
	std::cout << "=== QUIC Facade Example ===" << std::endl;

	// Server configuration (QUIC requires TLS)
	facade::quic_facade::server_config srv_config;
	srv_config.port = 4433;
	srv_config.server_id = "quic-demo-server";
	srv_config.cert_path = "server.crt";
	srv_config.key_path = "server.key";
	srv_config.alpn = "h3";
	srv_config.max_idle_timeout_ms = 30000;
	srv_config.max_connections = 1000;
	srv_config.require_client_cert = false;

	std::cout << "\n1. QUIC Server Configuration:" << std::endl;
	std::cout << "   Port: " << srv_config.port << std::endl;
	std::cout << "   ALPN: " << srv_config.alpn << std::endl;
	std::cout << "   Max idle timeout: " << srv_config.max_idle_timeout_ms << "ms" << std::endl;
	std::cout << "   Max connections: " << srv_config.max_connections << std::endl;
	std::cout << "   Client cert required: "
			  << (srv_config.require_client_cert ? "yes" : "no") << std::endl;

	// Client configuration
	facade::quic_facade::client_config cli_config;
	cli_config.host = "localhost";
	cli_config.port = 4433;
	cli_config.client_id = "quic-demo-client";
	cli_config.alpn = "h3";
	cli_config.verify_server = true;
	cli_config.enable_0rtt = false;

	std::cout << "\n2. QUIC Client Configuration:" << std::endl;
	std::cout << "   Host: " << cli_config.host << ":" << cli_config.port << std::endl;
	std::cout << "   ALPN: " << cli_config.alpn << std::endl;
	std::cout << "   Verify server: " << (cli_config.verify_server ? "yes" : "no") << std::endl;
	std::cout << "   0-RTT: " << (cli_config.enable_0rtt ? "enabled" : "disabled") << std::endl;

	// Note: Actual connection requires valid certificates and a running server
	std::cout << "\n3. Creating QUIC endpoints..." << std::endl;
	auto server = facade::quic_facade::create_server(srv_config);
	auto client = facade::quic_facade::create_client(cli_config);

	std::cout << "   Server: " << (server ? "created" : "null (certs needed)") << std::endl;
	std::cout << "   Client: " << (client ? "created" : "null (server needed)") << std::endl;

	std::cout << "\nDone." << std::endl;
	return 0;
}
