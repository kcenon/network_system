---
doc_id: "NET-API-002a"
doc_title: "API Reference - Facades"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "API"
---

# API Reference - Facades

> **SSOT**: This document is part of the API Reference. See also [API Reference - Protocols](API_REFERENCE_PROTOCOLS.md) and the [API Reference Index](API_REFERENCE.md).

> **Language:** **English** | [한국어](API_REFERENCE.kr.md)

Facade API documentation for the Network System library, covering TCP, UDP, WebSocket, HTTP, and QUIC facades.

## Table of Contents

- [Overview](#overview)
- [C++20 Concepts](#c20-concepts)
- [Core Classes](#core-classes)
- [Networking Components](#networking-components)
- [Utility Classes](#utility-classes)
- [Error Handling](#error-handling)
- [Configuration](#configuration)
- [Examples](#examples)
- [Thread Safety](#thread-safety)
- [Platform-Specific Notes](#platform-specific-notes)

## Overview

The Network System provides a modular, high-performance networking library with support for multiple protocols and platforms.

### Namespace Structure

```cpp
namespace network {
    namespace tcp { /* TCP implementation */ }
    namespace udp { /* UDP implementation */ }
    namespace http { /* HTTP implementation */ }
    namespace websocket { /* WebSocket implementation */ }
    namespace ssl { /* SSL/TLS support */ }
    namespace utils { /* Utility functions */ }
}
```

### Header Files

```cpp
#include <network/server.hpp>          // Server classes
#include <network/client.hpp>          // Client classes
#include <network/protocol.hpp>        // Protocol interfaces
#include <network/message.hpp>         // Message handling
#include <network/connection.hpp>      // Connection management
#include <network/error.hpp>           // Error handling
#include <network/config.hpp>          // Configuration
```

## C++20 Concepts

Network System provides 16 C++20 concepts for compile-time type validation. These concepts improve code quality through better error messages and self-documenting interfaces.

### Header Files

```cpp
#include <kcenon/network/concepts/concepts.h>       // Unified umbrella header
#include <kcenon/network/concepts/network_concepts.h>  // All concept definitions
```

### Namespace

All concepts are in the `network_system::concepts` namespace.

### Concept Reference

#### Data Buffer Concepts

| Concept | Signature | Description |
|---------|-----------|-------------|
| `ByteBuffer<T>` | `data()`, `size()` | Read-only buffer interface |
| `MutableByteBuffer<T>` | `data()`, `size()`, `resize(n)` | Resizable buffer extending ByteBuffer |

```cpp
template<ByteBuffer Buffer>
void send_data(const Buffer& buffer);

template<MutableByteBuffer Buffer>
void receive_data(Buffer& buffer, std::size_t size);
```

#### Callback Concepts

| Concept | Signature | Description |
|---------|-----------|-------------|
| `DataReceiveHandler<F>` | `F(const std::vector<uint8_t>&)` | Handler for received data |
| `ErrorHandler<F>` | `F(std::error_code)` | Error notification handler |
| `ConnectionHandler<F>` | `F()` | Connection state change handler |
| `SessionHandler<F, Session>` | `F(std::shared_ptr<Session>)` | Session event handler |
| `SessionDataHandler<F, Session>` | `F(std::shared_ptr<Session>, const std::vector<uint8_t>&)` | Session data handler |
| `SessionErrorHandler<F, Session>` | `F(std::shared_ptr<Session>, std::error_code)` | Session error handler |
| `DisconnectionHandler<F>` | `F(const std::string&)` | Disconnection event handler |
| `RetryCallback<F>` | `F(std::size_t)` | Reconnection attempt handler |

```cpp
template<DataReceiveHandler Handler>
void set_receive_handler(Handler&& handler);

template<ErrorHandler Handler>
void set_error_handler(Handler&& handler);
```

#### Network Component Concepts

| Concept | Requirements | Description |
|---------|--------------|-------------|
| `NetworkClient<T>` | `is_connected()`, `send_packet()`, `stop_client()` | Client interface |
| `NetworkServer<T>` | `start_server(port)`, `stop_server()` | Server interface |
| `NetworkSession<T>` | `get_session_id()`, `start_session()`, `stop_session()` | Session interface |

```cpp
template<NetworkClient Client>
void use_client(Client& client);

template<NetworkServer Server>
void manage_server(Server& server);
```

#### Pipeline Concepts

| Concept | Requirements | Description |
|---------|--------------|-------------|
| `DataTransformer<T>` | `transform(data)` -> `bool` | Data transformation |
| `ReversibleDataTransformer<T>` | `transform()`, `reverse_transform()` | Bidirectional transformation |

```cpp
template<DataTransformer T>
bool apply_transform(T& transformer, std::vector<uint8_t>& data);

template<ReversibleDataTransformer T>
void process_bidirectional(T& t, std::vector<uint8_t>& data);
```

#### Utility Concepts

| Concept | Requirements | Description |
|---------|--------------|-------------|
| `Duration<T>` | `T::rep`, `T::period` (arithmetic) | std::chrono::duration constraint |

```cpp
template<Duration D>
void set_timeout(D duration);
```

### Usage Example

```cpp
#include <kcenon/network/concepts/concepts.h>

using namespace network_system::concepts;

class MyClient {
public:
    template<DataReceiveHandler Handler>
    void on_data(Handler&& handler) {
        // Compile-time validated: handler accepts const std::vector<uint8_t>&
        data_handler_ = std::forward<Handler>(handler);
    }

    template<ErrorHandler Handler>
    void on_error(Handler&& handler) {
        // Compile-time validated: handler accepts std::error_code
        error_handler_ = std::forward<Handler>(handler);
    }

private:
    std::function<void(const std::vector<uint8_t>&)> data_handler_;
    std::function<void(std::error_code)> error_handler_;
};
```

See [advanced/CONCEPTS.md](advanced/CONCEPTS.md) for detailed documentation and more examples.

## Core Classes

### NetworkServer

Main server class for handling network connections.

```cpp
namespace network {

class NetworkServer {
public:
    /**
     * Constructs a new NetworkServer instance.
     * @param config Server configuration object
     */
    explicit NetworkServer(const ServerConfig& config);

    /**
     * Starts the server and begins listening for connections.
     * @return Result<void> indicating success or failure
     */
    Result<void> start();

    /**
     * Stops the server gracefully.
     * @param timeout Maximum time to wait for graceful shutdown
     * @return Result<void> indicating success or failure
     */
    Result<void> stop(std::chrono::seconds timeout = std::chrono::seconds(30));

    /**
     * Sets the message handler for incoming messages.
     * @param handler Function to handle incoming messages
     */
    void set_message_handler(MessageHandler handler);

    /**
     * Gets current server statistics.
     * @return ServerStats object with current metrics
     */
    ServerStats get_stats() const;

    /**
     * Broadcasts a message to all connected clients.
     * @param message Message to broadcast
     * @return Number of clients successfully sent to
     */
    size_t broadcast(const Message& message);

    /**
     * Sends a message to a specific client.
     * @param client_id Client identifier
     * @param message Message to send
     * @return Result<void> indicating success or failure
     */
    Result<void> send_to(ClientId client_id, const Message& message);

    /**
     * Disconnects a specific client.
     * @param client_id Client identifier
     * @param reason Disconnection reason
     */
    void disconnect_client(ClientId client_id, const std::string& reason = "");

    /**
     * Gets information about a connected client.
     * @param client_id Client identifier
     * @return Optional<ClientInfo> with client information if found
     */
    std::optional<ClientInfo> get_client_info(ClientId client_id) const;

    /**
     * Lists all connected clients.
     * @return Vector of client identifiers
     */
    std::vector<ClientId> get_connected_clients() const;
};

} // namespace network
```

### NetworkClient

Client class for connecting to network servers.

```cpp
namespace network {

class NetworkClient {
public:
    /**
     * Constructs a new NetworkClient instance.
     * @param config Client configuration object
     */
    explicit NetworkClient(const ClientConfig& config);

    /**
     * Connects to a server.
     * @param host Server hostname or IP address
     * @param port Server port number
     * @param timeout Connection timeout
     * @return Result<void> indicating success or failure
     */
    Result<void> connect(const std::string& host, uint16_t port,
                        std::chrono::seconds timeout = std::chrono::seconds(10));

    /**
     * Disconnects from the server.
     * @return Result<void> indicating success or failure
     */
    Result<void> disconnect();

    /**
     * Sends a message to the server.
     * @param message Message to send
     * @return Result<void> indicating success or failure
     */
    Result<void> send(const Message& message);

    /**
     * Receives a message from the server.
     * @param timeout Receive timeout (nullopt for blocking)
     * @return Result<Message> with received message or error
     */
    Result<Message> receive(std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * Sends a request and waits for response.
     * @param request Request message
     * @param timeout Response timeout
     * @return Result<Message> with response or error
     */
    Result<Message> request(const Message& request,
                           std::chrono::seconds timeout = std::chrono::seconds(30));

    /**
     * Checks if client is connected.
     * @return true if connected, false otherwise
     */
    bool is_connected() const;

    /**
     * Sets the message handler for incoming messages.
     * @param handler Function to handle incoming messages
     */
    void set_message_handler(MessageHandler handler);

    /**
     * Gets connection statistics.
     * @return ConnectionStats object with metrics
     */
    ConnectionStats get_stats() const;
};

} // namespace network
```


## Networking Components

### Connection

Represents a single network connection.

```cpp
namespace network {

class Connection {
public:
    using Id = uint64_t;

    /**
     * Gets the unique connection identifier.
     * @return Connection ID
     */
    Id get_id() const;

    /**
     * Gets the remote endpoint information.
     * @return Endpoint object with address and port
     */
    Endpoint get_remote_endpoint() const;

    /**
     * Gets the local endpoint information.
     * @return Endpoint object with address and port
     */
    Endpoint get_local_endpoint() const;

    /**
     * Checks if the connection is active.
     * @return true if active, false otherwise
     */
    bool is_active() const;

    /**
     * Sends data over the connection.
     * @param data Data buffer to send
     * @param size Size of data in bytes
     * @return Result<size_t> with bytes sent or error
     */
    Result<size_t> send(const uint8_t* data, size_t size);

    /**
     * Receives data from the connection.
     * @param buffer Buffer to receive into
     * @param size Maximum size to receive
     * @param timeout Receive timeout (optional)
     * @return Result<size_t> with bytes received or error
     */
    Result<size_t> receive(uint8_t* buffer, size_t size,
                          std::optional<std::chrono::milliseconds> timeout = std::nullopt);

    /**
     * Closes the connection.
     */
    void close();

    /**
     * Sets TCP no-delay option.
     * @param enable true to enable, false to disable
     * @return Result<void> indicating success or failure
     */
    Result<void> set_no_delay(bool enable);

    /**
     * Sets keep-alive option.
     * @param enable true to enable, false to disable
     * @param idle Idle time before sending keep-alive probes
     * @param interval Interval between keep-alive probes
     * @param count Number of probes before connection is dropped
     * @return Result<void> indicating success or failure
     */
    Result<void> set_keep_alive(bool enable,
                                std::chrono::seconds idle = std::chrono::seconds(60),
                                std::chrono::seconds interval = std::chrono::seconds(10),
                                int count = 6);

    /**
     * Gets connection statistics.
     * @return ConnectionStats object with metrics
     */
    ConnectionStats get_stats() const;
};

} // namespace network
```

### ConnectionPool

Manages a pool of reusable connections.

```cpp
namespace network {

class ConnectionPool {
public:
    /**
     * Configuration for connection pool.
     */
    struct Config {
        size_t min_connections = 5;
        size_t max_connections = 50;
        std::chrono::seconds idle_timeout = std::chrono::seconds(60);
        std::chrono::seconds connection_timeout = std::chrono::seconds(10);
        bool validate_on_checkout = true;
    };

    /**
     * Constructs a new connection pool.
     * @param config Pool configuration
     */
    explicit ConnectionPool(const Config& config);

    /**
     * Acquires a connection from the pool.
     * @param endpoint Target endpoint
     * @return Result<std::shared_ptr<Connection>> with connection or error
     */
    Result<std::shared_ptr<Connection>> acquire(const Endpoint& endpoint);

    /**
     * Releases a connection back to the pool.
     * @param connection Connection to release
     */
    void release(std::shared_ptr<Connection> connection);

    /**
     * Gets pool statistics.
     * @return PoolStats object with metrics
     */
    PoolStats get_stats() const;

    /**
     * Clears all idle connections.
     */
    void clear_idle();

    /**
     * Validates all connections in the pool.
     * @return Number of invalid connections removed
     */
    size_t validate_all();
};

} // namespace network
```


## Utility Classes

### Message

Message container for network communication.

```cpp
namespace network {

class Message {
public:
    /**
     * Message type enumeration.
     */
    enum class Type {
        REQUEST,
        RESPONSE,
        NOTIFICATION,
        ERROR
    };

    /**
     * Constructs an empty message.
     */
    Message();

    /**
     * Constructs a message with data.
     * @param data Message data
     */
    explicit Message(const std::string& data);

    /**
     * Constructs a message with binary data.
     * @param data Binary data
     * @param size Data size
     */
    Message(const uint8_t* data, size_t size);

    /**
     * Gets the message ID.
     * @return Message ID
     */
    uint64_t get_id() const;

    /**
     * Sets the message ID.
     * @param id Message ID
     */
    void set_id(uint64_t id);

    /**
     * Gets the message type.
     * @return Message type
     */
    Type get_type() const;

    /**
     * Sets the message type.
     * @param type Message type
     */
    void set_type(Type type);

    /**
     * Gets message data as string.
     * @return Message data
     */
    std::string get_data() const;

    /**
     * Gets message data as binary.
     * @return Pair of data pointer and size
     */
    std::pair<const uint8_t*, size_t> get_binary_data() const;

    /**
     * Sets a header value.
     * @param key Header key
     * @param value Header value
     */
    void set_header(const std::string& key, const std::string& value);

    /**
     * Gets a header value.
     * @param key Header key
     * @return Optional header value
     */
    std::optional<std::string> get_header(const std::string& key) const;

    /**
     * Serializes the message.
     * @return Serialized message bytes
     */
    std::vector<uint8_t> serialize() const;

    /**
     * Deserializes a message.
     * @param data Serialized data
     * @param size Data size
     * @return Result<Message> with deserialized message or error
     */
    static Result<Message> deserialize(const uint8_t* data, size_t size);
};

} // namespace network
```

### Buffer

Efficient buffer management for network I/O.

```cpp
namespace network {

class Buffer {
public:
    /**
     * Constructs a buffer with initial capacity.
     * @param capacity Initial capacity in bytes
     */
    explicit Buffer(size_t capacity = 4096);

    /**
     * Writes data to the buffer.
     * @param data Data to write
     * @param size Size of data
     * @return Number of bytes written
     */
    size_t write(const uint8_t* data, size_t size);

    /**
     * Reads data from the buffer.
     * @param data Buffer to read into
     * @param size Maximum size to read
     * @return Number of bytes read
     */
    size_t read(uint8_t* data, size_t size);

    /**
     * Peeks at data without consuming.
     * @param data Buffer to peek into
     * @param size Maximum size to peek
     * @return Number of bytes peeked
     */
    size_t peek(uint8_t* data, size_t size) const;

    /**
     * Discards bytes from the buffer.
     * @param size Number of bytes to discard
     */
    void discard(size_t size);

    /**
     * Gets the number of readable bytes.
     * @return Readable byte count
     */
    size_t readable_bytes() const;

    /**
     * Gets the number of writable bytes.
     * @return Writable byte count
     */
    size_t writable_bytes() const;

    /**
     * Clears the buffer.
     */
    void clear();

    /**
     * Reserves capacity.
     * @param capacity Minimum capacity to reserve
     */
    void reserve(size_t capacity);
};

} // namespace network
```


## Error Handling

### Error Types

```cpp
namespace network {

/**
 * Base exception class for network errors.
 */
class NetworkException : public std::runtime_error {
public:
    explicit NetworkException(const std::string& message);
    virtual ErrorCode get_error_code() const = 0;
};

/**
 * Connection-related errors.
 */
class ConnectionException : public NetworkException {
public:
    ConnectionException(const std::string& message, ErrorCode code);
    ErrorCode get_error_code() const override;
};

/**
 * Protocol-related errors.
 */
class ProtocolException : public NetworkException {
public:
    ProtocolException(const std::string& message, ErrorCode code);
    ErrorCode get_error_code() const override;
};

/**
 * Timeout errors.
 */
class TimeoutException : public NetworkException {
public:
    explicit TimeoutException(const std::string& message);
    ErrorCode get_error_code() const override;
};

} // namespace network
```

### Result Type

```cpp
namespace network {

/**
 * Result type for error handling without exceptions.
 */
template<typename T>
class Result {
public:
    /**
     * Creates a successful result.
     * @param value Success value
     * @return Result with value
     */
    static Result success(T value);

    /**
     * Creates an error result.
     * @param error Error object
     * @return Result with error
     */
    static Result error(Error error);

    /**
     * Checks if the result is successful.
     * @return true if successful, false otherwise
     */
    bool is_success() const;

    /**
     * Checks if the result is an error.
     * @return true if error, false otherwise
     */
    bool is_error() const;

    /**
     * Gets the value (throws if error).
     * @return Success value
     * @throws std::runtime_error if result is error
     */
    T& value();
    const T& value() const;

    /**
     * Gets the error (throws if success).
     * @return Error object
     * @throws std::runtime_error if result is success
     */
    Error& error();
    const Error& error() const;

    /**
     * Gets the value or default.
     * @param default_value Default value if error
     * @return Success value or default
     */
    T value_or(T default_value) const;

    /**
     * Maps the success value.
     * @param func Mapping function
     * @return Mapped result
     */
    template<typename U>
    Result<U> map(std::function<U(const T&)> func) const;

    /**
     * Maps the error.
     * @param func Error mapping function
     * @return Result with mapped error
     */
    Result<T> map_error(std::function<Error(const Error&)> func) const;
};

// Specialization for void
template<>
class Result<void> {
public:
    static Result success();
    static Result error(Error error);
    bool is_success() const;
    bool is_error() const;
    void value() const;  // Throws if error
    const Error& error() const;
};

} // namespace network
```

## Configuration

### Server Configuration

```cpp
namespace network {

struct ServerConfig {
    // Network settings
    std::string bind_address = "0.0.0.0";
    uint16_t port = 8080;
    int backlog = 128;

    // Connection settings
    size_t max_connections = 1000;
    std::chrono::seconds connection_timeout{30};
    std::chrono::seconds keep_alive_timeout{60};

    // Thread pool settings
    size_t worker_threads = std::thread::hardware_concurrency();
    size_t io_threads = 2;

    // Buffer settings
    size_t receive_buffer_size = 65536;
    size_t send_buffer_size = 65536;

    // SSL settings
    bool ssl_enabled = false;
    std::string cert_file;
    std::string key_file;
    std::string ca_file;

    // Protocol settings
    std::vector<std::string> supported_protocols;

    /**
     * Loads configuration from file.
     * @param file_path Configuration file path
     * @return Result<ServerConfig> with config or error
     */
    static Result<ServerConfig> from_file(const std::string& file_path);

    /**
     * Loads configuration from JSON.
     * @param json JSON configuration
     * @return Result<ServerConfig> with config or error
     */
    static Result<ServerConfig> from_json(const nlohmann::json& json);

    /**
     * Validates the configuration.
     * @return Result<void> indicating valid or error
     */
    Result<void> validate() const;
};

} // namespace network
```

### Client Configuration

```cpp
namespace network {

struct ClientConfig {
    // Connection settings
    std::chrono::seconds connect_timeout{10};
    std::chrono::seconds read_timeout{30};
    std::chrono::seconds write_timeout{30};

    // Retry settings
    int max_retries = 3;
    std::chrono::milliseconds retry_delay{1000};
    bool exponential_backoff = true;

    // Buffer settings
    size_t receive_buffer_size = 65536;
    size_t send_buffer_size = 65536;

    // SSL settings
    bool ssl_enabled = false;
    bool verify_peer = true;
    std::string ca_file;
    std::string cert_file;
    std::string key_file;

    // Keep-alive settings
    bool keep_alive_enabled = true;
    std::chrono::seconds keep_alive_interval{30};

    /**
     * Creates default configuration.
     * @return Default client configuration
     */
    static ClientConfig default_config();

    /**
     * Validates the configuration.
     * @return Result<void> indicating valid or error
     */
    Result<void> validate() const;
};

} // namespace network
```

## Examples

### Basic TCP Server

```cpp
#include <network/server.hpp>

int main() {
    // Create server configuration
    network::ServerConfig config;
    config.port = 8080;
    config.max_connections = 100;

    // Create and configure server
    network::NetworkServer server(config);

    // Set message handler
    server.set_message_handler([](const network::Message& msg,
                                  network::Connection& conn) {
        std::cout << "Received: " << msg.get_data() << std::endl;

        // Echo the message back
        network::Message response("Echo: " + msg.get_data());
        conn.send(response.serialize().data(), response.serialize().size());
    });

    // Start server
    auto result = server.start();
    if (!result.is_success()) {
        std::cerr << "Failed to start server: "
                  << result.error().message << std::endl;
        return 1;
    }

    std::cout << "Server listening on port 8080" << std::endl;

    // Run until interrupted
    std::signal(SIGINT, [](int) {
        std::cout << "Shutting down..." << std::endl;
    });

    // Wait for shutdown
    server.wait();

    return 0;
}
```

### Basic TCP Client

```cpp
#include <network/client.hpp>

int main() {
    // Create client configuration
    network::ClientConfig config;
    config.connect_timeout = std::chrono::seconds(5);

    // Create client
    network::NetworkClient client(config);

    // Connect to server
    auto result = client.connect("localhost", 8080);
    if (!result.is_success()) {
        std::cerr << "Failed to connect: "
                  << result.error().message << std::endl;
        return 1;
    }

    // Send message
    network::Message message("Hello, Server!");
    result = client.send(message);
    if (!result.is_success()) {
        std::cerr << "Failed to send: "
                  << result.error().message << std::endl;
        return 1;
    }

    // Receive response
    auto response = client.receive(std::chrono::seconds(5));
    if (response.is_success()) {
        std::cout << "Received: " << response.value().get_data() << std::endl;
    }

    // Disconnect
    client.disconnect();

    return 0;
}
```

### HTTP Server Example

```cpp
#include <network/http/server.hpp>

int main() {
    network::http::HttpConfig config;
    config.port = 8080;

    network::http::HttpServer server(config);

    // Add routes
    server.route("GET", "/", [](const auto& req) {
        return network::http::Response()
            .status(200)
            .header("Content-Type", "text/html")
            .body("<h1>Hello, World!</h1>");
    });

    server.route("GET", "/api/users", [](const auto& req) {
        nlohmann::json users = {
            {"users", {
                {{"id", 1}, {"name", "Alice"}},
                {{"id", 2}, {"name", "Bob"}}
            }}
        };
        return network::http::Response()
            .status(200)
            .json(users);
    });

    server.route("POST", "/api/users", [](const auto& req) {
        auto body = nlohmann::json::parse(req.get_body());
        // Process user creation
        return network::http::Response()
            .status(201)
            .json({{"id", 3}, {"name", body["name"]}});
    });

    // Add middleware
    server.use([](const auto& req, auto& res, auto next) {
        std::cout << req.get_method() << " " << req.get_uri() << std::endl;
        next();
    });

    // Start server
    server.start();
    std::cout << "HTTP server listening on port 8080" << std::endl;

    // Wait for shutdown
    std::signal(SIGINT, [](int) { /* shutdown */ });
    server.wait();

    return 0;
}
```

### WebSocket Server Example

```cpp
#include <network/websocket/server.hpp>

int main() {
    network::websocket::WebSocketConfig config;
    config.port = 8080;

    network::websocket::WebSocketServer server(config);

    server.on_connection([&](auto& conn) {
        std::cout << "Client connected: " << conn.get_info().id << std::endl;

        conn.on_message([&](const std::string& msg) {
            std::cout << "Received: " << msg << std::endl;

            // Echo to all clients
            server.broadcast("Echo: " + msg);
        });

        conn.on_close([](int code, const std::string& reason) {
            std::cout << "Client disconnected: " << code
                      << " - " << reason << std::endl;
        });

        // Send welcome message
        conn.send("Welcome to WebSocket server!");
    });

    server.start();
    std::cout << "WebSocket server listening on port 8080" << std::endl;

    // Wait for shutdown
    std::signal(SIGINT, [](int) { /* shutdown */ });
    server.wait();

    return 0;
}
```

### SSL/TLS Example

```cpp
#include <network/server.hpp>
#include <network/ssl/context.hpp>

int main() {
    // Configure SSL
    network::ServerConfig config;
    config.port = 8443;
    config.ssl_enabled = true;
    config.cert_file = "/path/to/server.crt";
    config.key_file = "/path/to/server.key";
    config.ca_file = "/path/to/ca.crt";

    // Create SSL server
    network::NetworkServer server(config);

    server.set_message_handler([](const auto& msg, auto& conn) {
        // Handle secure message
        std::cout << "Secure message: " << msg.get_data() << std::endl;
    });

    auto result = server.start();
    if (!result.is_success()) {
        std::cerr << "Failed to start SSL server: "
                  << result.error().message << std::endl;
        return 1;
    }

    std::cout << "SSL server listening on port 8443" << std::endl;
    server.wait();

    return 0;
}
```

### Async Client Example

```cpp
#include <network/async/client.hpp>

int main() {
    network::async::AsyncClient client;

    // Connect asynchronously
    client.connect_async("localhost", 8080)
        .then([&](auto result) {
            if (!result.is_success()) {
                std::cerr << "Connection failed" << std::endl;
                return;
            }

            // Send message asynchronously
            network::Message msg("Hello, Async!");
            return client.send_async(msg);
        })
        .then([&](auto result) {
            if (!result.is_success()) {
                std::cerr << "Send failed" << std::endl;
                return;
            }

            // Receive response asynchronously
            return client.receive_async();
        })
        .then([](auto result) {
            if (result.is_success()) {
                std::cout << "Response: "
                          << result.value().get_data() << std::endl;
            }
        });

    // Run event loop
    client.run();

    return 0;
}
```

## Thread Safety

All public methods in the Network System library are thread-safe unless explicitly documented otherwise. The following guarantees are provided:

1. **Server Classes**: Multiple threads can safely call methods on the same server instance
2. **Client Classes**: Send and receive operations are thread-safe
3. **Connection Classes**: Individual connections are thread-safe for concurrent operations
4. **Message Classes**: Message objects are immutable after construction
5. **Buffer Classes**: Not thread-safe; use separate instances per thread or add external synchronization



### Zero-Copy Operations

The library supports zero-copy operations where possible:

```cpp
// Zero-copy send using memory-mapped file
network::ZeroCopyFile file("/path/to/large/file");
connection.send_file(file);

// Zero-copy receive into pre-allocated buffer
network::Buffer buffer(1024 * 1024);  // 1MB buffer
connection.receive_zero_copy(buffer);
```

### Memory Pooling

Use memory pools for frequently allocated objects:

```cpp
// Configure memory pools
network::MemoryPoolConfig pool_config;
pool_config.message_pool_size = 1000;
pool_config.buffer_pool_size = 100;
pool_config.connection_pool_size = 50;

network::NetworkServer server(config, pool_config);
```

### Batch Operations

Batch multiple operations for better performance:

```cpp
// Batch send multiple messages
std::vector<network::Message> messages;
// ... populate messages
connection.send_batch(messages);

// Batch receive
auto messages = connection.receive_batch(max_messages, timeout);
```

## Platform-Specific Notes

### Linux

- Uses epoll for I/O multiplexing
- Supports SO_REUSEPORT for load balancing
- Kernel bypass with io_uring (optional)

### Windows

- Uses IOCP for I/O multiplexing
- Requires Windows 10 or Windows Server 2016+
- Visual Studio 2019 or later recommended

### macOS

- Uses kqueue for I/O multiplexing
- Requires macOS 10.14 or later
- Xcode 11 or later recommended

