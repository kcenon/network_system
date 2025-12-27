# Design Decisions

> **Language:** **English** | [한국어](DESIGN_DECISIONS_KO.md)

This document explains key design decisions in network_system, particularly patterns that may seem unusual but are intentional and necessary.

**Version**: 1.0.0
**Last Updated**: 2025-12-27

---

## Table of Contents

- [Intentional Leak Pattern in Thread Integration](#intentional-leak-pattern-in-thread-integration)
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

## References

- [C++ Static Destruction Order - ISO C++ FAQ](https://isocpp.org/wiki/faq/ctors#static-init-order)
- [Static Initialization Order Fiasco](https://isocpp.org/wiki/faq/ctors#static-init-order-fiasco)
- [Meyers Singleton Thread Safety](https://www.modernescpp.com/index.php/thread-safe-initialization-of-a-singleton)
- [LSAN Suppressions Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizerLeakSanitizer#suppression)

---

*This document is part of the network_system documentation. For the complete documentation, see [README.md](README.md).*
