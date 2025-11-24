# NET-303: Write Performance Tuning Guide

| Field | Value |
|-------|-------|
| **ID** | NET-303 |
| **Title** | Write Performance Tuning Guide |
| **Category** | DOC |
| **Priority** | LOW |
| **Status** | TODO |
| **Est. Duration** | 3-4 days |
| **Dependencies** | None |
| **Target Version** | v1.9.0 |

---

## What (Problem Statement)

### Current Problem
Users lack guidance on optimizing network system performance:
- No documentation on configuration options affecting performance
- Buffer sizes and their trade-offs not explained
- Thread pool tuning not documented
- No guidance on common performance pitfalls

### Goal
Create comprehensive performance tuning documentation:
- Configuration reference with performance impact
- Tuning guides for different workloads
- Profiling and benchmarking instructions
- Common anti-patterns and fixes

### Deliverables
1. `docs/PERFORMANCE_TUNING.md` - Main tuning guide
2. `docs/PERFORMANCE_REFERENCE.md` - Configuration reference
3. Example configurations for common scenarios
4. Benchmarking scripts and methodology

---

## How (Implementation Plan)

### Step 1: Document Configuration Reference
```markdown
# Performance Configuration Reference

## TCP/Messaging Configuration

### Buffer Sizes

| Configuration | Default | Range | Impact |
|--------------|---------|-------|--------|
| `receive_buffer_size` | 8KB | 1KB-1MB | Memory vs throughput |
| `send_buffer_size` | 8KB | 1KB-1MB | Memory vs throughput |
| `max_message_size` | 1MB | 1KB-100MB | Memory vs capability |

**Tuning Guide:**
- Increase buffers for high-throughput scenarios
- Decrease for memory-constrained environments
- Match to typical message size for best efficiency

```cpp
// High throughput configuration
messaging_server server("server");
server.set_receive_buffer_size(65536);  // 64KB
server.set_send_buffer_size(65536);     // 64KB
```

### Connection Limits

| Configuration | Default | Impact |
|--------------|---------|--------|
| `max_connections` | 10000 | Memory, file descriptors |
| `connection_timeout` | 30s | Resource cleanup |
| `keep_alive_timeout` | 60s | Connection reuse |

### Thread Pool

| Configuration | Default | Impact |
|--------------|---------|--------|
| `thread_count` | hardware_concurrency | CPU utilization |
| `io_threads` | 1 | I/O parallelism |

```cpp
// For I/O bound workloads
server.set_thread_count(std::thread::hardware_concurrency() * 2);

// For CPU bound workloads
server.set_thread_count(std::thread::hardware_concurrency());
```
```

### Step 2: Create Workload-Specific Guides
```markdown
# Tuning for Specific Workloads

## High Throughput (Many Small Messages)

**Characteristics:**
- Message size: < 1KB
- Message rate: > 100K/sec
- Connection count: < 1000

**Configuration:**
```cpp
messaging_server server("high-throughput");
server.set_receive_buffer_size(4096);     // Small buffers
server.set_send_buffer_size(4096);
server.set_max_pending_messages(10000);   // Large queue
server.set_thread_count(cpu_count);
```

**OS Tuning:**
```bash
# Increase socket buffer limits
sudo sysctl -w net.core.rmem_max=26214400
sudo sysctl -w net.core.wmem_max=26214400
```

## Low Latency (Real-time)

**Characteristics:**
- Latency requirement: < 1ms P99
- Message size: Variable
- Predictable performance critical

**Configuration:**
```cpp
messaging_server server("low-latency");
server.set_tcp_nodelay(true);             // Disable Nagle
server.set_thread_count(cpu_count * 2);   // Over-provision
server.set_receive_buffer_size(8192);
```

**OS Tuning:**
```bash
# Pin to isolated CPUs
taskset -c 0-3 ./server

# Use real-time scheduling (if permitted)
chrt -f 50 ./server
```

## Large File Transfer

**Characteristics:**
- Message size: > 1MB
- Throughput priority
- Memory management critical

**Configuration:**
```cpp
messaging_server server("file-transfer");
server.set_receive_buffer_size(262144);   // 256KB
server.set_send_buffer_size(262144);
server.set_max_message_size(100 * 1024 * 1024);  // 100MB
server.enable_zero_copy(true);
```

## High Connection Count

**Characteristics:**
- Connections: > 10K
- Memory efficiency critical
- Most connections idle

**Configuration:**
```cpp
messaging_server server("high-connections");
server.set_receive_buffer_size(2048);     // Minimize per-connection memory
server.set_send_buffer_size(2048);
server.set_max_connections(100000);
server.set_connection_timeout(std::chrono::seconds(60));
```

**OS Tuning:**
```bash
# Increase file descriptor limit
ulimit -n 200000

# Increase ephemeral port range
sudo sysctl -w net.ipv4.ip_local_port_range="1024 65535"
```
```

### Step 3: Document Common Anti-Patterns
```markdown
# Performance Anti-Patterns

## 1. Synchronous Callbacks with Blocking Operations

**Problem:**
```cpp
// BAD: Blocking in message handler
server.set_message_handler([](auto id, auto data) {
    auto result = database.query(data);  // Blocks!
    return result;
});
```

**Solution:**
```cpp
// GOOD: Async processing
server.set_message_handler([](auto id, auto data) {
    thread_pool.enqueue([=]() {
        auto result = database.query(data);
        server.send(id, result);
    });
});
```

## 2. Excessive Memory Allocation

**Problem:**
```cpp
// BAD: Allocates on every message
void process(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> copy = data;  // Unnecessary copy
    // ...
}
```

**Solution:**
```cpp
// GOOD: Use move semantics and buffer pools
void process(std::vector<uint8_t>&& data) {
    auto buffer = buffer_pool.acquire(data.size());
    std::move(data.begin(), data.end(), buffer->begin());
    // ...
}
```

## 3. Small Buffer with Large Messages

**Problem:**
- Buffer size: 1KB
- Message size: 10KB
- Results in 10 read operations per message

**Solution:**
```cpp
// Match buffer to typical message size
server.set_receive_buffer_size(16384);  // 16KB for 10KB messages
```

## 4. Too Many Threads

**Problem:**
- CPU cores: 8
- Thread count: 64
- Result: Excessive context switching

**Solution:**
```cpp
// Start with hardware concurrency
server.set_thread_count(std::thread::hardware_concurrency());
// Increase only if profiling shows benefit
```

## 5. Ignoring Backpressure

**Problem:**
```cpp
// BAD: Ignoring send failure
for (auto& msg : messages) {
    client.send(msg);  // May fail silently
}
```

**Solution:**
```cpp
// GOOD: Handle backpressure
for (auto& msg : messages) {
    auto result = client.send(msg);
    if (!result) {
        if (result.error() == error_code::would_block) {
            // Wait and retry
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
    }
}
```
```

### Step 4: Profiling Instructions
```markdown
# Profiling Guide

## CPU Profiling

### Using perf (Linux)
```bash
# Record CPU samples
perf record -g ./your_server &
# Run workload
./benchmark_client
# Stop server
kill %1
# Analyze
perf report
```

### Using Instruments (macOS)
```bash
xcrun xctrace record --template "Time Profiler" --launch -- ./your_server
```

## Memory Profiling

See [NET-206](NET-206-memory-profiling.md) for detailed memory profiling guide.

## Network Profiling

### Throughput Measurement
```bash
# Using iperf3
iperf3 -s &  # Server
iperf3 -c localhost -t 10  # Client
```

### Latency Measurement
```bash
# Using our benchmark tool
./build/benchmarks/latency_benchmark --host localhost --port 5555 --iterations 10000
```

## Flame Graphs
```bash
# Generate flame graph
perf record -F 99 -g ./your_server &
./benchmark_client
kill %1

perf script | ./FlameGraph/stackcollapse-perf.pl > out.folded
./FlameGraph/flamegraph.pl out.folded > flamegraph.svg
```
```

---

## Test (Verification Plan)

### Documentation Review
1. Technical accuracy review
2. Test all code snippets compile
3. Verify OS commands work

### Benchmark Verification
```bash
# Apply configurations and benchmark
for config in high_throughput low_latency high_connections; do
    ./benchmark --config $config --duration 60
done
```

### User Testing
- Have developer follow guide without prior knowledge
- Collect feedback on unclear sections
- Measure improvement from default config

---

## Acceptance Criteria

- [ ] Configuration reference complete
- [ ] Workload-specific guides for 4+ scenarios
- [ ] Anti-patterns documented with solutions
- [ ] Profiling instructions tested
- [ ] Example configurations provided
- [ ] All code snippets compile
- [ ] OS tuning commands verified

---

## Notes

- Performance varies significantly by hardware
- Include disclaimers about environment-specific tuning
- Reference benchmarks from NET-202
- Keep updated as new features are added
