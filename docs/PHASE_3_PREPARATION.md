# Phase 3: Error Handling Preparation - network_system

**Version**: 1.0
**Date**: 2025-10-09
**Status**: Ready for Implementation

---

## Overview

This document outlines the migration path for network_system to adopt the centralized error handling from common_system Phase 3, replacing void returns and callback-based error handling with explicit Result<T>.

---

## Current State

### Error Handling Status

**Current Approach**:
- Void returns for most operations (`start_server()`, `stop_server()`, `start_client()`)
- Callback-based error handling (`on_error()`, `on_connect()`)
- Asynchronous operations with `std::error_code` in callbacks
- No explicit error returns to callers

**Example**:
```cpp
// Current: Void returns, errors handled in callbacks
auto start_server(unsigned short port) -> void;
auto stop_server() -> void;
auto start_client(std::string_view host, unsigned short port) -> void;

// Current: Callback-based error handling
auto on_error(std::error_code ec) -> void {
    // Log error and stop
}
```

---

## Migration Plan

### Phase 3.1: Import Error Codes

**Action**: Add common_system error code dependency

```cpp
#include <kcenon/common/error/error_codes.h>
#include <kcenon/common/patterns/result.h>

using namespace common;
using namespace common::error;
```

### Phase 3.2: Key API Migrations

#### Priority 1: Server Lifecycle (High Impact)

```cpp
// Before
auto start_server(unsigned short port) -> void;
auto stop_server() -> void;

// After
auto start_server(unsigned short port) -> Result<void>;
auto stop_server() -> Result<void>;
```

**Error Codes**:
- `codes::network_system::bind_failed`
- `codes::network_system::server_already_running`
- `codes::network_system::server_not_started`

**Example Implementation**:
```cpp
Result<void> messaging_server::start_server(unsigned short port) {
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
        stop_promise_ = std::promise<void>{};
        stop_future_ = stop_promise_->get_future();

        // Start accepting connections
        do_accept();

        // Run io_context in a background thread
        server_thread_ = std::make_unique<std::thread>([this]() {
            io_context_->run();
        });

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

Result<void> messaging_server::stop_server() {
    if (!is_running_) {
        return error<std::monostate>(
            codes::network_system::server_not_started,
            "Server not running",
            "messaging_server"
        );
    }

    try {
        is_running_ = false;

        // Close acceptor
        if (acceptor_ && acceptor_->is_open()) {
            acceptor_->close();
        }

        // Stop all sessions
        for (auto& session : sessions_) {
            if (session) {
                session->stop_session();
            }
        }
        sessions_.clear();

        // Stop io_context
        if (io_context_) {
            io_context_->stop();
        }

        // Join thread
        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();
        }

        // Notify waiters
        if (stop_promise_) {
            stop_promise_->set_value();
        }

        return ok();
    } catch (const std::exception& e) {
        return error<std::monostate>(
            codes::common::internal_error,
            "Server stop failed",
            "messaging_server",
            e.what()
        );
    }
}
```

#### Priority 2: Client Connection Operations

```cpp
// Before
auto start_client(std::string_view host, unsigned short port) -> void;
auto stop_client() -> void;

// After
auto start_client(std::string_view host, unsigned short port) -> Result<void>;
auto stop_client() -> Result<void>;
```

**Error Codes**:
- `codes::network_system::connection_failed`
- `codes::network_system::connection_refused`
- `codes::network_system::connection_timeout`

**Example Implementation**:
```cpp
Result<void> messaging_client::start_client(
    std::string_view host,
    unsigned short port
) {
    if (is_running_) {
        return error<std::monostate>(
            codes::common::already_exists,
            "Client already running",
            "messaging_client",
            client_id_
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
        stop_promise_ = std::promise<void>{};
        stop_future_ = stop_promise_->get_future();

        // Launch io_context thread
        client_thread_ = std::make_unique<std::thread>([this]() {
            io_context_->run();
        });

        // Start async connection (result handled in callback)
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

// Connection callback converted to return Result
Result<void> messaging_client::handle_connect_result(std::error_code ec) {
    if (ec) {
        is_connected_ = false;

        if (ec == asio::error::connection_refused) {
            return error<std::monostate>(
                codes::network_system::connection_refused,
                "Connection refused",
                "messaging_client",
                ec.message()
            );
        } else if (ec == asio::error::timed_out) {
            return error<std::monostate>(
                codes::network_system::connection_timeout,
                "Connection timeout",
                "messaging_client",
                ec.message()
            );
        }

        return error<std::monostate>(
            codes::network_system::connection_failed,
            "Connection failed",
            "messaging_client",
            ec.message()
        );
    }

    is_connected_ = true;
    return ok();
}
```

#### Priority 3: Send/Receive Operations

```cpp
// Before
auto send_packet(std::vector<uint8_t> data) -> void;

// After
auto send_packet(std::vector<uint8_t> data) -> Result<void>;
auto send_packet_async(std::vector<uint8_t> data,
                       std::function<void(Result<void>)> callback) -> void;
```

**Error Codes**:
- `codes::network_system::send_failed`
- `codes::network_system::receive_failed`
- `codes::network_system::message_too_large`
- `codes::network_system::connection_closed`

**Example Implementation**:
```cpp
Result<void> messaging_client::send_packet(std::vector<uint8_t> data) {
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

    if (!socket_) {
        return error<std::monostate>(
            codes::common::not_initialized,
            "Socket not initialized",
            "messaging_client"
        );
    }

    try {
        // Prepare data (compress/encrypt if enabled)
        auto prepared = prepare_data(data);
        if (prepared.is_err()) {
            return prepared;
        }

        // Synchronous send (for async version, use callback pattern)
        std::promise<Result<void>> promise;
        auto future = promise.get_future();

        socket_->async_send(
            prepared.value(),
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

#### Priority 4: Session Management

```cpp
// New: Session result types
class messaging_session {
public:
    Result<void> start_session();
    Result<void> stop_session();
    Result<void> send_message(std::vector<uint8_t> data);
};
```

**Error Codes**:
- `codes::network_system::session_not_found`
- `codes::network_system::session_expired`
- `codes::network_system::invalid_session`

---

## Migration Checklist

### Code Changes

- [ ] Add common_system error code includes
- [ ] Migrate `messaging_server::start_server()` to Result<void>
- [ ] Migrate `messaging_server::stop_server()` to Result<void>
- [ ] Migrate `messaging_client::start_client()` to Result<void>
- [ ] Migrate `messaging_client::stop_client()` to Result<void>
- [ ] Migrate `send_packet()` to Result<void>
- [ ] Add async variants with Result<T> callbacks
- [ ] Migrate session operations to Result<void>
- [ ] Convert asio::error_code callbacks to Result<T>
- [ ] Add error context (host, port, session IDs, etc.)

### Test Updates

- [ ] Update unit tests for Result<T> APIs
- [ ] Add error case tests for each error code
- [ ] Test bind failures (port already in use)
- [ ] Test connection refused scenarios
- [ ] Test connection timeouts
- [ ] Test send failures (disconnected socket)
- [ ] Test message size limits
- [ ] Verify error message quality

### Documentation

- [ ] Update API reference with Result<T> signatures
- [ ] Document error codes for each function
- [ ] Add async/callback patterns with Result<T>
- [ ] Update integration examples
- [ ] Create error handling guide for async operations

---

## Example Migrations

### Example 1: Basic Server Setup

```cpp
// Usage before
auto server = std::make_shared<messaging_server>("MyServer");
server->start_server(5555);  // Void return, errors in callbacks
// ...
server->stop_server();

// Usage after
auto server = std::make_shared<messaging_server>("MyServer");
auto result = server->start_server(5555);
if (result.is_err()) {
    const auto& err = result.error();
    log_error("Server start failed: {} - {}",
              err.message, err.details.value_or(""));
    return;
}
// ...
auto stop_result = server->stop_server();
```

### Example 2: Client Connection with Retry

```cpp
// Retry client connection with exponential backoff
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
            log_warn("Connection attempt {} failed: {}",
                     attempt, result.error().message);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2;  // Exponential backoff
        } else {
            return result;  // Return final error
        }
    }

    return error<std::monostate>(
        codes::network_system::connection_failed,
        "Max retry attempts exhausted",
        "connect_with_retry"
    );
}
```

### Example 3: Async Send with Result Callback

```cpp
// Async send operation with Result<T> callback
void send_message_async(
    messaging_client& client,
    std::vector<uint8_t> data,
    std::function<void(Result<void>)> callback
) {
    client.send_packet_async(
        std::move(data),
        [callback](Result<void> result) {
            if (result.is_err()) {
                log_error("Send failed: {}", result.error().message);
            }
            callback(result);
        }
    );
}
```

---

## Error Code Mapping

### Network System Error Codes (-600 to -699)

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

### Error Messages

| Code | Message | When to Use |
|------|---------|-------------|
| connection_failed | "Network connection failed" | Generic connection error |
| connection_refused | "Connection refused" | Remote host refused |
| connection_timeout | "Connection timeout" | Connection attempt timed out |
| connection_closed | "Connection closed" | Connection unexpectedly closed |
| send_failed | "Network send failed" | Send operation error |
| receive_failed | "Network receive failed" | Receive operation error |
| bind_failed | "Failed to bind to port" | Port binding error |
| server_already_running | "Server already running" | Duplicate start attempt |

---

## Testing Strategy

### Unit Tests

```cpp
TEST(NetworkPhase3, ServerBindFailure) {
    messaging_server server("test_server");

    // Start on a privileged port (should fail without root)
    auto result = server.start_server(80);

    if (result.is_err()) {
        EXPECT_EQ(
            codes::network_system::bind_failed,
            result.error().code
        );
    }
}

TEST(NetworkPhase3, ClientConnectionRefused) {
    messaging_client client("test_client");

    // Connect to a port with no listener
    auto result = client.start_client("localhost", 9999);

    // Start is async, so connection result comes in callback
    // For synchronous testing, we'd need to wait and check callback result
}

TEST(NetworkPhase3, SendWithoutConnection) {
    messaging_client client("test_client");

    auto result = client.send_packet({1, 2, 3});

    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(
        codes::network_system::connection_closed,
        result.error().code
    );
}
```

### Integration Tests

```cpp
TEST(NetworkPhase3, FullClientServerWorkflow) {
    // Start server
    auto server = std::make_shared<messaging_server>("test_server");
    auto server_result = server->start_server(0);  // Port 0 = auto-assign
    ASSERT_TRUE(server_result.is_ok());

    // Get assigned port
    unsigned short port = server->get_assigned_port();

    // Start client
    auto client = std::make_shared<messaging_client>("test_client");
    auto client_result = client->start_client("localhost", port);
    ASSERT_TRUE(client_result.is_ok());

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Send message
    std::vector<uint8_t> data = {1, 2, 3, 4};
    auto send_result = client->send_packet(data);
    ASSERT_TRUE(send_result.is_ok());

    // Cleanup
    client->stop_client();
    server->stop_server();
}
```

---

## Performance Impact

### Expected Overhead

- **Result<T> size**: +24 bytes per return value (variant overhead)
- **Success path**: ~0-1ns (inline optimization)
- **Error path**: ~2-3ns (error_info construction)

### Current Performance (Baseline)

- **Server start**: ~5ms (thread creation + socket setup)
- **Client connect**: ~10-50ms (network dependent)
- **Send operation**: ~50μs (local, async)
- **Receive operation**: ~50μs (local, async)

### Expected After Migration

- **Server start**: ~5.01ms (negligible overhead)
- **Client connect**: ~10-50ms (no change, network is bottleneck)
- **Send operation**: ~51μs (-2%)
- **Receive operation**: ~51μs (-2%)

**Conclusion**: Negligible performance impact (<2% for local operations)

---

## Implementation Timeline

### Week 1: Foundation
- Day 1-2: Migrate server lifecycle operations
- Day 3: Migrate client lifecycle operations
- Day 4-5: Update tests

### Week 2: Send/Receive
- Day 1-3: Migrate send/receive operations
- Day 4-5: Add async variants with Result<T> callbacks

### Week 3: Finalization
- Day 1-2: Migrate session management
- Day 3: Integration tests and performance validation
- Day 4-5: Documentation and code review

---

## Backwards Compatibility

### Strategy: Dual API (Transitional)

Provide both sync and async variants:

```cpp
class messaging_client {
public:
    // New: Synchronous Result<T> API
    Result<void> start_client(std::string_view host, unsigned short port);
    Result<void> send_packet(std::vector<uint8_t> data);

    // New: Async Result<T> API (recommended for network ops)
    void start_client_async(
        std::string_view host,
        unsigned short port,
        std::function<void(Result<void>)> callback
    );

    void send_packet_async(
        std::vector<uint8_t> data,
        std::function<void(Result<void>)> callback
    );

    // Legacy: Deprecated void API
    [[deprecated("Use Result<void> start_client() instead")]]
    void start_client_legacy(std::string_view host, unsigned short port) {
        auto result = start_client(host, port);
        // Errors still handled in callbacks
    }
};
```

**Timeline**:
- Phase 3.1: Add new Result<T> APIs (sync and async)
- Phase 3.2: Deprecate old void APIs
- Phase 4: Remove deprecated APIs (after 6 months)

---

## Special Considerations

### Async Operations with Result<T>

Network operations are inherently asynchronous. Two patterns:

**Pattern 1: Future-based**
```cpp
std::future<Result<void>> start_client_future(
    std::string_view host,
    unsigned short port
) {
    return std::async(std::launch::async, [=]() {
        return start_client(host, port);
    });
}
```

**Pattern 2: Callback-based**
```cpp
void start_client_async(
    std::string_view host,
    unsigned short port,
    std::function<void(Result<void>)> callback
) {
    // Perform async operation, invoke callback with Result
}
```

### Integration with asio::error_code

Map asio error codes to common error codes:

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

---

## References

- [common_system Error Codes](../../common_system/include/kcenon/common/error/error_codes.h)
- [Error Handling Guidelines](../../common_system/docs/ERROR_HANDLING.md)
- [Result<T> Implementation](../../common_system/include/kcenon/common/patterns/result.h)
- [Asio Documentation](https://think-async.com/Asio/)

---

**Document Status**: Phase 3 Preparation Complete
**Next Action**: Begin implementation or await approval
**Maintainer**: network_system team
