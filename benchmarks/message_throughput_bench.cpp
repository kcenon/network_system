/**
 * @file message_throughput_bench.cpp
 * @brief Message throughput and processing benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include <vector>
#include <string>
#include <cstring>

// Simple message structure for benchmarking
struct message {
    std::vector<uint8_t> data;

    explicit message(size_t size) : data(size, 0) {}

    message(const std::string& str)
        : data(str.begin(), str.end()) {}
};

// Benchmark message creation
static void BM_Message_Create_Small(benchmark::State& state) {
    for (auto _ : state) {
        message msg(64);
        benchmark::DoNotOptimize(msg);
    }
    state.SetBytesProcessed(state.iterations() * 64);
}
BENCHMARK(BM_Message_Create_Small);

static void BM_Message_Create_Medium(benchmark::State& state) {
    for (auto _ : state) {
        message msg(1024);
        benchmark::DoNotOptimize(msg);
    }
    state.SetBytesProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_Message_Create_Medium);

static void BM_Message_Create_Large(benchmark::State& state) {
    for (auto _ : state) {
        message msg(65536);
        benchmark::DoNotOptimize(msg);
    }
    state.SetBytesProcessed(state.iterations() * 65536);
}
BENCHMARK(BM_Message_Create_Large);

// Benchmark message copying
static void BM_Message_Copy(benchmark::State& state) {
    int size = state.range(0);
    message original(size);

    for (auto _ : state) {
        message copy = original;
        benchmark::DoNotOptimize(copy);
    }

    state.SetBytesProcessed(state.iterations() * size);
}
BENCHMARK(BM_Message_Copy)->Arg(64)->Arg(1024)->Arg(4096)->Arg(65536);

// Benchmark message move
static void BM_Message_Move(benchmark::State& state) {
    int size = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        message original(size);
        state.ResumeTiming();

        message moved = std::move(original);
        benchmark::DoNotOptimize(moved);
    }

    state.SetBytesProcessed(state.iterations() * size);
}
BENCHMARK(BM_Message_Move)->Arg(64)->Arg(1024)->Arg(4096)->Arg(65536);

// Benchmark message serialization (simple memcpy)
static void BM_Message_Serialize(benchmark::State& state) {
    int size = state.range(0);
    message msg(size);
    std::vector<uint8_t> buffer(size);

    for (auto _ : state) {
        std::memcpy(buffer.data(), msg.data.data(), size);
        benchmark::ClobberMemory();
    }

    state.SetBytesProcessed(state.iterations() * size);
}
BENCHMARK(BM_Message_Serialize)->Arg(64)->Arg(1024)->Arg(4096)->Arg(65536);

// Benchmark message deserialization
static void BM_Message_Deserialize(benchmark::State& state) {
    int size = state.range(0);
    std::vector<uint8_t> buffer(size, 0x42);

    for (auto _ : state) {
        message msg(size);
        std::memcpy(msg.data.data(), buffer.data(), size);
        benchmark::DoNotOptimize(msg);
    }

    state.SetBytesProcessed(state.iterations() * size);
}
BENCHMARK(BM_Message_Deserialize)->Arg(64)->Arg(1024)->Arg(4096)->Arg(65536);

// Benchmark message queue operations
static void BM_MessageQueue_PushPop(benchmark::State& state) {
    std::vector<message> queue;
    queue.reserve(1000);

    for (auto _ : state) {
        queue.emplace_back(1024);
        if (!queue.empty()) {
            queue.pop_back();
        }
    }
}
BENCHMARK(BM_MessageQueue_PushPop);

// Benchmark burst message processing
static void BM_Message_BurstProcessing(benchmark::State& state) {
    int burst_size = state.range(0);
    std::vector<message> messages;
    messages.reserve(burst_size);

    for (auto _ : state) {
        state.PauseTiming();
        messages.clear();
        for (int i = 0; i < burst_size; ++i) {
            messages.emplace_back(1024);
        }
        state.ResumeTiming();

        // Simulate processing
        for (auto& msg : messages) {
            benchmark::DoNotOptimize(msg.data.data());
        }
    }

    state.SetItemsProcessed(state.iterations() * burst_size);
    state.SetBytesProcessed(state.iterations() * burst_size * 1024);
}
BENCHMARK(BM_Message_BurstProcessing)->Arg(10)->Arg(100)->Arg(1000);

// Benchmark sustained throughput
static void BM_Message_SustainedThroughput(benchmark::State& state) {
    message msg(1024);
    std::vector<uint8_t> buffer(1024);

    for (auto _ : state) {
        // Simulate send operation
        std::memcpy(buffer.data(), msg.data.data(), 1024);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_Message_SustainedThroughput)->UseRealTime();

// Benchmark concurrent message processing
static void BM_Message_ConcurrentProcessing(benchmark::State& state) {
    message msg(1024);

    for (auto _ : state) {
        // Simulate processing work
        int sum = 0;
        for (size_t i = 0; i < msg.data.size(); i += 64) {
            sum += msg.data[i];
        }
        benchmark::DoNotOptimize(sum);
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_Message_ConcurrentProcessing)->Threads(1)->Threads(4)->Threads(8);
