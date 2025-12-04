# QUIC API Reference

## Namespace

```cpp
namespace network_system::core
```

---

## messaging_quic_client

A QUIC client that provides reliable, multiplexed communication.

### Constructor

```cpp
explicit messaging_quic_client(std::string_view client_id);
```

**Parameters:**
- `client_id`: A string identifier for logging/debugging

**Example:**
```cpp
auto client = std::make_shared<messaging_quic_client>("my_client");
```

---

### Connection Management

#### start_client

```cpp
[[nodiscard]] auto start_client(std::string_view host,
                                unsigned short port) -> VoidResult;

[[nodiscard]] auto start_client(std::string_view host,
                                unsigned short port,
                                const quic_client_config& config) -> VoidResult;
```

Starts the client and connects to the QUIC server.

**Parameters:**
- `host`: Server hostname or IP address
- `port`: Server port number
- `config`: Optional QUIC client configuration

**Returns:** `VoidResult` indicating success or error

**Errors:**
- `already_exists`: Client is already running
- `invalid_argument`: Host is empty
- `internal_error`: Other failures

**Example:**
```cpp
quic_client_config config;
config.verify_server = false;
config.max_idle_timeout_ms = 60000;

auto result = client->start_client("example.com", 443, config);
if (result.is_err()) {
    std::cerr << "Failed: " << result.error().message << "\n";
}
```

#### stop_client

```cpp
[[nodiscard]] auto stop_client() -> VoidResult;
```

Stops the client and closes the connection.

**Returns:** `VoidResult` indicating success or error

#### wait_for_stop

```cpp
auto wait_for_stop() -> void;
```

Blocks until `stop_client()` is complete.

#### is_connected

```cpp
[[nodiscard]] auto is_connected() const noexcept -> bool;
```

Check if connected to the server.

**Returns:** `true` if connected

#### is_handshake_complete

```cpp
[[nodiscard]] auto is_handshake_complete() const noexcept -> bool;
```

Check if TLS handshake is complete.

**Returns:** `true` if handshake is done

---

### Data Transfer

#### send_packet (binary)

```cpp
[[nodiscard]] auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;
```

Send binary data on the default stream.

**Parameters:**
- `data`: Data to send (moved for efficiency)

**Returns:** `VoidResult` indicating success or error

**Errors:**
- `connection_closed`: Not connected
- `invalid_argument`: Data is empty
- `send_failed`: Other failures

#### send_packet (string)

```cpp
[[nodiscard]] auto send_packet(std::string_view data) -> VoidResult;
```

Send string data on the default stream.

**Parameters:**
- `data`: String to send

**Returns:** `VoidResult` indicating success or error

---

### Multi-Stream Support

#### create_stream

```cpp
[[nodiscard]] auto create_stream() -> Result<uint64_t>;
```

Create a new bidirectional stream.

**Returns:** Stream ID or error

**Example:**
```cpp
auto stream_result = client->create_stream();
if (stream_result.is_ok()) {
    uint64_t stream_id = stream_result.value();
    client->send_on_stream(stream_id, {'d', 'a', 't', 'a'});
}
```

#### create_unidirectional_stream

```cpp
[[nodiscard]] auto create_unidirectional_stream() -> Result<uint64_t>;
```

Create a new unidirectional stream.

**Returns:** Stream ID or error

#### send_on_stream

```cpp
[[nodiscard]] auto send_on_stream(uint64_t stream_id,
                                  std::vector<uint8_t>&& data,
                                  bool fin = false) -> VoidResult;
```

Send data on a specific stream.

**Parameters:**
- `stream_id`: Target stream ID
- `data`: Data to send (moved for efficiency)
- `fin`: True if this is the final data on the stream

**Returns:** `VoidResult` indicating success or error

#### close_stream

```cpp
[[nodiscard]] auto close_stream(uint64_t stream_id) -> VoidResult;
```

Close a stream.

**Parameters:**
- `stream_id`: Stream to close

**Returns:** `VoidResult` indicating success or error

---

### Callbacks

#### set_receive_callback

```cpp
auto set_receive_callback(
    std::function<void(const std::vector<uint8_t>&)> callback) -> void;
```

Set callback for received data on the default stream.

**Example:**
```cpp
client->set_receive_callback([](const std::vector<uint8_t>& data) {
    std::cout << "Received: " << data.size() << " bytes\n";
});
```

#### set_stream_receive_callback

```cpp
auto set_stream_receive_callback(
    std::function<void(uint64_t stream_id,
                       const std::vector<uint8_t>& data,
                       bool fin)> callback) -> void;
```

Set callback for stream data reception (all streams).

**Example:**
```cpp
client->set_stream_receive_callback(
    [](uint64_t stream_id, const auto& data, bool fin) {
        std::cout << "Stream " << stream_id << ": "
                  << data.size() << " bytes (fin=" << fin << ")\n";
    });
```

#### set_connected_callback

```cpp
auto set_connected_callback(std::function<void()> callback) -> void;
```

Set callback when connection is established.

#### set_disconnected_callback

```cpp
auto set_disconnected_callback(std::function<void()> callback) -> void;
```

Set callback when disconnected.

#### set_error_callback

```cpp
auto set_error_callback(std::function<void(std::error_code)> callback) -> void;
```

Set callback for errors.

---

### Configuration

#### set_alpn_protocols

```cpp
auto set_alpn_protocols(const std::vector<std::string>& protocols) -> void;
```

Set ALPN protocols for negotiation.

**Example:**
```cpp
client->set_alpn_protocols({"h3", "h3-29"});
```

#### alpn_protocol

```cpp
[[nodiscard]] auto alpn_protocol() const -> std::optional<std::string>;
```

Get the negotiated ALPN protocol.

**Returns:** Protocol string if negotiated, empty optional otherwise

#### stats

```cpp
[[nodiscard]] auto stats() const -> quic_connection_stats;
```

Get connection statistics.

---

## messaging_quic_server

A QUIC server that manages incoming client connections.

### Constructor

```cpp
explicit messaging_quic_server(std::string_view server_id);
```

**Parameters:**
- `server_id`: A string identifier for logging/debugging

---

### Server Lifecycle

#### start_server

```cpp
[[nodiscard]] auto start_server(unsigned short port) -> VoidResult;

[[nodiscard]] auto start_server(unsigned short port,
                                const quic_server_config& config) -> VoidResult;
```

Start the server on the specified port.

**Parameters:**
- `port`: UDP port to listen on
- `config`: Optional server configuration with TLS settings

**Returns:** `VoidResult` indicating success or error

**Errors:**
- `server_already_running`: Already running
- `bind_failed`: Port binding failed
- `internal_error`: Other failures

#### stop_server

```cpp
[[nodiscard]] auto stop_server() -> VoidResult;
```

Stop the server and close all connections.

#### wait_for_stop

```cpp
auto wait_for_stop() -> void;
```

Block until the server stops.

#### is_running

```cpp
[[nodiscard]] auto is_running() const noexcept -> bool;
```

Check if the server is running.

---

### Session Management

#### sessions

```cpp
[[nodiscard]] auto sessions() const
    -> std::vector<std::shared_ptr<session::quic_session>>;
```

Get all active sessions.

#### get_session

```cpp
[[nodiscard]] auto get_session(const std::string& session_id)
    -> std::shared_ptr<session::quic_session>;
```

Get a session by its ID.

**Returns:** Session pointer or nullptr if not found

#### session_count

```cpp
[[nodiscard]] auto session_count() const -> size_t;
```

Get the number of active sessions.

#### disconnect_session

```cpp
[[nodiscard]] auto disconnect_session(const std::string& session_id,
                                      uint64_t error_code = 0) -> VoidResult;
```

Disconnect a specific session.

**Parameters:**
- `session_id`: Session to disconnect
- `error_code`: Application error code (0 for no error)

#### disconnect_all

```cpp
auto disconnect_all(uint64_t error_code = 0) -> void;
```

Disconnect all active sessions.

---

### Broadcasting

#### broadcast

```cpp
[[nodiscard]] auto broadcast(std::vector<uint8_t>&& data) -> VoidResult;
```

Send data to all connected clients.

**Example:**
```cpp
std::vector<uint8_t> message = {'H', 'e', 'l', 'l', 'o'};
server->broadcast(std::move(message));
```

#### multicast

```cpp
[[nodiscard]] auto multicast(const std::vector<std::string>& session_ids,
                             std::vector<uint8_t>&& data) -> VoidResult;
```

Send data to specific sessions.

---

### Callbacks

#### set_connection_callback

```cpp
auto set_connection_callback(
    std::function<void(std::shared_ptr<session::quic_session>)> callback) -> void;
```

Set callback when a new client connects.

#### set_disconnection_callback

```cpp
auto set_disconnection_callback(
    std::function<void(std::shared_ptr<session::quic_session>)> callback) -> void;
```

Set callback when a client disconnects.

#### set_receive_callback

```cpp
auto set_receive_callback(
    std::function<void(std::shared_ptr<session::quic_session>,
                       const std::vector<uint8_t>&)> callback) -> void;
```

Set callback for received data from any session.

#### set_stream_receive_callback

```cpp
auto set_stream_receive_callback(
    std::function<void(std::shared_ptr<session::quic_session>,
                       uint64_t stream_id,
                       const std::vector<uint8_t>&,
                       bool fin)> callback) -> void;
```

Set callback for stream data from any session.

#### set_error_callback

```cpp
auto set_error_callback(std::function<void(std::error_code)> callback) -> void;
```

Set callback for server errors.

---

## Configuration Structures

### quic_client_config

```cpp
struct quic_client_config {
    std::optional<std::string> ca_cert_file;      // CA cert for server verification
    std::optional<std::string> client_cert_file;  // Client cert for mutual TLS
    std::optional<std::string> client_key_file;   // Client key for mutual TLS
    bool verify_server{true};                     // Verify server certificate
    std::vector<std::string> alpn_protocols;      // ALPN protocols
    uint64_t max_idle_timeout_ms{30000};          // Max idle timeout
    uint64_t initial_max_data{1048576};           // Initial max data (1 MB)
    uint64_t initial_max_stream_data{65536};      // Initial max stream data (64 KB)
    uint64_t initial_max_streams_bidi{100};       // Max bidirectional streams
    uint64_t initial_max_streams_uni{100};        // Max unidirectional streams
    bool enable_early_data{false};                // Enable 0-RTT
    std::optional<std::vector<uint8_t>> session_ticket;  // Session ticket for 0-RTT
};
```

### quic_server_config

```cpp
struct quic_server_config {
    std::string cert_file;                        // Server certificate (required)
    std::string key_file;                         // Server private key (required)
    std::optional<std::string> ca_cert_file;      // CA cert for client verification
    bool require_client_cert{false};              // Require client certificate
    std::vector<std::string> alpn_protocols;      // ALPN protocols
    uint64_t max_idle_timeout_ms{30000};          // Max idle timeout
    uint64_t initial_max_data{1048576};           // Initial max data
    uint64_t initial_max_stream_data{65536};      // Initial max stream data
    uint64_t initial_max_streams_bidi{100};       // Max bidirectional streams
    uint64_t initial_max_streams_uni{100};        // Max unidirectional streams
    size_t max_connections{10000};                // Max concurrent connections
    bool enable_retry{true};                      // Enable retry for DoS protection
    std::vector<uint8_t> retry_key;               // Key for retry token validation
};
```

### quic_connection_stats

```cpp
struct quic_connection_stats {
    uint64_t bytes_sent{0};                       // Total bytes sent
    uint64_t bytes_received{0};                   // Total bytes received
    uint64_t packets_sent{0};                     // Total packets sent
    uint64_t packets_received{0};                 // Total packets received
    uint64_t packets_lost{0};                     // Total packets lost
    std::chrono::microseconds smoothed_rtt{0};    // Smoothed RTT
    std::chrono::microseconds min_rtt{0};         // Minimum RTT
    size_t cwnd{0};                               // Congestion window size
};
```

---

## quic_session

Represents a QUIC session (connection) on the server side.

### Methods

#### session_id

```cpp
[[nodiscard]] auto session_id() const -> std::string;
```

Get the session identifier.

#### remote_endpoint

```cpp
[[nodiscard]] auto remote_endpoint() const -> asio::ip::udp::endpoint;
```

Get the remote endpoint (client address).

#### send

```cpp
[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult;
```

Send data to this client.

#### send_on_stream

```cpp
[[nodiscard]] auto send_on_stream(uint64_t stream_id,
                                  std::vector<uint8_t>&& data,
                                  bool fin = false) -> VoidResult;
```

Send data on a specific stream.

#### close

```cpp
auto close(uint64_t error_code = 0) -> void;
```

Close this session.

#### stats

```cpp
[[nodiscard]] auto stats() const -> quic_connection_stats;
```

Get session statistics.
