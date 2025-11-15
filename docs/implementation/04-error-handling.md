# Error Handling Strategy

> **Part 4 of 4** | [📑 Index](README.md) | [← Previous: Performance](03-performance-and-resources.md)

> **Language:** **English** | [한국어](04-error-handling_KO.md)

This document covers the migration to `Result<T>` pattern and error handling strategies for the network_system.

---


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
---

*Last Updated: 2025-10-20*
