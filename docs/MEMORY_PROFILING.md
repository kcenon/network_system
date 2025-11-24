# Memory Profiling Guide

This guide explains how to profile memory usage in the Network System library and detect memory leaks.

## Quick Start

```bash
# Build the memory profiling demo
cmake --build build --target network_memory_profile_demo

# Run all profiling scenarios
./build/bin/network_memory_profile_demo all

# Run specific scenario
./build/bin/network_memory_profile_demo connections
./build/bin/network_memory_profile_demo messages
./build/bin/network_memory_profile_demo stability
```

## Expected Memory Usage

### Per-Connection Overhead

| Component | Memory |
|-----------|--------|
| TCP Socket | ~1 KB |
| Session State | ~2 KB |
| Buffers (default) | ~8 KB |
| **Total** | **~11 KB** |

### Server Baseline

| Connections | Expected RSS |
|-------------|--------------|
| 0 | ~5 MB |
| 100 | ~6 MB |
| 1,000 | ~15 MB |
| 10,000 | ~115 MB |

*Note: Actual values may vary by platform and configuration.*

## Profiling Tools

### 1. AddressSanitizer (ASAN)

Best for: Detecting memory corruption, buffer overflows, use-after-free.

```bash
# Build with ASAN
cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
cmake --build build_asan

# Run with leak detection
ASAN_OPTIONS="detect_leaks=1" ./build_asan/bin/network_memory_profile_demo all
```

Or use the provided script:
```bash
./scripts/run_with_asan.sh
```

### 2. ThreadSanitizer (TSAN)

Best for: Detecting data races and threading issues.

```bash
# Build with TSAN
cmake -B build_tsan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=thread" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"
cmake --build build_tsan

# Run
./build_tsan/bin/network_memory_profile_demo all
```

### 3. Valgrind (Linux)

Best for: Detailed leak detection, memory access analysis.

```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./build/bin/network_memory_profile_demo connections
```

### 4. Heaptrack (Linux)

Best for: Tracking allocations over time, finding allocation hotspots.

```bash
heaptrack ./build/bin/network_memory_profile_demo messages
heaptrack_gui heaptrack.network_memory_profile_demo.*.gz
```

### 5. macOS Instruments

Best for: macOS-specific profiling with GUI.

```bash
# Leaks template
xcrun xctrace record --template "Leaks" \
    --launch -- ./build/bin/network_memory_profile_demo all

# Allocations template
xcrun xctrace record --template "Allocations" \
    --launch -- ./build/bin/network_memory_profile_demo connections
```

## Common Memory Issues

### Issue: Memory grows over time

**Symptoms**: RSS increases continuously during operation.

**Diagnosis**:
```bash
./build/bin/network_memory_profile_demo stability
```

**Common causes**:
1. Session cleanup not happening
2. Message queue growing unbounded
3. Connection pool not releasing

**Solutions**:
1. Ensure proper cleanup on disconnect
2. Set message queue limits
3. Configure connection pool timeout

### Issue: High per-connection memory

**Symptoms**: Memory usage much higher than expected per connection.

**Diagnosis**:
```bash
./build/bin/network_memory_profile_demo connections
```

**Solutions**:
1. Reduce buffer sizes
2. Enable buffer pooling
3. Check for retained references

### Issue: Memory not released after cleanup

**Symptoms**: Memory remains high after connections close.

**Diagnosis**:
```bash
# Run ASAN
ASAN_OPTIONS="detect_leaks=1" ./build_asan/bin/your_program
```

**Solutions**:
1. Fix any reported leaks
2. Check shared_ptr cycles
3. Verify cleanup order

## Profiling Scenarios

### Connection Profiling

Measures memory overhead per connection:

```cpp
// Creates many connections and measures:
// - Memory before/after each batch
// - Per-connection overhead
// - Cleanup effectiveness
./build/bin/network_memory_profile_demo connections
```

### Message Throughput Profiling

Measures memory during message transmission:

```cpp
// Sends many messages and measures:
// - Memory growth during transmission
// - Queue buffering overhead
// - Processing cleanup
./build/bin/network_memory_profile_demo messages
```

### Long-Running Stability

Detects memory leaks over time:

```cpp
// Repeatedly connects/disconnects and measures:
// - Memory drift over time
// - Peak memory usage
// - Leak indicators
./build/bin/network_memory_profile_demo stability
```

## Best Practices

1. **Regular profiling**: Run memory profiling in CI pipelines
2. **Baseline tracking**: Compare against expected values after changes
3. **Stress testing**: Profile under high load conditions
4. **Leak checking**: Run ASAN/Valgrind before releases
5. **Platform testing**: Memory behavior varies by OS

## CI Integration

Add memory profiling to your CI pipeline:

```yaml
# Example GitHub Actions step
- name: Memory Profile
  run: |
    cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="-fsanitize=address"
    cmake --build build_asan --target network_memory_profile_demo
    ASAN_OPTIONS="detect_leaks=1:halt_on_error=1" \
        ./build_asan/bin/network_memory_profile_demo stability
```

## Platform Notes

### macOS

- Uses `mach_task_basic_info` for memory stats
- RSS reported in bytes
- Instruments provides best GUI experience

### Linux

- Uses `/proc/self/statm` for memory stats
- Valgrind and Heaptrack available
- RSS in pages (multiply by page size)

### Windows

- Use Visual Studio Diagnostic Tools
- Consider DrMemory for leak detection
- Memory stats via Windows API

## Interpreting Results

### Good results

```
Memory delta from baseline: 50 KB
Memory leak indicator: OK
```

### Potential issues

```
Memory delta from baseline: 5000 KB
Memory leak indicator: POSSIBLE LEAK
```

When you see potential issues:
1. Run with ASAN to find specific leaks
2. Use Valgrind for detailed analysis
3. Check recent code changes
4. Review cleanup code paths

## Further Reading

- [AddressSanitizer Documentation](https://clang.llvm.org/docs/AddressSanitizer.html)
- [Valgrind Quick Start](https://valgrind.org/docs/manual/quick-start.html)
- [Heaptrack GitHub](https://github.com/KDE/heaptrack)
