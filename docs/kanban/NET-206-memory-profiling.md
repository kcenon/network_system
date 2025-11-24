# NET-206: Add Memory Profiling Tool Examples

| Field | Value |
|-------|-------|
| **ID** | NET-206 |
| **Title** | Add Memory Profiling Tool Examples |
| **Category** | DOC |
| **Priority** | MEDIUM |
| **Status** | DONE |
| **Est. Duration** | 2-3 days |
| **Dependencies** | None |
| **Target Version** | v1.8.0 |

---

## What (Problem Statement)

### Current Problem
Users lack guidance on profiling memory usage:
- No examples of using Valgrind/ASAN with the library
- Memory profiling sample code missing
- No documentation on expected memory patterns
- Leak detection workflow not documented

### Goal
Provide comprehensive memory profiling examples and documentation:
- Sample code for memory profiling
- Integration with common profiling tools
- Expected memory usage patterns documentation
- Leak detection best practices

### Deliverables
1. `samples/memory_profile_demo.cpp` - Interactive memory profiling demo
2. `docs/MEMORY_PROFILING.md` - Profiling guide
3. Scripts for running with various sanitizers
4. Expected baseline documentation

---

## How (Implementation Plan)

### Step 1: Create Memory Profile Demo
```cpp
// samples/memory_profile_demo.cpp

#include "network_system/core/messaging_server.h"
#include "network_system/core/messaging_client.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace network_system::core;

// Memory tracking utilities
size_t get_current_rss() {
#if defined(__APPLE__)
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;  // bytes on macOS
#elif defined(__linux__)
    std::ifstream statm("/proc/self/statm");
    size_t pages;
    statm >> pages;  // Skip first value
    statm >> pages;  // RSS in pages
    return pages * sysconf(_SC_PAGESIZE);
#else
    return 0;
#endif
}

void print_memory_stats(const std::string& label) {
    std::cout << "[" << label << "] RSS: "
              << get_current_rss() / (1024 * 1024) << " MB" << std::endl;
}

// Scenario 1: Connection memory overhead
void profile_connections(int num_connections) {
    std::cout << "\n=== Profiling Connection Memory ===\n";

    print_memory_stats("Before server start");

    messaging_server server("profile-server");
    server.start_server(5555);

    print_memory_stats("After server start");

    std::vector<std::unique_ptr<messaging_client>> clients;

    for (int i = 0; i < num_connections; ++i) {
        auto client = std::make_unique<messaging_client>("client-" + std::to_string(i));
        client->start_client("localhost", 5555);
        clients.push_back(std::move(client));

        if ((i + 1) % 100 == 0) {
            print_memory_stats("After " + std::to_string(i + 1) + " connections");
        }
    }

    size_t memory_after = get_current_rss();
    std::cout << "\nMemory per connection: ~"
              << (memory_after - get_current_rss()) / num_connections / 1024
              << " KB\n";

    // Cleanup
    clients.clear();
    std::this_thread::sleep_for(std::chrono::seconds(1));

    print_memory_stats("After cleanup");
}

// Scenario 2: Message throughput memory
void profile_message_throughput(int num_messages) {
    std::cout << "\n=== Profiling Message Memory ===\n";

    messaging_server server("profile-server");
    server.start_server(5556);

    messaging_client client("profile-client");
    client.start_client("localhost", 5556);

    print_memory_stats("Before messages");

    for (int i = 0; i < num_messages; ++i) {
        std::vector<uint8_t> msg(1024, static_cast<uint8_t>(i % 256));
        client.send_packet(std::move(msg));

        if ((i + 1) % 10000 == 0) {
            print_memory_stats("After " + std::to_string(i + 1) + " messages");
        }
    }

    print_memory_stats("After all messages");

    std::this_thread::sleep_for(std::chrono::seconds(2));
    print_memory_stats("After processing complete");
}

// Scenario 3: Long-running stability
void profile_long_running(int duration_seconds) {
    std::cout << "\n=== Profiling Long-Running Stability ===\n";

    messaging_server server("profile-server");
    server.start_server(5557);

    print_memory_stats("Start");

    auto start = std::chrono::steady_clock::now();
    int iterations = 0;

    while (std::chrono::steady_clock::now() - start <
           std::chrono::seconds(duration_seconds)) {

        // Connect, send, disconnect
        messaging_client client("temp-client");
        client.start_client("localhost", 5557);
        client.send_packet({'t', 'e', 's', 't'});
        client.disconnect();

        iterations++;

        if (iterations % 100 == 0) {
            print_memory_stats("After " + std::to_string(iterations) + " cycles");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    print_memory_stats("Final");
    std::cout << "Total iterations: " << iterations << std::endl;
}

int main(int argc, char* argv[]) {
    std::string mode = argc > 1 ? argv[1] : "all";

    if (mode == "connections" || mode == "all") {
        profile_connections(500);
    }

    if (mode == "messages" || mode == "all") {
        profile_message_throughput(50000);
    }

    if (mode == "stability" || mode == "all") {
        profile_long_running(30);  // 30 seconds
    }

    std::cout << "\n=== Profiling Complete ===\n";
    return 0;
}
```

### Step 2: Create Profiling Scripts
```bash
#!/bin/bash
# scripts/run_memory_profile.sh

set -e

BUILD_DIR="${BUILD_DIR:-build_profile}"
SAMPLE="memory_profile_demo"

echo "=== Building with profiling support ==="
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer"

cmake --build "$BUILD_DIR" --target "$SAMPLE"

echo ""
echo "=== Running with different profilers ==="

# Option 1: Simple run with RSS tracking
echo "--- Native memory tracking ---"
"./$BUILD_DIR/samples/$SAMPLE" all

# Option 2: Valgrind (if available)
if command -v valgrind &> /dev/null; then
    echo ""
    echo "--- Valgrind Memcheck ---"
    valgrind --leak-check=full \
             --show-leak-kinds=all \
             --track-origins=yes \
             "./$BUILD_DIR/samples/$SAMPLE" connections
fi

# Option 3: Heaptrack (if available)
if command -v heaptrack &> /dev/null; then
    echo ""
    echo "--- Heaptrack ---"
    heaptrack "./$BUILD_DIR/samples/$SAMPLE" messages
    echo "Run 'heaptrack_gui heaptrack.$SAMPLE.*.gz' to view results"
fi

echo ""
echo "=== Done ==="
```

```bash
#!/bin/bash
# scripts/run_with_asan.sh

set -e

BUILD_DIR="build_asan"

echo "=== Building with AddressSanitizer ==="
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"

cmake --build "$BUILD_DIR" --target memory_profile_demo

echo ""
echo "=== Running with ASAN ==="
ASAN_OPTIONS="detect_leaks=1:halt_on_error=0" \
    "./$BUILD_DIR/samples/memory_profile_demo" all
```

### Step 3: Create Documentation
```markdown
# Memory Profiling Guide

## Overview

This guide explains how to profile memory usage in the Network System library
and detect memory leaks.

## Quick Start

```bash
# Run built-in memory profiling demo
./build/samples/memory_profile_demo all
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

## Profiling Tools

### 1. AddressSanitizer (ASAN)

Best for: Detecting memory corruption, buffer overflows, use-after-free.

```bash
# Build with ASAN
cmake -B build_asan -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address"
cmake --build build_asan

# Run
ASAN_OPTIONS="detect_leaks=1" ./build_asan/your_program
```

### 2. Valgrind

Best for: Detailed leak detection, memory access analysis.

```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./build/your_program
```

### 3. Heaptrack

Best for: Tracking allocations over time, finding allocation hotspots.

```bash
heaptrack ./build/your_program
heaptrack_gui heaptrack.your_program.*.gz
```

### 4. macOS Instruments

Best for: macOS-specific profiling with GUI.

```bash
xcrun xctrace record --template "Leaks" --launch -- ./build/your_program
```

## Common Memory Issues

### Issue: Memory grows over time

**Symptoms**: RSS increases continuously during operation.

**Diagnosis**:
```bash
./scripts/run_memory_profile.sh stability
```

**Common causes**:
1. Session cleanup not happening
2. Message queue growing unbounded
3. Connection pool not releasing

### Issue: High per-connection memory

**Symptoms**: Memory usage much higher than expected.

**Diagnosis**:
```bash
./scripts/run_memory_profile.sh connections
```

**Solutions**:
1. Reduce buffer sizes
2. Enable buffer pooling
3. Check for retained references

## Best Practices

1. **Regular profiling**: Run memory profiling in CI
2. **Baseline tracking**: Compare against expected values
3. **Stress testing**: Profile under high load
4. **Leak checking**: Run ASAN/Valgrind before releases
```

---

## Test (Verification Plan)

### Build Tests
```bash
# Ensure demo compiles
cmake --build build --target memory_profile_demo

# Ensure ASAN build works
./scripts/run_with_asan.sh
```

### Demo Verification
```bash
# Run each profile mode
./build/samples/memory_profile_demo connections
./build/samples/memory_profile_demo messages
./build/samples/memory_profile_demo stability
```

### Documentation Verification
1. Follow guide steps exactly
2. Verify output matches documentation
3. Test on multiple platforms (Linux, macOS)

---

## Acceptance Criteria

- [ ] Memory profiling demo compiles and runs
- [ ] ASAN build script works
- [ ] Valgrind script works (on Linux)
- [ ] Documentation covers all major profilers
- [ ] Expected memory baselines documented
- [ ] Troubleshooting section complete
- [ ] Scripts tested on CI

---

## Notes

- Memory measurements vary by platform
- Valgrind significantly slows execution
- ASAN adds ~2x memory overhead
- Update baselines when major changes occur
