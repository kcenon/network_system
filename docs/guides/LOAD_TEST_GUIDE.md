# Network Load Testing Guide

**Version**: 0.1.0
**Last Updated**: 2025-10-26
**Phase**: Phase 7 - Network Load Testing & Real Baseline Metrics

---

## Overview

This guide explains how to run network load tests, interpret results, and update performance baselines for the network_system project. The load testing framework measures real TCP/UDP/WebSocket communication performance including throughput, latency distributions, and memory consumption.

---

## Quick Start

### Prerequisites

- CMake 3.16 or higher
- C++20 compatible compiler (GCC 11+, Clang 13+, MSVC 2019+)
- Python 3.7+ (for automation scripts)
- Git and GitHub CLI (optional, for CI workflows)

### Building Load Tests

```bash
# Clone repository
git clone https://github.com/kcenon/network_system.git
cd network_system

# Configure with integration tests enabled
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON

# Build
cmake --build build -j$(nproc)
```

### Running Load Tests

```bash
# Run all load tests
./build/bin/integration_tests/tcp_load_test
./build/bin/integration_tests/udp_load_test
./build/bin/integration_tests/websocket_load_test

# Run specific test cases
./build/bin/integration_tests/tcp_load_test --gtest_filter="*Throughput_64B"
./build/bin/integration_tests/udp_load_test --gtest_filter="*Concurrent*"
```

---

## Test Structure

### Test Suites

#### TCP Load Tests (`tcp_load_test`)

**Message Throughput Tests:**
- `Message_Throughput_64B` - 64-byte messages, 1000 iterations
- `Message_Throughput_1KB` - 1KB messages, 1000 iterations
- `Message_Throughput_64KB` - 64KB messages, 500 iterations

**Concurrency Tests:**
- `Concurrent_Connections_10` - 10 simultaneous connections
- `Concurrent_Connections_50` - 50 simultaneous connections

**Latency Tests:**
- `Round_Trip_Latency` - Small message round-trip time

#### UDP Load Tests (`udp_load_test`)

**Message Throughput Tests:**
- `Message_Throughput_64B` - 64-byte datagrams
- `Message_Throughput_512B` - 512-byte datagrams
- `Message_Throughput_1KB` - 1KB datagrams

**Concurrency Tests:**
- `Concurrent_Clients_10` - 10 simultaneous clients
- `Concurrent_Clients_50` - 50 simultaneous clients

**Performance Tests:**
- `Send_Latency` - UDP send operation latency
- `Burst_Send_Performance` - Burst sending (100 messages per burst)

#### WebSocket Load Tests (`websocket_load_test`)

**Text Message Tests:**
- `Text_Message_Throughput_64B` - 64-byte text frames
- `Text_Message_Throughput_1KB` - 1KB text frames

**Binary Message Tests:**
- `Binary_Message_Throughput` - 256-byte binary frames

**Concurrency Tests:**
- `Concurrent_Connections_10` - 10 WebSocket connections

**Latency Tests:**
- `Ping_Pong_Latency` - Small message round-trip (proxy for ping/pong)

### Test Configuration

All tests use the following defaults:
- **Environment**: Localhost loopback (127.0.0.1)
- **Port Selection**: Automatically finds available ports starting from protocol-specific ranges
- **CI Detection**: Tests skip automatically in CI environments to avoid timing-related flakiness
- **Output Format**: JSON files with structured performance metrics

---

## Understanding Results

### Output Files

Each test generates JSON files with performance data:

```json
{
  "timestamp": "2025-10-26T14:30:00Z",
  "results": [
    {
      "test_name": "TCP_64B",
      "protocol": "tcp",
      "latency_ms": {
        "min": 0.025,
        "max": 2.150,
        "mean": 0.180,
        "p50": 0.150,
        "p95": 0.350,
        "p99": 0.620,
        "stddev": 0.145
      },
      "throughput_msg_s": 5200.0,
      "bandwidth_mbps": 0.32,
      "memory": {
        "rss_mb": 12.5,
        "heap_mb": 0.0,
        "vm_mb": 85.3
      },
      "platform": "ubuntu-22.04",
      "compiler": "gcc-11"
    }
  ]
}
```

### Key Metrics

#### Throughput
- **Definition**: Messages successfully sent per second
- **Units**: msg/s (messages per second)
- **Interpretation**: Higher is better
- **Typical Range**:
  - TCP: 1,000 - 10,000 msg/s (64B messages on loopback)
  - UDP: 2,000 - 20,000 msg/s (faster due to connectionless nature)
  - WebSocket: 1,000 - 8,000 msg/s (includes framing overhead)

#### Latency
- **P50 (Median)**: 50% of operations complete within this time
- **P95**: 95% of operations complete within this time
- **P99**: 99% of operations complete within this time
- **Units**: milliseconds (ms)
- **Interpretation**: Lower is better
- **Typical Range** (loopback):
  - P50: 0.1 - 0.5 ms
  - P95: 0.3 - 2.0 ms
  - P99: 0.5 - 5.0 ms

#### Memory (RSS)
- **Definition**: Resident Set Size (physical memory used)
- **Units**: MB (megabytes)
- **Interpretation**: Lower is better for fixed workload
- **Per-Connection Memory**: Typically 100-500 KB per connection

### Performance Expectations

#### TCP
- **Strengths**: Reliable, ordered delivery
- **Expected Throughput**: 2,000+ msg/s for 64B messages
- **Expected P99 Latency**: < 100 ms
- **Memory**: ~1 MB per connection

#### UDP
- **Strengths**: Low latency, high throughput
- **Expected Throughput**: 5,000+ msg/s for 64B messages
- **Expected P99 Latency**: < 50 ms
- **Memory**: ~500 KB per client
- **Note**: May experience packet loss under heavy load

#### WebSocket
- **Strengths**: Full-duplex, HTTP-compatible
- **Expected Throughput**: 1,000+ msg/s for 64B messages
- **Expected P99 Latency**: < 100 ms
- **Memory**: ~2 MB per connection (includes HTTP upgrade overhead)

---

## Automated Baseline Collection

### GitHub Actions Workflow

The `.github/workflows/network-load-tests.yml` workflow runs load tests on:
- **Schedule**: Weekly (Sunday 00:00 UTC / Monday 09:00 KST)
- **Manual Trigger**: Via GitHub Actions UI or CLI

### Manual Workflow Execution

```bash
# Run load tests without updating baseline
gh workflow run network-load-tests.yml

# Run load tests and create PR to update baseline
gh workflow run network-load-tests.yml --field update_baseline=true
```

### Workflow Steps

1. **Build**: Compile network_system with Release configuration
2. **Test Execution**: Run TCP/UDP/WebSocket load tests
3. **Metrics Collection**: Aggregate results using `scripts/collect_metrics.py`
4. **Baseline Update** (optional): Update BASELINE.md using `scripts/update_baseline.py`
5. **Pull Request**: Automatically create PR with baseline changes

### Reviewing Baseline Updates

When a baseline update PR is created:

1. **Check Performance Changes**: Compare new metrics with existing baseline
2. **Verify Regression**: Ensure no significant performance degradation
3. **Platform Consistency**: Compare results across Ubuntu/macOS/Windows
4. **Merge Decision**: Approve if metrics are acceptable, request investigation if degraded

---

## Local Metrics Collection

### Collecting Metrics Manually

```bash
# Run tests to generate JSON output
./build/bin/integration_tests/tcp_load_test
./build/bin/integration_tests/udp_load_test
./build/bin/integration_tests/websocket_load_test

# Aggregate metrics
python3 scripts/collect_metrics.py \
  --tcp-json tcp_64b_results.json \
  --udp-json udp_64b_results.json \
  --websocket-json websocket_text_64b_results.json \
  --output metrics.json

# View summary
cat metrics.json | jq .
```

### Updating Baseline Locally

```bash
# Dry run (preview changes)
python3 scripts/update_baseline.py \
  --metrics metrics.json \
  --baseline BASELINE.md \
  --platform "$(uname -s)-$(uname -r)" \
  --dry-run

# Apply changes
python3 scripts/update_baseline.py \
  --metrics metrics.json \
  --baseline BASELINE.md \
  --platform "$(uname -s)-$(uname -r)"
```

---

## Troubleshooting

### Tests Fail with "Address Already in Use"

**Cause**: Another process is using the port range.

**Solution**:
```bash
# Check listening ports
netstat -tuln | grep -E "1[89][0-9]{3}"

# Kill conflicting processes or run tests sequentially
./build/bin/integration_tests/tcp_load_test
# Wait for completion before running next test
./build/bin/integration_tests/udp_load_test
```

### Low Throughput

**Possible Causes**:
- Running in Debug build (use Release: `-DCMAKE_BUILD_TYPE=Release`)
- High system load (close background applications)
- Thermal throttling (ensure adequate cooling)
- Virtualized environment (bare metal performs better)

**Verification**:
```bash
# Check build type
cat build/CMakeCache.txt | grep CMAKE_BUILD_TYPE

# Monitor system load
top -bn1 | head -20
```

### High Memory Usage

**Possible Causes**:
- Memory leak in test or application code
- Large message buffers not released
- Connection pooling keeping connections alive

**Investigation**:
```bash
# Run with Valgrind (Linux)
valgrind --leak-check=full ./build/bin/integration_tests/tcp_load_test

# Run with AddressSanitizer
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address"
cmake --build build
./build/bin/integration_tests/tcp_load_test
```

### Tests Skip in CI

**Cause**: Tests automatically skip in CI environments to avoid timing issues.

**Override** (not recommended):
```bash
# Unset CI environment variable
unset CI
./build/bin/integration_tests/tcp_load_test
```

---

## Best Practices

### When to Run Load Tests

- **Before Release**: Verify no performance regression
- **After Optimization**: Validate improvements
- **Weekly (Automated)**: Track performance trends over time
- **After Infrastructure Change**: Ensure baseline stability

### Interpreting Results

1. **Compare Against Baseline**: Look for significant deviations (>20%)
2. **Check Multiple Runs**: Ensure consistency across runs
3. **Consider Environment**: Account for different hardware/OS
4. **Focus on P95/P99**: Worst-case latency matters for user experience

### Updating Baselines

- **Significant Improvement**: Update baseline to reflect optimization
- **Expected Degradation**: Update with justification (e.g., new features)
- **Unexpected Regression**: Investigate root cause before updating
- **Platform Differences**: Maintain separate baselines per platform

---

## Advanced Usage

### Custom Test Scenarios

Modify test parameters in source files:

```cpp
// integration_tests/performance/tcp_load_test.cpp
constexpr size_t num_messages = 1000;  // Change iteration count
constexpr size_t message_size = 64;    // Change message size
```

### Profiling with Perf (Linux)

```bash
# Build with debug symbols
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build

# Profile with perf
perf record -g ./build/bin/integration_tests/tcp_load_test
perf report
```

### Custom Metrics Scripts

Create custom analysis scripts:

```python
import json

# Load metrics
with open('metrics.json', 'r') as f:
    data = json.load(f)

# Custom analysis
for protocol, metrics in data['protocols'].items():
    throughput = metrics['throughput_msg_s']
    latency_p99 = metrics['latency']['p99_ms']
    print(f"{protocol}: {throughput:.0f} msg/s, P99={latency_p99:.2f}ms")
```

---

## References

- [BASELINE.md](../BASELINE.md) - Performance baseline documentation
- [WEBSOCKET_IMPLEMENTATION_PLAN.md](WEBSOCKET_IMPLEMENTATION_PLAN.md) - Phase 7 implementation details
- [Google Test Documentation](https://google.github.io/googletest/) - Test framework reference
- [GitHub Actions Documentation](https://docs.github.com/en/actions) - CI/CD workflow reference

---

**Document Maintained By**: kcenon
**For Issues/Questions**: [GitHub Issues](https://github.com/kcenon/network_system/issues)
