# Performance Tuning Guide

**Last Updated**: 2025-11-25

This guide provides comprehensive guidance on optimizing network system performance for different workloads and environments.

---

## Table of Contents

- [Quick Reference](#quick-reference)
- [Configuration Reference](#configuration-reference)
- [Workload-Specific Tuning](#workload-specific-tuning)
- [Common Anti-Patterns](#common-anti-patterns)
- [Profiling Guide](#profiling-guide)
- [OS-Level Tuning](#os-level-tuning)
- [Advanced Topics](#advanced-topics)

---

## Quick Reference

### Configuration Overview

| Component | Configuration | Impact |
|-----------|--------------|--------|
| Thread Pool | `worker_count` | CPU utilization, concurrency |
| Logger | `async_logging` | Logging overhead, I/O blocking |
| Monitoring | `metrics_interval` | Metrics collection overhead |
| Connection Pool | `pool_size` | Connection reuse, memory usage |

### Quick Wins

```cpp
// 1. Use production config for deployed systems
auto config = network_config::production();

// 2. Enable connection pooling for repeated connections
auto pool = std::make_shared<connection_pool>(host, port, 20);

// 3. Use async logging to avoid blocking
config.logger.async_logging = true;
```

---

## Configuration Reference

### Thread Pool Configuration

Thread pool controls the number of worker threads for async operations. With the Thread System Migration Epic (#271), network_system now uses `thread_system` for unified thread management.

```cpp
struct thread_pool_config {
    size_t worker_count = 0;        // 0 = auto-detect
    size_t queue_capacity = 10000;  // Task queue size
    std::string pool_name = "network_pool";
};
```

#### Thread System Integration (Recommended)

When `BUILD_WITH_THREAD_SYSTEM` is enabled, the basic_thread_pool internally delegates to `thread_system::thread_pool`, providing:

- **Adaptive Job Queue**: Automatically switches between mutex-based and lock-free modes based on contention
- **Scheduler Support**: Proper delayed task execution without detached threads
- **Unified Metrics**: Consistent performance monitoring across all subsystems

```cpp
#include <kcenon/network/integration/thread_system_adapter.h>

// Use thread_system for maximum performance
bind_thread_system_pool_into_manager("network_pool");

// Or configure explicitly
auto adapter = thread_system_pool_adapter::from_service_or_default("network_pool");
auto& manager = integration::thread_integration_manager::instance();
manager.set_thread_pool(adapter);

// Check thread pool metrics
auto metrics = manager.get_metrics();
std::cout << "Workers: " << metrics.worker_threads << "\n";
std::cout << "Pending: " << metrics.pending_tasks << "\n";
```

#### worker_count

**Default**: 0 (auto-detect via `std::thread::hardware_concurrency()`)

**Impact**:
- **Too few**: Underutilized CPU, poor throughput
- **Too many**: Excessive context switching, cache misses
- **Just right**: Optimal throughput and latency

**Tuning Guidelines**:

```cpp
// I/O-bound workloads (network operations, file I/O)
config.thread_pool.worker_count = std::thread::hardware_concurrency() * 2;

// CPU-bound workloads (computation, encoding)
config.thread_pool.worker_count = std::thread::hardware_concurrency();

// Mixed workloads (default)
config.thread_pool.worker_count = 0;  // Auto-detect

// Low-latency requirements (over-provision)
config.thread_pool.worker_count = std::thread::hardware_concurrency() * 3;
```

#### queue_capacity

**Default**: 10000

**Impact**: Maximum pending tasks before backpressure

**Tuning**:

```cpp
// High throughput, bursty traffic
config.thread_pool.queue_capacity = 50000;

// Low memory, steady traffic
config.thread_pool.queue_capacity = 1000;
```

### Logger Configuration

Logging can significantly impact performance if not properly configured.

```cpp
struct logger_config {
    log_level min_level = log_level::info;
    bool async_logging = true;
    size_t buffer_size = 8192;
    std::string log_file_path;
};
```

#### async_logging

**Default**: true (production), false (development/testing)

**Performance Impact**:
- **Sync logging**: Blocks calling thread until log written to disk (~1-10ms per log)
- **Async logging**: Non-blocking, buffered writes (~10-100us per log)

**Tradeoff**: Async logging may lose recent logs on crash

```cpp
// Production: Use async for performance
config.logger.async_logging = true;

// Development/Debug: Use sync for immediate output
config.logger.async_logging = false;

// High-performance: Disable verbose logging
config.logger.min_level = log_level::warn;
```

#### min_level

**Recommendation by Environment**:

| Environment | Level | Rationale |
|-------------|-------|-----------|
| Development | debug | Full visibility for debugging |
| Testing | warn | Reduce noise, catch errors |
| Production | info | Operational visibility |
| High-Performance | warn | Minimize overhead |

### Monitoring Configuration

Monitoring provides metrics but adds overhead.

```cpp
struct monitoring_config {
    bool enabled = true;
    std::chrono::seconds metrics_interval{5};
    std::string service_name = "network_system";
};
```

**Performance Cost**: ~0.1% CPU per second for metrics collection

```cpp
// Development/Production: Enable for observability
config.monitoring.enabled = true;
config.monitoring.metrics_interval = std::chrono::seconds(5);

// High-performance: Reduce frequency or disable
config.monitoring.metrics_interval = std::chrono::seconds(60);

// Benchmarking: Disable to eliminate overhead
config.monitoring.enabled = false;
```

### Connection Pool Configuration

See [Connection Pooling Guide](CONNECTION_POOLING.md) for detailed tuning.

**Quick Reference**:

```cpp
// Rule of thumb: pool_size >= expected_concurrent_requests
auto pool = std::make_shared<connection_pool>(host, port, pool_size);
```

---

## Workload-Specific Tuning

### High Throughput (Many Requests/sec)

**Characteristics**:
- Request rate: > 10K/sec
- Message size: < 10KB
- Many concurrent connections

**Configuration**:

```cpp
auto config = network_config::production();

// Increase thread pool for parallelism
config.thread_pool.worker_count = std::thread::hardware_concurrency() * 2;
config.thread_pool.queue_capacity = 50000;

// Reduce logging overhead
config.logger.min_level = log_level::warn;
config.logger.async_logging = true;

// Reduce monitoring frequency
config.monitoring.metrics_interval = std::chrono::seconds(10);
```

**Additional Tips**:

```cpp
// Use large connection pool
auto pool = std::make_shared<connection_pool>(host, port, 100);

// OS tuning (see OS-Level Tuning section)
// - Increase socket buffer sizes
// - Increase file descriptor limits
// - Enable TCP fast open
```

### Low Latency (Real-Time)

**Characteristics**:
- Latency requirement: < 1ms P99
- Predictable performance critical
- May sacrifice throughput for latency

**Configuration**:

```cpp
auto config = network_config::production();

// Over-provision threads to avoid queuing
config.thread_pool.worker_count = std::thread::hardware_concurrency() * 2;
config.thread_pool.queue_capacity = 100;  // Fail fast if overloaded

// Minimize logging overhead
config.logger.min_level = log_level::error;
config.logger.async_logging = true;

// Disable monitoring during critical path
config.monitoring.enabled = false;
```

**Additional Tips**:

```cpp
// Pre-warm connection pool
pool->initialize();

// Avoid memory allocation in hot path
// - Use object pools
// - Pre-allocate buffers
// - Avoid dynamic_cast

// OS tuning
// - Pin process to isolated CPU cores
// - Use SCHED_FIFO scheduling (if permitted)
// - Disable CPU frequency scaling
```

### High Connection Count

**Characteristics**:
- Connections: > 10K concurrent
- Most connections idle
- Memory efficiency critical

**Configuration**:

```cpp
auto config = network_config::production();

// Moderate thread count (connections are mostly idle)
config.thread_pool.worker_count = std::thread::hardware_concurrency();
config.thread_pool.queue_capacity = 10000;

// Standard logging
config.logger.min_level = log_level::info;
config.logger.async_logging = true;
```

**Additional Tips**:

```cpp
// Connection pool not beneficial (unique connections)
// Instead, focus on per-connection memory efficiency

// OS tuning
// - Increase file descriptor limit (ulimit -n 100000)
// - Increase ephemeral port range
// - Enable TCP connection recycling
// - Tune TCP keepalive timers
```

### Large Message Transfer

**Characteristics**:
- Message size: > 1MB
- Throughput priority
- Memory management critical

**Configuration**:

```cpp
auto config = network_config::production();

// Fewer threads (I/O bound, not CPU bound)
config.thread_pool.worker_count = std::thread::hardware_concurrency();
config.thread_pool.queue_capacity = 100;

// Reduce log verbosity for large transfers
config.logger.min_level = log_level::warn;
```

**Additional Tips**:

```cpp
// Use move semantics to avoid copies
std::vector<uint8_t> large_data = /* ... */;
client->send_packet(std::move(large_data));  // Move, don't copy

// OS tuning
// - Increase socket buffer sizes
// - Increase TCP window size
// - Enable TCP window scaling
```

---

## Common Anti-Patterns

### 1. Synchronous Operations in Callbacks

**Problem**: Blocking in async callbacks blocks the I/O thread.

```cpp
// BAD: Blocking database query in message handler
server->set_receive_callback([](auto session, const auto& data) {
    auto result = database.query(data);  // BLOCKS I/O thread!
    session->send_packet(serialize(result));
});
```

**Solution**: Offload blocking work to thread pool.

```cpp
// GOOD: Async processing in thread pool
server->set_receive_callback([](auto session, const auto& data) {
    thread_pool.enqueue([session, data]() {
        auto result = database.query(data);  // Runs in worker thread
        session->send_packet(serialize(result));
    });
});
```

### 2. Excessive Memory Allocation

**Problem**: Allocating on every message causes memory churn.

```cpp
// BAD: Allocates new vector on every message
void process(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> copy = data;  // Unnecessary allocation
    // Process copy...
}
```

**Solution**: Use move semantics and buffer pools.

```cpp
// GOOD: Move data, reuse buffers
void process(std::vector<uint8_t>&& data) {
    // Process data directly (no copy)
    // ...
}

// BETTER: Use buffer pool for zero-allocation
auto buffer = buffer_pool.acquire();
process_into(data, *buffer);
buffer_pool.release(std::move(buffer));
```

### 3. Synchronous Logging in Hot Path

**Problem**: Sync logging can add 1-10ms per log call.

```cpp
// BAD: Logging every message
server->set_receive_callback([](auto session, const auto& data) {
    log_info("Received " + std::to_string(data.size()) + " bytes");  // Slow!
    process(data);
});
```

**Solution**: Use async logging or reduce log frequency.

```cpp
// GOOD: Async logging
config.logger.async_logging = true;

// BETTER: Sample logging (1% of messages)
static std::atomic<uint64_t> counter{0};
server->set_receive_callback([](auto session, const auto& data) {
    if (counter.fetch_add(1) % 100 == 0) {
        log_info("Received message (sample)");
    }
    process(data);
});
```

### 4. Too Many Threads

**Problem**: Excessive threads cause context switching overhead.

```cpp
// BAD: Creating too many threads
config.thread_pool.worker_count = 256;  // On 8-core system
```

**Solution**: Match thread count to workload type.

```cpp
// GOOD: Start with hardware concurrency
config.thread_pool.worker_count = std::thread::hardware_concurrency();

// Profile and adjust based on measurements
// - I/O bound: 1.5-2x hardware_concurrency
// - CPU bound: 1x hardware_concurrency
```

### 5. Ignoring Backpressure

**Problem**: Sending without checking for backpressure.

```cpp
// BAD: Ignoring send result
for (const auto& msg : messages) {
    client->send_packet(std::vector<uint8_t>(msg));  // May fail silently
}
```

**Solution**: Handle backpressure and flow control.

```cpp
// GOOD: Check send result and handle backpressure
for (const auto& msg : messages) {
    auto result = client->send_packet(std::vector<uint8_t>(msg));
    if (result.is_err()) {
        if (result.error().code == error_codes::network_system::would_block) {
            // Implement backpressure: wait, drop, or queue
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            // Retry...
        } else {
            // Handle other errors
            log_error("Send failed: " + result.error().message);
            break;
        }
    }
}
```

---

## Profiling Guide

### CPU Profiling

#### Using perf (Linux)

```bash
# Record CPU profile
perf record -F 99 -g ./your_server &
SERVER_PID=$!

# Run workload
./benchmark_client

# Stop server
kill $SERVER_PID

# View report
perf report

# Generate flamegraph (requires FlameGraph tools)
perf script | stackcollapse-perf.pl | flamegraph.pl > flamegraph.svg
```

#### Using Instruments (macOS)

```bash
# Profile with Time Profiler
xcrun xctrace record --template "Time Profiler" --launch -- ./your_server

# Or use Instruments GUI
open -a Instruments
# Select "Time Profiler" template
```

#### Using Visual Studio Profiler (Windows)

```
1. Debug > Performance Profiler
2. Select "CPU Usage"
3. Start profiling
4. Run workload
5. Stop profiling and analyze
```

### Memory Profiling

See [Memory Profiling Guide](MEMORY_PROFILING.md) for detailed instructions.

**Quick Check**:

```bash
# Check memory usage over time
while true; do
    ps aux | grep your_server | grep -v grep
    sleep 5
done

# Detect memory leaks with Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./your_server
```

### Latency Measurement

```bash
# Using built-in benchmarks
cd build
./benchmarks/latency_benchmark --host localhost --port 5555 --iterations 10000

# View latency distribution
# - P50 (median)
# - P95 (95th percentile)
# - P99 (99th percentile)
# - P999 (99.9th percentile)
```

### Throughput Measurement

```bash
# Using built-in benchmarks
./benchmarks/message_throughput_bench

# Results show:
# - Messages per second
# - Bytes per second
# - CPU utilization
```

---

## OS-Level Tuning

### Linux

#### File Descriptor Limits

```bash
# Check current limit
ulimit -n

# Temporary increase (current shell)
ulimit -n 65536

# Permanent increase (edit /etc/security/limits.conf)
* soft nofile 65536
* hard nofile 65536

# Verify after reboot
ulimit -n
```

#### Socket Buffer Sizes

```bash
# Check current settings
sysctl net.core.rmem_max
sysctl net.core.wmem_max

# Increase for high throughput
sudo sysctl -w net.core.rmem_max=26214400  # 25MB
sudo sysctl -w net.core.wmem_max=26214400  # 25MB

# Make permanent (edit /etc/sysctl.conf)
net.core.rmem_max = 26214400
net.core.wmem_max = 26214400

# Apply changes
sudo sysctl -p
```

#### TCP Settings

```bash
# Enable TCP fast open (reduces handshake latency)
sudo sysctl -w net.ipv4.tcp_fastopen=3

# Increase ephemeral port range
sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"

# Enable TCP window scaling
sudo sysctl -w net.ipv4.tcp_window_scaling=1

# Make permanent
echo "net.ipv4.tcp_fastopen = 3" | sudo tee -a /etc/sysctl.conf
echo "net.ipv4.ip_local_port_range = 1024 65535" | sudo tee -a /etc/sysctl.conf
echo "net.ipv4.tcp_window_scaling = 1" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

#### CPU Affinity (Low Latency)

```bash
# Pin process to specific CPUs (isolate from other processes)
taskset -c 0-3 ./your_server

# Use real-time scheduling (requires privileges)
sudo chrt -f 50 ./your_server
```

### macOS

#### File Descriptor Limits

```bash
# Check current limit
ulimit -n

# Temporary increase
ulimit -n 10240

# Permanent increase (edit /etc/launchd.conf)
sudo launchctl limit maxfiles 65536 200000
```

### Windows

#### TCP Settings

```powershell
# Increase maximum number of connections
netsh int ipv4 set dynamicport tcp start=1024 num=64512

# Enable TCP chimney offload
netsh int tcp set global chimney=enabled

# Disable Nagle algorithm (low latency)
# (Application level: set TCP_NODELAY socket option)
```

---

## Advanced Topics

### Zero-Copy Techniques

Minimize data copies for maximum performance:

```cpp
// Use move semantics
std::vector<uint8_t> data = create_data();
client->send_packet(std::move(data));  // No copy

// Avoid intermediate buffers
// BAD:
auto data = receive();
auto parsed = parse(data);      // Copy 1
auto processed = process(parsed);  // Copy 2
send(processed);                // Copy 3

// GOOD:
auto data = receive();
process_in_place(data);         // No copies
send(std::move(data));
```

### Object Pooling

Reuse objects to avoid allocation overhead:

```cpp
// Message buffer pool
class buffer_pool {
public:
    auto acquire() -> std::unique_ptr<std::vector<uint8_t>> {
        std::lock_guard<std::mutex> lock(mutex_);
        if (pool_.empty()) {
            return std::make_unique<std::vector<uint8_t>>();
        }
        auto buffer = std::move(pool_.back());
        pool_.pop_back();
        return buffer;
    }

    auto release(std::unique_ptr<std::vector<uint8_t>> buffer) -> void {
        buffer->clear();
        std::lock_guard<std::mutex> lock(mutex_);
        pool_.push_back(std::move(buffer));
    }

private:
    std::vector<std::unique_ptr<std::vector<uint8_t>>> pool_;
    std::mutex mutex_;
};
```

### Lock-Free Queues

For ultra-low latency, consider lock-free data structures:

```cpp
// Use lock-free queue for task submission
#include <atomic>
#include <memory>

template<typename T>
class lockfree_queue {
    // Implementation using atomic operations
    // - CAS (compare-and-swap)
    // - Memory ordering guarantees
};
```

**Tradeoff**: Increased complexity, harder to debug

### NUMA Awareness

For multi-socket systems:

```bash
# Check NUMA topology
numactl --hardware

# Run on specific NUMA node
numactl --cpunodebind=0 --membind=0 ./your_server

# Interleave memory across nodes (for balanced access)
numactl --interleave=all ./your_server
```

---

## Performance Checklist

### Development

- [ ] Use `network_config::development()` for full logging
- [ ] Enable all monitoring for debugging
- [ ] Use sync logging for immediate output

### Production

- [ ] Use `network_config::production()` for optimized settings
- [ ] Enable async logging
- [ ] Configure appropriate thread pool size
- [ ] Use connection pooling for repeated connections
- [ ] Monitor metrics regularly
- [ ] Set up OS-level tuning for your platform

### Benchmarking

- [ ] Disable monitoring to eliminate overhead
- [ ] Use consistent hardware and environment
- [ ] Run multiple iterations for statistical significance
- [ ] Measure baseline before optimization
- [ ] Document configuration used

---

## See Also

- [Benchmarks](BENCHMARKS.md) - Performance measurements and results
- [Connection Pooling](CONNECTION_POOLING.md) - Connection pool usage guide
- [Memory Profiling](MEMORY_PROFILING.md) - Memory optimization techniques
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues and solutions
