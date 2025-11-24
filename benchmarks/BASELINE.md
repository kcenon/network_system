# Baseline Performance Metrics

**Document Version**: 1.0
**Created**: 2025-10-07
**System**: network_system
**Purpose**: Establish baseline performance metrics for regression detection

---

## Overview

This document records baseline performance metrics for the network_system. These metrics serve as reference points for detecting performance regressions during development.

**Regression Threshold**: <5% performance degradation is acceptable. Any regression >5% should be investigated and justified.

---

## Test Environment

### Hardware Specifications
- **CPU**: To be recorded on first benchmark run
- **Cores**: To be recorded on first benchmark run
- **RAM**: To be recorded on first benchmark run
- **Network**: Localhost (loopback interface)
- **OS**: macOS / Linux / Windows

### Software Configuration
- **Compiler**: Clang/GCC/MSVC (see CI workflow)
- **C++ Standard**: C++20
- **Build Type**: Release with optimizations
- **CMake Version**: 3.16+
- **Network Backend**: Asio/Boost.Asio

### Test Configuration
- **Protocol**: TCP (primary), UDP (secondary)
- **Test Mode**: Localhost loopback
- **Buffer Sizes**: 4KB, 64KB, 1MB
- **Packet Sizes**: 64B, 1KB, 64KB

---

## Benchmark Categories

### 1. Connection Performance

#### 1.1 Connection Establishment
**Metric**: Time to establish TCP connection
**Test File**: `connection_bench.cpp`

| Connection Type | Mean (μs) | Median (μs) | P95 (μs) | P99 (μs) | Notes |
|-----------------|-----------|-------------|----------|----------|-------|
| Localhost | TBD | TBD | TBD | TBD | Loopback |
| Keep-alive reuse | TBD | TBD | TBD | TBD | Persistent |

**Target**: <100μs for localhost connection
**Status**: ⏳ Awaiting initial benchmark run

#### 1.2 Connection Throughput
**Metric**: Connections established per second

| Concurrent Connections | Conn/sec | Notes |
|------------------------|----------|-------|
| 1 (serial) | TBD | No parallelism |
| 10 | TBD | |
| 100 | TBD | |
| 1000 | TBD | High concurrency |

**Target**: >10,000 conn/sec for localhost
**Status**: ⏳ Awaiting initial benchmark run

#### 1.3 Connection Teardown
**Metric**: Time to properly close connection

| Teardown Type | Mean (μs) | Notes |
|---------------|-----------|-------|
| Graceful close | TBD | FIN handshake |
| Forced close | TBD | RST |

**Status**: ⏳ Awaiting initial benchmark run

### 2. Session Management

#### 2.1 Session Creation Overhead
**Metric**: Time to create session object
**Test File**: `session_bench.cpp`

| Session Type | Mean (ns) | Median (ns) | Notes |
|--------------|-----------|-------------|-------|
| Basic session | TBD | TBD | Minimal state |
| With buffers | TBD | TBD | Pre-allocated |
| With encryption | TBD | TBD | TLS overhead |

**Status**: ⏳ Awaiting initial benchmark run

#### 2.2 Session Pool Performance
**Metric**: Session reuse from pool

| Pool State | Acquire (ns) | Release (ns) | Notes |
|------------|--------------|--------------|-------|
| Hot (available) | TBD | TBD | Ready to use |
| Cold (create new) | TBD | TBD | Allocation |

**Status**: ⏳ Awaiting initial benchmark run

### 3. Data Transfer Performance

#### 3.1 Send Throughput
**Metric**: Data transmission rate
**Test File**: `session_bench.cpp`

| Packet Size | Throughput (MB/s) | Latency (μs) | CPU (%) | Notes |
|-------------|-------------------|--------------|---------|-------|
| 64 bytes | TBD | TBD | TBD | Small packet |
| 1 KB | TBD | TBD | TBD | |
| 64 KB | TBD | TBD | TBD | Optimal size |
| 1 MB | TBD | TBD | TBD | Large block |

**Target**: >1 GB/s for localhost, 64KB packets
**Status**: ⏳ Awaiting initial benchmark run

#### 3.2 Receive Throughput
**Metric**: Data reception rate

| Packet Size | Throughput (MB/s) | Latency (μs) | Notes |
|-------------|-------------------|--------------|-------|
| 64 bytes | TBD | TBD | |
| 1 KB | TBD | TBD | |
| 64 KB | TBD | TBD | |
| 1 MB | TBD | TBD | |

**Status**: ⏳ Awaiting initial benchmark run

#### 3.3 Round-Trip Latency
**Metric**: Time for send → receive → response

| Payload Size | RTT (μs) | Notes |
|--------------|----------|-------|
| 0 bytes (ping) | TBD | Minimal overhead |
| 64 bytes | TBD | |
| 1 KB | TBD | |
| 64 KB | TBD | |

**Target**: <50μs for 0-byte ping on localhost
**Status**: ⏳ Awaiting initial benchmark run

### 4. Concurrent Connections

#### 4.1 Multiple Concurrent Connections
**Metric**: Aggregate throughput with many connections

| Connection Count | Total Throughput (MB/s) | Per-Connection (MB/s) | Notes |
|------------------|-------------------------|----------------------|-------|
| 1 | TBD | TBD | Baseline |
| 10 | TBD | TBD | |
| 100 | TBD | TBD | |
| 1000 | TBD | TBD | High concurrency |

**Status**: ⏳ Awaiting initial benchmark run

#### 4.2 Connection Scalability
**Metric**: Resource usage per connection

| Connection Count | Memory (MB) | CPU (%) | Handles | Notes |
|------------------|-------------|---------|---------|-------|
| 100 | TBD | TBD | TBD | |
| 1,000 | TBD | TBD | TBD | |
| 10,000 | TBD | TBD | TBD | C10K problem |

**Status**: ⏳ Awaiting measurement

### 5. Protocol Overhead

#### 5.1 TCP vs UDP Performance
**Metric**: Protocol comparison

| Metric | TCP (μs) | UDP (μs) | Difference | Notes |
|--------|----------|----------|------------|-------|
| Send latency | TBD | TBD | TBD | Single packet |
| Receive latency | TBD | TBD | TBD | |
| Throughput (MB/s) | TBD | TBD | TBD | 64KB packets |

**Status**: ⏳ Awaiting initial benchmark run

#### 5.2 Framing Overhead
**Metric**: Cost of message framing

| Framing Type | Overhead (ns) | Notes |
|--------------|---------------|-------|
| No framing (raw) | 0 | Baseline |
| Length prefix | TBD | 4-byte header |
| Delimiter | TBD | Scan for delimiter |
| Protocol buffers | TBD | Full serialization |

**Status**: ⏳ Awaiting initial benchmark run

---

## HTTP Performance

### 9. HTTP Server/Client Performance
**Test File**: `http_bench.cpp`

#### 9.1 HTTP Request Throughput
**Metric**: Requests per second (RPS)

| Request Type | RPS | Latency (μs) | Notes |
|--------------|-----|--------------|-------|
| Simple GET | TBD | TBD | 13-byte response |
| GET with headers | TBD | TBD | 4 custom headers |
| POST 64B | TBD | TBD | Echo endpoint |
| POST 1KB | TBD | TBD | |
| POST 64KB | TBD | TBD | |

**Target**: >10,000 RPS for simple GET on localhost
**Status**: ⏳ Awaiting initial benchmark run

#### 9.2 HTTP Latency Distribution
**Metric**: Request-response latency percentiles

| Percentile | Latency (μs) | Notes |
|------------|--------------|-------|
| P50 | TBD | Median |
| P95 | TBD | |
| P99 | TBD | Tail latency |
| P999 | TBD | Extreme tail |
| Average | TBD | |
| Min | TBD | Best case |
| Max | TBD | Worst case |

**Target**: P99 <1ms for simple GET on localhost
**Status**: ⏳ Awaiting initial benchmark run

#### 9.3 HTTP Concurrent Connections
**Metric**: Performance under concurrent load

| Concurrent Clients | Total RPS | Per-Client RPS | Success Rate | Notes |
|--------------------|-----------|----------------|--------------|-------|
| 1 | TBD | TBD | TBD | Baseline |
| 5 | TBD | TBD | TBD | |
| 10 | TBD | TBD | TBD | |
| 25 | TBD | TBD | TBD | |
| 50 | TBD | TBD | TBD | High concurrency |

**Target**: >90% success rate at 50 concurrent clients
**Status**: ⏳ Awaiting initial benchmark run

#### 9.4 HTTP Response Size Performance
**Metric**: Throughput by response size

| Response Size | Throughput (MB/s) | Latency (μs) | Notes |
|---------------|-------------------|--------------|-------|
| 256 bytes | TBD | TBD | Small |
| 1 KB | TBD | TBD | |
| 4 KB | TBD | TBD | |
| 16 KB | TBD | TBD | |
| 64 KB | TBD | TBD | Large |

**Status**: ⏳ Awaiting initial benchmark run

#### 9.5 HTTP Client Creation Overhead
**Metric**: Time to create http_client instance

| Operation | Time (ns) | Notes |
|-----------|-----------|-------|
| Client creation | TBD | Shared pointer allocation |

**Status**: ⏳ Awaiting initial benchmark run

---

## Async I/O Performance

### 6. Event Loop Performance

#### 6.1 Event Processing Latency
**Metric**: Time to process I/O event

| Event Type | Mean (ns) | Median (ns) | Notes |
|------------|-----------|-------------|-------|
| Read ready | TBD | TBD | |
| Write ready | TBD | TBD | |
| Connection | TBD | TBD | Accept |
| Timeout | TBD | TBD | Timer fired |

**Status**: ⏳ Awaiting initial benchmark run

#### 6.2 Event Loop Throughput
**Metric**: Events processed per second

| Concurrent Events | Events/sec | Latency (μs) | Notes |
|-------------------|------------|--------------|-------|
| 10 | TBD | TBD | |
| 100 | TBD | TBD | |
| 1000 | TBD | TBD | |
| 10000 | TBD | TBD | High load |

**Target**: >1M events/sec
**Status**: ⏳ Awaiting initial benchmark run

---

## Buffer Management

### 7. Buffer Performance

#### 7.1 Buffer Allocation
**Metric**: Time to allocate network buffer

| Buffer Size | Alloc (ns) | Free (ns) | Notes |
|-------------|------------|-----------|-------|
| 4 KB | TBD | TBD | Small |
| 64 KB | TBD | TBD | Standard |
| 1 MB | TBD | TBD | Large |

**Status**: ⏳ Awaiting initial benchmark run

#### 7.2 Buffer Pool Performance
**Metric**: Reuse from buffer pool

| Pool State | Acquire (ns) | Release (ns) | Notes |
|------------|--------------|--------------|-------|
| Hot | TBD | TBD | Available |
| Cold | TBD | TBD | Allocate new |

**Target**: <100ns for hot acquisition
**Status**: ⏳ Awaiting initial benchmark run

#### 7.3 Zero-Copy Operations
**Metric**: Performance gain from zero-copy

| Operation | Copy (μs) | Zero-Copy (μs) | Speedup | Notes |
|-----------|-----------|----------------|---------|-------|
| Send 64KB | TBD | TBD | TBD | If supported |
| Send 1MB | TBD | TBD | TBD | |

**Status**: ⏳ Awaiting initial benchmark run (platform-dependent)

---

## Memory Performance

### 8. Memory Usage

#### 8.1 Per-Connection Memory
**Metric**: Memory footprint per connection

| Connection State | Memory (KB) | Notes |
|------------------|-------------|-------|
| Established | TBD | Minimal state |
| With buffers | TBD | Send/recv buffers |
| With TLS | TBD | Crypto overhead |

**Target**: <50KB per connection (without TLS)
**Status**: ⏳ Awaiting measurement

---

## How to Run Benchmarks

### Building Benchmarks
```bash
cd network_system
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
cmake --build build --target benchmarks
```

### Running Benchmarks
```bash
cd build/benchmarks
./connection_bench
./session_bench
```

### Recording Results
1. Disable firewall for localhost (if needed)
2. Run each benchmark 10 times
3. Use localhost/loopback for consistency
4. Record statistics: min, max, mean, median, p95, p99
5. Update this document with actual values
6. Commit updated BASELINE.md

---

## Regression Detection

### Automated Checks
The benchmarks.yml workflow runs benchmarks on every PR and compares results against this baseline.

### Performance Targets
- **Connection latency**: <100μs (localhost)
- **Throughput**: >1 GB/s (localhost, 64KB packets)
- **RTT**: <50μs (0-byte ping)
- **Event throughput**: >1M events/sec
- **Memory per connection**: <50KB
- **Concurrent connections**: 10,000+ supported

---

## Historical Changes

| Date | Version | Change | Impact | Approved By |
|------|---------|--------|--------|-------------|
| 2025-10-07 | 1.0 | Initial baseline document created | N/A | Initial setup |

---

## Notes

- All benchmarks use Google Benchmark framework
- Benchmarks use localhost for consistency (no network variability)
- Real network performance depends on bandwidth, latency, packet loss
- For accurate comparisons, run benchmarks on same hardware
- CI environment results are used as primary baseline
- Zero-copy features are platform-dependent (Linux: sendfile, Windows: TransmitFile)
- TLS benchmarks require OpenSSL or similar library

---

**Status**: 📝 Template created - awaiting initial benchmark data collection
