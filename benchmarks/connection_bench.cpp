/**
 * @file connection_bench.cpp
 * @brief Connection establishment and management benchmarks
 * Phase 0, Task 0.2: Baseline Performance Benchmarking
 */

#include <benchmark/benchmark.h>
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>

// Mock connection class for benchmarking
class mock_connection {
public:
    mock_connection() : connected_(false), id_(next_id_++) {}

    bool connect(const std::string& host, int port) {
        // Simulate connection overhead
        std::this_thread::sleep_for(std::chrono::microseconds(10));
        connected_ = true;
        host_ = host;
        port_ = port;
        return true;
    }

    bool disconnect() {
        connected_ = false;
        return true;
    }

    bool is_connected() const { return connected_; }

    bool send(const std::vector<uint8_t>& data) {
        if (!connected_) return false;
        bytes_sent_ += data.size();
        return true;
    }

    size_t get_bytes_sent() const { return bytes_sent_; }

private:
    bool connected_;
    std::string host_;
    int port_;
    size_t bytes_sent_ = 0;
    int id_;
    static std::atomic<int> next_id_;
};

std::atomic<int> mock_connection::next_id_{0};

// Benchmark connection creation
static void BM_Connection_Create(benchmark::State& state) {
    for (auto _ : state) {
        mock_connection conn;
        benchmark::DoNotOptimize(conn);
    }
}
BENCHMARK(BM_Connection_Create);

// Benchmark connection establishment
static void BM_Connection_Connect(benchmark::State& state) {
    for (auto _ : state) {
        mock_connection conn;
        bool result = conn.connect("localhost", 8080);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_Connection_Connect);

// Benchmark connection lifecycle (connect + disconnect)
static void BM_Connection_Lifecycle(benchmark::State& state) {
    for (auto _ : state) {
        mock_connection conn;
        conn.connect("localhost", 8080);
        conn.disconnect();
    }
}
BENCHMARK(BM_Connection_Lifecycle);

// Benchmark connection pool - create multiple connections
static void BM_ConnectionPool_Create(benchmark::State& state) {
    int pool_size = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<std::unique_ptr<mock_connection>> pool;
        pool.reserve(pool_size);
        state.ResumeTiming();

        for (int i = 0; i < pool_size; ++i) {
            pool.push_back(std::make_unique<mock_connection>());
        }

        benchmark::DoNotOptimize(pool);
    }
}
BENCHMARK(BM_ConnectionPool_Create)->Arg(5)->Arg(10)->Arg(20)->Arg(50);

// Benchmark connection pool - connect all
static void BM_ConnectionPool_ConnectAll(benchmark::State& state) {
    int pool_size = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<std::unique_ptr<mock_connection>> pool;
        for (int i = 0; i < pool_size; ++i) {
            pool.push_back(std::make_unique<mock_connection>());
        }
        state.ResumeTiming();

        for (auto& conn : pool) {
            conn->connect("localhost", 8080);
        }

        benchmark::DoNotOptimize(pool);
    }

    state.SetItemsProcessed(state.iterations() * pool_size);
}
BENCHMARK(BM_ConnectionPool_ConnectAll)->Arg(5)->Arg(10)->Arg(20)->Arg(50);

// Benchmark connection send operation
static void BM_Connection_Send(benchmark::State& state) {
    int message_size = state.range(0);
    std::vector<uint8_t> message(message_size, 0x42);

    mock_connection conn;
    conn.connect("localhost", 8080);

    for (auto _ : state) {
        conn.send(message);
    }

    state.SetBytesProcessed(state.iterations() * message_size);
}
BENCHMARK(BM_Connection_Send)->Arg(64)->Arg(1024)->Arg(4096)->Arg(65536);

// Benchmark burst send
static void BM_Connection_BurstSend(benchmark::State& state) {
    int burst_size = state.range(0);
    std::vector<uint8_t> message(1024, 0x42);

    mock_connection conn;
    conn.connect("localhost", 8080);

    for (auto _ : state) {
        for (int i = 0; i < burst_size; ++i) {
            conn.send(message);
        }
    }

    state.SetItemsProcessed(state.iterations() * burst_size);
    state.SetBytesProcessed(state.iterations() * burst_size * 1024);
}
BENCHMARK(BM_Connection_BurstSend)->Arg(10)->Arg(100)->Arg(1000);

// Benchmark connection state check
static void BM_Connection_StateCheck(benchmark::State& state) {
    mock_connection conn;
    conn.connect("localhost", 8080);

    for (auto _ : state) {
        bool is_connected = conn.is_connected();
        benchmark::DoNotOptimize(is_connected);
    }
}
BENCHMARK(BM_Connection_StateCheck);

// Benchmark concurrent connection operations
static void BM_Connection_Concurrent(benchmark::State& state) {
    for (auto _ : state) {
        mock_connection conn;
        conn.connect("localhost", 8080);
        std::vector<uint8_t> message(1024, 0x42);
        conn.send(message);
    }
}
BENCHMARK(BM_Connection_Concurrent)->Threads(4)->Threads(8)->Threads(16);

// Benchmark connection reuse
static void BM_Connection_Reuse(benchmark::State& state) {
    mock_connection conn;
    conn.connect("localhost", 8080);
    std::vector<uint8_t> message(1024, 0x42);

    for (auto _ : state) {
        conn.send(message);
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * 1024);
}
BENCHMARK(BM_Connection_Reuse);
