# Design Decisions

> **Language:** **English** | [한국어](DESIGN_DECISIONS_KO.md)

This document explains key design decisions in network_system, particularly patterns that may seem unusual but are intentional and necessary.

**Version**: 1.0.0
**Last Updated**: 2025-12-27

---

## Table of Contents

- [Intentional Leak Pattern in Thread Integration](#intentional-leak-pattern-in-thread-integration)
- [Send-Side Backpressure Mechanism](#send-side-backpressure-mechanism)
- [References](#references)

---

## Intentional Leak Pattern in Thread Integration

### Overview

Several core classes in network_system use an **Intentional Leak pattern** where memory is deliberately not freed during destruction. This is not a bug but a carefully considered design decision.

### Location

The following classes use this pattern:

**Thread Integration:**
- `include/kcenon/network/integration/thread_integration.h` - `basic_thread_pool`
- `include/kcenon/network/integration/thread_integration.h` - `thread_integration_manager`

**IO Context Management:**
- `include/kcenon/network/integration/io_context_thread_manager.h` - `io_context_thread_manager`
- `include/kcenon/network/core/network_context.h` - `network_context`

**Client Components:**
- `src/core/messaging_client.cpp` - `messaging_client` (io_context)

### Problem

During application shutdown, C++ destroys static objects in reverse initialization order. When `thread_integration_manager` (a singleton) is destroyed:

1. Its destructor signals worker threads to stop
2. Worker threads may still be executing tasks
3. Tasks may reference `impl` members (queues, mutexes, condition variables)
4. If `impl` is destroyed before threads complete, undefined behavior occurs

### Symptoms Without This Pattern

| Platform | Symptom |
|----------|---------|
| Linux | `malloc.c: assertion failed` or heap corruption |
| macOS | `malloc: double free` or heap corruption |
| Windows | Access violation in thread pool destructor |
| All | Sporadic crashes during application shutdown |
| ASAN | `heap-use-after-free` reports |

### Solution: Intentional Leak

Instead of:
```cpp
std::unique_ptr<impl> pimpl_;  // Destroyed in destructor - DANGEROUS
```

We use:
```cpp
std::shared_ptr<impl> pimpl_;  // With no-op deleter - SAFE
```

Initialized as:
```cpp
pimpl_ = std::shared_ptr<impl>(new impl(), [](impl*) {
    // Intentionally do nothing - leak the memory
    // This is safe because:
    // 1. Only ~100-200 bytes leaked per singleton
    // 2. Process is about to exit anyway
    // 3. OS reclaims all memory on process termination
});
```

### Why This Is Safe

1. **Minimal Memory Impact**: Only ~100-200 bytes per singleton instance
2. **Shutdown Context**: Only happens when the process is terminating
3. **OS Cleanup**: Operating system reclaims all process memory on exit
4. **No Runtime Impact**: Pattern only affects shutdown, not normal operation

### Alternative Solutions Considered

| Solution | Pros | Cons |
|----------|------|------|
| Join threads in destructor | Clean shutdown | Deadlock if thread is blocked |
| `std::quick_exit()` | Skips destructors | Breaks RAII, resource leaks |
| Thread-local storage | No static dependency | More complex, portability issues |
| Reference counting | Clean cleanup | Complex, race conditions possible |
| **Intentional leak** | Simple, reliable | ~100-200 bytes leaked |

The intentional leak pattern was chosen because it provides the best balance of simplicity, reliability, and minimal impact.

### When This Pattern Applies

- Singleton objects with background threads
- Static objects accessed by threads during shutdown
- Objects where destructor order is non-deterministic

### Code Example

```cpp
class thread_integration_manager {
private:
    class impl;

    /**
     * @brief PIMPL pointer with intentional leak pattern
     *
     * Uses shared_ptr with no-op deleter to prevent heap corruption during
     * static destruction. This is intentional - see docs/DESIGN_DECISIONS.md.
     *
     * @note Memory is intentionally not freed. This is safe because:
     *       - Only ~100-200 bytes per singleton instance
     *       - Process terminates immediately after static destruction
     *       - OS reclaims all process memory on exit
     *
     * @warning Do not "fix" this by changing to unique_ptr or adding a real
     *          deleter. Doing so will cause heap corruption on shutdown.
     */
    std::shared_ptr<impl> pimpl_;
};
```

### Sanitizer Suppression

Memory sanitizers (ASAN/LSAN) may report these intentional leaks as errors. To suppress false positives, use the suppression file:

```
# sanitizers/lsan_suppressions.txt
leak:kcenon::network::integration::basic_thread_pool
leak:kcenon::network::integration::thread_integration_manager
```

When running with LSAN, set the environment variable:
```bash
LSAN_OPTIONS=suppressions=sanitizers/lsan_suppressions.txt ./your_test
```

Or configure CMake to use the suppression file automatically (see CMakeLists.txt).

---

## Send-Side Backpressure Mechanism

### Overview

TCP sockets now include a **backpressure mechanism** to prevent memory exhaustion when sending to slow receivers. This is implemented through `socket_config` settings and automatic flow control.

### Location

- `include/kcenon/network/internal/common_defs.h` - `socket_config`, `socket_metrics`
- `include/kcenon/network/internal/tcp_socket.h` - backpressure methods
- `src/internal/tcp_socket.cpp` - implementation

### Problem

Without backpressure control, when sending data to a slow receiver:

1. Application calls `async_send()` repeatedly
2. Data accumulates in pending send buffers
3. Memory usage grows unbounded
4. Eventually, Out-of-Memory (OOM) occurs

### Solution: High/Low Water Mark Pattern

The implementation uses a **hysteresis pattern** to avoid oscillation:

```
     ┌──────────────────────────────────────────────────────────────────┐
     │                                                                   │
     │  Pending       ▲                                                  │
     │  Bytes         │                                                  │
     │                │                                                  │
     │  max_pending ──┼───────────────────────────────────────────────  │
     │                │    × try_send() returns false                    │
     │                │                                                  │
     │  high_water ───┼─────────────────────────────                    │
     │                │    ↑ backpressure callback(true)                │
     │                │    │                                             │
     │                │    │        (hysteresis zone)                   │
     │                │    │                                             │
     │  low_water ────┼────↓───────────────────────                     │
     │                │    ↓ backpressure callback(false)               │
     │                │                                                  │
     │            0 ──┼──────────────────────────────────────────────── │
     │                │                                                  │
     │                └──────────────────────────────────► Time          │
     └──────────────────────────────────────────────────────────────────┘
```

### Configuration

```cpp
struct socket_config {
    std::size_t max_pending_bytes{0};     // 0 = unlimited (backward compatible)
    std::size_t high_water_mark{1024*1024};  // 1MB - trigger backpressure
    std::size_t low_water_mark{256*1024};    // 256KB - release backpressure
};
```

### Usage Example

```cpp
// Create socket with backpressure limits
socket_config config;
config.max_pending_bytes = 4 * 1024 * 1024;  // 4MB hard limit
config.high_water_mark = 2 * 1024 * 1024;    // 2MB soft limit
config.low_water_mark = 512 * 1024;          // 512KB resume threshold

auto socket = std::make_shared<tcp_socket>(std::move(raw_socket), config);

// Register for backpressure notifications
socket->set_backpressure_callback([](bool apply) {
    if (apply) {
        // Slow down or pause sending
        pause_producer();
    } else {
        // Resume normal sending
        resume_producer();
    }
});

// Use try_send for flow control
if (!socket->try_send(std::move(data), handler)) {
    // Buffer is full, queue for later or apply backpressure
    queue_for_later(data);
}
```

### API Reference

| Method | Description |
|--------|-------------|
| `tcp_socket(socket, config)` | Constructor with backpressure config |
| `set_backpressure_callback(cb)` | Register for backpressure events |
| `try_send(data, handler)` | Non-blocking send with limit check |
| `pending_bytes()` | Get current pending bytes count |
| `is_backpressure_active()` | Check if backpressure is active |
| `metrics()` | Get socket metrics for monitoring |
| `reset_metrics()` | Reset all metrics to zero |
| `config()` | Get current socket configuration |

### Metrics

The `socket_metrics` struct provides runtime monitoring:

```cpp
struct socket_metrics {
    std::atomic<std::size_t> total_bytes_sent{0};
    std::atomic<std::size_t> current_pending_bytes{0};
    std::atomic<std::size_t> peak_pending_bytes{0};
    std::atomic<std::size_t> backpressure_events{0};
    std::atomic<std::size_t> rejected_sends{0};
    std::atomic<std::size_t> send_count{0};
};
```

### Design Decisions

1. **Backward Compatible**: Default `max_pending_bytes=0` means unlimited buffering
2. **Thread-Safe**: All counters use `std::atomic` for lock-free access
3. **Non-Intrusive**: Existing `async_send()` continues to work without changes
4. **Hysteresis**: High/low water marks prevent callback oscillation

---

## References

- [C++ Static Destruction Order - ISO C++ FAQ](https://isocpp.org/wiki/faq/ctors#static-init-order)
- [Static Initialization Order Fiasco](https://isocpp.org/wiki/faq/ctors#static-init-order-fiasco)
- [Meyers Singleton Thread Safety](https://www.modernescpp.com/index.php/thread-safe-initialization-of-a-singleton)
- [LSAN Suppressions Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer#suppression)

---

*This document is part of the network_system documentation. For the complete documentation, see [README.md](README.md).*
