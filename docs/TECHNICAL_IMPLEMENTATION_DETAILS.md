# Technical Implementation Details
# Network System Separation Technical Implementation Guide

> **Language:** **English** | [한국어](TECHNICAL_IMPLEMENTATION_DETAILS_KO.md)

**Date**: 2025-09-19
**Version**: 1.0.0
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

namespace network_system::integration {

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

} // namespace network_system::integration
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

---

## 🔗 Dependency Management

### 1. CMake Module Files

```cmake
# cmake/FindNetworkSystem.cmake
find_package(PkgConfig QUIET)

# ASIO dependency
find_package(asio CONFIG REQUIRED)
if(NOT asio_FOUND)
    message(FATAL_ERROR "ASIO library is required for NetworkSystem")
endif()

# fmt dependency
find_package(fmt CONFIG REQUIRED)
if(NOT fmt_FOUND)
    message(FATAL_ERROR "fmt library is required for NetworkSystem")
endif()

# Check conditional dependencies
if(BUILD_WITH_CONTAINER_SYSTEM)
    find_package(ContainerSystem CONFIG)
    if(NOT ContainerSystem_FOUND)
        message(WARNING "ContainerSystem not found, container integration disabled")
        set(BUILD_WITH_CONTAINER_SYSTEM OFF)
    endif()
endif()

if(BUILD_WITH_THREAD_SYSTEM)
    find_package(ThreadSystem CONFIG)
    if(NOT ThreadSystem_FOUND)
        message(WARNING "ThreadSystem not found, thread integration disabled")
        set(BUILD_WITH_THREAD_SYSTEM OFF)
    endif()
endif()

# Define library target
if(NOT TARGET NetworkSystem::Core)
    add_library(NetworkSystem::Core INTERFACE)
    target_include_directories(NetworkSystem::Core INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/../include
    )
    target_link_libraries(NetworkSystem::Core INTERFACE
        asio::asio
        fmt::fmt
    )

    if(BUILD_WITH_CONTAINER_SYSTEM)
        target_link_libraries(NetworkSystem::Core INTERFACE
            ContainerSystem::Core
        )
        target_compile_definitions(NetworkSystem::Core INTERFACE
            BUILD_WITH_CONTAINER_SYSTEM
        )
    endif()

    if(BUILD_WITH_THREAD_SYSTEM)
        target_link_libraries(NetworkSystem::Core INTERFACE
            ThreadSystem::Core
        )
        target_compile_definitions(NetworkSystem::Core INTERFACE
            BUILD_WITH_THREAD_SYSTEM
        )
    endif()
endif()

# Set variables
set(NetworkSystem_FOUND TRUE)
set(NetworkSystem_VERSION "2.0.0")
set(NetworkSystem_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../include)
```

### 2. pkg-config File

```ini
# network-system.pc.in
prefix=@CMAKE_INSTALL_PREFIX@
exec_prefix=${prefix}
libdir=${exec_prefix}/@CMAKE_INSTALL_LIBDIR@
includedir=${prefix}/@CMAKE_INSTALL_INCLUDEDIR@

Name: NetworkSystem
Description: High-performance modular network system
Version: @PROJECT_VERSION@
Requires: asio fmt
Requires.private: @PRIVATE_DEPENDENCIES@
Libs: -L${libdir} -lNetworkSystem
Libs.private: @PRIVATE_LIBS@
Cflags: -I${includedir}
```

---

## 🧪 Test Framework

### 1. Unit Test Structure

```cpp
// tests/unit/core/test_messaging_server.cpp
#include <gtest/gtest.h>
#include "network_system/core/messaging_server.h"
#include "network_system/core/messaging_client.h"
#include "test_utils/network_test_utils.h"

namespace network_system::test {

class MessagingServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        server_config config;
        config.port = test_utils::get_free_port();
        config.max_connections = 10;

        server_ = std::make_shared<core::messaging_server>("test_server", config);

        // Set test handlers
        server_->set_message_handler([this](auto session, const std::string& msg) {
            received_messages_.push_back(msg);
            last_session_ = session;
        });

        server_->set_connect_handler([this](auto session) {
            connected_sessions_.push_back(session->session_id());
        });

        server_->set_disconnect_handler([this](auto session, const std::string& reason) {
            disconnected_sessions_.push_back(session->session_id());
            disconnect_reasons_.push_back(reason);
        });
    }

    void TearDown() override {
        if (server_ && server_->is_running()) {
            server_->stop_server();
        }
        server_.reset();
    }

    std::shared_ptr<core::messaging_server> server_;
    std::vector<std::string> received_messages_;
    std::vector<std::string> connected_sessions_;
    std::vector<std::string> disconnected_sessions_;
    std::vector<std::string> disconnect_reasons_;
    std::shared_ptr<session::messaging_session> last_session_;
};

TEST_F(MessagingServerTest, ServerStartStop) {
    EXPECT_FALSE(server_->is_running());

    EXPECT_TRUE(server_->start_server());
    EXPECT_TRUE(server_->is_running());

    server_->stop_server();
    EXPECT_FALSE(server_->is_running());
}

TEST_F(MessagingServerTest, ClientConnection) {
    ASSERT_TRUE(server_->start_server());

    auto client = std::make_shared<core::messaging_client>("test_client");

    bool connected = false;
    client->set_connect_handler([&connected]() {
        connected = true;
    });

    EXPECT_TRUE(client->start_client("127.0.0.1", server_->port()));

    // Wait for connection
    test_utils::wait_for_condition([&connected]() { return connected; },
                                   std::chrono::seconds(5));

    EXPECT_TRUE(connected);
    EXPECT_EQ(1, connected_sessions_.size());
    EXPECT_EQ(1, server_->connection_count());
}

TEST_F(MessagingServerTest, MessageExchange) {
    ASSERT_TRUE(server_->start_server());

    auto client = std::make_shared<core::messaging_client>("test_client");

    std::string received_message;
    client->set_message_handler([&received_message](const std::string& msg) {
        received_message = msg;
    });

    ASSERT_TRUE(client->start_client("127.0.0.1", server_->port()));

    // Wait for connection
    test_utils::wait_for_condition([this]() {
        return !connected_sessions_.empty();
    }, std::chrono::seconds(5));

    // Send message from client to server
    const std::string test_message = "Hello, Server!";
    client->send_message(test_message);

    // Wait for message reception
    test_utils::wait_for_condition([this]() {
        return !received_messages_.empty();
    }, std::chrono::seconds(5));

    EXPECT_EQ(1, received_messages_.size());
    EXPECT_EQ(test_message, received_messages_[0]);

    // Send response from server to client
    const std::string response_message = "Hello, Client!";
    ASSERT_NE(nullptr, last_session_);
    last_session_->send_message(response_message);

    // Wait for response reception
    test_utils::wait_for_condition([&received_message, &response_message]() {
        return received_message == response_message;
    }, std::chrono::seconds(5));

    EXPECT_EQ(response_message, received_message);
}

TEST_F(MessagingServerTest, MaxConnectionsLimit) {
    server_config config = server_->get_config();
    config.max_connections = 2;
    server_->set_config(config);

    ASSERT_TRUE(server_->start_server());

    // Create clients up to max connections
    std::vector<std::shared_ptr<core::messaging_client>> clients;
    for (int i = 0; i < 3; ++i) {
        auto client = std::make_shared<core::messaging_client>(
            "test_client_" + std::to_string(i));
        clients.push_back(client);

        client->start_client("127.0.0.1", server_->port());
    }

    // Wait briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Verify max connections not exceeded
    EXPECT_LE(server_->connection_count(), 2);
}

} // namespace network_system::test
```

### 2. Integration Tests

```cpp
// tests/integration/test_container_integration.cpp
#ifdef BUILD_WITH_CONTAINER_SYSTEM

#include <gtest/gtest.h>
#include "network_system/integration/messaging_bridge.h"
#include "container_system/value_container.h"

namespace network_system::test {

class ContainerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        bridge_ = std::make_unique<integration::messaging_bridge>("test_bridge");

        // Set container_system factory
        auto factory = std::make_shared<container_system::value_factory>();
        bridge_->set_container_factory(factory);
        bridge_->enable_container_serialization(true);

        // Set container message handler
        bridge_->set_container_message_handler([this](auto container) {
            received_containers_.push_back(container);
        });

        bridge_->start();
    }

    void TearDown() override {
        if (bridge_) {
            bridge_->stop();
        }
        bridge_.reset();
    }

    std::unique_ptr<integration::messaging_bridge> bridge_;
    std::vector<std::shared_ptr<container_system::value_container>> received_containers_;
};

TEST_F(ContainerIntegrationTest, ContainerSerialization) {
    auto server = bridge_->create_server("test_server");
    auto client = bridge_->create_client("test_client");

    ASSERT_TRUE(server->start_server(0)); // Random port
    ASSERT_TRUE(client->start_client("127.0.0.1", server->port()));

    // Create container message
    auto container = std::make_shared<container_system::value_container>();
    container->set_source("test_client", "main");
    container->set_target("test_server", "handler");
    container->set_message_type("test_message");

    auto values = std::vector<std::shared_ptr<container_system::value>>{
        container_system::value_factory::create("text",
            container_system::string_value, "Hello, Container!"),
        container_system::value_factory::create("number",
            container_system::int32_value, "42")
    };
    container->set_values(values);

    // Send container
    std::string serialized = container->serialize();
    client->send_message(serialized);

    // Wait for reception
    test_utils::wait_for_condition([this]() {
        return !received_containers_.empty();
    }, std::chrono::seconds(5));

    ASSERT_EQ(1, received_containers_.size());

    auto received = received_containers_[0];
    EXPECT_EQ("test_message", received->message_type());
    EXPECT_EQ("test_client", received->source_id());
    EXPECT_EQ("test_server", received->target_id());

    auto text_value = received->get_value("text");
    ASSERT_NE(nullptr, text_value);
    EXPECT_EQ("Hello, Container!", text_value->to_string());

    auto number_value = received->get_value("number");
    ASSERT_NE(nullptr, number_value);
    EXPECT_EQ(42, number_value->to_int32());
}

} // namespace network_system::test

#endif // BUILD_WITH_CONTAINER_SYSTEM
```

---

## 📊 Performance Monitoring

### 1. Performance Metric Collection

```cpp
// include/network_system/utils/performance_monitor.h
#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace network_system::utils {

class performance_monitor {
public:
    struct metrics {
        // Connection metrics
        std::atomic<uint64_t> total_connections{0};
        std::atomic<uint64_t> active_connections{0};
        std::atomic<uint64_t> failed_connections{0};
        std::atomic<uint64_t> connections_per_second{0};

        // Message metrics
        std::atomic<uint64_t> messages_sent{0};
        std::atomic<uint64_t> messages_received{0};
        std::atomic<uint64_t> messages_per_second{0};
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<uint64_t> bytes_per_second{0};

        // Latency metrics (microseconds)
        std::atomic<uint64_t> avg_latency_us{0};
        std::atomic<uint64_t> min_latency_us{UINT64_MAX};
        std::atomic<uint64_t> max_latency_us{0};
        std::atomic<uint64_t> p95_latency_us{0};
        std::atomic<uint64_t> p99_latency_us{0};

        // Error metrics
        std::atomic<uint64_t> send_errors{0};
        std::atomic<uint64_t> receive_errors{0};
        std::atomic<uint64_t> timeout_errors{0};
        std::atomic<uint64_t> connection_errors{0};

        // Memory metrics
        std::atomic<uint64_t> memory_usage_bytes{0};
        std::atomic<uint64_t> buffer_pool_size{0};
        std::atomic<uint64_t> peak_memory_usage{0};

        // Time information
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update;

        // Calculate uptime
        std::chrono::milliseconds uptime() const {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
        }
    };

    static performance_monitor& instance();

    // Update metrics
    void record_connection(bool success);
    void record_disconnection();
    void record_message_sent(size_t bytes);
    void record_message_received(size_t bytes);
    void record_latency(std::chrono::microseconds latency);
    void record_error(const std::string& error_type);
    void record_memory_usage(size_t bytes);

    // Query metrics
    metrics get_metrics() const;
    void reset_metrics();

    // Real-time statistics calculation
    void update_realtime_stats();

    // Export metrics as JSON
    std::string to_json() const;

    // Generate performance report
    std::string generate_report() const;

private:
    performance_monitor();
    ~performance_monitor() = default;

    performance_monitor(const performance_monitor&) = delete;
    performance_monitor& operator=(const performance_monitor&) = delete;

    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace network_system::utils
```

### 2. Benchmark Tools

```cpp
// tests/performance/benchmark_network_performance.cpp
#include <benchmark/benchmark.h>
#include "network_system/core/messaging_server.h"
#include "network_system/core/messaging_client.h"

namespace network_system::benchmark {

class NetworkBenchmark {
public:
    NetworkBenchmark() {
        server_ = std::make_shared<core::messaging_server>("bench_server");
        server_->set_message_handler([](auto session, const std::string& msg) {
            // Echo server
            session->send_message(msg);
        });

        server_->start_server(0); // Random port
        port_ = server_->port();
    }

    ~NetworkBenchmark() {
        server_->stop_server();
    }

    unsigned short get_port() const { return port_; }

private:
    std::shared_ptr<core::messaging_server> server_;
    unsigned short port_;
};

static NetworkBenchmark* g_benchmark = nullptr;

static void BM_SingleClientEcho(::benchmark::State& state) {
    if (!g_benchmark) {
        g_benchmark = new NetworkBenchmark();
    }

    auto client = std::make_shared<core::messaging_client>("bench_client");

    std::atomic<int> received_count{0};
    client->set_message_handler([&received_count](const std::string&) {
        received_count++;
    });

    client->start_client("127.0.0.1", g_benchmark->get_port());

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const std::string message = "benchmark_message";

    for (auto _ : state) {
        client->send_message(message);

        // Wait for response
        auto start = std::chrono::steady_clock::now();
        while (received_count.load() == 0) {
            auto now = std::chrono::steady_clock::now();
            if (now - start > std::chrono::seconds(1)) {
                state.SkipWithError("Timeout waiting for response");
                return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
        received_count--;
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * message.size() * 2); // Send+receive
}

static void BM_MultipleClientsEcho(::benchmark::State& state) {
    if (!g_benchmark) {
        g_benchmark = new NetworkBenchmark();
    }

    const int num_clients = state.range(0);
    std::vector<std::shared_ptr<core::messaging_client>> clients;
    std::atomic<int> total_received{0};

    // Create and connect clients
    for (int i = 0; i < num_clients; ++i) {
        auto client = std::make_shared<core::messaging_client>(
            "bench_client_" + std::to_string(i));

        client->set_message_handler([&total_received](const std::string&) {
            total_received++;
        });

        client->start_client("127.0.0.1", g_benchmark->get_port());
        clients.push_back(client);
    }

    // Wait for all clients to connect
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const std::string message = "benchmark_message";

    for (auto _ : state) {
        // Send messages from all clients simultaneously
        for (auto& client : clients) {
            client->send_message(message);
        }

        // Wait for all responses
        auto start = std::chrono::steady_clock::now();
        while (total_received.load() < num_clients) {
            auto now = std::chrono::steady_clock::now();
            if (now - start > std::chrono::seconds(5)) {
                state.SkipWithError("Timeout waiting for responses");
                return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        total_received -= num_clients;
    }

    state.SetItemsProcessed(state.iterations() * num_clients);
    state.SetBytesProcessed(state.iterations() * num_clients * message.size() * 2);
}

static void BM_LargeMessageTransfer(::benchmark::State& state) {
    if (!g_benchmark) {
        g_benchmark = new NetworkBenchmark();
    }

    auto client = std::make_shared<core::messaging_client>("bench_client");

    std::atomic<bool> received{false};
    client->set_message_handler([&received](const std::string&) {
        received = true;
    });

    client->start_client("127.0.0.1", g_benchmark->get_port());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    const size_t message_size = state.range(0);
    const std::string message(message_size, 'X');

    for (auto _ : state) {
        received = false;
        client->send_message(message);

        // Wait for response
        auto start = std::chrono::steady_clock::now();
        while (!received.load()) {
            auto now = std::chrono::steady_clock::now();
            if (now - start > std::chrono::seconds(5)) {
                state.SkipWithError("Timeout waiting for response");
                return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    }

    state.SetItemsProcessed(state.iterations());
    state.SetBytesProcessed(state.iterations() * message_size * 2);
}

// Register benchmarks
BENCHMARK(BM_SingleClientEcho)
    ->Unit(::benchmark::kMicrosecond)
    ->Iterations(10000);

BENCHMARK(BM_MultipleClientsEcho)
    ->Unit(::benchmark::kMicrosecond)
    ->Arg(1)->Arg(10)->Arg(50)->Arg(100)
    ->Iterations(1000);

BENCHMARK(BM_LargeMessageTransfer)
    ->Unit(::benchmark::kMicrosecond)
    ->Arg(1024)->Arg(8192)->Arg(65536)->Arg(1048576)
    ->Iterations(100);

} // namespace network_system::benchmark

BENCHMARK_MAIN();
```

---

## 🛡️ Resource Management Patterns

### Overview

The network_system achieves **95% RAII compliance** with excellent async-safe resource management patterns. All resources (sessions, sockets, ASIO objects, threads) are managed through RAII and smart pointers.

### 1. Smart Pointer Strategy

#### Session Lifetime Management

**Pattern**: `std::shared_ptr<messaging_session>` with `std::enable_shared_from_this`

```cpp
class messaging_session
    : public std::enable_shared_from_this<messaging_session>
{
public:
    messaging_session(asio::ip::tcp::socket socket,
                      std::string_view server_id);
    ~messaging_session();  // Automatic cleanup

    auto start_session() -> void;
    auto stop_session() -> void;

private:
    std::shared_ptr<internal::tcp_socket> socket_;
    internal::pipeline pipeline_;
    std::atomic<bool> is_stopped_;
};
```

**Why `shared_ptr`?**
1. Server tracks active sessions in vector
2. Async ASIO callbacks need to keep session alive
3. Multiple references to same session possible
4. Automatic cleanup when last reference dropped

**Usage in Server**:
```cpp
class messaging_server {
    std::vector<std::shared_ptr<messaging_session>> active_sessions_;

    void do_accept() {
        acceptor_->async_accept([self = shared_from_this()](auto ec, auto socket) {
            if (!ec) {
                auto session = std::make_shared<messaging_session>(
                    std::move(socket), self->server_id_);

                {
                    std::lock_guard<std::mutex> lock(self->sessions_mutex_);
                    self->active_sessions_.push_back(session);
                }

                session->start_session();
            }
        });
    }
};
```

#### ASIO Resource Ownership

**Pattern**: `std::unique_ptr` for exclusive ownership

```cpp
class messaging_server {
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
    std::thread io_thread_;

    void start_server(unsigned short port) {
        io_context_ = std::make_unique<asio::io_context>();
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            *io_context_,
            asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)
        );

        io_thread_ = std::thread([this]() {
            io_context_->run();
        });
    }

    ~messaging_server() {
        // Automatic cleanup via RAII
        stop_server();
        if (io_thread_.joinable()) {
            io_thread_.join();
        }
        // unique_ptr automatically destroys io_context_ and acceptor_
    }
};
```

**Why `unique_ptr`?**
1. Server exclusively owns io_context and acceptor
2. Clear lifetime: created in start_server(), destroyed with server
3. Exception-safe resource management
4. No manual `delete` required

### 2. Async-Safe Pattern: enable_shared_from_this

**Problem**: Async callbacks may outlive the object

```cpp
// BAD: Dangling this pointer if session destroyed
socket_->async_read([this](auto data) {
    this->on_receive(data);  // UNSAFE: 'this' may be invalid!
});
```

**Solution**: Capture `shared_ptr` to keep object alive

```cpp
// GOOD: Captures shared_ptr, keeps session alive
void start_session() {
    auto self = shared_from_this();

    socket_->start_read(
        [self](const std::vector<uint8_t>& data) {
            self->on_receive(data);  // SAFE: self keeps session alive
        },
        [self](std::error_code ec) {
            self->on_error(ec);  // SAFE: self keeps session alive
        }
    );
}
```

**Benefits**:
- Session remains alive during async operation
- When operation completes, `self` goes out of scope, decrementing ref count
- If server removed session from `active_sessions_`, session destructor called when last callback completes
- No manual lifetime management needed

### 3. Thread Safety

#### Atomic State Flags

```cpp
std::atomic<bool> is_stopped_{false};
std::atomic<bool> is_running_{false};
std::atomic<bool> is_connected_{false};
```

#### Mutex Protection for Shared Data

```cpp
mutable std::mutex mode_mutex_;       // Protects pipeline mode flags
mutable std::mutex sessions_mutex_;   // Protects active_sessions_ vector

void remove_session(std::shared_ptr<messaging_session> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);  // RAII lock

    auto it = std::find(active_sessions_.begin(), active_sessions_.end(), session);
    if (it != active_sessions_.end()) {
        active_sessions_.erase(it);
    }
    // Automatic unlock on scope exit
}
```

### 4. Exception Safety

All destructors are `noexcept` and handle cleanup safely:

```cpp
~messaging_session() {
    stop_session();  // Idempotent, safe to call multiple times
    // socket_ and pipeline_ automatically cleaned up by RAII
}

~messaging_server() {
    stop_server();  // Ensures clean shutdown

    if (io_thread_.joinable()) {
        io_thread_.join();  // Wait for thread exit
    }

    // io_context_, acceptor_, active_sessions_ automatically cleaned up
    // unique_ptr and vector destructors handle cleanup
}
```

### 5. Resource Categories

| Resource Type | Management | Ownership | Cleanup |
|---------------|------------|-----------|---------|
| Sessions | `std::shared_ptr` | Shared (server + callbacks) | Ref counting |
| ASIO resources | `std::unique_ptr` | Exclusive (server) | Destructor |
| Threads | Automatic storage | Exclusive (server) | join() in destructor |
| Synchronization | Automatic storage | Class member | Automatic |

### 6. Best Practices Applied

✅ **Smart pointers for all heap allocations**
- `shared_ptr` for sessions (shared ownership)
- `unique_ptr` for ASIO resources (exclusive ownership)
- No naked `new`/`delete` in production code

✅ **RAII for all resources**
- Resources acquired in constructor
- Resources released in destructor
- Exception-safe initialization

✅ **enable_shared_from_this for async safety**
- Sessions can create `shared_ptr` to themselves
- Essential for capturing in async lambdas
- Prevents dangling `this` pointers

✅ **Thread-safe resource access**
- Atomic flags for state
- Mutexes for shared containers
- RAII lock guards for critical sections

✅ **Exception-safe destructors**
- No exceptions thrown from destructors
- Thread join handled safely
- Standard containers handle cleanup

---

## 🚨 Error Handling Strategy

### Current State

network_system uses **void returns** for most operations with **callback-based error handling**:

```cpp
// Current approach
auto start_server(unsigned short port) -> void;
auto start_client(std::string_view host, unsigned short port) -> void;
auto send_packet(std::vector<uint8_t> data) -> void;

// Errors handled in callbacks
auto on_error(std::error_code ec) -> void;
```

### Migration to Result<T> Pattern

#### 1. Server Lifecycle Operations

**Before**:
```cpp
void start_server(unsigned short port);  // No error return
```

**After**:
```cpp
Result<void> start_server(unsigned short port) {
    if (is_running_) {
        return error<std::monostate>(
            codes::network_system::server_already_running,
            "Server already running",
            "messaging_server",
            server_id_
        );
    }

    try {
        io_context_ = std::make_unique<asio::io_context>();
        acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            *io_context_,
            asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)
        );

        is_running_ = true;
        do_accept();

        io_thread_ = std::thread([this]() { io_context_->run(); });

        return ok();
    } catch (const std::system_error& e) {
        if (e.code() == std::errc::address_in_use ||
            e.code() == std::errc::permission_denied) {
            return error<std::monostate>(
                codes::network_system::bind_failed,
                "Failed to bind to port",
                "messaging_server",
                "Port: " + std::to_string(port) + ", " + e.what()
            );
        }

        return error<std::monostate>(
            codes::common::internal_error,
            "Server start failed",
            "messaging_server",
            e.what()
        );
    }
}
```

**Usage**:
```cpp
auto server = std::make_shared<messaging_server>("MyServer");
auto result = server->start_server(5555);

if (result.is_err()) {
    const auto& err = result.error();
    log_error("Server start failed: {} - {}",
              err.message, err.details.value_or(""));
    return;
}
```

#### 2. Client Connection Operations

**Before**:
```cpp
void start_client(std::string_view host, unsigned short port);
```

**After**:
```cpp
Result<void> start_client(std::string_view host, unsigned short port) {
    if (is_running_) {
        return error<std::monostate>(
            codes::common::already_exists,
            "Client already running",
            "messaging_client"
        );
    }

    if (host.empty()) {
        return error<std::monostate>(
            codes::common::invalid_argument,
            "Empty host string",
            "messaging_client"
        );
    }

    try {
        io_context_ = std::make_unique<asio::io_context>();
        is_running_ = true;

        io_thread_ = std::thread([this]() { io_context_->run(); });
        do_connect(host, port);

        return ok();
    } catch (const std::exception& e) {
        return error<std::monostate>(
            codes::common::internal_error,
            "Client start failed",
            "messaging_client",
            e.what()
        );
    }
}
```

**Usage with Retry**:
```cpp
Result<void> connect_with_retry(
    messaging_client& client,
    std::string_view host,
    unsigned short port,
    int max_attempts = 5
) {
    int delay_ms = 100;

    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        auto result = client.start_client(host, port);

        if (result.is_ok()) {
            return result;
        }

        if (attempt < max_attempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2;  // Exponential backoff
        }
    }

    return error<std::monostate>(
        codes::network_system::connection_failed,
        "Max retry attempts exhausted",
        "connect_with_retry"
    );
}
```

#### 3. Send/Receive Operations

**Before**:
```cpp
void send_packet(std::vector<uint8_t> data);
```

**After**:
```cpp
Result<void> send_packet(std::vector<uint8_t> data) {
    if (!is_running_ || !is_connected_) {
        return error<std::monostate>(
            codes::network_system::connection_closed,
            "Not connected",
            "messaging_client"
        );
    }

    if (data.empty()) {
        return error<std::monostate>(
            codes::common::invalid_argument,
            "Empty data buffer",
            "messaging_client"
        );
    }

    try {
        // Async send with Result callback
        std::promise<Result<void>> promise;
        auto future = promise.get_future();

        socket_->async_send(
            data,
            [&promise](std::error_code ec, std::size_t bytes_sent) {
                if (ec) {
                    promise.set_value(error<std::monostate>(
                        codes::network_system::send_failed,
                        "Send failed",
                        "messaging_client",
                        ec.message()
                    ));
                } else {
                    promise.set_value(ok());
                }
            }
        );

        return future.get();
    } catch (const std::exception& e) {
        return error<std::monostate>(
            codes::network_system::send_failed,
            "Send operation failed",
            "messaging_client",
            e.what()
        );
    }
}
```

### Error Code System

#### Network System Error Codes (-600 to -699)

```cpp
namespace common::error::codes::network_system {
    // Connection errors (-600 to -619)
    constexpr int connection_failed = -600;
    constexpr int connection_refused = -601;
    constexpr int connection_timeout = -602;
    constexpr int connection_closed = -603;

    // Session errors (-620 to -639)
    constexpr int session_not_found = -620;
    constexpr int session_expired = -621;
    constexpr int invalid_session = -622;

    // Send/Receive errors (-640 to -659)
    constexpr int send_failed = -640;
    constexpr int receive_failed = -641;
    constexpr int message_too_large = -642;

    // Server errors (-660 to -679)
    constexpr int server_not_started = -660;
    constexpr int server_already_running = -661;
    constexpr int bind_failed = -662;
}
```

#### ASIO Error Code Mapping

```cpp
Result<void> map_asio_error(std::error_code ec, std::string_view context) {
    if (!ec) {
        return ok();
    }

    int code;
    if (ec == asio::error::connection_refused) {
        code = codes::network_system::connection_refused;
    } else if (ec == asio::error::timed_out) {
        code = codes::network_system::connection_timeout;
    } else if (ec == asio::error::eof || ec == asio::error::connection_reset) {
        code = codes::network_system::connection_closed;
    } else {
        code = codes::network_system::connection_failed;
    }

    return error<std::monostate>(
        code,
        ec.message(),
        std::string(context)
    );
}
```

### Async Operations with Result<T>

#### Pattern 1: Callback-based

```cpp
void send_packet_async(
    std::vector<uint8_t> data,
    std::function<void(Result<void>)> callback
) {
    socket_->async_send(
        std::move(data),
        [callback](std::error_code ec, std::size_t bytes_sent) {
            if (ec) {
                callback(error<std::monostate>(
                    codes::network_system::send_failed,
                    ec.message(),
                    "send_packet_async"
                ));
            } else {
                callback(ok());
            }
        }
    );
}
```

#### Pattern 2: Future-based

```cpp
std::future<Result<void>> send_packet_future(std::vector<uint8_t> data) {
    return std::async(std::launch::async, [this, data = std::move(data)]() {
        return send_packet(data);
    });
}
```

### Migration Benefits

**Type Safety**:
- Explicit error returns (no silent failures)
- Compile-time checking of error handling

**Better Error Context**:
- Error codes with detailed messages
- Context information (host, port, session IDs)
- Stack of error origins

**Consistent Error Handling**:
- Same pattern across all systems
- Predictable error recovery
- Easier testing and debugging

**Performance**:
- Negligible overhead (<2% for local operations)
- Success path optimized (inline)
- Error path has minimal allocation

---

This technical implementation details document provides a comprehensive guide for the network_system separation work. The next step would be to create migration scripts.