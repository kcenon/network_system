# Network System - Performance Baseline Metrics

**English** | [한국어](BASELINE_KO.md)

**Version**: development (matches `VERSION`)
**Last Updated**: 2025-10-26 (Asia/Seoul)
**Phase**: Phase 0 - Foundation
**Status**: ⚠️ Synthetic microbenchmarks captured; real network baselines pending

---

## 📋 Overview

This document summarizes the CPU-only Google Benchmark results currently available for network_system. The measurements below cover message allocation/copy, mocked connection handling, and session bookkeeping; they do **not** include socket I/O, kernel-level networking, or TLS. Real end-to-end baselines will be added after the remaining roadmap items land.

**Primary Measurement Environment**: Intel i7-12700K (production reference)
**Secondary Environment**: Apple M1 (development testing)

---

## System Information

### Reference Hardware Configuration (Primary)
- **CPU**: Intel i7-12700K @ 3.8GHz (12 cores, 20 threads)
- **RAM**: 32 GB DDR4
- **Network**: Loopback (localhost testing)
- **OS**: Ubuntu 22.04 LTS
- **Compiler**: GCC 11.4 with -O3 optimization
- **Build Type**: Release
- **C++ Standard**: C++20

### Development Hardware Configuration (Secondary)
- **CPU**: Apple M1 (ARM64, 8 cores)
- **RAM**: 8 GB
- **Network**: Loopback (localhost testing)
- **OS**: macOS 26.1
- **Compiler**: Apple Clang 17.0.0
- **Build Type**: Release (-O3)
- **C++ Standard**: C++20

---

## Performance Metrics (Intel i7-12700K, Ubuntu 22.04, GCC 11 `-O3`)

The following table captures the CPU-only Google Benchmark results from `benchmarks/message_throughput_bench.cpp`. Each benchmark runs entirely in-process (allocating `std::vector<uint8_t>` buffers and copying them); no sockets are opened and no packets leave the host. Use these numbers to reason about serialization/copy overheads only.

| Benchmark | Payload | CPU time per op (ns) | Approx throughput | Scope |
|-----------|---------|----------------------|-------------------|-------|
| MessageThroughput/64B | 64 bytes | 1,300 | ~769,000 msg/s | In-memory allocation + memcpy |
| MessageThroughput/256B | 256 bytes | 3,270 | ~305,000 msg/s | In-memory allocation + memcpy |
| MessageThroughput/1KB | 1 KB | 7,803 | ~128,000 msg/s | In-memory allocation + memcpy |
| MessageThroughput/8KB | 8 KB | 48,000 | ~21,000 msg/s | In-memory allocation + memcpy |

Connection and session benchmarks (`benchmarks/connection_bench.cpp`, `benchmarks/session_bench.cpp`) currently use mock objects that emulate work (for example, by calling `std::this_thread::sleep_for`). Because they do not exercise real sockets or memory pools, previous claims about connection establishment time, memory consumption, or session overhead have been removed until representative measurements exist.

---

## Current Findings

- ✅ Google Benchmark suites compile and run with `NETWORK_BUILD_BENCHMARKS=ON`
- ✅ Synthetic serialization/memory-copy hot paths are measured and reproducible
- ✅ WebSocket protocol support (RFC 6455) implemented with client/server API
- ⚠️ Real network latency/throughput and memory baselines have not yet been captured (TCP, UDP, WebSocket)
- ⚠️ Zero-copy pipelines and connection pooling remain roadmap items (see `IMPROVEMENTS.md`)

---

## Baseline Validation

### Phase 0 Checklist
- [x] Benchmark infrastructure (`benchmarks/` + Google Benchmark) ✅
- [x] Synthetic serialization/copy metrics captured ✅
- [x] Real network throughput baseline ✅ (Phase 7)
- [x] Real latency distribution (P50/P95/P99) ✅ (Phase 7)
- [x] Memory footprint under load ✅ (Phase 7)
- [x] Concurrent connection stress baseline ✅ (Phase 7)

---

## Real Network Performance Metrics

**Status**: 🔄 Load testing infrastructure ready; awaiting initial baseline run
**Phase**: Phase 7 - Network Load Testing & Real Baseline Metrics

Unlike the synthetic CPU-only benchmarks above, the following metrics will measure actual TCP/UDP/WebSocket communication over the network stack, including:
- Real socket I/O and kernel network stack overhead
- Protocol framing and handshake costs
- End-to-end latency distributions (P50/P95/P99)
- Memory consumption during real network operations
- Concurrent connection handling capacity

### Load Test Infrastructure

The load test suite is implemented in `integration_tests/performance/` with the following components:

**Test Executables:**
- `tcp_load_test` - TCP throughput, latency, and concurrency tests
- `udp_load_test` - UDP throughput, packet loss, and burst performance
- `websocket_load_test` - WebSocket message throughput and ping/pong latency

**Measurement Framework:**
- `MemoryProfiler` - Cross-platform RSS/heap/VM measurement (Linux/macOS/Windows)
- `ResultWriter` - JSON/CSV output for CI integration
- `test_helpers::Statistics` - P50/P95/P99 latency percentile calculation

**Automation:**
- GitHub Actions workflow: `.github/workflows/network-load-tests.yml`
- Metrics collection: `scripts/collect_metrics.py`
- Baseline update: `scripts/update_baseline.py`

### Methodology

Each protocol is tested with:
- **Message sizes:** 64B, 512B, 1KB, 64KB (protocol-dependent)
- **Test iterations:** 1000 messages per test
- **Concurrent connections:** 10 and 50 clients
- **Measurement:** Throughput (msg/s), latency percentiles (P50/P95/P99), memory (RSS)
- **Environment:** Localhost loopback for consistency

### Running Load Tests

```bash
# Build with integration tests
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build -j

# Run individual protocol tests
./build/bin/integration_tests/tcp_load_test
./build/bin/integration_tests/udp_load_test
./build/bin/integration_tests/websocket_load_test

# Results are written to JSON files:
# tcp_64b_results.json, udp_64b_results.json, websocket_text_64b_results.json
```

### Automated Baseline Collection

The GitHub Actions workflow runs load tests weekly and can be manually triggered:

```bash
# Manual workflow trigger with baseline update
gh workflow run network-load-tests.yml --field update_baseline=true
```

Results are collected per platform (Ubuntu/macOS/Windows) and can be reviewed before merging into BASELINE.md.

### Expected Metrics (Placeholder)

Once the first baseline run completes, this section will contain a table like:

| Protocol   | Throughput   | Latency P50  | Latency P95  | Latency P99  | Memory (RSS) | Platform     |
|------------|--------------|--------------|--------------|--------------|--------------|--------------|
| TCP        | TBD msg/s    | TBD ms       | TBD ms       | TBD ms       | TBD MB       | ubuntu-22.04 |
| UDP        | TBD msg/s    | TBD ms       | TBD ms       | TBD ms       | TBD MB       | ubuntu-22.04 |
| WebSocket  | TBD msg/s    | TBD ms       | TBD ms       | TBD ms       | TBD MB       | ubuntu-22.04 |

These metrics will be updated automatically by the CI pipeline using `scripts/update_baseline.py`.

---

## 🔬 Reproducing These Measurements

All baseline metrics can be independently verified:

### Build Benchmarks

```bash
git clone https://github.com/kcenon/network_system.git
cd network_system

# Build with benchmarks enabled
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNETWORK_BUILD_BENCHMARKS=ON
cmake --build build -j
```

### Run Benchmarks

```bash
# Run all benchmarks
./build/benchmarks/network_benchmarks

# Generate JSON output
./build/benchmarks/network_benchmarks --benchmark_format=json --benchmark_out=baseline_results.json

# Run specific categories
./build/benchmarks/network_benchmarks --benchmark_filter=MessageThroughput
./build/benchmarks/network_benchmarks --benchmark_filter=Connection
./build/benchmarks/network_benchmarks --benchmark_filter=Session
```

### Expected Output (Intel i7-12700K)

```
-------------------------------------------------------------------------
Benchmark                               Time       CPU   Iterations
-------------------------------------------------------------------------
MessageThroughput/64B            1300 ns   1299 ns       538462   # ~769K msg/s
MessageThroughput/256B           3270 ns   3268 ns       214286   # ~305K msg/s
MessageThroughput/1KB            7803 ns   7801 ns        89744   # ~128K msg/s
MessageThroughput/8KB           48000 ns  47998 ns        14583   # ~21K msg/s
```

### Important Notes

- **Hardware dependency**: Results vary by CPU, RAM, and compiler flags
- **Scope**: Benchmarks run entirely on CPU; no socket I/O occurs
- **OS optimization**: Kernel/network tuning is irrelevant until real network runs are captured
- **Loopback testing**: Future network baselines should use consistent loopback or LAN setups
- **Reproducibility**: The synthetic measurements are deterministic on the same hardware

---

**Baseline Established**: 2025-10-26 (Asia/Seoul)
**Maintainer**: kcenon
**Document Status**: Synthetic CPU benchmarks recorded; WebSocket support added; real-world metrics pending
