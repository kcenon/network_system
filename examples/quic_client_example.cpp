/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Network System Project
 *
 * QUIC Client Example
 *
 * This example demonstrates how to use the QUIC client API
 * to connect to a QUIC server and send/receive data.
 *
 * Note: This sample uses messaging_quic_client directly instead of quic_facade
 * because it needs QUIC-specific features like multi-stream support, 0-RTT,
 * and connection statistics. The quic_facade returns i_protocol_client which
 * provides a unified interface but doesn't expose QUIC-specific methods.
 *
 * Key features demonstrated:
 * - Basic connection and disconnection
 * - Sending data on the default stream
 * - Creating and using multiple streams (QUIC-specific)
 * - Handling callbacks for various events
 * - Using configuration options
 */

#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <mutex>

#define NETWORK_USE_EXPERIMENTAL
#include "internal/experimental/quic_client.h"

using namespace kcenon::network;

/**
 * @brief Simple QUIC client demo
 */
class QuicClientDemo
{
public:
	QuicClientDemo(const std::string& server_host, unsigned short server_port)
		: server_host_(server_host)
		, server_port_(server_port)
		, connected_(false)
		, message_received_(false)
	{
	}

	bool run()
	{
		std::cout << "=== QUIC Client Example ===" << std::endl;
		std::cout << "Connecting to " << server_host_ << ":" << server_port_ << std::endl;

		// Create QUIC client directly for QUIC-specific features
		client_ = std::make_shared<core::messaging_quic_client>("quic_demo_client");

		// Set up callbacks
		setup_callbacks();

		// Start the client
		auto result = client_->start_client(server_host_, server_port_);
		if (result.is_err())
		{
			std::cerr << "Failed to start client: " << result.error().message << std::endl;
			return false;
		}

		std::cout << "Client started, waiting for connection..." << std::endl;

		// Wait for connection (with timeout)
		{
			std::unique_lock<std::mutex> lock(mutex_);
			bool connected = cv_.wait_for(lock, std::chrono::seconds(10), [this]() {
				return connected_.load();
			});

			if (!connected)
			{
				std::cerr << "Connection timeout" << std::endl;
				(void)client_->stop_client();
				return false;
			}
		}

		std::cout << "Connected to server!" << std::endl;

		// Demo: Send data on default stream
		demo_send_default_stream();

		// Demo: Create and use multiple streams
		demo_multi_stream();

		// Wait a bit for responses
		std::this_thread::sleep_for(std::chrono::seconds(2));

		// Get statistics
		auto stats = client_->stats();
		std::cout << "\n=== Connection Statistics ===" << std::endl;
		std::cout << "Bytes sent: " << stats.bytes_sent << std::endl;
		std::cout << "Bytes received: " << stats.bytes_received << std::endl;
		std::cout << "Packets sent: " << stats.packets_sent << std::endl;
		std::cout << "Packets received: " << stats.packets_received << std::endl;
		std::cout << "Packets lost: " << stats.packets_lost << std::endl;

		// Stop the client
		std::cout << "\nStopping client..." << std::endl;
		(void)client_->stop_client();
		client_->wait_for_stop();

		std::cout << "Client stopped." << std::endl;
		return true;
	}

private:
	void setup_callbacks()
	{
		// Connected callback
		client_->set_connected_callback([this]() {
			std::cout << "[Callback] Connected to server" << std::endl;
			connected_.store(true);
			cv_.notify_all();
		});

		// Disconnected callback
		client_->set_disconnected_callback([this]() {
			std::cout << "[Callback] Disconnected from server" << std::endl;
			connected_.store(false);
		});

		// Error callback
		client_->set_error_callback([](std::error_code ec) {
			std::cerr << "[Callback] Error: " << ec.message() << std::endl;
		});

		// Receive callback for default stream
		client_->set_receive_callback([this](const std::vector<uint8_t>& data) {
			std::string msg(data.begin(), data.end());
			std::cout << "[Callback] Received on default stream: " << msg << std::endl;
			message_received_.store(true);
		});

		// Stream receive callback for all streams
		client_->set_stream_receive_callback(
			[](uint64_t stream_id, const std::vector<uint8_t>& data, bool fin) {
				std::string msg(data.begin(), data.end());
				std::cout << "[Callback] Stream " << stream_id << ": "
				          << data.size() << " bytes"
				          << (fin ? " (FIN)" : "") << std::endl;
				std::cout << "  Data: " << msg << std::endl;
			});
	}

	void demo_send_default_stream()
	{
		std::cout << "\n=== Sending on Default Stream ===" << std::endl;

		// Send string data
		auto result = client_->send_packet("Hello, QUIC Server!");
		if (result.is_ok())
		{
			std::cout << "Sent string message on default stream" << std::endl;
		}
		else
		{
			std::cerr << "Failed to send: " << result.error().message << std::endl;
		}

		// Send binary data
		std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03, 0x04, 0x05};
		result = client_->send_packet(std::move(binary_data));
		if (result.is_ok())
		{
			std::cout << "Sent binary data on default stream" << std::endl;
		}
		else
		{
			std::cerr << "Failed to send: " << result.error().message << std::endl;
		}
	}

	void demo_multi_stream()
	{
		std::cout << "\n=== Multi-Stream Demo ===" << std::endl;

		// Create a new bidirectional stream
		auto stream_result = client_->create_stream();
		if (stream_result.is_err())
		{
			std::cerr << "Failed to create stream: " << stream_result.error().message << std::endl;
			return;
		}

		uint64_t stream_id = stream_result.value();
		std::cout << "Created bidirectional stream: " << stream_id << std::endl;

		// Send data on the new stream
		std::string msg = "Hello from stream " + std::to_string(stream_id);
		std::vector<uint8_t> data(msg.begin(), msg.end());

		auto send_result = client_->send_on_stream(stream_id, std::move(data));
		if (send_result.is_ok())
		{
			std::cout << "Sent message on stream " << stream_id << std::endl;
		}
		else
		{
			std::cerr << "Failed to send: " << send_result.error().message << std::endl;
		}

		// Create a unidirectional stream
		auto uni_stream_result = client_->create_unidirectional_stream();
		if (uni_stream_result.is_ok())
		{
			uint64_t uni_stream_id = uni_stream_result.value();
			std::cout << "Created unidirectional stream: " << uni_stream_id << std::endl;

			std::string uni_msg = "Unidirectional message";
			std::vector<uint8_t> uni_data(uni_msg.begin(), uni_msg.end());

			// Send with FIN to close the stream after this message
			auto uni_send_result = client_->send_on_stream(uni_stream_id,
			                                                std::move(uni_data),
			                                                true /* fin */);
			if (uni_send_result.is_ok())
			{
				std::cout << "Sent message on unidirectional stream with FIN" << std::endl;
			}
		}

		// Close the bidirectional stream
		auto close_result = client_->close_stream(stream_id);
		if (close_result.is_ok())
		{
			std::cout << "Closed stream " << stream_id << std::endl;
		}
	}

	std::string server_host_;
	unsigned short server_port_;
	std::shared_ptr<core::messaging_quic_client> client_;
	std::atomic<bool> connected_;
	std::atomic<bool> message_received_;
	std::mutex mutex_;
	std::condition_variable cv_;
};

/**
 * @brief Example with custom configuration
 */
void demo_with_config()
{
	std::cout << "\n=== QUIC Client with Custom Config ===" << std::endl;

	// Create client directly for QUIC-specific features
	auto client = std::make_shared<core::messaging_quic_client>("config_demo_client");

	// Configure QUIC-specific options
	core::quic_client_config config;
	config.verify_server = false;  // For testing with self-signed certs
	config.alpn_protocols = {"h3"};
	config.max_idle_timeout_ms = 60000;  // 60 seconds
	config.enable_early_data = true;  // Enable 0-RTT

	// For mutual TLS, set optional fields:
	// config.ca_cert_file = "/path/to/ca.pem";
	// config.client_cert_file = "/path/to/client.pem";
	// config.client_key_file = "/path/to/client-key.pem";

	auto result = client->start_client("example.com", 443, config);
	if (result.is_err())
	{
		std::cout << "Expected failure (no server): " << result.error().message << std::endl;
	}

	(void)client->stop_client();
}

int main(int argc, char* argv[])
{
	std::string host = "127.0.0.1";
	unsigned short port = 4433;

	if (argc >= 2)
	{
		host = argv[1];
	}
	if (argc >= 3)
	{
		port = static_cast<unsigned short>(std::stoi(argv[2]));
	}

	// Run the main demo
	QuicClientDemo demo(host, port);

	// Note: This will likely fail without a running QUIC server
	// The demo is primarily to show the API usage
	std::cout << "\nNote: This demo requires a running QUIC server." << std::endl;
	std::cout << "Without a server, connection will timeout." << std::endl;
	std::cout << std::endl;

	bool success = demo.run();

	// Also show configuration example
	demo_with_config();

	return success ? 0 : 1;
}
