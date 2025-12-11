# C++20 Concepts for Network System

**Version:** 0.1.0
**Last Updated:** 2025-12-10
**Status:** Complete

This guide provides comprehensive documentation for the C++20 Concepts feature in network_system.

---

## Table of Contents

- [Overview](#overview)
- [Why Concepts?](#why-concepts)
- [Compiler Requirements](#compiler-requirements)
- [Header Files](#header-files)
- [Concept Reference](#concept-reference)
  - [Data Buffer Concepts](#data-buffer-concepts)
  - [Callback Concepts](#callback-concepts)
  - [Network Component Concepts](#network-component-concepts)
  - [Pipeline Concepts](#pipeline-concepts)
  - [Utility Concepts](#utility-concepts)
- [Usage Patterns](#usage-patterns)
- [Integration with common_system](#integration-with-common_system)
- [Best Practices](#best-practices)
- [Examples](#examples)

---

## Overview

network_system provides 16 C++20 concepts for compile-time validation of network-related types. These concepts improve code quality through:

- **Better error messages**: Clear compile-time errors instead of cryptic template failures
- **Self-documenting code**: Concepts express interface requirements explicitly
- **Type safety**: Catch type mismatches at compile time, not runtime

### Quick Start

```cpp
#include <kcenon/network/concepts/concepts.h>

// Using a concept to constrain a template parameter
template<network_system::concepts::DataReceiveHandler Handler>
void set_receive_callback(Handler&& handler) {
    // Compile-time guaranteed: handler accepts const std::vector<uint8_t>&
    receive_handler_ = std::forward<Handler>(handler);
}

// The compiler will reject invalid handlers with clear error messages
set_receive_callback([](const std::vector<uint8_t>& data) {
    // Process data...
});
```

---

## Why Concepts?

### Before Concepts (SFINAE)

```cpp
// Hard to read, cryptic error messages
template<typename F,
         typename = std::enable_if_t<
             std::is_invocable_v<F, const std::vector<uint8_t>&>>>
void set_handler(F&& handler);
```

**Error message without concepts**:
```
error: no matching function for call to 'set_handler'
note: candidate template ignored: substitution failure [with F = int]:
      no type named 'type' in 'std::enable_if<false, void>'
```

### With Concepts

```cpp
// Clear and readable
template<DataReceiveHandler Handler>
void set_handler(Handler&& handler);
```

**Error message with concepts**:
```
error: constraints not satisfied for template 'set_handler'
note: because 'int' does not satisfy 'DataReceiveHandler'
note: because 'std::invocable<int, const std::vector<uint8_t>&>' evaluated to false
```

---

## Compiler Requirements

| Compiler | Minimum Version | Recommended |
|----------|----------------|-------------|
| GCC | 10+ | 11+ |
| Clang | 10+ | 14+ |
| MSVC | 2022 (19.29+) | 2022 |
| Apple Clang | 13+ | 14+ |

Concepts require **C++20** standard. Enable with:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

---

## Header Files

### Primary Headers

| Header | Purpose |
|--------|---------|
| `<kcenon/network/concepts/concepts.h>` | Unified umbrella header (recommended) |
| `<kcenon/network/concepts/network_concepts.h>` | All 16 concept definitions |

### Backward Compatibility Headers

| Header | Purpose |
|--------|---------|
| `<network_system/concepts/concepts.h>` | Legacy namespace support |
| `<network_system/concepts/network_concepts.h>` | Legacy namespace support |

### Usage

```cpp
// Recommended: Use the umbrella header
#include <kcenon/network/concepts/concepts.h>

// Or directly include network concepts
#include <kcenon/network/concepts/network_concepts.h>

// Concepts are in the network_system::concepts namespace
using namespace network_system::concepts;
```

---

## Concept Reference

### Data Buffer Concepts

#### ByteBuffer

A type that can serve as a network data buffer with read-only access.

```cpp
template <typename T>
concept ByteBuffer = requires(const T t) {
    { t.data() } -> std::convertible_to<const void*>;
    { t.size() } -> std::convertible_to<std::size_t>;
};
```

**Requirements**:
- `data()`: Returns pointer to byte data
- `size()`: Returns number of bytes

**Satisfied by**:
- `std::vector<uint8_t>`
- `std::string`
- `std::array<uint8_t, N>`
- `std::span<const uint8_t>`

**Example**:
```cpp
template<ByteBuffer Buffer>
void send_data(const Buffer& buffer) {
    const void* ptr = buffer.data();
    std::size_t len = buffer.size();
    // Send ptr with len bytes...
}

// Usage
std::vector<uint8_t> data = {1, 2, 3, 4};
send_data(data);  // OK

std::string str = "hello";
send_data(str);   // OK

int x = 42;
send_data(x);     // Compile error: 'int' does not satisfy 'ByteBuffer'
```

---

#### MutableByteBuffer

A mutable byte buffer that can be resized, extending ByteBuffer.

```cpp
template <typename T>
concept MutableByteBuffer = ByteBuffer<T> && requires(T t, std::size_t n) {
    { t.resize(n) };
    { t.data() } -> std::convertible_to<void*>;
};
```

**Requirements**:
- All `ByteBuffer` requirements
- `resize(n)`: Resize buffer to n bytes
- `data()`: Returns mutable pointer

**Satisfied by**:
- `std::vector<uint8_t>`
- `std::string`

**Example**:
```cpp
template<MutableByteBuffer Buffer>
void receive_data(Buffer& buffer, std::size_t expected_size) {
    buffer.resize(expected_size);
    void* ptr = buffer.data();
    // Fill buffer with received data...
}

// Usage
std::vector<uint8_t> buffer;
receive_data(buffer, 1024);  // OK

std::array<uint8_t, 100> arr;
receive_data(arr, 50);       // Compile error: no resize() method
```

---

### Callback Concepts

#### DataReceiveHandler

A callback type for handling received data.

```cpp
template <typename F>
concept DataReceiveHandler = std::invocable<F, const std::vector<uint8_t>&>;
```

**Signature**: `void(const std::vector<uint8_t>&)`

**Example**:
```cpp
template<DataReceiveHandler Handler>
void set_receive_handler(Handler&& handler) {
    receive_handler_ = std::forward<Handler>(handler);
}

// Valid handlers
set_receive_handler([](const std::vector<uint8_t>& data) {
    std::cout << "Received " << data.size() << " bytes\n";
});

void my_handler(const std::vector<uint8_t>& data);
set_receive_handler(my_handler);

struct MyHandler {
    void operator()(const std::vector<uint8_t>& data) { /* ... */ }
};
set_receive_handler(MyHandler{});
```

---

#### ErrorHandler

A callback type for handling network errors.

```cpp
template <typename F>
concept ErrorHandler = std::invocable<F, std::error_code>;
```

**Signature**: `void(std::error_code)`

**Example**:
```cpp
template<ErrorHandler Handler>
void set_error_handler(Handler&& handler) {
    error_handler_ = std::forward<Handler>(handler);
}

set_error_handler([](std::error_code ec) {
    std::cerr << "Error: " << ec.message() << "\n";
});
```

---

#### ConnectionHandler

A callback type for handling connection state changes.

```cpp
template <typename F>
concept ConnectionHandler = std::invocable<F>;
```

**Signature**: `void()`

**Example**:
```cpp
template<ConnectionHandler Handler>
void set_connected_handler(Handler&& handler) {
    on_connected_ = std::forward<Handler>(handler);
}

set_connected_handler([]() {
    std::cout << "Connected!\n";
});
```

---

#### SessionHandler

A callback type for handling session events with a session pointer.

```cpp
template <typename F, typename Session>
concept SessionHandler = std::invocable<F, std::shared_ptr<Session>>;
```

**Signature**: `void(std::shared_ptr<Session>)`

**Example**:
```cpp
template<typename Session, SessionHandler<Session> Handler>
void set_session_handler(Handler&& handler) {
    session_handler_ = std::forward<Handler>(handler);
}

set_session_handler([](std::shared_ptr<MySession> session) {
    std::cout << "Session: " << session->get_session_id() << "\n";
});
```

---

#### SessionDataHandler

A callback type for handling data received on a specific session.

```cpp
template <typename F, typename Session>
concept SessionDataHandler =
    std::invocable<F, std::shared_ptr<Session>, const std::vector<uint8_t>&>;
```

**Signature**: `void(std::shared_ptr<Session>, const std::vector<uint8_t>&)`

---

#### SessionErrorHandler

A callback type for handling errors on a specific session.

```cpp
template <typename F, typename Session>
concept SessionErrorHandler =
    std::invocable<F, std::shared_ptr<Session>, std::error_code>;
```

**Signature**: `void(std::shared_ptr<Session>, std::error_code)`

---

#### DisconnectionHandler

A callback type for handling disconnection events with session ID.

```cpp
template <typename F>
concept DisconnectionHandler = std::invocable<F, const std::string&>;
```

**Signature**: `void(const std::string&)` (session ID)

**Example**:
```cpp
template<DisconnectionHandler Handler>
void set_disconnect_handler(Handler&& handler) {
    on_disconnect_ = std::forward<Handler>(handler);
}

set_disconnect_handler([](const std::string& session_id) {
    std::cout << "Disconnected: " << session_id << "\n";
});
```

---

#### RetryCallback

A callback type for reconnection attempt notifications.

```cpp
template <typename F>
concept RetryCallback = std::invocable<F, std::size_t>;
```

**Signature**: `void(std::size_t)` (attempt number)

**Example**:
```cpp
template<RetryCallback Handler>
void set_retry_handler(Handler&& handler) {
    on_retry_ = std::forward<Handler>(handler);
}

set_retry_handler([](std::size_t attempt) {
    std::cout << "Retry attempt: " << attempt << "\n";
});
```

---

### Network Component Concepts

#### NetworkClient

A type that satisfies basic network client requirements.

```cpp
template <typename T>
concept NetworkClient = requires(T t, std::vector<uint8_t> data) {
    { t.is_connected() } -> std::convertible_to<bool>;
    { t.send_packet(std::move(data)) };
    { t.stop_client() };
};
```

**Requirements**:
- `is_connected()`: Returns connection status
- `send_packet(data)`: Sends data packet
- `stop_client()`: Stops the client

**Example**:
```cpp
template<NetworkClient Client>
void use_client(Client& client, const std::string& message) {
    if (client.is_connected()) {
        std::vector<uint8_t> data(message.begin(), message.end());
        client.send_packet(std::move(data));
    }
}

// Works with messaging_client
network_system::core::messaging_client client("MyClient");
use_client(client, "Hello!");
```

---

#### NetworkServer

A type that satisfies basic network server requirements.

```cpp
template <typename T>
concept NetworkServer = requires(T t, unsigned short port) {
    { t.start_server(port) };
    { t.stop_server() };
};
```

**Requirements**:
- `start_server(port)`: Starts listening on port
- `stop_server()`: Stops the server

**Example**:
```cpp
template<NetworkServer Server>
void manage_server(Server& server, unsigned short port) {
    server.start_server(port);
    // ... run server ...
    server.stop_server();
}
```

---

#### NetworkSession

A type that represents a network session.

```cpp
template <typename T>
concept NetworkSession = requires(T t) {
    { t.get_session_id() } -> std::convertible_to<std::string>;
    { t.start_session() };
    { t.stop_session() };
};
```

**Requirements**:
- `get_session_id()`: Returns unique session identifier
- `start_session()`: Starts the session
- `stop_session()`: Stops the session

**Example**:
```cpp
template<NetworkSession Session>
void handle_session(std::shared_ptr<Session> session) {
    auto id = session->get_session_id();
    std::cout << "Handling session: " << id << "\n";
    session->start_session();
}
```

---

### Pipeline Concepts

#### DataTransformer

A type that can transform data (e.g., compression, encryption).

```cpp
template <typename T>
concept DataTransformer = requires(T t, std::vector<uint8_t>& data) {
    { t.transform(data) } -> std::convertible_to<bool>;
};
```

**Requirements**:
- `transform(data)`: Transforms data in-place, returns success

**Example**:
```cpp
template<DataTransformer Transformer>
bool apply_transform(Transformer& t, std::vector<uint8_t>& data) {
    return t.transform(data);
}

// Custom compressor
class GzipCompressor {
public:
    bool transform(std::vector<uint8_t>& data) {
        // Compress data...
        return true;
    }
};

GzipCompressor compressor;
std::vector<uint8_t> data = { /* ... */ };
apply_transform(compressor, data);  // OK
```

---

#### ReversibleDataTransformer

A transformer that supports both forward and reverse operations.

```cpp
template <typename T>
concept ReversibleDataTransformer =
    DataTransformer<T> && requires(T t, std::vector<uint8_t>& data) {
        { t.reverse_transform(data) } -> std::convertible_to<bool>;
    };
```

**Requirements**:
- All `DataTransformer` requirements
- `reverse_transform(data)`: Reverses the transformation

**Example**:
```cpp
template<ReversibleDataTransformer Transformer>
void process_bidirectional(Transformer& t, std::vector<uint8_t>& data) {
    t.transform(data);          // e.g., compress
    // ... transmit ...
    t.reverse_transform(data);  // e.g., decompress
}

class SymmetricEncryptor {
public:
    bool transform(std::vector<uint8_t>& data) { /* encrypt */ return true; }
    bool reverse_transform(std::vector<uint8_t>& data) { /* decrypt */ return true; }
};
```

---

### Utility Concepts

#### Duration

A type that represents a time duration (compatible with std::chrono).

```cpp
template <typename T>
concept Duration = requires {
    typename T::rep;
    typename T::period;
} && std::is_arithmetic_v<typename T::rep>;
```

**Satisfied by**:
- `std::chrono::seconds`
- `std::chrono::milliseconds`
- `std::chrono::microseconds`
- Any `std::chrono::duration` specialization

**Example**:
```cpp
template<Duration D>
void set_timeout(D duration) {
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    timeout_ms_ = ms.count();
}

set_timeout(std::chrono::seconds(30));       // OK
set_timeout(std::chrono::milliseconds(500)); // OK
set_timeout(5);                              // Compile error: int is not Duration
```

---

## Usage Patterns

### Constraining Template Parameters

```cpp
// Single concept constraint
template<DataReceiveHandler Handler>
void set_handler(Handler&& handler);

// Multiple concept constraints
template<typename T>
    requires NetworkClient<T> && std::copy_constructible<T>
void use_client(T client);

// Abbreviated function template syntax
void process(ByteBuffer auto const& buffer);
```

### Combining Concepts

```cpp
// Using conjunction
template<typename T>
    requires DataTransformer<T> && std::default_initializable<T>
auto create_transformer() -> T;

// Using disjunction
template<typename T>
    requires ByteBuffer<T> || std::ranges::contiguous_range<T>
void send(const T& data);
```

### Concept-Based Overloading

```cpp
// Different implementations based on concept satisfaction
template<ByteBuffer T>
void send(const T& buffer) {
    send_raw(buffer.data(), buffer.size());
}

template<typename T>
    requires std::ranges::range<T> && (!ByteBuffer<T>)
void send(const T& range) {
    std::vector<uint8_t> buffer(std::ranges::begin(range), std::ranges::end(range));
    send_raw(buffer.data(), buffer.size());
}
```

---

## Integration with common_system

When `BUILD_WITH_COMMON_SYSTEM` is enabled, additional concepts from common_system are available:

| Concept | Description |
|---------|-------------|
| `Resultable<T>` | Types that produce Result<T> |
| `Unwrappable<T>` | Types with unwrap() method |
| `Mappable<T>` | Types supporting map operations |

### Example with Result Integration

```cpp
#include <kcenon/network/concepts/concepts.h>

// When common_system is available
#ifdef BUILD_WITH_COMMON_SYSTEM

template<typename T>
    requires common_system::concepts::Resultable<T>
void handle_result(T&& result) {
    if (result.is_ok()) {
        // Process success...
    } else {
        // Handle error...
    }
}

#endif
```

---

## Best Practices

### 1. Prefer Concepts Over SFINAE

```cpp
// Good: Clear and readable
template<DataReceiveHandler Handler>
void set_handler(Handler&& h);

// Avoid: Hard to read
template<typename F, typename = std::enable_if_t<...>>
void set_handler(F&& h);
```

### 2. Use Descriptive Concept Names

```cpp
// Good: Describes the requirement
template<NetworkClient Client>
void use_client(Client& c);

// Avoid: Too generic
template<typename T>
void use_client(T& c);
```

### 3. Document Concept Requirements

```cpp
/**
 * @brief Process network data with a transformer
 * @tparam T Must satisfy ReversibleDataTransformer concept
 * @param transformer The data transformer to apply
 * @param data Data to transform (modified in place)
 */
template<ReversibleDataTransformer T>
void process(T& transformer, std::vector<uint8_t>& data);
```

### 4. Provide Clear Error Messages

Use `static_assert` for additional context:

```cpp
template<typename Handler>
void set_handler(Handler&& h) {
    static_assert(DataReceiveHandler<Handler>,
        "Handler must be callable with const std::vector<uint8_t>&");
    // ...
}
```

---

## Examples

### Complete Example: Type-Safe Callback Registration

```cpp
#include <kcenon/network/concepts/concepts.h>
#include <functional>
#include <iostream>
#include <vector>

using namespace network_system::concepts;

class TypeSafeClient {
public:
    // Constrained callback setters
    template<DataReceiveHandler Handler>
    void on_data(Handler&& handler) {
        data_handler_ = std::forward<Handler>(handler);
    }

    template<ErrorHandler Handler>
    void on_error(Handler&& handler) {
        error_handler_ = std::forward<Handler>(handler);
    }

    template<ConnectionHandler Handler>
    void on_connect(Handler&& handler) {
        connect_handler_ = std::forward<Handler>(handler);
    }

    template<RetryCallback Handler>
    void on_retry(Handler&& handler) {
        retry_handler_ = std::forward<Handler>(handler);
    }

private:
    std::function<void(const std::vector<uint8_t>&)> data_handler_;
    std::function<void(std::error_code)> error_handler_;
    std::function<void()> connect_handler_;
    std::function<void(std::size_t)> retry_handler_;
};

int main() {
    TypeSafeClient client;

    // All these compile successfully
    client.on_data([](const std::vector<uint8_t>& data) {
        std::cout << "Received: " << data.size() << " bytes\n";
    });

    client.on_error([](std::error_code ec) {
        std::cerr << "Error: " << ec.message() << "\n";
    });

    client.on_connect([]() {
        std::cout << "Connected!\n";
    });

    client.on_retry([](std::size_t attempt) {
        std::cout << "Retry #" << attempt << "\n";
    });

    // These would NOT compile (good!)
    // client.on_data([](int x) {});           // Wrong parameter type
    // client.on_error([](std::string s) {});  // Wrong parameter type
    // client.on_connect([](int x) {});        // Handler shouldn't take parameters

    return 0;
}
```

### Complete Example: Generic Network Client Wrapper

```cpp
#include <kcenon/network/concepts/concepts.h>
#include <memory>
#include <string>
#include <vector>

using namespace network_system::concepts;

// Generic wrapper that works with any NetworkClient
template<NetworkClient ClientType>
class ClientWrapper {
public:
    explicit ClientWrapper(std::shared_ptr<ClientType> client)
        : client_(std::move(client)) {}

    bool send_message(const std::string& message) {
        if (!client_->is_connected()) {
            return false;
        }
        std::vector<uint8_t> data(message.begin(), message.end());
        client_->send_packet(std::move(data));
        return true;
    }

    void shutdown() {
        client_->stop_client();
    }

    bool is_ready() const {
        return client_->is_connected();
    }

private:
    std::shared_ptr<ClientType> client_;
};

// Usage with messaging_client
#include <kcenon/network/core/messaging_client.h>

int main() {
    auto client = std::make_shared<network_system::core::messaging_client>("MyClient");

    // This compiles because messaging_client satisfies NetworkClient
    ClientWrapper wrapper(client);

    if (wrapper.is_ready()) {
        wrapper.send_message("Hello, World!");
    }

    wrapper.shutdown();
    return 0;
}
```

---

## See Also

- [API Reference](../API_REFERENCE.md) - Complete API documentation
- [Architecture](../ARCHITECTURE.md) - System design overview
- [Integration with common_system](../integration/with-common-system.md) - Common system integration
- [C++ Reference: Concepts](https://en.cppreference.com/w/cpp/language/constraints)

---

**Last Updated**: 2025-12-10
**Maintained by**: kcenon@naver.com
