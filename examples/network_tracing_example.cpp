/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file network_tracing_example.cpp
 * @brief Demonstrates distributed tracing for network operations.
 * @example network_tracing_example.cpp
 *
 * @par Category
 * Observability - Distributed tracing
 *
 * Demonstrates:
 * - Creating trace contexts
 * - Creating spans for operation timing
 * - Propagating context via headers
 *
 * @see kcenon::network::tracing::trace_context
 * @see kcenon::network::tracing::span
 */

#include <kcenon/network/tracing/tracing.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace kcenon::network;

void simulate_database_query(tracing::trace_context& ctx)
{
	auto db_span = ctx.create_span("database_query");
	std::cout << "   Executing database query..." << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	std::cout << "   Query completed" << std::endl;
	// span ends automatically (RAII)
}

void simulate_cache_lookup(tracing::trace_context& ctx)
{
	auto cache_span = ctx.create_span("cache_lookup");
	std::cout << "   Checking cache..." << std::endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	std::cout << "   Cache miss" << std::endl;
}

int main()
{
	std::cout << "=== Network Tracing Example ===" << std::endl;

	// 1. Create a trace context for an incoming request
	std::cout << "\n1. Creating trace context:" << std::endl;
	auto ctx = tracing::trace_context::create("handle_request");
	std::cout << "   Trace context created for 'handle_request'" << std::endl;

	// 2. Create spans for sub-operations
	std::cout << "\n2. Creating nested spans:" << std::endl;
	{
		auto request_span = ctx.create_span("process_request");
		simulate_cache_lookup(ctx);
		simulate_database_query(ctx);
		std::cout << "   Request processing complete" << std::endl;
	}

	// 3. Propagate context via headers
	std::cout << "\n3. Header propagation:" << std::endl;
	auto headers = ctx.to_headers();
	std::cout << "   Trace headers (" << headers.size() << "):" << std::endl;
	for (const auto& [key, value] : headers)
	{
		std::cout << "     " << key << ": " << value << std::endl;
	}

	// 4. Reconstruct context from headers (simulating receiving service)
	std::cout << "\n4. Reconstructing context from headers:" << std::endl;
	std::unordered_map<std::string, std::string> header_map;
	for (const auto& [key, value] : headers)
	{
		header_map[key] = value;
	}
	auto downstream_ctx = tracing::trace_context::from_headers(header_map);
	auto downstream_span = downstream_ctx.create_span("downstream_service");
	std::cout << "   Downstream context reconstructed" << std::endl;
	std::cout << "   Downstream span created" << std::endl;

	std::cout << "\nDone." << std::endl;
	return 0;
}
