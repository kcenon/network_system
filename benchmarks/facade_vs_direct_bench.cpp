// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/**
 * @file facade_vs_direct_bench.cpp
 * @brief Benchmark comparing facade API vs direct core API overhead
 *
 * Validates:
 * 1. Throughput claims (305K+ msg/sec target)
 * 2. Latency percentile tracking (p50, p95, p99)
 * 3. Facade API vs direct core API overhead comparison
 *
 * Part of issue #704: Benchmark validation
 */

#include <benchmark/benchmark.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/facade/tcp_facade.h"
#include "kcenon/network/interfaces/i_protocol_client.h"
#include "kcenon/network/interfaces/i_protocol_server.h"
#include "internal/core/messaging_client.h"
#include "internal/core/messaging_server.h"

namespace {

using namespace kcenon::network;

// ============================================================================
// Helpers
// ============================================================================

/**
 * @brief Find an available TCP port for benchmarks
 */
uint16_t find_available_port(uint16_t start = 10000)
{
	for (uint16_t port = start; port < 60000; ++port)
	{
		try
		{
			asio::io_context io;
			asio::ip::tcp::acceptor acceptor(io);
			asio::ip::tcp::endpoint ep(asio::ip::tcp::v4(), port);
			acceptor.open(ep.protocol());
			acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
			acceptor.bind(ep);
			acceptor.close();
			return port;
		}
		catch (...)
		{
			// Port in use, try next
		}
	}
	return 0;
}

/**
 * @brief Calculate percentile from a sorted vector of doubles
 */
double percentile(const std::vector<double>& sorted_data, double pct)
{
	if (sorted_data.empty())
	{
		return 0.0;
	}

	auto idx = static_cast<size_t>((pct / 100.0) * static_cast<double>(sorted_data.size()));
	if (idx >= sorted_data.size())
	{
		idx = sorted_data.size() - 1;
	}

	return sorted_data[idx];
}

/**
 * @brief Generate test payload of specified size
 */
std::vector<uint8_t> make_payload(size_t size)
{
	std::vector<uint8_t> data(size);
	for (size_t i = 0; i < size; ++i)
	{
		data[i] = static_cast<uint8_t>(i & 0xFF);
	}
	return data;
}

// ============================================================================
// Section 1: Client Creation Overhead — Facade vs Direct
// ============================================================================

/**
 * @brief Benchmark direct messaging_client construction
 *
 * Measures the cost of creating a messaging_client directly,
 * without the facade factory pattern overhead.
 */
static void BM_DirectClient_Create(benchmark::State& state)
{
	for (auto _ : state)
	{
		auto client = std::make_shared<core::messaging_client>("bench_client");
		benchmark::DoNotOptimize(client);
	}

	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_DirectClient_Create);

/**
 * @brief Benchmark facade client creation
 *
 * Measures the cost of creating a client through tcp_facade,
 * which includes config validation and ID generation overhead.
 */
static void BM_FacadeClient_Create(benchmark::State& state)
{
	facade::tcp_facade facade;

	for (auto _ : state)
	{
		auto client = facade.create_client({
			.host = "127.0.0.1",
			.port = 8080,
			.client_id = "bench_client"
		});
		benchmark::DoNotOptimize(client);
	}

	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_FacadeClient_Create);

/**
 * @brief Benchmark direct messaging_server construction
 */
static void BM_DirectServer_Create(benchmark::State& state)
{
	for (auto _ : state)
	{
		auto server = std::make_shared<core::messaging_server>("bench_server");
		benchmark::DoNotOptimize(server);
	}

	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_DirectServer_Create);

/**
 * @brief Benchmark facade server creation
 */
static void BM_FacadeServer_Create(benchmark::State& state)
{
	facade::tcp_facade facade;

	for (auto _ : state)
	{
		auto server = facade.create_server({
			.port = 8080,
			.server_id = "bench_server"
		});
		benchmark::DoNotOptimize(server);
	}

	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_FacadeServer_Create);

// ============================================================================
// Section 2: Send Throughput — Direct Core API
// ============================================================================

/**
 * @brief Benchmark send throughput via direct core API
 *
 * Creates a real TCP server + client, then measures how fast
 * messages can be queued for sending. Reports throughput (msg/sec)
 * and latency percentiles (p50, p95, p99).
 */
static void BM_DirectAPI_SendThroughput(benchmark::State& state)
{
	const auto payload_size = static_cast<size_t>(state.range(0));

	auto port = find_available_port(10000 + static_cast<uint16_t>(payload_size % 10000));
	if (port == 0)
	{
		state.SkipWithError("No available port");
		return;
	}

	// Setup server
	auto server = std::make_shared<core::messaging_server>("direct_bench_server");
	auto start_result = server->start_server(port);
	if (start_result.is_err())
	{
		state.SkipWithError("Failed to start server");
		return;
	}

	// Allow server to begin accepting
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Setup client
	auto client = std::make_shared<core::messaging_client>("direct_bench_client");
	auto connect_result = client->start_client("127.0.0.1", port);
	if (connect_result.is_err())
	{
		(void)server->stop_server();
		state.SkipWithError("Failed to connect client");
		return;
	}

	// Wait for connection establishment
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	auto payload = make_payload(payload_size);
	std::vector<double> latencies_us;
	latencies_us.reserve(static_cast<size_t>(state.max_iterations));

	for (auto _ : state)
	{
		auto copy = payload;
		auto send_start = std::chrono::high_resolution_clock::now();
		auto result = client->send_packet(std::move(copy));
		auto send_end = std::chrono::high_resolution_clock::now();

		benchmark::DoNotOptimize(result);

		auto latency = std::chrono::duration<double, std::micro>(send_end - send_start).count();
		latencies_us.push_back(latency);
	}

	// Calculate percentiles
	std::sort(latencies_us.begin(), latencies_us.end());

	if (!latencies_us.empty())
	{
		state.counters["p50_us"] = percentile(latencies_us, 50.0);
		state.counters["p95_us"] = percentile(latencies_us, 95.0);
		state.counters["p99_us"] = percentile(latencies_us, 99.0);
		state.counters["avg_us"] = std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0)
			/ static_cast<double>(latencies_us.size());
	}

	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
	state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
		* static_cast<int64_t>(payload_size));

	// Cleanup — allow thread resources to be reclaimed
	(void)client->stop_client();
	(void)server->stop_server();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
BENCHMARK(BM_DirectAPI_SendThroughput)
	->Arg(64)      // Small message
	->Arg(256)     // Typical message
	->Arg(1024)    // 1KB
	->Arg(4096);   // 4KB

// ============================================================================
// Section 3: Send Throughput — Facade API
// ============================================================================

/**
 * @brief Benchmark send throughput via facade API
 *
 * Same measurement as BM_DirectAPI_SendThroughput but using the
 * facade API path. This validates that facade introduces zero
 * performance overhead for send operations.
 */
static void BM_FacadeAPI_SendThroughput(benchmark::State& state)
{
	const auto payload_size = static_cast<size_t>(state.range(0));

	auto port = find_available_port(20000 + static_cast<uint16_t>(payload_size % 10000));
	if (port == 0)
	{
		state.SkipWithError("No available port");
		return;
	}

	facade::tcp_facade facade;
	std::shared_ptr<interfaces::i_protocol_server> server;
	std::shared_ptr<interfaces::i_protocol_client> client;

	try
	{
		// Setup server via facade
		server = facade.create_server({
			.port = port,
			.server_id = "facade_bench_server"
		});
		auto start_result = server->start(port);
		if (start_result.is_err())
		{
			state.SkipWithError("Failed to start facade server");
			return;
		}

		// Allow server to begin accepting
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		// Setup client via facade
		client = facade.create_client({
			.host = "127.0.0.1",
			.port = port,
			.client_id = "facade_bench_client"
		});
		auto connect_result = client->start("127.0.0.1", port);
		if (connect_result.is_err())
		{
			(void)server->stop();
			state.SkipWithError("Failed to connect facade client");
			return;
		}
	}
	catch (const std::exception& e)
	{
		if (server)
		{
			(void)server->stop();
		}
		state.SkipWithError(e.what());
		return;
	}

	// Wait for connection establishment
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	auto payload = make_payload(payload_size);
	std::vector<double> latencies_us;
	latencies_us.reserve(static_cast<size_t>(state.max_iterations));

	for (auto _ : state)
	{
		auto copy = payload;
		auto send_start = std::chrono::high_resolution_clock::now();
		auto result = client->send(std::move(copy));
		auto send_end = std::chrono::high_resolution_clock::now();

		benchmark::DoNotOptimize(result);

		auto latency = std::chrono::duration<double, std::micro>(send_end - send_start).count();
		latencies_us.push_back(latency);
	}

	// Calculate percentiles
	std::sort(latencies_us.begin(), latencies_us.end());

	if (!latencies_us.empty())
	{
		state.counters["p50_us"] = percentile(latencies_us, 50.0);
		state.counters["p95_us"] = percentile(latencies_us, 95.0);
		state.counters["p99_us"] = percentile(latencies_us, 99.0);
		state.counters["avg_us"] = std::accumulate(latencies_us.begin(), latencies_us.end(), 0.0)
			/ static_cast<double>(latencies_us.size());
	}

	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
	state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
		* static_cast<int64_t>(payload_size));

	// Cleanup — allow thread resources to be reclaimed
	(void)client->stop();
	(void)server->stop();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
BENCHMARK(BM_FacadeAPI_SendThroughput)
	->Arg(64)      // Small message
	->Arg(256)     // Typical message
	->Arg(1024)    // 1KB
	->Arg(4096);   // 4KB

// ============================================================================
// Section 4: Burst Throughput Validation
// ============================================================================

/**
 * @brief Benchmark burst send throughput to validate 305K+ msg/sec claim
 *
 * Sends a batch of small messages in tight loop to measure peak
 * throughput. Uses direct core API for maximum performance measurement.
 * Reports total throughput and per-message latency percentiles.
 */
static void BM_BurstThroughput_Validation(benchmark::State& state)
{
	const auto batch_size = static_cast<size_t>(state.range(0));
	constexpr size_t payload_size = 64; // Small messages for peak throughput

	auto port = find_available_port(30000);
	if (port == 0)
	{
		state.SkipWithError("No available port");
		return;
	}

	// Setup server
	auto server = std::make_shared<core::messaging_server>("burst_server");
	auto start_result = server->start_server(port);
	if (start_result.is_err())
	{
		state.SkipWithError("Failed to start server");
		return;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	// Setup client
	auto client = std::make_shared<core::messaging_client>("burst_client");
	auto connect_result = client->start_client("127.0.0.1", port);
	if (connect_result.is_err())
	{
		(void)server->stop_server();
		state.SkipWithError("Failed to connect client");
		return;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	auto payload = make_payload(payload_size);
	std::vector<double> batch_latencies_us;

	for (auto _ : state)
	{
		auto batch_start = std::chrono::high_resolution_clock::now();

		for (size_t i = 0; i < batch_size; ++i)
		{
			auto copy = payload;
			auto result = client->send_packet(std::move(copy));
			benchmark::DoNotOptimize(result);
		}

		auto batch_end = std::chrono::high_resolution_clock::now();
		auto batch_us = std::chrono::duration<double, std::micro>(batch_end - batch_start).count();
		batch_latencies_us.push_back(batch_us);
	}

	// Report throughput as messages per second
	std::sort(batch_latencies_us.begin(), batch_latencies_us.end());

	if (!batch_latencies_us.empty())
	{
		double median_batch_us = percentile(batch_latencies_us, 50.0);
		double msgs_per_sec = (median_batch_us > 0.0)
			? (static_cast<double>(batch_size) / median_batch_us) * 1e6
			: 0.0;

		state.counters["msgs_per_sec"] = msgs_per_sec;
		state.counters["batch_p50_us"] = percentile(batch_latencies_us, 50.0);
		state.counters["batch_p95_us"] = percentile(batch_latencies_us, 95.0);
		state.counters["batch_p99_us"] = percentile(batch_latencies_us, 99.0);
	}

	state.SetItemsProcessed(static_cast<int64_t>(state.iterations())
		* static_cast<int64_t>(batch_size));
	state.SetBytesProcessed(static_cast<int64_t>(state.iterations())
		* static_cast<int64_t>(batch_size)
		* static_cast<int64_t>(payload_size));

	// Cleanup — allow thread resources to be reclaimed
	(void)client->stop_client();
	(void)server->stop_server();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
BENCHMARK(BM_BurstThroughput_Validation)
	->Arg(100)     // Small burst
	->Arg(1000)    // Medium burst
	->Arg(10000);  // Large burst (target: 305K+ msg/sec)

// ============================================================================
// Section 5: Connection Lifecycle Overhead
// ============================================================================

/**
 * @brief Benchmark full connection lifecycle via direct API
 *
 * Measures the complete cycle: create → connect → send → disconnect → destroy.
 * Iteration count is limited to avoid thread resource exhaustion on CI runners.
 */
static void BM_DirectAPI_FullLifecycle(benchmark::State& state)
{
	auto port = find_available_port(40000);
	if (port == 0)
	{
		state.SkipWithError("No available port");
		return;
	}

	// Shared server for all iterations
	auto server = std::make_shared<core::messaging_server>("lifecycle_server");
	auto start_result = server->start_server(port);
	if (start_result.is_err())
	{
		state.SkipWithError("Failed to start server");
		return;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	auto payload = make_payload(256);

	for (auto _ : state)
	{
		try
		{
			// Create
			auto client = std::make_shared<core::messaging_client>("lifecycle_client");

			// Connect
			auto connect_result = client->start_client("127.0.0.1", port);
			if (connect_result.is_err())
			{
				continue;
			}

			// Brief wait for connection
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			// Send
			auto copy = payload;
			(void)client->send_packet(std::move(copy));

			// Disconnect and allow thread cleanup
			(void)client->stop_client();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		catch (const std::exception&)
		{
			// Thread resource exhaustion — stop gracefully
			break;
		}
	}

	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));

	(void)server->stop_server();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
BENCHMARK(BM_DirectAPI_FullLifecycle)->Iterations(10);

/**
 * @brief Benchmark full connection lifecycle via facade API
 *
 * Iteration count is limited to avoid thread resource exhaustion on CI runners.
 */
static void BM_FacadeAPI_FullLifecycle(benchmark::State& state)
{
	auto port = find_available_port(41000);
	if (port == 0)
	{
		state.SkipWithError("No available port");
		return;
	}

	facade::tcp_facade facade;
	std::shared_ptr<interfaces::i_protocol_server> server;

	try
	{
		server = facade.create_server({
			.port = port,
			.server_id = "facade_lifecycle_server"
		});
		auto start_result = server->start(port);
		if (start_result.is_err())
		{
			state.SkipWithError("Failed to start server");
			return;
		}
	}
	catch (const std::exception& e)
	{
		state.SkipWithError(e.what());
		return;
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	auto payload = make_payload(256);

	for (auto _ : state)
	{
		try
		{
			// Create via facade
			auto client = facade.create_client({
				.host = "127.0.0.1",
				.port = port,
				.client_id = "facade_lifecycle_client"
			});

			// Connect
			auto connect_result = client->start("127.0.0.1", port);
			if (connect_result.is_err())
			{
				continue;
			}

			// Brief wait for connection
			std::this_thread::sleep_for(std::chrono::milliseconds(10));

			// Send
			auto copy = payload;
			(void)client->send(std::move(copy));

			// Disconnect and allow thread cleanup
			(void)client->stop();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		catch (const std::exception&)
		{
			// Thread resource exhaustion — stop gracefully
			break;
		}
	}

	state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));

	(void)server->stop();
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
BENCHMARK(BM_FacadeAPI_FullLifecycle)->Iterations(10);

}  // namespace
