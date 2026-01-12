# Composition-Based Design Specification

> **Document Version:** 1.0.0
> **Related Issue:** [#411](https://github.com/kcenon/network_system/issues/411), [#422](https://github.com/kcenon/network_system/issues/422)
> **Last Updated:** 2025-01-12

## Executive Summary

This document describes the target architecture for migrating from CRTP-based design to composition-based design. The goal is to reduce code duplication by ~60%, improve testability, and maintain API compatibility.

## Table of Contents

1. [Design Goals](#design-goals)
2. [Interface Hierarchy](#interface-hierarchy)
3. [Utility Classes](#utility-classes)
4. [Migration Strategy](#migration-strategy)
5. [Implementation Details](#implementation-details)
6. [API Compatibility](#api-compatibility)

---

## Design Goals

### Primary Goals

1. **Reduce Code Duplication**: Target 60% reduction in boilerplate code
2. **Improve Testability**: Enable mocking through interface injection
3. **Maintain Performance**: No significant runtime overhead
4. **API Stability**: Backward-compatible public interface

### Design Principles

- **Favor Composition over Inheritance** (GoF principle)
- **Program to an Interface, not an Implementation**
- **Single Responsibility Principle**: Each class has one reason to change
- **Dependency Injection**: Dependencies provided externally

---

## Interface Hierarchy

### Core Interfaces

```cpp
namespace kcenon::network::interfaces {

// Base interface for all network components
class INetworkComponent {
public:
    virtual ~INetworkComponent() = default;

    [[nodiscard]] virtual auto is_running() const -> bool = 0;
    [[nodiscard]] virtual auto wait_for_stop() -> void = 0;
};

// Interface for client-side components
class IClient : public INetworkComponent {
public:
    [[nodiscard]] virtual auto start(std::string_view host, uint16_t port) -> VoidResult = 0;
    [[nodiscard]] virtual auto stop() -> VoidResult = 0;
    [[nodiscard]] virtual auto send(std::vector<uint8_t>&& data) -> VoidResult = 0;
};

// Interface for server-side components
class IServer : public INetworkComponent {
public:
    [[nodiscard]] virtual auto start(uint16_t port) -> VoidResult = 0;
    [[nodiscard]] virtual auto stop() -> VoidResult = 0;
};

} // namespace kcenon::network::interfaces
```

### Protocol-Specific Interfaces

```cpp
namespace kcenon::network::interfaces {

// TCP-specific client interface
class ITcpClient : public IClient {
public:
    using receive_callback_t = std::function<void(std::vector<uint8_t>&&)>;
    using connected_callback_t = std::function<void()>;
    using disconnected_callback_t = std::function<void()>;
    using error_callback_t = std::function<void(std::string_view)>;

    virtual auto set_receive_callback(receive_callback_t) -> void = 0;
    virtual auto set_connected_callback(connected_callback_t) -> void = 0;
    virtual auto set_disconnected_callback(disconnected_callback_t) -> void = 0;
    virtual auto set_error_callback(error_callback_t) -> void = 0;
};

// TCP-specific server interface
class ITcpServer : public IServer {
public:
    using connection_callback_t = std::function<void(std::shared_ptr<ISession>)>;
    using disconnection_callback_t = std::function<void(std::string_view)>;
    using receive_callback_t = std::function<void(std::string_view, std::vector<uint8_t>&&)>;
    using error_callback_t = std::function<void(std::string_view, std::string_view)>;

    virtual auto set_connection_callback(connection_callback_t) -> void = 0;
    virtual auto set_disconnection_callback(disconnection_callback_t) -> void = 0;
    virtual auto set_receive_callback(receive_callback_t) -> void = 0;
    virtual auto set_error_callback(error_callback_t) -> void = 0;
};

// UDP-specific interfaces (include endpoint info)
class IUdpClient : public IClient {
public:
    using receive_callback_t = std::function<void(
        std::vector<uint8_t>&&,
        const asio::ip::udp::endpoint&
    )>;

    virtual auto set_receive_callback(receive_callback_t) -> void = 0;
    virtual auto set_target(std::string_view host, uint16_t port) -> VoidResult = 0;
};

// WebSocket-specific interfaces
class IWebSocketClient : public IClient {
public:
    using text_callback_t = std::function<void(std::string&&)>;
    using binary_callback_t = std::function<void(std::vector<uint8_t>&&)>;
    using disconnected_callback_t = std::function<void(uint16_t code, std::string_view reason)>;

    [[nodiscard]] virtual auto start(
        std::string_view host,
        uint16_t port,
        std::string_view path = "/"
    ) -> VoidResult = 0;

    virtual auto send_text(std::string_view message) -> VoidResult = 0;
    virtual auto send_binary(std::vector<uint8_t>&& data) -> VoidResult = 0;
    virtual auto close(uint16_t code, std::string_view reason) -> VoidResult = 0;

    virtual auto set_text_callback(text_callback_t) -> void = 0;
    virtual auto set_binary_callback(binary_callback_t) -> void = 0;
    virtual auto set_disconnected_callback(disconnected_callback_t) -> void = 0;
};

// QUIC-specific interfaces (multi-stream support)
class IQuicClient : public IClient {
public:
    using stream_callback_t = std::function<void(uint64_t stream_id, std::vector<uint8_t>&&)>;

    [[nodiscard]] virtual auto create_stream() -> Result<uint64_t> = 0;
    [[nodiscard]] virtual auto send_on_stream(
        uint64_t stream_id,
        std::vector<uint8_t>&& data,
        bool fin = false
    ) -> VoidResult = 0;
    virtual auto close_stream(uint64_t stream_id) -> VoidResult = 0;

    virtual auto set_stream_callback(stream_callback_t) -> void = 0;
};

} // namespace kcenon::network::interfaces
```

### Session Interface

```cpp
namespace kcenon::network::interfaces {

class ISession {
public:
    virtual ~ISession() = default;

    [[nodiscard]] virtual auto id() const -> std::string_view = 0;
    [[nodiscard]] virtual auto is_connected() const -> bool = 0;
    [[nodiscard]] virtual auto send(std::vector<uint8_t>&& data) -> VoidResult = 0;
    virtual auto close() -> void = 0;
};

} // namespace kcenon::network::interfaces
```

---

## Utility Classes

### 1. LifecycleManager

Handles common start/stop lifecycle logic:

```cpp
namespace kcenon::network::utils {

class LifecycleManager {
public:
    LifecycleManager() = default;
    ~LifecycleManager() = default;

    // Non-copyable, movable
    LifecycleManager(const LifecycleManager&) = delete;
    LifecycleManager& operator=(const LifecycleManager&) = delete;
    LifecycleManager(LifecycleManager&&) = default;
    LifecycleManager& operator=(LifecycleManager&&) = default;

    [[nodiscard]] auto is_running() const -> bool {
        return is_running_.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto try_start() -> bool {
        bool expected = false;
        return is_running_.compare_exchange_strong(
            expected, true,
            std::memory_order_acq_rel
        );
    }

    auto mark_stopped() -> void {
        is_running_.store(false, std::memory_order_release);
        if (stop_promise_) {
            stop_promise_->set_value();
            stop_promise_.reset();
        }
    }

    auto wait_for_stop() -> void {
        if (stop_future_.valid()) {
            stop_future_.wait();
        }
    }

    auto prepare_stop() -> bool {
        bool expected = false;
        if (stop_initiated_.compare_exchange_strong(expected, true)) {
            stop_promise_.emplace();
            stop_future_ = stop_promise_->get_future();
            return true;
        }
        return false;
    }

    auto reset() -> void {
        is_running_.store(false, std::memory_order_release);
        stop_initiated_.store(false, std::memory_order_release);
        stop_promise_.reset();
        stop_future_ = {};
    }

private:
    std::atomic<bool> is_running_{false};
    std::atomic<bool> stop_initiated_{false};
    std::optional<std::promise<void>> stop_promise_;
    std::future<void> stop_future_;
};

} // namespace kcenon::network::utils
```

### 2. CallbackManager

Thread-safe callback storage and invocation:

```cpp
namespace kcenon::network::utils {

template<typename... CallbackTypes>
class CallbackManager {
public:
    template<typename T>
    auto set(T&& callback) -> void {
        std::lock_guard lock(mutex_);
        std::get<std::decay_t<T>>(callbacks_) = std::forward<T>(callback);
    }

    template<typename T>
    [[nodiscard]] auto get() const -> const T& {
        std::lock_guard lock(mutex_);
        return std::get<T>(callbacks_);
    }

    template<typename T, typename... Args>
    auto invoke(Args&&... args) -> void {
        T callback;
        {
            std::lock_guard lock(mutex_);
            callback = std::get<T>(callbacks_);
        }
        if (callback) {
            callback(std::forward<Args>(args)...);
        }
    }

    template<typename T, typename... Args>
    auto invoke_if(bool condition, Args&&... args) -> void {
        if (condition) {
            invoke<T>(std::forward<Args>(args)...);
        }
    }

private:
    mutable std::mutex mutex_;
    std::tuple<CallbackTypes...> callbacks_;
};

// Convenience type aliases
using TcpClientCallbacks = CallbackManager<
    std::function<void(std::vector<uint8_t>&&)>,     // receive
    std::function<void()>,                            // connected
    std::function<void()>,                            // disconnected
    std::function<void(std::string_view)>             // error
>;

using TcpServerCallbacks = CallbackManager<
    std::function<void(std::shared_ptr<ISession>)>,           // connection
    std::function<void(std::string_view)>,                    // disconnection
    std::function<void(std::string_view, std::vector<uint8_t>&&)>,  // receive
    std::function<void(std::string_view, std::string_view)>   // error
>;

} // namespace kcenon::network::utils
```

### 3. ConnectionState

Manages connection state with thread safety:

```cpp
namespace kcenon::network::utils {

enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Disconnecting
};

class ConnectionState {
public:
    [[nodiscard]] auto status() const -> ConnectionStatus {
        return status_.load(std::memory_order_acquire);
    }

    [[nodiscard]] auto is_connected() const -> bool {
        return status_.load(std::memory_order_acquire) == ConnectionStatus::Connected;
    }

    auto set_connecting() -> bool {
        auto expected = ConnectionStatus::Disconnected;
        return status_.compare_exchange_strong(expected, ConnectionStatus::Connecting);
    }

    auto set_connected() -> void {
        status_.store(ConnectionStatus::Connected, std::memory_order_release);
    }

    auto set_disconnecting() -> bool {
        auto expected = ConnectionStatus::Connected;
        return status_.compare_exchange_strong(expected, ConnectionStatus::Disconnecting);
    }

    auto set_disconnected() -> void {
        status_.store(ConnectionStatus::Disconnected, std::memory_order_release);
    }

private:
    std::atomic<ConnectionStatus> status_{ConnectionStatus::Disconnected};
};

} // namespace kcenon::network::utils
```

---

## Migration Strategy

### Phase 1.2: Create Infrastructure (Non-Breaking)

1. Add interface headers to `include/kcenon/network/interfaces/`
2. Add utility classes to `include/kcenon/network/utils/`
3. Ensure existing CRTP code continues to work

### Phase 1.3: Implement Composition Classes

Create new implementations that use composition:

```cpp
namespace kcenon::network::core {

class tcp_client : public interfaces::ITcpClient {
public:
    explicit tcp_client(
        std::shared_ptr<integration::thread_pool_interface> thread_pool = nullptr
    );

    // ITcpClient implementation
    auto start(std::string_view host, uint16_t port) -> VoidResult override;
    auto stop() -> VoidResult override;
    auto send(std::vector<uint8_t>&& data) -> VoidResult override;
    auto is_running() const -> bool override;
    auto wait_for_stop() -> void override;

    auto set_receive_callback(receive_callback_t cb) -> void override;
    auto set_connected_callback(connected_callback_t cb) -> void override;
    auto set_disconnected_callback(disconnected_callback_t cb) -> void override;
    auto set_error_callback(error_callback_t cb) -> void override;

private:
    utils::LifecycleManager lifecycle_;
    utils::ConnectionState connection_;
    utils::TcpClientCallbacks callbacks_;

    // Protocol-specific members
    std::unique_ptr<internal::tcp_socket> socket_;
    std::shared_ptr<integration::thread_pool_interface> thread_pool_;
    asio::io_context io_context_;
};

} // namespace kcenon::network::core
```

### Phase 1.4: Deprecation and Cleanup

1. Mark CRTP bases as `[[deprecated]]`
2. Update all internal usage to composition classes
3. Remove CRTP bases in next major version

---

## Implementation Details

### Directory Structure

```
include/kcenon/network/
├── interfaces/                    # NEW: Interface definitions
│   ├── i_network_component.h
│   ├── i_client.h
│   ├── i_server.h
│   ├── i_session.h
│   ├── i_tcp_client.h
│   ├── i_tcp_server.h
│   ├── i_udp_client.h
│   ├── i_udp_server.h
│   ├── i_websocket_client.h
│   ├── i_websocket_server.h
│   ├── i_quic_client.h
│   └── i_quic_server.h
├── utils/                         # NEW: Utility classes
│   ├── lifecycle_manager.h
│   ├── callback_manager.h
│   └── connection_state.h
├── core/                          # EXISTING: Updated implementations
│   ├── messaging_client_base.h    # [[deprecated]] in Phase 1.4
│   ├── messaging_client.h         # Updated to use composition
│   └── ...
└── ...
```

### Naming Convention

| Old Name (CRTP) | New Name (Composition) |
|-----------------|------------------------|
| `messaging_client_base` | (deprecated) |
| `messaging_client` | `tcp_client` (implements `ITcpClient`) |
| `messaging_server` | `tcp_server` (implements `ITcpServer`) |
| `messaging_udp_client` | `udp_client` (implements `IUdpClient`) |
| `messaging_ws_client` | `websocket_client` (implements `IWebSocketClient`) |
| `messaging_quic_client` | `quic_client` (implements `IQuicClient`) |

---

## API Compatibility

### Backward Compatibility Strategy

1. **Type Aliases**: Provide aliases for old names

```cpp
namespace kcenon::network::core {
    // Backward compatibility
    using messaging_client = tcp_client;
    using messaging_server = tcp_server;
    // ... etc
}
```

2. **Template Wrappers**: For users depending on template interface

```cpp
template<typename T>
class messaging_client_compat : public tcp_client {
    // Adapter for CRTP-style usage
};
```

### Breaking Changes (Major Version)

In the next major version:
- Remove CRTP base classes
- Remove deprecated aliases
- Update all documentation

### Version Timeline

| Version | Changes |
|---------|---------|
| v2.1.0 | Add interfaces and utilities (non-breaking) |
| v2.2.0 | Add composition implementations (non-breaking) |
| v2.3.0 | Deprecate CRTP bases |
| v3.0.0 | Remove CRTP bases (breaking) |

---

## Testing Strategy

### Interface Mocking

```cpp
// Test can now easily mock interfaces
class MockTcpClient : public interfaces::ITcpClient {
public:
    MOCK_METHOD(VoidResult, start, (std::string_view, uint16_t), (override));
    MOCK_METHOD(VoidResult, stop, (), (override));
    MOCK_METHOD(VoidResult, send, (std::vector<uint8_t>&&), (override));
    // ... etc
};
```

### Utility Class Testing

Each utility class is independently testable:

```cpp
TEST(LifecycleManagerTest, TryStartReturnsFalseWhenRunning) {
    utils::LifecycleManager manager;
    ASSERT_TRUE(manager.try_start());
    ASSERT_FALSE(manager.try_start());  // Already running
}
```

---

## Metrics

### Expected Improvements

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| Lines of code (base classes) | ~1035 | ~400 | ~60% reduction |
| Unit test coverage | ~40% | ~80% | 2x improvement |
| Time to add new protocol | ~8 hours | ~3 hours | 60% faster |
| Cyclomatic complexity | High | Medium | Improved |

### Performance Considerations

- Virtual call overhead: ~1-2ns per call (negligible for network I/O)
- Memory overhead: One vtable pointer per instance (~8 bytes)
- Compile time: Reduced due to less template instantiation

---

## Conclusion

The composition-based design provides:

1. **Cleaner Architecture**: Clear separation of concerns
2. **Better Testability**: Easy interface mocking
3. **Reduced Duplication**: Shared utility classes
4. **Maintainability**: Single point of change for common patterns
5. **Backward Compatibility**: Gradual migration path

The migration will be executed in phases to ensure stability and allow for thorough testing at each step.
