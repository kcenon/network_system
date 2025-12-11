# Architecture & Components

> **Part 1 of 4** | [📑 Index](README.md) | [Next: Dependency & Testing →](02-dependency-and-testing.md)

> **Language:** **English** | [한국어](01-architecture-and-components_KO.md)

This document covers the architectural design and core component implementation of the network_system separation project.

## Table of Contents

- [🏗️ Architecture Design](#-architecture-design)
  - [1. Module Layer Structure](#1-module-layer-structure)
  - [2. Namespace Design](#2-namespace-design)
- [🔧 Core Component Implementation](#-core-component-implementation)
  - [1. messaging_bridge Class](#1-messaging_bridge-class)
  - [2. Core API Design](#2-core-api-design)
  - [3. Session Management](#3-session-management)
- [🔗 Dependency Management](#-dependency-management)
  - [1. CMake Module Files](#1-cmake-module-files)
  - [2. pkg-config File](#2-pkg-config-file)
- [🧪 Test Framework](#-test-framework)
  - [1. Unit Test Structure](#1-unit-test-structure)
  - [2. Integration Tests](#2-integration-tests)
- [📊 Performance Monitoring](#-performance-monitoring)
  - [1. Performance Metric Collection](#1-performance-metric-collection)
  - [2. Benchmark Tools](#2-benchmark-tools)
- [🛡️ Resource Management Patterns](#-resource-management-patterns)
  - [Overview](#overview)
  - [1. Smart Pointer Strategy](#1-smart-pointer-strategy)
    - [Session Lifetime Management](#session-lifetime-management)
    - [ASIO Resource Ownership](#asio-resource-ownership)
  - [2. Async-Safe Pattern: enable_shared_from_this](#2-async-safe-pattern-enable_shared_from_this)
  - [3. Thread Safety](#3-thread-safety)
    - [Atomic State Flags](#atomic-state-flags)
    - [Mutex Protection for Shared Data](#mutex-protection-for-shared-data)
  - [4. Exception Safety](#4-exception-safety)
  - [5. Resource Categories](#5-resource-categories)
  - [6. Best Practices Applied](#6-best-practices-applied)
- [🚨 Error Handling Strategy](#-error-handling-strategy)
  - [Current State](#current-state)
  - [Migration to Result<T> Pattern](#migration-to-resultt-pattern)
    - [1. Server Lifecycle Operations](#1-server-lifecycle-operations)
    - [2. Client Connection Operations](#2-client-connection-operations)
    - [3. Send/Receive Operations](#3-sendreceive-operations)
  - [Error Code System](#error-code-system)
    - [Network System Error Codes (-600 to -699)](#network-system-error-codes-600-to-699)
    - [ASIO Error Code Mapping](#asio-error-code-mapping)
  - [Async Operations with Result<T>](#async-operations-with-resultt)
    - [Pattern 1: Callback-based](#pattern-1-callback-based)
    - [Pattern 2: Future-based](#pattern-2-future-based)
  - [Migration Benefits](#migration-benefits)

**Date**: 2025-09-19
**Version**: 0.1.0.0
**Owner**: kcenon

---

## 🏗️ Architecture Design

### 1. Module Layer Structure

```
                    ┌─────────────────────────────────────┐
                    │        Application Layer           │
                    │   (messaging_system, other apps)   │
                    └─────────────────┬───────────────────┘
                                      │
                    ┌─────────────────▼───────────────────┐
                    │     Integration Layer               │
                    │  ┌─────────────┬─────────────────┐  │
                    │  │ messaging   │ container       │  │
                    │  │ _bridge     │ _integration    │  │
                    │  └─────────────┴─────────────────┘  │
                    └─────────────────┬───────────────────┘
                                      │
                    ┌─────────────────▼───────────────────┐
                    │     Network System Core             │
                    │  ┌───────┬─────────┬─────────────┐  │
                    │  │ Core  │ Session │ Internal    │  │
                    │  │ API   │ Mgmt    │ Impl        │  │
                    │  └───────┴─────────┴─────────────┘  │
                    └─────────────────┬───────────────────┘
                                      │
                    ┌─────────────────▼───────────────────┐
                    │     External Dependencies           │
                    │  ASIO │ fmt │ thread_system │ etc.  │
                    └─────────────────────────────────────┘
```

### 2. Namespace Design

```cpp
namespace network_system {
    // Core API
    namespace core {
        class messaging_client;
        class messaging_server;
        class network_config;
    }

    // Session management
    namespace session {
        class messaging_session;
        class session_manager;
        class connection_pool;
    }

    // Internal implementation (not exposed externally)
    namespace internal {
        class tcp_socket;
        class send_coroutine;
        class message_pipeline;
        class protocol_handler;
    }

    // System integration
    namespace integration {
        class messaging_bridge;
        class container_integration;
        class thread_integration;

        // Compatibility namespace (for existing code)
        namespace compat {
            namespace network_module = network_system;
        }
    }

    // Utilities
    namespace utils {
        class network_utils;
        class address_resolver;
        class performance_monitor;
    }
}
```

---

## 🔧 Core Component Implementation

### 1. messaging_bridge Class

```cpp
// include/network_system/integration/messaging_bridge.h
#pragma once

#include "network_system/core/messaging_client.h"
#include "network_system/core/messaging_server.h"
#include "network_system/utils/performance_monitor.h"

#ifdef BUILD_WITH_CONTAINER_SYSTEM
#include "container_system/value_container.h"
#endif

#ifdef BUILD_WITH_THREAD_SYSTEM
#include "thread_system/thread_pool.h"
#endif

namespace kcenon::network::integration {

class messaging_bridge {
public:
    // Constructor/Destructor
    explicit messaging_bridge(const std::string& bridge_id = "default");
    ~messaging_bridge();

    // Delete copy and move constructors (singleton pattern)
    messaging_bridge(const messaging_bridge&) = delete;
    messaging_bridge& operator=(const messaging_bridge&) = delete;
    messaging_bridge(messaging_bridge&&) = delete;
    messaging_bridge& operator=(messaging_bridge&&) = delete;

    // Server/Client factory
    std::shared_ptr<core::messaging_server> create_server(
        const std::string& server_id,
        const core::server_config& config = {}
    );

    std::shared_ptr<core::messaging_client> create_client(
        const std::string& client_id,
        const core::client_config& config = {}
    );

    // Compatibility API (for existing messaging_system code)
    namespace compat {
        using messaging_server = core::messaging_server;
        using messaging_client = core::messaging_client;
        using messaging_session = session::messaging_session;

        // Same function signatures as existing ones
        std::shared_ptr<messaging_server> make_messaging_server(
            const std::string& server_id
        );
        std::shared_ptr<messaging_client> make_messaging_client(
            const std::string& client_id
        );
    }

#ifdef BUILD_WITH_CONTAINER_SYSTEM
    // Container System integration
    void set_container_factory(
        std::shared_ptr<container_system::value_factory> factory
    );

    void enable_container_serialization(bool enable = true);

    template<typename MessageHandler>
    void set_container_message_handler(MessageHandler&& handler) {
        static_assert(std::is_invocable_v<MessageHandler,
            std::shared_ptr<container_system::value_container>>,
            "Handler must accept std::shared_ptr<container_system::value_container>");

        container_message_handler_ = std::forward<MessageHandler>(handler);
    }
#endif

#ifdef BUILD_WITH_THREAD_SYSTEM
    // Thread System integration
    void set_thread_pool(
        std::shared_ptr<thread_system::thread_pool> pool
    );

    void set_async_mode(bool async = true);
#endif

    // Configuration management
    struct bridge_config {
        bool enable_performance_monitoring = true;
        bool enable_message_logging = false;
        bool enable_connection_pooling = true;
        size_t max_connections_per_client = 10;
        std::chrono::milliseconds connection_timeout{30000};
        std::chrono::milliseconds message_timeout{5000};
        size_t message_buffer_size = 8192;
        bool enable_compression = false;
        bool enable_encryption = false;
    };

    void set_config(const bridge_config& config);
    bridge_config get_config() const;

    // Performance monitoring
    struct performance_metrics {
        // Connection statistics
        std::atomic<uint64_t> total_connections{0};
        std::atomic<uint64_t> active_connections{0};
        std::atomic<uint64_t> failed_connections{0};

        // Message statistics
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};

        // Performance statistics
        std::atomic<uint64_t> avg_latency_us{0};
        std::atomic<uint64_t> max_latency_us{0};
        std::atomic<uint64_t> min_latency_us{UINT64_MAX};

        // Error statistics
        std::atomic<uint64_t> send_errors{0};
        std::atomic<uint64_t> receive_errors{0};
        std::atomic<uint64_t> timeout_errors{0};

        // Time information
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update;
    };

    performance_metrics get_metrics() const;
    void reset_metrics();

    // Event handlers
    using connection_event_handler = std::function<void(
        const std::string& endpoint, bool connected)>;
    using error_event_handler = std::function<void(
        const std::string& error_msg, int error_code)>;

    void set_connection_event_handler(connection_event_handler handler);
    void set_error_event_handler(error_event_handler handler);

    // Lifecycle management
    void start();
    void stop();
    bool is_running() const;

    // Debugging and diagnostics
    std::string get_status_report() const;
    void enable_debug_logging(bool enable = true);

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace kcenon::network::integration
```

### 2. Core API Design

```cpp
// include/network_system/core/messaging_server.h
#pragma once

#include "network_system/session/messaging_session.h"
#include <functional>
#include <memory>
#include <string>

namespace network_system::core {

struct server_config {
    unsigned short port = 8080;
    std::string bind_address = "0.0.0.0";
    size_t max_connections = 1000;
    size_t message_size_limit = 1024 * 1024; // 1MB
    std::chrono::seconds timeout{30};
    bool enable_keep_alive = true;
    bool enable_nodelay = true;
    size_t receive_buffer_size = 8192;
    size_t send_buffer_size = 8192;
};

class messaging_server {
public:
    // Constructor
    explicit messaging_server(const std::string& server_id);
    messaging_server(const std::string& server_id, const server_config& config);

    // Destructor
    ~messaging_server();

    // Delete copy/move
    messaging_server(const messaging_server&) = delete;
    messaging_server& operator=(const messaging_server&) = delete;
    messaging_server(messaging_server&&) = delete;
    messaging_server& operator=(messaging_server&&) = delete;

    // Server lifecycle
    bool start_server();
    bool start_server(unsigned short port);
    bool start_server(const std::string& bind_address, unsigned short port);
    void stop_server();
    void wait_for_stop();

    // Status queries
    bool is_running() const;
    std::string server_id() const;
    unsigned short port() const;
    std::string bind_address() const;
    size_t connection_count() const;

    // Configuration management
    void set_config(const server_config& config);
    server_config get_config() const;

    // Event handler setup
    using message_handler = std::function<void(
        std::shared_ptr<session::messaging_session>,
        const std::string&)>;
    using connect_handler = std::function<void(
        std::shared_ptr<session::messaging_session>)>;
    using disconnect_handler = std::function<void(
        std::shared_ptr<session::messaging_session>,
        const std::string&)>;
    using error_handler = std::function<void(
        const std::string&, int)>;

    void set_message_handler(message_handler handler);
    void set_connect_handler(connect_handler handler);
    void set_disconnect_handler(disconnect_handler handler);
    void set_error_handler(error_handler handler);

    // Message broadcasting
    void broadcast_message(const std::string& message);
    void broadcast_to_group(const std::string& group_id, const std::string& message);

    // Session management
    std::shared_ptr<session::messaging_session> get_session(
        const std::string& session_id);
    std::vector<std::shared_ptr<session::messaging_session>> get_all_sessions();
    void disconnect_session(const std::string& session_id);
    void disconnect_all_sessions();

    // Group management
    void add_session_to_group(const std::string& session_id,
                              const std::string& group_id);
    void remove_session_from_group(const std::string& session_id,
                                   const std::string& group_id);
    std::vector<std::string> get_session_groups(const std::string& session_id);

    // Statistics
    struct server_statistics {
        uint64_t total_connections = 0;
        uint64_t current_connections = 0;
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_activity;
    };

    server_statistics get_statistics() const;
    void reset_statistics();

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace network_system::core
```

### 3. Session Management

```cpp
// include/network_system/session/messaging_session.h
#pragma once

#include <string>
#include <memory>
#include <chrono>
#include <functional>
#include <unordered_map>

namespace network_system::session {

enum class session_state {
    connecting,
    connected,
    disconnecting,
    disconnected,
    error
};

class messaging_session {
public:
    // Destructor
    virtual ~messaging_session() = default;

    // Message send/receive
    virtual void send_message(const std::string& message) = 0;
    virtual void send_raw_data(const std::vector<uint8_t>& data) = 0;
    virtual void send_binary_message(const void* data, size_t size) = 0;

    // Session information
    virtual std::string session_id() const = 0;
    virtual std::string client_id() const = 0;
    virtual std::string remote_address() const = 0;
    virtual unsigned short remote_port() const = 0;
    virtual std::string local_address() const = 0;
    virtual unsigned short local_port() const = 0;

    // Connection state
    virtual session_state state() const = 0;
    virtual bool is_connected() const = 0;
    virtual std::chrono::steady_clock::time_point connect_time() const = 0;
    virtual std::chrono::steady_clock::time_point last_activity() const = 0;

    // Session management
    virtual void disconnect() = 0;
    virtual void disconnect(const std::string& reason) = 0;

    // Session data (key-value store)
    virtual void set_session_data(const std::string& key,
                                  const std::string& value) = 0;
    virtual std::string get_session_data(const std::string& key) const = 0;
    virtual bool has_session_data(const std::string& key) const = 0;
    virtual void remove_session_data(const std::string& key) = 0;
    virtual void clear_session_data() = 0;

    // Session groups
    virtual void join_group(const std::string& group_id) = 0;
    virtual void leave_group(const std::string& group_id) = 0;
    virtual std::vector<std::string> get_groups() const = 0;
    virtual bool is_in_group(const std::string& group_id) const = 0;

    // Statistics
    struct session_statistics {
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        uint64_t errors_count = 0;
        std::chrono::steady_clock::time_point last_message_time;
    };

    virtual session_statistics get_statistics() const = 0;
    virtual void reset_statistics() = 0;

    // Event handlers
    using message_handler = std::function<void(const std::string&)>;
    using state_change_handler = std::function<void(session_state, session_state)>;
    using error_handler = std::function<void(const std::string&, int)>;

    virtual void set_message_handler(message_handler handler) = 0;
    virtual void set_state_change_handler(state_change_handler handler) = 0;
    virtual void set_error_handler(error_handler handler) = 0;

protected:
    messaging_session() = default;
};

// Factory functions
std::shared_ptr<messaging_session> create_tcp_session(
    const std::string& session_id,
    const std::string& client_id
);

} // namespace network_system::session
```
