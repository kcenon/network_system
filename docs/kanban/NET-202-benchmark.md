# NET-202: Reactivate HTTP Performance Benchmarks

| Field | Value |
|-------|-------|
| **ID** | NET-202 |
| **Title** | Reactivate HTTP Performance Benchmarks |
| **Category** | TEST |
| **Priority** | MEDIUM |
| **Status** | TODO |
| **Est. Duration** | 3-4 days |
| **Dependencies** | None |
| **Target Version** | v1.8.0 |

---

## What (무엇을 바꾸려는가)

### Current Problem
HTTP 성능 벤치마크가 비활성화되어 있거나 오래되었습니다:
- 벤치마크 코드가 주석 처리됨
- 기존 벤치마크가 현재 API와 호환되지 않음
- 성능 베이스라인이 문서화되지 않음
- CI에서 성능 회귀 감지 불가

### Goal
HTTP 서버 성능 벤치마크 재활성화 및 현대화:
- 현재 API에 맞게 벤치마크 업데이트
- 표준 벤치마크 도구와 비교
- 성능 베이스라인 문서화
- CI 통합

### Metrics to Measure
1. **Throughput** - Requests per second (RPS)
2. **Latency** - P50, P95, P99, P999
3. **Concurrency** - 동시 연결 처리 능력
4. **Memory** - 연결당 메모리 사용량

---

## How (어떻게 바꾸려는가)

### Implementation Plan

#### Step 1: Update Benchmark Code
```cpp
// benchmarks/http_benchmark.cpp

#include <benchmark/benchmark.h>
#include "network_system/protocols/http_server.h"
#include "network_system/protocols/http_client.h"

class HttpBenchmarkFixture : public benchmark::Fixture {
public:
    void SetUp(benchmark::State& state) override {
        server_ = std::make_unique<http_server>();
        server_->route("GET", "/", [](const auto& req) {
            return http_response::ok("Hello, World!");
        });
        server_->start(0);  // Random available port
        port_ = server_->port();
    }

    void TearDown(benchmark::State& state) override {
        server_->stop();
        server_.reset();
    }

protected:
    std::unique_ptr<http_server> server_;
    uint16_t port_;
};

BENCHMARK_DEFINE_F(HttpBenchmarkFixture, SimpleGet)(benchmark::State& state) {
    http_client client;
    client.connect("localhost", port_);

    for (auto _ : state) {
        auto response = client.get("/");
        benchmark::DoNotOptimize(response);
    }

    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_DEFINE_F(HttpBenchmarkFixture, ConcurrentRequests)(benchmark::State& state) {
    const int num_clients = state.range(0);
    std::vector<std::unique_ptr<http_client>> clients;

    for (int i = 0; i < num_clients; ++i) {
        auto client = std::make_unique<http_client>();
        client->connect("localhost", port_);
        clients.push_back(std::move(client));
    }

    for (auto _ : state) {
        std::vector<std::future<http_response>> futures;
        for (auto& client : clients) {
            futures.push_back(std::async(std::launch::async, [&]() {
                return client->get("/");
            }));
        }
        for (auto& f : futures) {
            benchmark::DoNotOptimize(f.get());
        }
    }

    state.SetItemsProcessed(state.iterations() * num_clients);
}

BENCHMARK_REGISTER_F(HttpBenchmarkFixture, SimpleGet)->Unit(benchmark::kMicrosecond);
BENCHMARK_REGISTER_F(HttpBenchmarkFixture, ConcurrentRequests)
    ->Arg(10)->Arg(50)->Arg(100)->Arg(500)
    ->Unit(benchmark::kMicrosecond);
```

#### Step 2: Add Latency Distribution Benchmark
```cpp
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, LatencyDistribution)(benchmark::State& state) {
    http_client client;
    client.connect("localhost", port_);

    std::vector<double> latencies;
    latencies.reserve(10000);

    for (auto _ : state) {
        auto start = std::chrono::high_resolution_clock::now();
        auto response = client.get("/");
        auto end = std::chrono::high_resolution_clock::now();

        double latency_us = std::chrono::duration<double, std::micro>(end - start).count();
        latencies.push_back(latency_us);
        benchmark::DoNotOptimize(response);
    }

    // Calculate percentiles
    std::sort(latencies.begin(), latencies.end());
    state.counters["p50"] = latencies[latencies.size() * 0.50];
    state.counters["p95"] = latencies[latencies.size() * 0.95];
    state.counters["p99"] = latencies[latencies.size() * 0.99];
    state.counters["p999"] = latencies[latencies.size() * 0.999];
}

BENCHMARK_REGISTER_F(HttpBenchmarkFixture, LatencyDistribution)
    ->Iterations(10000)
    ->Unit(benchmark::kMicrosecond);
```

#### Step 3: Add Memory Benchmark
```cpp
BENCHMARK_DEFINE_F(HttpBenchmarkFixture, MemoryPerConnection)(benchmark::State& state) {
    const int num_connections = state.range(0);

    auto baseline_memory = get_current_memory_usage();

    std::vector<std::unique_ptr<http_client>> clients;
    for (int i = 0; i < num_connections; ++i) {
        auto client = std::make_unique<http_client>();
        client->connect("localhost", port_);
        clients.push_back(std::move(client));
    }

    auto peak_memory = get_current_memory_usage();
    auto memory_per_conn = (peak_memory - baseline_memory) / num_connections;

    state.counters["memory_per_conn_bytes"] = memory_per_conn;
    state.counters["total_connections"] = num_connections;

    for (auto _ : state) {
        // Measure request handling memory
        for (auto& client : clients) {
            benchmark::DoNotOptimize(client->get("/"));
        }
    }
}

BENCHMARK_REGISTER_F(HttpBenchmarkFixture, MemoryPerConnection)
    ->Arg(100)->Arg(1000)->Arg(5000)
    ->Unit(benchmark::kMillisecond);
```

#### Step 4: Create Comparison Script
```bash
#!/bin/bash
# scripts/run_http_benchmark.sh

echo "=== Network System HTTP Benchmark ==="
echo "Date: $(date)"
echo ""

# Build benchmarks
cmake --build build --target http_benchmark

# Run internal benchmarks
echo "--- Internal Benchmarks ---"
./build/benchmarks/http_benchmark \
    --benchmark_format=json \
    --benchmark_out=benchmark_results.json

# Compare with wrk (if available)
if command -v wrk &> /dev/null; then
    echo ""
    echo "--- Comparison with wrk ---"
    ./build/samples/simple_http_server &
    SERVER_PID=$!
    sleep 1

    wrk -t4 -c100 -d10s http://localhost:8080/

    kill $SERVER_PID
fi

# Generate report
python3 scripts/generate_benchmark_report.py benchmark_results.json
```

#### Step 5: Update BASELINE.md
```markdown
# HTTP Performance Baseline

## Environment
- CPU: Apple M1 Pro
- Memory: 16GB
- OS: macOS 14.0
- Compiler: Apple Clang 15.0

## Results (2025-01-XX)

| Metric | Value | Notes |
|--------|-------|-------|
| Simple GET RPS | 150,000+ | Single connection |
| Concurrent RPS (100 clients) | 300,000+ | 100 concurrent connections |
| P50 Latency | 15 μs | |
| P95 Latency | 35 μs | |
| P99 Latency | 80 μs | |
| Memory per Connection | ~2KB | Idle connection |
| Max Concurrent Connections | 10,000+ | Limited by file descriptors |
```

---

## Test (어떻게 테스트하는가)

### Running Benchmarks
```bash
# Build with Release mode
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target http_benchmark

# Run benchmarks
./build/benchmarks/http_benchmark

# Run with specific filter
./build/benchmarks/http_benchmark --benchmark_filter="SimpleGet"

# Output to JSON
./build/benchmarks/http_benchmark --benchmark_out=results.json --benchmark_out_format=json
```

### CI Integration
```yaml
# .github/workflows/benchmark.yml
benchmark:
  runs-on: ubuntu-latest
  steps:
    - uses: actions/checkout@v4
    - name: Build
      run: |
        cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build --target http_benchmark
    - name: Run Benchmarks
      run: ./build/benchmarks/http_benchmark --benchmark_out=results.json
    - name: Compare with Baseline
      run: python3 scripts/compare_benchmark.py baseline.json results.json
```

### Manual Verification
1. Release 빌드에서 벤치마크 실행
2. `wrk` 또는 `ab`와 결과 비교
3. 여러 번 실행하여 결과 일관성 확인

---

## Acceptance Criteria

- [ ] 모든 벤치마크 코드 현재 API와 호환
- [ ] Google Benchmark 프레임워크 사용
- [ ] Throughput, Latency, Memory 벤치마크 포함
- [ ] 베이스라인 문서 업데이트
- [ ] CI 워크플로우 추가
- [ ] 외부 도구(wrk)와 비교 스크립트

---

## Notes

- 벤치마크는 Release 빌드에서만 의미 있음
- 결과는 하드웨어에 따라 크게 달라질 수 있음
- CI에서는 상대적 성능 비교에 집중
