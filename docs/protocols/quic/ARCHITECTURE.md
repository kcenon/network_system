# QUIC Implementation Architecture

## Layer Structure

The QUIC implementation follows a layered architecture consistent with the existing network_system design. The public API classes use the composition-based pattern with interfaces and utility classes for lifecycle management.

```
┌─────────────────────────────────────────────────────────┐
│  Public API (Composition Pattern)                       │
│  messaging_quic_client : implements i_quic_client       │
│  messaging_quic_server : implements i_quic_server       │
├─────────────────────────────────────────────────────────┤
│  Session Layer                                          │
│  (quic_session)                                         │
├─────────────────────────────────────────────────────────┤
│  Protocol Implementation                                │
│  protocols/quic/                                        │
│  ├─ connection.cpp   (connection state machine)         │
│  ├─ stream.cpp       (stream multiplexing)              │
│  ├─ stream_manager.cpp (stream lifecycle)               │
│  ├─ packet.cpp       (packet encoding/decoding)         │
│  ├─ frame.cpp        (frame parsing/building)           │
│  ├─ crypto.cpp       (TLS 1.3 + QUIC encryption)        │
│  ├─ keys.cpp         (key derivation)                   │
│  ├─ varint.cpp       (variable-length integers)         │
│  ├─ flow_control.cpp (flow control management)          │
│  ├─ loss_detector.cpp (loss detection)                  │
│  ├─ congestion_controller.cpp (congestion control)      │
│  └─ rtt_estimator.cpp (RTT estimation)                  │
├─────────────────────────────────────────────────────────┤
│  Internal Socket Layer                                  │
│  (quic_socket)                                          │
├─────────────────────────────────────────────────────────┤
│  ASIO UDP Socket                                        │
└─────────────────────────────────────────────────────────┘
```

## Key Components

### 1. Public API Layer

Both `messaging_quic_client` and `messaging_quic_server` use the composition-based pattern with interfaces and shared utility classes:

#### Composition Components

```cpp
// Client uses composition with lifecycle_manager and callback_manager:
class messaging_quic_client : public std::enable_shared_from_this<messaging_quic_client> {
public:
    // Lifecycle management (via lifecycle_manager)
    auto start_client(host, port) -> VoidResult;
    auto stop_client() -> VoidResult;
    auto wait_for_stop() -> void;
    auto is_running() const -> bool;
    auto is_connected() const -> bool;

    // Thread-safe callback management (via callback_manager)
    auto set_receive_callback(callback) -> void;
    auto set_connected_callback(callback) -> void;
    auto set_disconnected_callback(callback) -> void;
    auto set_error_callback(callback) -> void;

private:
    lifecycle_manager lifecycle_;      // Handles start/stop state
    quic_client_callbacks callbacks_;  // Type-safe callback storage
};

// Server uses similar composition:
class messaging_quic_server : public std::enable_shared_from_this<messaging_quic_server> {
public:
    // Lifecycle management
    auto start_server(port) -> VoidResult;
    auto stop_server() -> VoidResult;
    auto wait_for_stop() -> void;
    auto is_running() const -> bool;

    // Thread-safe callback management
    auto set_connection_callback(callback) -> void;
    auto set_disconnection_callback(callback) -> void;
    auto set_receive_callback(callback) -> void;
    auto set_error_callback(callback) -> void;

private:
    lifecycle_manager lifecycle_;
    quic_server_callbacks callbacks_;
};
```

#### messaging_quic_client

The client provides a high-level API for QUIC connections:

```cpp
class messaging_quic_client {
public:
    // Connection management
    auto start_client(host, port, config) -> VoidResult;
    auto stop_client() -> VoidResult;
    auto is_connected() const -> bool;

    // Data transfer
    auto send_packet(data) -> VoidResult;
    auto create_stream() -> Result<uint64_t>;
    auto send_on_stream(stream_id, data, fin) -> VoidResult;

    // Callbacks
    auto set_receive_callback(callback) -> void;
    auto set_connected_callback(callback) -> void;
    // ...
};
```

#### messaging_quic_server

The server manages multiple client connections:

```cpp
class messaging_quic_server {
public:
    // Server lifecycle
    auto start_server(port, config) -> VoidResult;
    auto stop_server() -> VoidResult;
    auto is_running() const -> bool;

    // Session management
    auto sessions() const -> vector<shared_ptr<quic_session>>;
    auto get_session(session_id) -> shared_ptr<quic_session>;
    auto disconnect_session(session_id) -> VoidResult;

    // Broadcasting
    auto broadcast(data) -> VoidResult;
    auto multicast(session_ids, data) -> VoidResult;
};
```

### 2. Session Layer

`quic_session` represents a single QUIC connection and wraps the underlying connection state:

- Connection state machine management
- Stream multiplexing coordination
- Cryptographic key management
- Flow control enforcement

### 3. Protocol Implementation

#### Connection Management (`connection.cpp`)

Implements the QUIC connection state machine (RFC 9000 Section 5):

```
States: IDLE -> HANDSHAKE -> ESTABLISHED -> CLOSING -> DRAINING -> CLOSED
```

Key responsibilities:
- State transitions
- Handshake coordination
- Idle timeout handling
- Connection closure

#### Stream Management (`stream.cpp`, `stream_manager.cpp`)

QUIC streams provide multiplexed data channels:

- **Bidirectional streams**: Two-way communication
- **Unidirectional streams**: One-way data flow
- **Stream IDs**: Client-initiated (0, 4, 8, ...) vs Server-initiated (1, 5, 9, ...)

Stream types (RFC 9000 Section 2.1):
```
0x00: Client-Initiated Bidirectional
0x01: Server-Initiated Bidirectional
0x02: Client-Initiated Unidirectional
0x03: Server-Initiated Unidirectional
```

#### Packet Processing (`packet.cpp`)

QUIC packet types (RFC 9000 Section 17):

| Packet Type | Description |
|-------------|-------------|
| Initial | Connection establishment (long header) |
| Handshake | Cryptographic handshake (long header) |
| 0-RTT | Early data (long header) |
| 1-RTT | Application data (short header) |
| Retry | Server retry request |
| Version Negotiation | Version mismatch handling |

#### Frame Processing (`frame.cpp`)

QUIC frame types (RFC 9000 Section 12):

```cpp
enum class frame_type : uint64_t {
    PADDING = 0x00,
    PING = 0x01,
    ACK = 0x02,
    RESET_STREAM = 0x04,
    STOP_SENDING = 0x05,
    CRYPTO = 0x06,
    NEW_TOKEN = 0x07,
    STREAM = 0x08,  // 0x08-0x0f
    MAX_DATA = 0x10,
    MAX_STREAM_DATA = 0x11,
    MAX_STREAMS = 0x12,
    DATA_BLOCKED = 0x14,
    STREAM_DATA_BLOCKED = 0x15,
    STREAMS_BLOCKED = 0x16,
    NEW_CONNECTION_ID = 0x18,
    RETIRE_CONNECTION_ID = 0x19,
    PATH_CHALLENGE = 0x1a,
    PATH_RESPONSE = 0x1b,
    CONNECTION_CLOSE = 0x1c,
    HANDSHAKE_DONE = 0x1e,
};
```

#### Cryptography (`crypto.cpp`, `keys.cpp`)

TLS 1.3 integration (RFC 9001):

1. **Initial Secrets**: Derived from connection ID
2. **Handshake Secrets**: TLS 1.3 key exchange
3. **Application Secrets**: Post-handshake keys
4. **Key Updates**: Periodic key rotation

Key derivation uses HKDF with SHA-256.

#### Flow Control (`flow_control.cpp`)

QUIC implements two levels of flow control:

1. **Connection-level**: Total data across all streams
2. **Stream-level**: Per-stream data limits

#### Loss Detection (`loss_detector.cpp`)

Implements RFC 9002 loss detection:

- **Packet number spaces**: Initial, Handshake, 1-RTT
- **ACK-based detection**: Missing packets in ACK ranges
- **Time-based detection**: Probe timeout (PTO)
- **RACK-TLP**: Modern loss recovery algorithm

#### Congestion Control (`congestion_controller.cpp`)

NewReno-based congestion control (RFC 9002):

- **Slow Start**: Exponential window growth
- **Congestion Avoidance**: Linear window growth
- **Fast Recovery**: Rapid recovery from loss

#### RTT Estimation (`rtt_estimator.cpp`)

Maintains RTT statistics:

```cpp
struct rtt_state {
    duration smoothed_rtt;  // Exponential weighted average
    duration rtt_var;       // RTT variance
    duration min_rtt;       // Minimum observed RTT
    duration latest_rtt;    // Most recent RTT sample
};
```

### 4. Internal Socket Layer

`quic_socket` wraps the UDP socket and provides:

- Async send/receive operations
- Packet coalescing
- Connection ID routing
- Retry token generation

## Data Flow

### Outgoing Data

```
Application
    │
    ▼
messaging_quic_client::send_packet()
    │
    ▼
stream_manager::send_on_stream()
    │
    ▼
connection::prepare_packets()
    │  ├─ frame::build_stream_frame()
    │  └─ crypto::encrypt()
    ▼
quic_socket::send()
    │
    ▼
UDP Socket
```

### Incoming Data

```
UDP Socket
    │
    ▼
quic_socket::receive()
    │
    ▼
packet::parse_header()
    │
    ▼
crypto::decrypt()
    │
    ▼
frame::parse_frames()
    │
    ▼
connection::process_frames()
    │  ├─ stream_manager (STREAM frames)
    │  ├─ flow_control (MAX_DATA, etc.)
    │  └─ loss_detector (ACK frames)
    ▼
Application callback
```

## Thread Safety

- **Public API**: All methods are thread-safe
- **Socket access**: Protected by mutex
- **Session map**: Protected by shared_mutex for concurrent reads
- **Atomic flags**: Used for state management
- **Callbacks**: Invoked on I/O threads; user implementations must be thread-safe

## Memory Management

- **Smart pointers**: `shared_ptr` for sessions, `unique_ptr` for internal components
- **Move semantics**: Data buffers moved to avoid copies
- **Buffer pools**: Pre-allocated packet buffers for efficiency

## Error Handling

All fallible operations return `Result<T>` or `VoidResult`:

```cpp
auto result = client->start_client(host, port);
if (result.is_err()) {
    std::cerr << "Error: " << result.error().message << "\n";
    std::cerr << "Code: " << result.error().code << "\n";
}
```

## Configuration

See [CONFIGURATION.md](CONFIGURATION.md) for detailed configuration options.
