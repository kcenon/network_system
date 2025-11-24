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
 * @file http_bench.cpp
 * @brief HTTP server/client performance benchmarks
 *
 * NET-202: Reactivate HTTP Performance Benchmarks
 *
 * Measures:
 * - Request throughput (RPS)
 * - Latency distribution (P50, P95, P99, P999)
 * - Concurrent connection handling
 * - Memory usage per connection
 */

#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <future>
#include <algorithm>
#include <numeric>

#include "kcenon/network/core/http_server.h"
#include "kcenon/network/core/http_client.h"

using namespace network_system::core;
using namespace network_system::internal;
using namespace std::chrono_literals;

namespace {

// Global server instance for benchmarks
std::shared_ptr<http_server> g_http_server;
uint16_t g_server_port = 0;
std::atomic<bool> g_server_ready{false};

// Find available port for testing
uint16_t find_available_port(uint16_t start = 9000)
{
    for (uint16_t port = start; port < 65535; ++port)
    {
        try
        {
            asio::io_context io_context;
            asio::ip::tcp::acceptor acceptor(io_context);
            asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
            acceptor.open(endpoint.protocol());
            acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
            acceptor.bind(endpoint);
            acceptor.close();
            return port;
        }
        catch (...)
        {
            continue;
        }
    }
    return 0;
}

// Setup HTTP server for benchmarks
void setup_benchmark_server()
{
    if (g_http_server)
    {
        return;
    }

    g_server_port = find_available_port();
    if (g_server_port == 0)
    {
        throw std::runtime_error("No available port found");
    }

    g_http_server = std::make_shared<http_server>("benchmark_http_server");

    // Simple GET endpoint
    g_http_server->get("/", [](const http_request_context&) {
        http_response response;
        response.status_code = 200;
        response.set_body_string("Hello, World!");
        response.set_header("Content-Type", "text/plain");
        return response;
    });

    // Echo endpoint for POST benchmarks
    g_http_server->post("/echo", [](const http_request_context& ctx) {
        http_response response;
        response.status_code = 200;
        response.body = ctx.request.body;
        response.set_header("Content-Type", "application/octet-stream");
        return response;
    });

    // Variable size response endpoint
    g_http_server->get("/size/:bytes", [](const http_request_context& ctx) {
        http_response response;
        response.status_code = 200;

        auto bytes_str = ctx.get_path_param("bytes").value_or("1024");
        size_t bytes = std::stoul(bytes_str);
        std::string body(bytes, 'X');

        response.set_body_string(body);
        response.set_header("Content-Type", "application/octet-stream");
        return response;
    });

    auto result = g_http_server->start(g_server_port);
    if (result.is_err())
    {
        throw std::runtime_error("Failed to start HTTP server: " + result.error().message);
    }

    // Wait for server to be ready
    std::this_thread::sleep_for(100ms);
    g_server_ready = true;
}

void teardown_benchmark_server()
{
    if (g_http_server)
    {
        g_http_server->stop();
        g_http_server.reset();
        g_server_ready = false;
    }
}

std::string get_server_url(const std::string& path = "/")
{
    return "http://localhost:" + std::to_string(g_server_port) + path;
}

}  // namespace

// HTTP Benchmark Fixture
class HttpBenchmarkFixture : public benchmark::Fixture
{
public:
    void SetUp(const ::benchmark::State& state) override
    {
        if (!g_server_ready)
        {
            try
            {
                setup_benchmark_server();
            }
            catch (const std::exception& e)
            {
                // Server setup failed
            }
        }
    }

    void TearDown(const ::benchmark::State& state) override
    {
        // Server teardown is handled globally
    }
};

// Benchmark: Simple GET request throughput
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, HTTP_SimpleGet)(benchmark::State& state)
{
    if (!g_server_ready)
    {
        state.SkipWithError("HTTP server not ready");
        return;
    }

    auto client = std::make_shared<http_client>();
    std::string url = get_server_url("/");

    for (auto _ : state)
    {
        auto response = client->get(url);
        if (response.is_ok() && response.value().status_code == 200)
        {
            benchmark::DoNotOptimize(response.value().body);
        }
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(HttpBenchmarkFixture, HTTP_SimpleGet)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(1000);

// Benchmark: POST request with variable payload sizes
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, HTTP_Post_PayloadSize)(benchmark::State& state)
{
    if (!g_server_ready)
    {
        state.SkipWithError("HTTP server not ready");
        return;
    }

    const int payload_size = state.range(0);
    std::string payload(payload_size, 'A');

    auto client = std::make_shared<http_client>();
    std::string url = get_server_url("/echo");

    std::map<std::string, std::string> headers = {
        {"Content-Type", "application/octet-stream"}
    };

    for (auto _ : state)
    {
        auto response = client->post(url, payload, headers);
        if (response.is_ok() && response.value().status_code == 200)
        {
            benchmark::DoNotOptimize(response.value().body);
        }
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * payload_size);
}
BENCHMARK_REGISTER_F(HttpBenchmarkFixture, HTTP_Post_PayloadSize)
    ->Arg(64)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16384)
    ->Arg(65536)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: Latency distribution
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, HTTP_LatencyDistribution)(benchmark::State& state)
{
    if (!g_server_ready)
    {
        state.SkipWithError("HTTP server not ready");
        return;
    }

    auto client = std::make_shared<http_client>();
    std::string url = get_server_url("/");

    std::vector<double> latencies;
    latencies.reserve(10000);

    for (auto _ : state)
    {
        auto start = std::chrono::high_resolution_clock::now();

        auto response = client->get(url);

        auto end = std::chrono::high_resolution_clock::now();
        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);

        if (response.is_ok())
        {
            benchmark::DoNotOptimize(response.value().body);
        }
    }

    // Calculate percentiles
    if (!latencies.empty())
    {
        std::sort(latencies.begin(), latencies.end());

        auto percentile = [&latencies](double p) {
            size_t idx = static_cast<size_t>(p * latencies.size());
            if (idx >= latencies.size()) idx = latencies.size() - 1;
            return latencies[idx];
        };

        state.counters["p50_us"] = percentile(0.50);
        state.counters["p95_us"] = percentile(0.95);
        state.counters["p99_us"] = percentile(0.99);
        state.counters["p999_us"] = percentile(0.999);
        state.counters["avg_us"] = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        state.counters["min_us"] = latencies.front();
        state.counters["max_us"] = latencies.back();
    }
}
BENCHMARK_REGISTER_F(HttpBenchmarkFixture, HTTP_LatencyDistribution)
    ->Iterations(1000)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: Concurrent requests
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, HTTP_ConcurrentRequests)(benchmark::State& state)
{
    if (!g_server_ready)
    {
        state.SkipWithError("HTTP server not ready");
        return;
    }

    const int num_clients = state.range(0);
    std::string url = get_server_url("/");

    for (auto _ : state)
    {
        std::vector<std::future<bool>> futures;
        futures.reserve(num_clients);

        for (int i = 0; i < num_clients; ++i)
        {
            futures.push_back(std::async(std::launch::async, [url]() {
                auto client = std::make_shared<http_client>();
                auto response = client->get(url);
                return response.is_ok() && response.value().status_code == 200;
            }));
        }

        int successful = 0;
        for (auto& f : futures)
        {
            if (f.get())
            {
                successful++;
            }
        }

        benchmark::DoNotOptimize(successful);
    }

    state.SetItemsProcessed(state.iterations() * num_clients);
}
BENCHMARK_REGISTER_F(HttpBenchmarkFixture, HTTP_ConcurrentRequests)
    ->Arg(1)
    ->Arg(5)
    ->Arg(10)
    ->Arg(25)
    ->Arg(50)
    ->Unit(benchmark::kMillisecond);

// Benchmark: Request/Response cycle with headers
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, HTTP_RequestWithHeaders)(benchmark::State& state)
{
    if (!g_server_ready)
    {
        state.SkipWithError("HTTP server not ready");
        return;
    }

    auto client = std::make_shared<http_client>();
    std::string url = get_server_url("/");

    std::map<std::string, std::string> headers = {
        {"Accept", "application/json"},
        {"User-Agent", "NetworkSystem-Benchmark/1.0"},
        {"X-Request-ID", "benchmark-test"},
        {"Authorization", "Bearer test-token-12345"}
    };

    for (auto _ : state)
    {
        auto response = client->get(url, {}, headers);
        if (response.is_ok())
        {
            benchmark::DoNotOptimize(response.value().body);
        }
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK_REGISTER_F(HttpBenchmarkFixture, HTTP_RequestWithHeaders)
    ->Unit(benchmark::kMicrosecond)
    ->Iterations(500);

// Benchmark: Variable response size
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, HTTP_ResponseSize)(benchmark::State& state)
{
    if (!g_server_ready)
    {
        state.SkipWithError("HTTP server not ready");
        return;
    }

    const int response_size = state.range(0);
    auto client = std::make_shared<http_client>();
    std::string url = get_server_url("/size/" + std::to_string(response_size));

    for (auto _ : state)
    {
        auto response = client->get(url);
        if (response.is_ok() && response.value().status_code == 200)
        {
            benchmark::DoNotOptimize(response.value().body);
        }
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * response_size);
}
BENCHMARK_REGISTER_F(HttpBenchmarkFixture, HTTP_ResponseSize)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16384)
    ->Arg(65536)
    ->Unit(benchmark::kMicrosecond);

// Benchmark: Client creation overhead
static void BM_HTTP_ClientCreation(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto client = std::make_shared<http_client>();
        benchmark::DoNotOptimize(client);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HTTP_ClientCreation);

// Benchmark: Sequential requests (connection reuse pattern)
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, HTTP_SequentialRequests)(benchmark::State& state)
{
    if (!g_server_ready)
    {
        state.SkipWithError("HTTP server not ready");
        return;
    }

    const int batch_size = state.range(0);
    auto client = std::make_shared<http_client>();
    std::string url = get_server_url("/");

    for (auto _ : state)
    {
        int successful = 0;
        for (int i = 0; i < batch_size; ++i)
        {
            auto response = client->get(url);
            if (response.is_ok() && response.value().status_code == 200)
            {
                successful++;
            }
        }
        benchmark::DoNotOptimize(successful);
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK_REGISTER_F(HttpBenchmarkFixture, HTTP_SequentialRequests)
    ->Arg(5)
    ->Arg(10)
    ->Arg(25)
    ->Unit(benchmark::kMillisecond);

// NOTE: HTTP_ThroughputStress benchmark is disabled in CI because it requires
// a running HTTP server and can cause timeouts. Run manually for stress testing.
// To enable, define ENABLE_HTTP_STRESS_BENCHMARK before including this file.
#ifdef ENABLE_HTTP_STRESS_BENCHMARK
// Benchmark: HTTP throughput stress test
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, HTTP_ThroughputStress)(benchmark::State& state)
{
    if (!g_server_ready)
    {
        state.SkipWithError("HTTP server not ready");
        return;
    }

    const int num_threads = state.range(0);
    std::string url = get_server_url("/");
    std::atomic<int64_t> total_requests{0};

    for (auto _ : state)
    {
        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        std::atomic<bool> stop{false};

        auto start = std::chrono::high_resolution_clock::now();

        for (int t = 0; t < num_threads; ++t)
        {
            threads.emplace_back([url, &total_requests, &stop]() {
                auto client = std::make_shared<http_client>();

                while (!stop)
                {
                    auto response = client->get(url);
                    if (response.is_ok() && response.value().status_code == 200)
                    {
                        total_requests++;
                    }
                }
            });
        }

        // Run for 100ms
        std::this_thread::sleep_for(100ms);
        stop = true;

        for (auto& t : threads)
        {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (duration_ms > 0)
        {
            state.counters["rps"] = (total_requests * 1000.0) / duration_ms;
        }
    }
}
BENCHMARK_REGISTER_F(HttpBenchmarkFixture, HTTP_ThroughputStress)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Unit(benchmark::kMillisecond);
#endif  // ENABLE_HTTP_STRESS_BENCHMARK

// Cleanup function for main
void http_benchmark_cleanup()
{
    teardown_benchmark_server();
}
