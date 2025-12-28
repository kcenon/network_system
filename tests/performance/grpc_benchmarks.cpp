/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

#include <benchmark/benchmark.h>
#include <kcenon/network/protocols/grpc/frame.h>
#include <kcenon/network/protocols/grpc/status.h>
#include <kcenon/network/protocols/grpc/service_registry.h>

#include <numeric>
#include <random>
#include <vector>

namespace grpc = kcenon::network::protocols::grpc;

// ============================================================================
// Message Serialization Benchmarks
// ============================================================================

static void BM_GrpcMessageSerialize_Small(benchmark::State& state)
{
    std::vector<uint8_t> data(64);  // 64 bytes
    std::iota(data.begin(), data.end(), 0);
    grpc::grpc_message msg(data, false);

    for (auto _ : state)
    {
        auto serialized = msg.serialize();
        benchmark::DoNotOptimize(serialized);
    }

    state.SetBytesProcessed(state.iterations() * 64);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GrpcMessageSerialize_Small);

static void BM_GrpcMessageSerialize_Medium(benchmark::State& state)
{
    std::vector<uint8_t> data(1024);  // 1 KB
    std::iota(data.begin(), data.end(), 0);
    grpc::grpc_message msg(data, false);

    for (auto _ : state)
    {
        auto serialized = msg.serialize();
        benchmark::DoNotOptimize(serialized);
    }

    state.SetBytesProcessed(state.iterations() * 1024);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GrpcMessageSerialize_Medium);

static void BM_GrpcMessageSerialize_Large(benchmark::State& state)
{
    std::vector<uint8_t> data(64 * 1024);  // 64 KB
    std::iota(data.begin(), data.end(), 0);
    grpc::grpc_message msg(data, false);

    for (auto _ : state)
    {
        auto serialized = msg.serialize();
        benchmark::DoNotOptimize(serialized);
    }

    state.SetBytesProcessed(state.iterations() * 64 * 1024);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GrpcMessageSerialize_Large);

// ============================================================================
// Message Parsing Benchmarks
// ============================================================================

static void BM_GrpcMessageParse_Small(benchmark::State& state)
{
    std::vector<uint8_t> data(64);
    std::iota(data.begin(), data.end(), 0);
    grpc::grpc_message msg(data, false);
    auto serialized = msg.serialize();

    for (auto _ : state)
    {
        auto result = grpc::grpc_message::parse(serialized);
        benchmark::DoNotOptimize(result);
    }

    state.SetBytesProcessed(state.iterations() * 64);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GrpcMessageParse_Small);

static void BM_GrpcMessageParse_Medium(benchmark::State& state)
{
    std::vector<uint8_t> data(1024);
    std::iota(data.begin(), data.end(), 0);
    grpc::grpc_message msg(data, false);
    auto serialized = msg.serialize();

    for (auto _ : state)
    {
        auto result = grpc::grpc_message::parse(serialized);
        benchmark::DoNotOptimize(result);
    }

    state.SetBytesProcessed(state.iterations() * 1024);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GrpcMessageParse_Medium);

static void BM_GrpcMessageParse_Large(benchmark::State& state)
{
    std::vector<uint8_t> data(64 * 1024);
    std::iota(data.begin(), data.end(), 0);
    grpc::grpc_message msg(data, false);
    auto serialized = msg.serialize();

    for (auto _ : state)
    {
        auto result = grpc::grpc_message::parse(serialized);
        benchmark::DoNotOptimize(result);
    }

    state.SetBytesProcessed(state.iterations() * 64 * 1024);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GrpcMessageParse_Large);

// ============================================================================
// Round-Trip Benchmarks
// ============================================================================

static void BM_GrpcMessageRoundTrip(benchmark::State& state)
{
    const size_t size = state.range(0);
    std::vector<uint8_t> data(size);
    std::iota(data.begin(), data.end(), 0);

    for (auto _ : state)
    {
        grpc::grpc_message msg(data, false);
        auto serialized = msg.serialize();
        auto result = grpc::grpc_message::parse(serialized);
        benchmark::DoNotOptimize(result);
    }

    state.SetBytesProcessed(state.iterations() * size);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GrpcMessageRoundTrip)
    ->Arg(64)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16384)
    ->Arg(65536);

// ============================================================================
// Service Registry Benchmarks
// ============================================================================

static void BM_ServiceRegistryLookup(benchmark::State& state)
{
    grpc::service_registry registry;

    // Create and register services
    std::vector<std::unique_ptr<grpc::generic_service>> services;
    for (int i = 0; i < 100; ++i)
    {
        auto service = std::make_unique<grpc::generic_service>(
            "benchmark.Service" + std::to_string(i));

        auto handler = [](grpc::server_context& ctx,
                          const std::vector<uint8_t>& req)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
            return {grpc::grpc_status::ok_status(), {}};
        };
        service->register_unary_method("Method", handler);

        registry.register_service(service.get());
        services.push_back(std::move(service));
    }

    std::string lookup_name = "benchmark.Service50";

    for (auto _ : state)
    {
        auto* found = registry.find_service(lookup_name);
        benchmark::DoNotOptimize(found);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ServiceRegistryLookup);

static void BM_MethodPathLookup(benchmark::State& state)
{
    grpc::service_registry registry;

    // Create and register a service with multiple methods
    grpc::generic_service service("benchmark.TestService");

    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& req)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };

    for (int i = 0; i < 50; ++i)
    {
        service.register_unary_method("Method" + std::to_string(i), handler);
    }

    registry.register_service(&service);

    std::string method_path = "/benchmark.TestService/Method25";

    for (auto _ : state)
    {
        auto result = registry.find_method(method_path);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MethodPathLookup);

// ============================================================================
// Method Path Parsing Benchmarks
// ============================================================================

static void BM_ParseMethodPath(benchmark::State& state)
{
    std::string path = "/com.example.api.v1.UserService/GetUserProfile";

    for (auto _ : state)
    {
        auto result = grpc::parse_method_path(path);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ParseMethodPath);

static void BM_BuildMethodPath(benchmark::State& state)
{
    std::string service = "com.example.api.v1.UserService";
    std::string method = "GetUserProfile";

    for (auto _ : state)
    {
        auto result = grpc::build_method_path(service, method);
        benchmark::DoNotOptimize(result);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_BuildMethodPath);

// ============================================================================
// Timeout Parsing Benchmarks
// ============================================================================

static void BM_ParseTimeout(benchmark::State& state)
{
    std::vector<std::string> timeouts = {"100m", "1S", "30S", "1M", "1H"};
    size_t idx = 0;

    for (auto _ : state)
    {
        auto result = grpc::parse_timeout(timeouts[idx % timeouts.size()]);
        benchmark::DoNotOptimize(result);
        ++idx;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_ParseTimeout);

static void BM_FormatTimeout(benchmark::State& state)
{
    std::vector<uint64_t> values = {100, 1000, 30000, 60000, 3600000};
    size_t idx = 0;

    for (auto _ : state)
    {
        auto result = grpc::format_timeout(values[idx % values.size()]);
        benchmark::DoNotOptimize(result);
        ++idx;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_FormatTimeout);

// ============================================================================
// Status Creation Benchmarks
// ============================================================================

static void BM_StatusCreation_Ok(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto status = grpc::grpc_status::ok_status();
        benchmark::DoNotOptimize(status);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_StatusCreation_Ok);

static void BM_StatusCreation_Error(benchmark::State& state)
{
    for (auto _ : state)
    {
        auto status = grpc::grpc_status::error_status(
            grpc::status_code::internal,
            "Internal server error occurred during processing");
        benchmark::DoNotOptimize(status);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_StatusCreation_Error);

// ============================================================================
// Health Service Benchmarks
// ============================================================================

static void BM_HealthServiceSetStatus(benchmark::State& state)
{
    grpc::health_service health;
    size_t idx = 0;

    for (auto _ : state)
    {
        std::string name = "service." + std::to_string(idx % 100);
        auto status = (idx % 2 == 0) ? grpc::health_status::serving
                                     : grpc::health_status::not_serving;
        health.set_status(name, status);
        ++idx;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HealthServiceSetStatus);

static void BM_HealthServiceGetStatus(benchmark::State& state)
{
    grpc::health_service health;

    // Pre-populate
    for (int i = 0; i < 100; ++i)
    {
        health.set_status("service." + std::to_string(i),
                          grpc::health_status::serving);
    }

    size_t idx = 0;

    for (auto _ : state)
    {
        std::string name = "service." + std::to_string(idx % 100);
        auto result = health.get_status(name);
        benchmark::DoNotOptimize(result);
        ++idx;
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_HealthServiceGetStatus);

// ============================================================================
// Generic Service Method Registration Benchmark
// ============================================================================

static void BM_GenericServiceMethodRegistration(benchmark::State& state)
{
    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& req)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };

    for (auto _ : state)
    {
        state.PauseTiming();
        grpc::generic_service service("benchmark.Service");
        state.ResumeTiming();

        for (int i = 0; i < 10; ++i)
        {
            service.register_unary_method("Method" + std::to_string(i), handler);
        }

        benchmark::DoNotOptimize(service);
    }

    state.SetItemsProcessed(state.iterations() * 10);
}
BENCHMARK(BM_GenericServiceMethodRegistration);

// ============================================================================
// Handler Retrieval Benchmarks
// ============================================================================

static void BM_GetUnaryHandler(benchmark::State& state)
{
    grpc::generic_service service("benchmark.Service");

    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& req)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };

    for (int i = 0; i < 50; ++i)
    {
        service.register_unary_method("Method" + std::to_string(i), handler);
    }

    for (auto _ : state)
    {
        auto* h = service.get_unary_handler("Method25");
        benchmark::DoNotOptimize(h);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_GetUnaryHandler);

BENCHMARK_MAIN();
