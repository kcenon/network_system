# network_system Performance Benchmarks

> **Language:** **English** | [한국어](README_KO.md)

Phase 0, Task 0.2: Baseline Performance Benchmarking

## Overview

This directory contains comprehensive performance benchmarks for the network_system, measuring:

- **Message Throughput**: Message creation, serialization, and processing performance
- **Connection Management**: Connection establishment, lifecycle, and pool management
- **Session Management**: Session creation, lookup, and cleanup performance
- **TCP Receive Dispatch**: std::span vs std::vector receive callback overhead comparison

## Building

### Prerequisites

```bash
# macOS (Homebrew)
brew install google-benchmark

# Ubuntu/Debian
sudo apt-get install libbenchmark-dev

# From source
git clone https://github.com/google/benchmark.git
cd benchmark
cmake -E make_directory build
cmake -E chdir build cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on -DCMAKE_BUILD_TYPE=Release ../
cmake --build build --config Release
sudo cmake --build build --config Release --target install
```

### Build Benchmarks

```bash
cd network_system
cmake -B build -S . -DNETWORK_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Or use the build target
cd build
make network_benchmarks
```

## Running Benchmarks

### Run All Benchmarks

```bash
./build/benchmarks/network_benchmarks
```

### Run Specific Benchmark Categories

```bash
# Message throughput benchmarks only
./build/benchmarks/network_benchmarks --benchmark_filter=Message

# Connection benchmarks only
./build/benchmarks/network_benchmarks --benchmark_filter=Connection

# Session benchmarks only
./build/benchmarks/network_benchmarks --benchmark_filter=Session

# TCP receive dispatch benchmarks only
./build/benchmarks/network_benchmarks --benchmark_filter=TcpReceive
```

### Output Formats

```bash
# Console output (default)
./build/benchmarks/network_benchmarks

# JSON output
./build/benchmarks/network_benchmarks --benchmark_format=json

# CSV output
./build/benchmarks/network_benchmarks --benchmark_format=csv

# Save to file
./build/benchmarks/network_benchmarks --benchmark_format=json --benchmark_out=results.json
```

### Advanced Options

```bash
# Run for minimum time (for stable results)
./build/benchmarks/network_benchmarks --benchmark_min_time=5.0

# Specify number of iterations
./build/benchmarks/network_benchmarks --benchmark_repetitions=10

# Show all statistics
./build/benchmarks/network_benchmarks --benchmark_report_aggregates_only=false
```

## Benchmark Categories

### 1. Message Throughput Benchmarks

**File**: `message_throughput_bench.cpp`

Measures message processing performance:

- Message creation (small 64B, medium 1KB, large 64KB)
- Message copying and moving (64B to 64KB)
- Message serialization/deserialization
- Message queue operations
- Burst message processing (10, 100, 1000 messages)
- Sustained throughput measurement
- Concurrent message processing (1, 4, 8 threads)

**Target Metrics**:
- Message creation (1KB): < 1μs
- Message copy (1KB): < 500ns
- Message move: < 100ns
- Serialization throughput: > 1 GB/s
- Sustained throughput: > 100k messages/sec

### 2. Connection Benchmarks

**File**: `connection_bench.cpp`

Measures connection management performance:

- Connection creation overhead
- Connection establishment latency
- Connection lifecycle (connect + disconnect)
- Connection pool creation (5, 10, 20, 50 connections)
- Connection pool connect-all operations
- Send operations (64B to 64KB messages)
- Burst send (10, 100, 1000 messages)
- Connection state checks
- Concurrent connection operations (4, 8, 16 threads)
- Connection reuse efficiency

**Target Metrics**:
- Connection creation: < 1μs
- Connection establishment: < 100μs
- Send latency (1KB): < 10μs
- Connection pool (20 conns): < 1ms
- Concurrent scalability: linear up to 8 threads

### 3. Session Management Benchmarks

**File**: `session_bench.cpp`

Measures session management performance:

- Session creation
- Session create/destroy cycle
- Session data storage and retrieval
- Session lookup
- Many sessions (10, 100, 1000)
- Session lookup with many sessions
- Session cleanup/expiration
- Session activity updates
- Concurrent session access (4, 8, 16 threads)

**Target Metrics**:
- Session creation: < 1μs
- Session lookup: < 100ns
- Data storage: < 500ns
- Cleanup (100 sessions): < 100μs
- Concurrent lookup: lock-free or minimal contention

### 4. TCP Receive Dispatch Benchmarks

**File**: `tcp_receive_bench.cpp`

Measures the overhead difference between span-based and vector-based receive dispatch:

- Span dispatch (zero allocation, 64B to 64KB)
- Vector fallback (per-iteration allocation, 64B to 64KB)
- Multi-callback span sharing (3 handlers)
- Multi-callback vector copying (3 handlers)
- Subspan operations (header/payload parsing)
- Vector slice operations (legacy pattern)

**Target Metrics**:
- Span dispatch: 10-50x faster than vector fallback
- Span 64B: < 1ns
- Span 64KB: < 300ns
- No per-read allocation with span path
- Efficient subspan operations for protocol parsing

## Baseline Results

**To be measured**: Run benchmarks and record results in `docs/BASELINE.md`

Expected baseline ranges (to be confirmed):

| Metric | Target | Acceptable |
|--------|--------|------------|
| Message Creation (1KB) | < 1μs | < 10μs |
| Message Serialization (1KB) | < 500ns | < 5μs |
| Connection Creation | < 1μs | < 10μs |
| Connection Send (1KB) | < 10μs | < 100μs |
| Session Creation | < 1μs | < 10μs |
| Session Lookup | < 100ns | < 1μs |
| Throughput (1KB messages) | > 100k/s | > 50k/s |

## Interpreting Results

### Understanding Benchmark Output

```
---------------------------------------------------------------
Benchmark                         Time           CPU Iterations
---------------------------------------------------------------
BM_Message_Create_Medium        456 ns        455 ns    1534891
```

- **Time**: Wall clock time per iteration
- **CPU**: CPU time per iteration
- **Iterations**: Number of times the benchmark was run

### Throughput Metrics

Many benchmarks report throughput:

```
BM_Message_Throughput    1.23 GB/s    100k items/s
```

- **Bytes/s**: Data processing rate
- **Items/s**: Message/operation processing rate

### Comparison

To compare before/after performance:

```bash
# Baseline
./build/benchmarks/network_benchmarks --benchmark_out=baseline.json --benchmark_out_format=json

# After changes
./build/benchmarks/network_benchmarks --benchmark_out=after.json --benchmark_out_format=json

# Compare (requires benchmark tools)
compare.py baseline.json after.json
```

## CI Integration

Benchmarks are run in CI on every PR to detect performance regressions.

See `.github/workflows/benchmarks.yml` for configuration.

## Troubleshooting

### Google Benchmark Not Found

```bash
# Check installation
find /usr -name "*benchmark*" 2>/dev/null

# Try pkg-config
pkg-config --modversion benchmark

# Reinstall
brew reinstall google-benchmark  # macOS
```

### Build Errors

```bash
# Clean build
rm -rf build
cmake -B build -S . -DNETWORK_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

### Unstable Results

```bash
# Increase minimum time
./build/benchmarks/network_benchmarks --benchmark_min_time=10.0

# Disable CPU frequency scaling (Linux)
sudo cpupower frequency-set --governor performance
```

## Contributing

When adding new benchmarks:

1. Follow existing naming conventions (`BM_Category_SpecificTest`)
2. Use appropriate benchmark types (Fixture, Threaded, etc.)
3. Set meaningful labels and counters (SetBytesProcessed, SetItemsProcessed)
4. Document target metrics in file header
5. Clean up resources in TearDown/after benchmark
6. Update this README with new benchmark description

## References

- [Google Benchmark Documentation](https://github.com/google/benchmark)
- [Benchmark Best Practices](https://github.com/google/benchmark/blob/main/docs/user_guide.md)
- [network_system Architecture](../README.md)

---

**Last Updated**: 2025-10-07
**Phase**: 0 - Foundation and Tooling
**Status**: Baseline measurement in progress
