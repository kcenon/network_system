# CRTP Base Classes Analysis

> **Document Version:** 1.0.0
> **Related Issue:** [#411](https://github.com/kcenon/network_system/issues/411), [#422](https://github.com/kcenon/network_system/issues/422)
> **Last Updated:** 2025-01-12

## Executive Summary

This document provides a comprehensive analysis of all CRTP (Curiously Recurring Template Pattern) base classes in the network_system project. The analysis identifies **9 CRTP base classes** across 4 protocols (TCP, UDP, WebSocket, QUIC), documenting their responsibilities, dependencies, and opportunities for consolidation.

## Table of Contents

1. [Overview](#overview)
2. [CRTP Base Classes Inventory](#crtp-base-classes-inventory)
3. [Detailed Class Analysis](#detailed-class-analysis)
4. [Common Patterns](#common-patterns)
5. [Code Duplication Analysis](#code-duplication-analysis)
6. [Dependency Graph](#dependency-graph)
7. [Migration Considerations](#migration-considerations)

---

## Overview

### What is CRTP?

The Curiously Recurring Template Pattern (CRTP) is a C++ idiom where a class `X` derives from a class template using `X` itself as a template argument:

```cpp
template<typename Derived>
class Base {
    void interface() {
        static_cast<Derived*>(this)->implementation();
    }
};

class Derived : public Base<Derived> {
    void implementation() { /* ... */ }
};
```

### Current Architecture

The network_system project uses CRTP to provide:
- Static polymorphism (compile-time dispatch)
- Code reuse through template inheritance
- Type-safe callback mechanisms

### Identified Issues

1. **Code Duplication (~60%)**: Similar boilerplate across all 9 base classes
2. **Tight Coupling**: CRTP creates strong coupling between base and derived classes
3. **Testing Difficulty**: Static polymorphism makes mocking challenging
4. **Maintenance Burden**: Changes to common patterns require updates to all base classes

---

## CRTP Base Classes Inventory

| # | Class Name | Protocol | Type | File Location |
|---|------------|----------|------|---------------|
| 1 | `messaging_client_base` | TCP | Client | `include/kcenon/network/core/messaging_client_base.h` |
| 2 | `messaging_server_base` | TCP | Server | `include/kcenon/network/core/messaging_server_base.h` |
| 3 | `messaging_udp_client_base` | UDP | Client | `include/kcenon/network/core/messaging_udp_client_base.h` |
| 4 | `messaging_udp_server_base` | UDP | Server | `include/kcenon/network/core/messaging_udp_server_base.h` |
| 5 | `messaging_ws_client_base` | WebSocket | Client | `include/kcenon/network/core/messaging_ws_client_base.h` |
| 6 | `messaging_ws_server_base` | WebSocket | Server | `include/kcenon/network/core/messaging_ws_server_base.h` |
| 7 | `messaging_quic_client_base` | QUIC | Client | `include/kcenon/network/core/messaging_quic_client_base.h` |
| 8 | `messaging_quic_server_base` | QUIC | Server | `include/kcenon/network/core/messaging_quic_server_base.h` |
| 9 | `secure_messaging_client` | TCP+TLS | Client | `include/kcenon/network/core/secure_messaging_client.h` |

### Derived Classes

| Base Class | Derived Class(es) |
|------------|-------------------|
| `messaging_client_base` | `messaging_client`, `secure_messaging_client` |
| `messaging_server_base` | `messaging_server` |
| `messaging_udp_client_base` | `messaging_udp_client` |
| `messaging_udp_server_base` | `messaging_udp_server` |
| `messaging_ws_client_base` | `messaging_ws_client` |
| `messaging_ws_server_base` | `messaging_ws_server` |
| `messaging_quic_client_base` | `messaging_quic_client` |
| `messaging_quic_server_base` | `messaging_quic_server` |

---

## Detailed Class Analysis

### 1. messaging_client_base (TCP Client)

**File:** `include/kcenon/network/core/messaging_client_base.h`

**Template Declaration:**
```cpp
template<typename Derived>
class messaging_client_base : public std::enable_shared_from_this<Derived>
```

**CRTP Interface (Required by Derived):**
```cpp
auto do_start(std::string_view host, unsigned short port) -> VoidResult;
auto do_stop() -> VoidResult;
auto do_send(std::vector<uint8_t>&& data) -> VoidResult;
```

**Public Methods:**
- `start_client(host, port)` - Validates and delegates to `do_start()`
- `stop_client()` - Validates and delegates to `do_stop()`
- `send_packet(data)` - Validates and delegates to `do_send()`
- `wait_for_stop()` - Blocks until client stops

**Callback Setters:**
- `set_receive_callback()`
- `set_connected_callback()`
- `set_disconnected_callback()`
- `set_error_callback()`

**Thread Safety Members:**
- `std::atomic<bool> is_running_`
- `std::atomic<bool> is_connected_`
- `std::atomic<bool> stop_initiated_`
- `std::mutex callback_mutex_`

---

### 2. messaging_server_base (TCP Server)

**File:** `include/kcenon/network/core/messaging_server_base.h`

**Template Declaration:**
```cpp
template<typename Derived, typename SessionType = session::messaging_session>
class messaging_server_base
```

**CRTP Interface:**
```cpp
auto do_start(unsigned short port) -> VoidResult;
auto do_stop() -> VoidResult;
```

**Public Methods:**
- `start_server(port)` - Validates and delegates to `do_start()`
- `stop_server()` - Validates and delegates to `do_stop()`
- `wait_for_stop()` - Blocks until server stops

**Callback Setters:**
- `set_connection_callback()`
- `set_disconnection_callback()`
- `set_receive_callback()`
- `set_error_callback()`

**Callback Getters (for derived classes):**
- `get_connection_callback()`
- `get_disconnection_callback()`
- `get_receive_callback()`
- `get_error_callback()`

---

### 3. messaging_udp_client_base (UDP Client)

**File:** `include/kcenon/network/core/messaging_udp_client_base.h`

**Template Declaration:**
```cpp
template<typename Derived>
class messaging_udp_client_base
```

**CRTP Interface:**
```cpp
auto do_start(std::string_view host, uint16_t port) -> VoidResult;
auto do_stop() -> VoidResult;
```

**Special Callback Type:**
```cpp
using callback_t = std::function<void(
    std::vector<uint8_t>&&,
    const asio::ip::udp::endpoint&  // Sender endpoint included
)>;
```

---

### 4. messaging_udp_server_base (UDP Server)

**File:** `include/kcenon/network/core/messaging_udp_server_base.h`

**Template Declaration:**
```cpp
template<typename Derived>
class messaging_udp_server_base
```

**CRTP Interface:**
```cpp
auto do_start(uint16_t port) -> VoidResult;
auto do_stop() -> VoidResult;
```

---

### 5. messaging_ws_client_base (WebSocket Client)

**File:** `include/kcenon/network/core/messaging_ws_client_base.h`

**Template Declaration:**
```cpp
template<typename Derived>
class messaging_ws_client_base
```

**CRTP Interface:**
```cpp
auto do_start(std::string_view host, uint16_t port, std::string_view path) -> VoidResult;
auto do_stop() -> VoidResult;
```

**WebSocket-Specific Callbacks:**
```cpp
using message_callback_t = std::function<void(std::vector<uint8_t>&&)>;
using text_message_callback_t = std::function<void(std::string&&)>;
using binary_message_callback_t = std::function<void(std::vector<uint8_t>&&)>;
using disconnected_callback_t = std::function<void(uint16_t code, std::string_view reason)>;
```

---

### 6. messaging_ws_server_base (WebSocket Server)

**File:** `include/kcenon/network/core/messaging_ws_server_base.h`

**Template Declaration:**
```cpp
template<typename Derived>
class messaging_ws_server_base
```

**CRTP Interface:**
```cpp
auto do_start(uint16_t port, std::string_view path) -> VoidResult;
auto do_stop() -> VoidResult;
```

**WebSocket-Specific Callbacks:**
```cpp
using connection_callback_t = std::function<void(std::shared_ptr<ws_connection>)>;
using disconnection_callback_t = std::function<void(std::string_view id, uint16_t code)>;
```

---

### 7. messaging_quic_client_base (QUIC Client)

**File:** `include/kcenon/network/core/messaging_quic_client_base.h`

**Template Declaration:**
```cpp
template<typename Derived>
class messaging_quic_client_base
```

**CRTP Interface:**
```cpp
auto do_start(std::string_view host, unsigned short port) -> VoidResult;
auto do_stop() -> VoidResult;
```

**QUIC-Specific Callbacks:**
```cpp
using receive_callback_t = std::function<void(std::vector<uint8_t>&&)>;
using stream_receive_callback_t = std::function<void(uint64_t stream_id, std::vector<uint8_t>&&)>;
```

---

### 8. messaging_quic_server_base (QUIC Server)

**File:** `include/kcenon/network/core/messaging_quic_server_base.h`

**Template Declaration:**
```cpp
template<typename Derived>
class messaging_quic_server_base
```

**CRTP Interface:**
```cpp
auto do_start(unsigned short port) -> VoidResult;
auto do_stop() -> VoidResult;
```

**QUIC-Specific Callbacks:**
```cpp
using connection_callback_t = std::function<void(std::shared_ptr<quic_session>)>;
using stream_receive_callback_t = std::function<void(
    std::shared_ptr<quic_session>, uint64_t stream_id, std::vector<uint8_t>&&
)>;
```

---

### 9. secure_messaging_client (TLS Client)

**File:** `include/kcenon/network/core/secure_messaging_client.h`

**Note:** This is a derived class from `messaging_client_base`, not a separate CRTP base.

**Declaration:**
```cpp
class secure_messaging_client
    : public messaging_client_base<secure_messaging_client>
```

**Additional Features:**
- SSL/TLS context management
- Certificate verification options
- Secure socket handling

---

## Common Patterns

### 1. Lifecycle Management (All Classes)

```cpp
// Common pattern in all CRTP bases
auto start_*(...) -> VoidResult {
    if (is_running_.load()) {
        return make_error(already_running);
    }
    return static_cast<Derived*>(this)->do_start(...);
}

auto stop_*() -> VoidResult {
    if (!is_running_.load()) {
        return make_error(not_running);
    }
    return static_cast<Derived*>(this)->do_stop();
}
```

### 2. Thread Safety (All Classes)

```cpp
// Identical in all bases
std::atomic<bool> is_running_{false};
std::atomic<bool> stop_initiated_{false};
std::mutex callback_mutex_;
std::optional<std::promise<void>> stop_promise_;
```

### 3. Callback Architecture (All Classes)

```cpp
// Pattern repeated in every base
protected:
    callback_type callback_;

public:
    void set_callback(callback_type cb) {
        std::lock_guard lock(callback_mutex_);
        callback_ = std::move(cb);
    }

protected:
    void invoke_callback(args...) {
        std::lock_guard lock(callback_mutex_);
        if (callback_) {
            callback_(args...);
        }
    }
```

### 4. Stop Synchronization (All Classes)

```cpp
// Identical implementation everywhere
void wait_for_stop() {
    if (stop_future_.valid()) {
        stop_future_.wait();
    }
}
```

---

## Code Duplication Analysis

### Estimated Duplication: ~60%

| Pattern | Lines per Class | Total Lines | Duplicated |
|---------|-----------------|-------------|------------|
| Lifecycle mgmt | ~30 | 270 | 100% |
| Thread safety members | ~10 | 90 | 100% |
| Callback architecture | ~40 | 360 | 80% |
| Stop synchronization | ~15 | 135 | 100% |
| Error handling | ~20 | 180 | 90% |
| **Total** | ~115 | 1035 | **~60%** |

### Unique Code per Protocol

| Protocol | Unique Features |
|----------|-----------------|
| TCP | Pipeline support, session management |
| UDP | Endpoint tracking, stateless handling |
| WebSocket | Message types, path routing, ping/pong |
| QUIC | Multi-stream, 0-RTT, session tickets |

---

## Dependency Graph

```
                    ┌─────────────────────┐
                    │   ASIO Library      │
                    └─────────┬───────────┘
                              │
              ┌───────────────┼───────────────┐
              │               │               │
    ┌─────────▼─────┐  ┌──────▼──────┐  ┌────▼────────┐
    │  tcp_socket   │  │ udp_socket  │  │ quic_socket │
    │secure_tcp_sock│  │             │  │             │
    │websocket_sock │  │             │  │             │
    └───────┬───────┘  └──────┬──────┘  └──────┬──────┘
            │                 │                │
    ┌───────▼───────┐  ┌──────▼──────┐  ┌──────▼──────┐
    │ CRTP Bases    │  │ CRTP Bases  │  │ CRTP Bases  │
    │ (TCP/WS)      │  │ (UDP)       │  │ (QUIC)      │
    └───────┬───────┘  └──────┬──────┘  └──────┬──────┘
            │                 │                │
    ┌───────▼───────┐  ┌──────▼──────┐  ┌──────▼──────┐
    │ Implementations│ │Implementations│ │Implementations│
    └───────────────┘  └─────────────┘  └─────────────┘
```

### External Dependencies

- **ASIO**: All network I/O
- **OpenSSL**: TLS/SSL for secure connections
- **Thread Pool**: Async execution (integration layer)
- **Result Types**: Error handling (utils layer)

---

## Migration Considerations

### Challenges

1. **API Compatibility**: Public interface must remain stable
2. **Performance**: Composition may add virtual call overhead
3. **Template Users**: Existing code using templates directly
4. **Test Coverage**: Need comprehensive tests before migration

### Recommended Approach

1. **Phase 1.2**: Create interface classes without removing CRTP
2. **Phase 1.3**: Implement composition adapters
3. **Phase 1.4**: Deprecate and remove CRTP bases

### Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| API breakage | Medium | High | Semantic versioning, deprecation notices |
| Performance regression | Low | Medium | Benchmarking, optional CRTP mode |
| Test gaps | Medium | Medium | Increase coverage before migration |

---

## Appendix: File Paths

```
include/kcenon/network/core/
├── messaging_client_base.h
├── messaging_client.h
├── messaging_server_base.h
├── messaging_server.h
├── messaging_udp_client_base.h
├── messaging_udp_client.h
├── messaging_udp_server_base.h
├── messaging_udp_server.h
├── messaging_ws_client_base.h
├── messaging_ws_client.h
├── messaging_ws_server_base.h
├── messaging_ws_server.h
├── messaging_quic_client_base.h
├── messaging_quic_client.h
├── messaging_quic_server_base.h
├── messaging_quic_server.h (assumed)
└── secure_messaging_client.h

include/kcenon/network/session/
├── messaging_session.h
└── quic_session.h
```
