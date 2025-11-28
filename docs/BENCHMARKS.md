# Network System Performance Benchmarks

**Last Updated**: 2025-11-15

This document provides comprehensive performance metrics, benchmarking methodologies, and load testing results for the network system.

---

## Table of Contents

- [Overview](#overview)
- [Benchmark Types](#benchmark-types)
- [Synthetic Benchmarks](#synthetic-benchmarks)
- [Real Network Load Tests](#real-network-load-tests)
- [Benchmark Methodology](#benchmark-methodology)
- [Performance Comparison](#performance-comparison)
- [Reproducing Benchmarks](#reproducing-benchmarks)

---

## Overview

The network system provides two types of performance benchmarks:

1. **Synthetic Benchmarks**: CPU-only microbenchmarks using Google Benchmark
2. **Real Network Load Tests**: End-to-end protocol performance with actual socket I/O

**Current Status**:
- ✅ Synthetic benchmarks fully implemented and documented
- 🚧 Real network load tests available but require additional validation
- 🚧 Comparative benchmarks vs other libraries (planned)

---

## Benchmark Types

### Synthetic Benchmarks (CPU-Only)

**Purpose**: Measure CPU performance of core operations without network I/O

**Location**: `benchmarks/` directory

**Programs**:
- `message_throughput_bench.cpp` - Message allocation and copy performance
- `connection_bench.cpp` - Mock connection operations
- `session_bench.cpp` - Session bookkeeping overhead

**Key Characteristics**:
- No actual socket I/O
- Mock objects simulate network operations
- Measures CPU-bound operations only
- Fast execution (seconds)
- Reproducible on any machine

**Use Cases**:
- Development iteration speed
- Regression detection for CPU-bound code
- Profiling memory allocation patterns

### Real Network Load Tests

**Purpose**: Measure end-to-end performance with actual network I/O

**Location**: `integration_tests/` directory

**Programs**:
- `tcp_load_test` - TCP throughput and latency
- `udp_load_test` - UDP datagram performance
- `websocket_load_test` - WebSocket message performance

**Key Characteristics**:
- Real socket I/O operations
- Network stack overhead included
- Platform and network dependent
- Longer execution time (minutes)
- Requires network setup

**Use Cases**:
- Production performance validation
- Capacity planning
- Network protocol optimization
- Platform-specific tuning

---

## Synthetic Benchmarks

### Test Platform

**Hardware**:
- **CPU**: Intel i7-12700K @ 3.2GHz (12 cores, 20 threads)
- **RAM**: 32GB DDR4-3200
- **Storage**: NVMe SSD

**Software**:
- **OS**: Ubuntu 22.04 LTS
- **Compiler**: GCC 11.3
- **Build**: Release mode with `-O3` optimization
- **C++ Standard**: C++20

### Message Throughput Benchmarks

**Description**: Measures in-memory message allocation and copy performance

**Test**: `benchmarks/message_throughput_bench.cpp:12-183`

**Methodology**:
1. Allocate message buffer of specific size
2. Perform `memcpy` to simulate data copy
3. Measure time per operation
4. Calculate throughput (messages/second)

**Results**:

| Payload Size | CPU Time/Op (ns) | Throughput (msg/s) | Scope |
|--------------|------------------|-------------------|-------|
| 64 bytes | 1,300 | ~769,000 | In-memory allocation + memcpy |
| 256 bytes | 3,270 | ~305,000 | In-memory allocation + memcpy |
| 1 KB | 7,803 | ~128,000 | In-memory allocation + memcpy |
| 8 KB | 48,000 | ~21,000 | In-memory allocation + memcpy |

**Analysis**:
- Small messages (64B-256B): Excellent throughput (300K-750K msg/s)
- Medium messages (1KB): Good throughput (~128K msg/s)
- Large messages (8KB): Moderate throughput (~21K msg/s)
- Linear scaling with payload size
- Memory allocation dominates for small payloads
- Memory copy dominates for large payloads

**Caveats**:
- ⚠️ No network I/O - actual network throughput will be lower
- ⚠️ No serialization/deserialization overhead
- ⚠️ No protocol overhead (headers, framing)

### Connection Benchmarks

**Description**: Measures mock connection operations

**Test**: `benchmarks/connection_bench.cpp:15-197`

**Methodology**:
- Mock connection objects simulate network operations
- `mock_connection::connect()` sleeps for 10 µs
- Measures connection setup/teardown overhead

**Results**:

| Operation | Time/Op | Throughput | Notes |
|-----------|---------|------------|-------|
| Connect (mock) | ~10 µs | ~100K/s | Simulated delay |
| Disconnect | ~1 µs | ~1M/s | Cleanup only |
| Send (mock) | ~5 µs | ~200K/s | No actual I/O |

**Caveats**:
- ⚠️ Mock objects only - not real network performance
- ⚠️ Actual TCP handshake takes 1-100ms depending on network
- ⚠️ Use real load tests for production estimates

### Session Benchmarks

**Description**: Measures session bookkeeping overhead

**Test**: `benchmarks/session_bench.cpp:1-176`

**Methodology**:
- Create/destroy session objects
- Update session state
- Measure CPU overhead without I/O

**Results**:

| Operation | Time/Op | Throughput | Notes |
|-----------|---------|------------|-------|
| Create session | ~2 µs | ~500K/s | Object allocation |
| Update state | ~0.5 µs | ~2M/s | Atomic operations |
| Destroy session | ~1.5 µs | ~666K/s | RAII cleanup |

**Analysis**:
- Session overhead is minimal (< 5 µs)
- Atomic state updates are very fast
- RAII ensures efficient cleanup

---

## Real Network Load Tests

### TCP Load Test

**Test**: `integration_tests/tcp_load_test`

**Metrics Collected**:
- **Throughput**: Messages per second
- **Latency**: P50, P95, P99 percentiles (end-to-end)
- **Memory**: RSS, heap, VM consumption
- **Concurrency**: Performance with multiple connections

**Test Scenarios**:
1. Single connection, varying payload sizes
2. Multiple connections (10, 100, 1000)
3. Sustained load (duration test)
4. Burst traffic patterns

**Pending Results**: Real network measurements are being collected and will be documented after validation.

### UDP Load Test

**Test**: `integration_tests/udp_load_test`

**Focus**:
- Datagram throughput
- Packet loss rates
- Latency distribution
- Burst handling

**Pending Results**: UDP-specific metrics are being collected.

### WebSocket Load Test

**Test**: `integration_tests/websocket_load_test`

**Focus**:
- Message framing overhead
- Masking/unmasking performance
- Fragmentation impact
- Concurrent WebSocket connections

**Pending Results**: WebSocket-specific metrics are being collected.

### Automated Baseline Collection

**CI Integration**: GitHub Actions workflow for periodic load testing

**Workflow**: `.github/workflows/network-load-tests.yml`

**Schedule**:
- Weekly automated runs (every Sunday)
- Manual trigger available
- On-demand baseline updates

**Trigger Manually**:
```bash
gh workflow run network-load-tests.yml --field update_baseline=true
```

**Output**:
- Baseline metrics updated in `BASELINE.md`
- Historical trends tracked
- Regression detection

---

## Benchmark Methodology

### Google Benchmark Configuration

**Settings**:
```cpp
// Minimum iterations for statistical significance
BENCHMARK(MessageThroughput)->MinTime(1.0);

// Multiple payload sizes
BENCHMARK(MessageThroughput)
    ->Args({64})
    ->Args({256})
    ->Args({1024})
    ->Args({8192});

// Report statistics
BENCHMARK(MessageThroughput)
    ->ComputeStatistics("mean", [](const std::vector<double>& v) {
        return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
    });
```

**Output Formats**:
- Console (human-readable)
- JSON (machine-readable)
- CSV (for analysis)

### Load Test Methodology

**Test Setup**:
1. Start server on localhost
2. Connect N clients
3. Send M messages per client
4. Measure end-to-end latency
5. Collect resource metrics
6. Graceful shutdown

**Duration**:
- Short test: 1 minute (quick validation)
- Medium test: 10 minutes (steady-state)
- Long test: 1 hour (stability)

**Metrics Collection**:
```bash
# CPU and memory monitoring
top -b -d 1 -n 60 > cpu_mem.log

# Network statistics
netstat -s > netstat_before.log
# ... run test ...
netstat -s > netstat_after.log

# Application metrics
./tcp_load_test --duration=60 --clients=100 --messages=1000000
```

---

## Performance Comparison

### Industry Standards

**Comparison vs Other Libraries** (Planned):

| Library | TCP Throughput | WebSocket Msg/s | Memory/Connection |
|---------|---------------|-----------------|-------------------|
| network_system | TBD | TBD | TBD |
| Boost.Asio | TBD | N/A | TBD |
| libuv | TBD | TBD | TBD |
| WebSocket++ | N/A | TBD | TBD |

**Status**: Comparative benchmarks are planned after real network measurements are validated.

### Expected Performance Targets

**TCP**:
- Throughput: > 1M messages/second (small payloads)
- Latency: < 1ms P50 (localhost)
- Connections: > 10,000 concurrent

**WebSocket**:
- Throughput: > 500K messages/second
- Latency: < 2ms P50 (including framing)
- Connections: > 5,000 concurrent

**Memory**:
- Per connection: < 10 KB (excluding buffers)
- Buffer overhead: Configurable (default 8KB)

---

## Reproducing Benchmarks

### Build with Benchmarks

```bash
# Clone repository
git clone https://github.com/kcenon/network_system.git
cd network_system

# Build with benchmarks enabled
cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DNETWORK_BUILD_BENCHMARKS=ON \
    -DBUILD_TESTS=ON

cmake --build build -j$(nproc)
```

### Run Synthetic Benchmarks

```bash
# Run all benchmarks
./build/benchmarks/network_benchmarks

# Run specific benchmark category
./build/benchmarks/network_benchmarks --benchmark_filter=MessageThroughput
./build/benchmarks/network_benchmarks --benchmark_filter=Connection
./build/benchmarks/network_benchmarks --benchmark_filter=Session

# Generate JSON output
./build/benchmarks/network_benchmarks \
    --benchmark_format=json \
    --benchmark_out=results.json

# Generate CSV output
./build/benchmarks/network_benchmarks \
    --benchmark_format=csv \
    --benchmark_out=results.csv
```

### Run Load Tests

```bash
# TCP load test
./build/bin/integration_tests/tcp_load_test \
    --duration=60 \
    --clients=100 \
    --messages=1000000

# UDP load test
./build/bin/integration_tests/udp_load_test \
    --duration=60 \
    --packet_rate=10000

# WebSocket load test
./build/bin/integration_tests/websocket_load_test \
    --duration=60 \
    --connections=100 \
    --messages=500000
```

### Expected Output (Synthetic Benchmarks)

```
-------------------------------------------------------------------------
Benchmark                               Time       CPU   Iterations
-------------------------------------------------------------------------
MessageThroughput/64B            1300 ns   1299 ns       538462   # ~769K msg/s
MessageThroughput/256B           3270 ns   3268 ns       214286   # ~305K msg/s
MessageThroughput/1KB            7803 ns   7801 ns        89744   # ~128K msg/s
MessageThroughput/8KB           48000 ns  47998 ns        14583   # ~21K msg/s

Connection/Connect              10000 ns   9998 ns        70000   # Mock
Connection/Disconnect            1000 ns    999 ns       700000   # Mock

Session/Create                   2000 ns   1998 ns       350000
Session/Update                    500 ns    499 ns      1400000
Session/Destroy                  1500 ns   1499 ns       466667
```

### Analyzing Results

**Statistical Significance**:
- Run multiple iterations (Google Benchmark auto-determines)
- Check coefficient of variation (< 5% recommended)
- Compare mean, median, stddev

**Regression Detection**:
```bash
# Compare against baseline
./build/benchmarks/network_benchmarks \
    --benchmark_out=current.json \
    --benchmark_format=json

# Use Google Benchmark compare tool
compare.py benchmarks baseline.json current.json
```

**Visualization**:
```python
# Plot results with matplotlib
import json
import matplotlib.pyplot as plt

with open('results.json') as f:
    data = json.load(f)

# Extract and plot data
# ... visualization code ...
```

---

## Performance Optimization Tips

### For Synthetic Benchmarks

**Reduce Allocation Overhead**:
- Use object pools for message buffers
- Pre-allocate buffers when size is known
- Consider stack allocation for small messages

**Minimize Copies**:
- Use move semantics (`std::move`)
- Pass by reference when possible
- Avoid unnecessary conversions

### For Real Network Tests

**Tune TCP Settings**:
```bash
# Increase TCP buffer sizes
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.wmem_max=134217728
sudo sysctl -w net.ipv4.tcp_rmem="4096 87380 134217728"
sudo sysctl -w net.ipv4.tcp_wmem="4096 65536 134217728"

# Increase connection backlog
sudo sysctl -w net.core.somaxconn=65535
```

**Application Tuning**:
```cpp
// Tune buffer sizes
config.send_buffer_size = 64 * 1024;  // 64 KB
config.recv_buffer_size = 64 * 1024;

// Enable TCP_NODELAY for low latency
config.tcp_nodelay = true;

// Tune thread pool size
config.io_threads = std::thread::hardware_concurrency() * 2;
```

---

## Pending Measurements

The following measurements are being collected and will be added after validation:

- ⏳ End-to-end TCP throughput with real socket I/O
- ⏳ End-to-end latency distributions (P50/P95/P99)
- ⏳ Memory footprint under sustained load
- ⏳ Concurrent connection scalability limits
- ⏳ TLS/SSL performance overhead
- ⏳ WebSocket framing overhead
- ⏳ Comparative benchmarks vs Boost.Asio, libuv
- ⏳ Platform-specific optimizations (Linux vs Windows vs macOS)

---

## See Also

- [BASELINE.md](../BASELINE.md) - Current performance baseline
- [LOAD_TEST_GUIDE.md](LOAD_TEST_GUIDE.md) - Detailed load testing guide
- [FEATURES.md](FEATURES.md) - Feature descriptions
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
- [IMPROVEMENTS.md](../IMPROVEMENTS.md) - Planned optimizations

---

## Contributing Performance Data

We welcome community contributions of benchmark results!

**To Contribute**:
1. Run benchmarks on your platform
2. Document hardware, OS, compiler
3. Save results as JSON
4. Submit PR with results in `benchmarks/results/`

**Platform Coverage Needed**:
- ARM64 (Apple Silicon, AWS Graviton)
- Different network conditions (localhost, LAN, WAN)
- Windows MSVC vs MinGW comparison
- Various Linux distributions

---

**Last Updated**: 2025-11-15
**Maintained by**: kcenon@naver.com
