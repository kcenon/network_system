/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, Network System Contributors
All rights reserved.
*****************************************************************************/

/**
 * @file connection_pool_example.cpp
 * @brief Demonstrates connection pooling usage patterns
 *
 * This example shows:
 * - Basic pool initialization and usage
 * - Concurrent connection usage with multiple threads
 * - RAII-based automatic connection release
 * - Error handling patterns
 *
 * Build:
 *   cmake --build build --target connection_pool_example
 *
 * Run (requires a running server on localhost:5555):
 *   ./build/samples/connection_pool_example
 */

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
#include <vector>

#include "network_system/core/connection_pool.h"
#include "network_system/core/messaging_server.h"
#include "network_system/session/messaging_session.h"

using namespace network_system::core;

// RAII wrapper for automatic connection release
class scoped_connection
{
public:
	explicit scoped_connection(std::shared_ptr<connection_pool> pool)
		: pool_(std::move(pool)), client_(pool_->acquire())
	{
	}

	~scoped_connection()
	{
		if (client_)
		{
			pool_->release(std::move(client_));
		}
	}

	auto operator->() { return client_.get(); }
	auto get() { return client_.get(); }

	// Disable copy
	scoped_connection(const scoped_connection&) = delete;
	scoped_connection& operator=(const scoped_connection&) = delete;

	// Enable move
	scoped_connection(scoped_connection&& other) noexcept
		: pool_(std::move(other.pool_)), client_(std::move(other.client_))
	{
	}

	scoped_connection& operator=(scoped_connection&& other) noexcept
	{
		if (this != &other)
		{
			if (client_)
			{
				pool_->release(std::move(client_));
			}
			pool_ = std::move(other.pool_);
			client_ = std::move(other.client_);
		}
		return *this;
	}

private:
	std::shared_ptr<connection_pool> pool_;
	std::unique_ptr<messaging_client> client_;
};

/**
 * @brief Demonstrates basic connection pool usage
 */
void basic_usage_example(std::shared_ptr<connection_pool> pool)
{
	std::cout << "=== Basic Usage Example ===\n";

	// Acquire a connection from the pool
	auto client = pool->acquire();

	std::cout << "Acquired connection. Active: " << pool->active_count() << "/"
			  << pool->pool_size() << "\n";

	// Send some data
	std::vector<uint8_t> data = {'p', 'i', 'n', 'g'};
	auto result = client->send_packet(std::move(data));

	if (result.is_ok())
	{
		std::cout << "Successfully sent ping message\n";
	}
	else
	{
		std::cerr << "Failed to send: " << result.error().message << "\n";
	}

	// Give server time to respond
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Release back to pool
	pool->release(std::move(client));

	std::cout << "Released connection. Active: " << pool->active_count() << "/"
			  << pool->pool_size() << "\n\n";
}

/**
 * @brief Demonstrates RAII-based connection management
 */
void raii_example(std::shared_ptr<connection_pool> pool)
{
	std::cout << "=== RAII Example ===\n";

	{
		// Connection automatically acquired
		scoped_connection conn(pool);

		std::cout << "Acquired via RAII. Active: " << pool->active_count()
				  << "/" << pool->pool_size() << "\n";

		// Use the connection
		std::vector<uint8_t> data = {'t', 'e', 's', 't'};
		conn->send_packet(std::move(data));

		// Connection automatically released when scope ends
	}

	std::cout << "After scope exit. Active: " << pool->active_count() << "/"
			  << pool->pool_size() << "\n\n";
}

/**
 * @brief Demonstrates concurrent usage with multiple threads
 */
void concurrent_usage_example(std::shared_ptr<connection_pool> pool)
{
	std::cout << "=== Concurrent Usage Example ===\n";

	constexpr int num_threads = 10;
	constexpr int requests_per_thread = 5;

	std::atomic<int> success_count{0};
	std::atomic<int> failure_count{0};
	std::vector<std::thread> workers;

	auto start_time = std::chrono::steady_clock::now();

	// Spawn worker threads
	for (int i = 0; i < num_threads; ++i)
	{
		workers.emplace_back(
			[pool, i, &success_count, &failure_count, requests_per_thread]()
			{
				for (int j = 0; j < requests_per_thread; ++j)
				{
					auto client = pool->acquire();

					// Simulate some work
					std::vector<uint8_t> data = {static_cast<uint8_t>('A' + i),
												 static_cast<uint8_t>('0' + j)};

					auto result = client->send_packet(std::move(data));

					if (result.is_ok())
					{
						success_count.fetch_add(1, std::memory_order_relaxed);
					}
					else
					{
						failure_count.fetch_add(1, std::memory_order_relaxed);
					}

					pool->release(std::move(client));

					// Small delay to simulate processing
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			});
	}

	// Wait for all workers
	for (auto& w : workers)
	{
		w.join();
	}

	auto end_time = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
		end_time - start_time);

	int total_requests = num_threads * requests_per_thread;
	std::cout << "Completed " << success_count.load() << "/" << total_requests
			  << " requests successfully\n";
	std::cout << "Failed: " << failure_count.load() << "\n";
	std::cout << "Duration: " << duration.count() << "ms\n";
	std::cout << "Throughput: "
			  << (total_requests * 1000.0 / duration.count()) << " req/s\n\n";
}

/**
 * @brief Demonstrates error handling patterns
 */
void error_handling_example(std::shared_ptr<connection_pool> pool)
{
	std::cout << "=== Error Handling Example ===\n";

	// Example 1: Handle send failure
	{
		auto client = pool->acquire();

		std::vector<uint8_t> data = {'e', 'r', 'r', 'o', 'r', '_', 't', 'e', 's', 't'};
		auto result = client->send_packet(std::move(data));

		if (result.is_err())
		{
			std::cerr << "Send failed: " << result.error().message << "\n";
			std::cerr << "Error code: " << result.error().code << "\n";
			// Connection is still returned to pool
		}

		pool->release(std::move(client));
	}

	// Example 2: Check connection before use
	{
		auto client = pool->acquire();

		if (!client->is_connected())
		{
			std::cerr << "Acquired disconnected client (unexpected)\n";
			pool->release(std::move(client));
		}
		else
		{
			std::cout << "Connection is healthy\n";
			pool->release(std::move(client));
		}
	}

	std::cout << "\n";
}

/**
 * @brief Demonstrates pool monitoring
 */
void monitoring_example(std::shared_ptr<connection_pool> pool)
{
	std::cout << "=== Monitoring Example ===\n";

	// Acquire some connections
	std::vector<std::unique_ptr<messaging_client>> held_connections;

	for (int i = 0; i < 3; ++i)
	{
		held_connections.push_back(pool->acquire());

		double utilization =
			static_cast<double>(pool->active_count()) / pool->pool_size() * 100;

		std::cout << "Pool utilization: " << pool->active_count() << "/"
				  << pool->pool_size() << " (" << utilization << "%)\n";
	}

	// Release all
	for (auto& client : held_connections)
	{
		pool->release(std::move(client));
	}

	std::cout << "After release: " << pool->active_count() << "/"
			  << pool->pool_size() << "\n\n";
}

int main()
{
	std::cout << "Connection Pool Example\n";
	std::cout << "=======================\n\n";

	// Start a simple echo server for testing
	auto server = std::make_shared<messaging_server>("test_server");

	// Set up echo handler
	server->set_receive_callback(
		[](std::shared_ptr<network_system::session::messaging_session> session,
		   const std::vector<uint8_t>& data)
		{
			// Echo back the data
			session->send_packet(std::vector<uint8_t>(data));
		});

	auto server_result = server->start_server(5555);
	if (server_result.is_err())
	{
		std::cerr << "Failed to start server: " << server_result.error().message
				  << "\n";
		return 1;
	}

	std::cout << "Started test server on port 5555\n\n";

	// Give server time to start
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// Create and initialize connection pool
	auto pool = std::make_shared<connection_pool>("127.0.0.1", 5555, 5);

	auto init_result = pool->initialize();
	if (init_result.is_err())
	{
		std::cerr << "Failed to initialize pool: "
				  << init_result.error().message << "\n";
		server->stop_server();
		return 1;
	}

	std::cout << "Initialized connection pool with " << pool->pool_size()
			  << " connections\n\n";

	// Run examples
	basic_usage_example(pool);
	raii_example(pool);
	concurrent_usage_example(pool);
	error_handling_example(pool);
	monitoring_example(pool);

	// Cleanup
	std::cout << "=== Cleanup ===\n";
	std::cout << "Stopping server...\n";
	server->stop_server();
	std::cout << "Done.\n";

	return 0;
}
