/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Network System Project
 *
 * QUIC Server Example
 *
 * This example demonstrates how to use the messaging_quic_server class
 * to create a QUIC server that accepts client connections.
 *
 * Key features demonstrated:
 * - Starting and stopping the server
 * - Handling client connections and disconnections
 * - Receiving data from clients
 * - Broadcasting to all connected clients
 * - Multicasting to specific clients
 * - Session management
 */

#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <signal.h>

#include "kcenon/network/core/messaging_quic_server.h"
#include "kcenon/network/session/quic_session.h"

using namespace network_system::core;
using namespace network_system::session;

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signal_handler(int)
{
	std::cout << "\nShutdown signal received..." << std::endl;
	g_running.store(false);
}

/**
 * @brief Simple QUIC server demo
 */
class QuicServerDemo
{
public:
	explicit QuicServerDemo(unsigned short port)
	    : port_(port)
	{
	}

	bool run()
	{
		std::cout << "=== QUIC Server Example ===" << std::endl;
		std::cout << "Starting server on port " << port_ << std::endl;

		// Create the QUIC server
		server_ = std::make_shared<messaging_quic_server>("quic_demo_server");

		// Set up callbacks
		setup_callbacks();

		// Create configuration
		quic_server_config config;
		// For production, set TLS certificate paths:
		// config.cert_file = "/path/to/server.crt";
		// config.key_file = "/path/to/server.key";
		config.max_idle_timeout_ms = 30000;
		config.max_connections = 100;
		config.alpn_protocols = {"h3", "hq-interop"};

		// Start the server
		auto result = server_->start_server(port_, config);
		if (result.is_err())
		{
			std::cerr << "Failed to start server: " << result.error().message
			          << std::endl;
			return false;
		}

		std::cout << "Server started successfully!" << std::endl;
		std::cout << "Press Ctrl+C to stop the server..." << std::endl;
		std::cout << std::endl;

		// Main loop - print statistics periodically
		while (g_running.load())
		{
			std::this_thread::sleep_for(std::chrono::seconds(5));

			if (g_running.load())
			{
				print_status();
			}
		}

		// Graceful shutdown
		std::cout << "\n=== Shutting Down ===" << std::endl;

		// Disconnect all clients
		std::cout << "Disconnecting " << server_->session_count()
		          << " clients..." << std::endl;
		server_->disconnect_all(0);

		// Stop the server
		std::cout << "Stopping server..." << std::endl;
		(void)server_->stop_server();
		server_->wait_for_stop();

		std::cout << "Server stopped." << std::endl;
		return true;
	}

private:
	void setup_callbacks()
	{
		// Connection callback - called when a new client connects
		server_->set_connection_callback(
		    [this](std::shared_ptr<quic_session> session)
		    {
			    std::cout << "[Connect] New client: " << session->session_id()
			              << " from " << session->remote_endpoint().address()
			              << std::endl;

			    // Send welcome message
			    std::string welcome = "Welcome to QUIC server!";
			    auto result = session->send(
			        std::vector<uint8_t>(welcome.begin(), welcome.end()));
			    if (result.is_err())
			    {
				    std::cerr << "  Failed to send welcome: "
				              << result.error().message << std::endl;
			    }
		    });

		// Disconnection callback - called when a client disconnects
		server_->set_disconnection_callback(
		    [](std::shared_ptr<quic_session> session)
		    {
			    std::cout << "[Disconnect] Client left: "
			              << session->session_id() << std::endl;
		    });

		// Receive callback - called when data is received
		server_->set_receive_callback(
		    [this](std::shared_ptr<quic_session> session,
		           const std::vector<uint8_t>& data)
		    {
			    std::string msg(data.begin(), data.end());
			    std::cout << "[Receive] From " << session->session_id() << ": "
			              << msg << std::endl;

			    // Echo the message back
			    std::string response = "Echo: " + msg;
			    auto result = session->send(
			        std::vector<uint8_t>(response.begin(), response.end()));
			    if (result.is_err())
			    {
				    std::cerr << "  Failed to echo: " << result.error().message
				              << std::endl;
			    }

			    // Handle special commands
			    handle_command(session, msg);
		    });

		// Stream receive callback - called for stream-specific data
		server_->set_stream_receive_callback(
		    [](std::shared_ptr<quic_session> session,
		       uint64_t stream_id,
		       const std::vector<uint8_t>& data,
		       bool fin)
		    {
			    std::cout << "[Stream] From " << session->session_id()
			              << " on stream " << stream_id << ": "
			              << data.size() << " bytes"
			              << (fin ? " (FIN)" : "") << std::endl;
		    });

		// Error callback
		server_->set_error_callback(
		    [](std::error_code ec)
		    {
			    std::cerr << "[Error] Server error: " << ec.message()
			              << std::endl;
		    });
	}

	void handle_command(std::shared_ptr<quic_session> session,
	                    const std::string& msg)
	{
		if (msg == "/status")
		{
			// Send server status
			std::string status = "Server status: " +
			                     std::to_string(server_->session_count()) +
			                     " clients connected";
			(void)session->send(
			    std::vector<uint8_t>(status.begin(), status.end()));
		}
		else if (msg == "/broadcast")
		{
			// Broadcast to all clients
			demo_broadcast();
		}
		else if (msg.substr(0, 5) == "/list")
		{
			// List all connected clients
			std::string list = "Connected clients:\n";
			for (const auto& s : server_->sessions())
			{
				list += "  - " + s->session_id() + "\n";
			}
			(void)session->send(std::vector<uint8_t>(list.begin(), list.end()));
		}
	}

	void demo_broadcast()
	{
		std::cout << "\n=== Broadcasting to All Clients ===" << std::endl;

		std::string broadcast_msg = "Broadcast: Hello everyone!";
		auto result = server_->broadcast(
		    std::vector<uint8_t>(broadcast_msg.begin(), broadcast_msg.end()));

		if (result.is_ok())
		{
			std::cout << "Broadcast sent to " << server_->session_count()
			          << " clients" << std::endl;
		}
		else
		{
			std::cerr << "Broadcast failed: " << result.error().message
			          << std::endl;
		}
	}

	void print_status()
	{
		std::cout << "\n--- Server Status ---" << std::endl;
		std::cout << "Running: " << (server_->is_running() ? "Yes" : "No")
		          << std::endl;
		std::cout << "Connected clients: " << server_->session_count()
		          << std::endl;

		auto sessions = server_->sessions();
		if (!sessions.empty())
		{
			std::cout << "Sessions:" << std::endl;
			for (const auto& session : sessions)
			{
				if (session && session->is_active())
				{
					auto stats = session->stats();
					std::cout << "  " << session->session_id()
					          << " - Bytes: sent=" << stats.bytes_sent
					          << ", recv=" << stats.bytes_received << std::endl;
				}
			}
		}
		std::cout << "---------------------" << std::endl;
	}

	unsigned short port_;
	std::shared_ptr<messaging_quic_server> server_;
};

/**
 * @brief Example showing multicast functionality
 */
void demo_multicast(std::shared_ptr<messaging_quic_server> server)
{
	std::cout << "\n=== Multicast Demo ===" << std::endl;

	auto sessions = server->sessions();
	if (sessions.size() < 2)
	{
		std::cout << "Need at least 2 clients for multicast demo" << std::endl;
		return;
	}

	// Get first two session IDs
	std::vector<std::string> target_ids;
	target_ids.push_back(sessions[0]->session_id());
	target_ids.push_back(sessions[1]->session_id());

	std::string multicast_msg = "Multicast: Selected clients only!";
	auto result = server->multicast(
	    target_ids,
	    std::vector<uint8_t>(multicast_msg.begin(), multicast_msg.end()));

	if (result.is_ok())
	{
		std::cout << "Multicast sent to " << target_ids.size() << " clients"
		          << std::endl;
	}
	else
	{
		std::cerr << "Multicast failed: " << result.error().message
		          << std::endl;
	}
}

/**
 * @brief Example with minimal setup
 */
void simple_server_example()
{
	std::cout << "\n=== Simple QUIC Server ===" << std::endl;

	auto server = std::make_shared<messaging_quic_server>("simple_server");

	// Minimal setup - just echo received messages
	server->set_receive_callback(
	    [](std::shared_ptr<quic_session> session,
	       const std::vector<uint8_t>& data)
	    {
		    // Echo back
		    std::vector<uint8_t> response(data);
		    (void)session->send(std::move(response));
	    });

	auto result = server->start_server(4434);
	if (result.is_ok())
	{
		std::cout << "Simple server started on port 4434" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		(void)server->stop_server();
	}
}

int main(int argc, char* argv[])
{
	// Set up signal handler for graceful shutdown
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	unsigned short port = 4433;

	if (argc >= 2)
	{
		port = static_cast<unsigned short>(std::stoi(argv[1]));
	}

	std::cout << "QUIC Server Example" << std::endl;
	std::cout << "===================" << std::endl;
	std::cout << std::endl;

	// Run the main demo
	QuicServerDemo demo(port);
	bool success = demo.run();

	return success ? 0 : 1;
}
