# WebSocket Implementation Plan

**Date**: 2025-10-24
**Author**: Network System Development Team
**Version**: 1.0

---

## 📋 Executive Summary

This document outlines a comprehensive plan to add WebSocket protocol support to the `network_system` library, following the same architectural patterns as the existing TCP and UDP implementations.

### Goals
- Add WebSocket protocol as a first-class transport layer alongside TCP and UDP
- Implement RFC 6455 WebSocket specification
- Maintain consistency with existing TCP/UDP patterns
- Provide both client and server implementations
- Support text and binary message frames
- Enable seamless integration with existing network_system infrastructure

### Success Criteria
- WebSocket client can connect to standard WebSocket servers
- WebSocket server can accept connections from standard WebSocket clients
- Full RFC 6455 compliance for core features
- Zero-copy message handling with move semantics (consistent with TCP/UDP)
- Thread-safe API consistent with TCP/UDP
- VoidResult/Result error handling pattern (consistent with existing API)
- Integration with existing infrastructure:
  - Pipeline for compression/encryption
  - Logger integration via NETWORK_LOG_* macros
  - ws_session_manager for connection limits and backpressure (mirrors session_manager pattern)
  - Monitoring hooks compatible with TCP/UDP metrics
- Comprehensive test coverage (>80%)
- Sample applications demonstrating usage
- Documentation updates (network_system.h, README.md)

---

## 🏗️ Architecture Overview

### Layer Structure

```
┌─────────────────────────────────────────────┐
│     Application Layer (User Code)          │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│   Core Layer - messaging_ws_client/server   │
│   - High-level WebSocket API                │
│   - Connection management                   │
│   - Message send/receive                    │
│   - Event callbacks                         │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│   Protocol Layer - websocket_protocol       │
│   - HTTP handshake                          │
│   - Frame encoding/decoding                 │
│   - Message fragmentation                   │
│   - Control frames (ping/pong/close)        │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│   Internal Layer - websocket_socket         │
│   - ASIO TCP socket wrapper                 │
│   - Async read/write operations             │
│   - Buffer management                       │
└─────────────────────────────────────────────┘
                    ↓
┌─────────────────────────────────────────────┐
│        ASIO (TCP transport)                 │
└─────────────────────────────────────────────┘
```

### API Compatibility Strategy

**Problem**: WebSocket API (`send_text`, `send_binary`) differs from TCP/UDP API (`send_packet`).

**Solution**: Provide adapter layer in `compatibility.h` and messaging_bridge for unified access.

#### Approach 1: Explicit API (Direct Usage)
Users directly call WebSocket-specific methods when they need text/binary distinction:
```cpp
// WebSocket-specific usage
auto ws_client = std::make_shared<messaging_ws_client>("ws_client");
ws_client->send_text("Hello", handler);
ws_client->send_binary({0x01, 0x02}, handler);
```

#### Approach 2: Adapter Pattern (Unified API via compatibility.h)
Provide adapter functions for protocol-agnostic code:
```cpp
// In compatibility.h (extends existing network_system::compat namespace)
namespace network_system::compat {

// Adapter: Convert binary send to WebSocket binary frame
inline auto send_message(
    std::shared_ptr<messaging_ws_client> client,
    std::vector<uint8_t>&& data,
    std::function<void(std::error_code, std::size_t)> handler = nullptr
) -> VoidResult {
    return client->send_binary(std::move(data), std::move(handler));
}

// Adapter: Convert string send to WebSocket text frame
inline auto send_message(
    std::shared_ptr<messaging_ws_client> client,
    std::string&& text,
    std::function<void(std::error_code, std::size_t)> handler = nullptr
) -> VoidResult {
    return client->send_text(std::move(text), std::move(handler));
}

// TCP adapter (matches actual API - send_packet does NOT take handler)
inline auto send_message(
    std::shared_ptr<messaging_client> client,
    std::vector<uint8_t>&& data
) -> VoidResult {
    return client->send_packet(std::move(data));
}

} // namespace network_system::compat
```

Usage:
```cpp
// Protocol-agnostic code using adapter
using namespace network_system::compat;

auto tcp_client = std::make_shared<messaging_client>("tcp");
auto ws_client = std::make_shared<messaging_ws_client>("ws");

// TCP: No handler (fire-and-forget)
send_message(tcp_client, std::vector<uint8_t>{1,2,3});

// WebSocket: Optional handler for completion notification
send_message(ws_client, std::vector<uint8_t>{1,2,3}, [](auto ec, auto len) {
    if (!ec) std::cout << "WS sent " << len << " bytes\n";
});  // Sends as binary frame

send_message(ws_client, std::string("hello"), [](auto ec, auto len) {
    if (!ec) std::cout << "WS sent text " << len << " bytes\n";
});  // Sends as text frame
```

**Design Decision**: Support both approaches
- Approach 1 for WebSocket-aware code that needs text/binary control
- Approach 2 for protocol-agnostic middleware that doesn't care about text vs binary

---

## 📁 File Structure

```
network_system/
├── include/network_system/
│   ├── core/
│   │   ├── messaging_ws_client.h      # WebSocket client API
│   │   ├── messaging_ws_server.h      # WebSocket server API
│   │   └── ws_session_manager.h       # WebSocket session manager
│   ├── internal/
│   │   ├── websocket_socket.h         # Low-level socket wrapper
│   │   ├── websocket_protocol.h       # Protocol implementation
│   │   ├── websocket_frame.h          # Frame structures
│   │   └── websocket_handshake.h      # HTTP/1.1 handshake logic
│   └── utils/
│       └── websocket_types.h          # WebSocket-specific types
├── src/
│   ├── core/
│   │   ├── messaging_ws_client.cpp
│   │   ├── messaging_ws_server.cpp
│   │   └── ws_session_manager.cpp     # WebSocket session manager impl
│   └── internal/
│       ├── websocket_socket.cpp
│       ├── websocket_protocol.cpp
│       ├── websocket_frame.cpp
│       └── websocket_handshake.cpp      # HTTP/1.1 handshake impl
├── tests/
│   └── unit/                          # Unit tests only
│       ├── websocket_frame_test.cpp       # Frame encoding/decoding tests
│       ├── websocket_handshake_test.cpp   # Handshake tests
│       └── websocket_protocol_test.cpp    # Protocol tests
├── integration_tests/
│   └── websocket/                     # Integration tests (requires NETWORK_BUILD_INTEGRATION_TESTS)
│       ├── websocket_client_server_test.cpp  # Client-server integration
│       └── websocket_interop_test.cpp        # Interoperability tests
├── samples/
│   ├── websocket_echo_demo.cpp        # Echo server/client
│   ├── websocket_chat_demo.cpp        # Chat application
│   └── websocket_broadcast_demo.cpp   # Broadcast example
└── docs/
    └── WEBSOCKET_SUPPORT.md           # User documentation
```

---

## 🔧 Implementation Phases

### Phase 1: WebSocket Frame Layer (Foundation)
**Duration**: 3-4 days
**Priority**: High

#### Components

**1.1 websocket_frame.h/cpp**
```cpp
namespace network_system::internal {

// Frame opcodes (RFC 6455)
enum class ws_opcode : uint8_t {
    continuation = 0x0,
    text         = 0x1,
    binary       = 0x2,
    close        = 0x8,
    ping         = 0x9,
    pong         = 0xA
};

// Frame header structure
struct ws_frame_header {
    bool fin;              // Final fragment flag
    bool rsv1, rsv2, rsv3; // Reserved bits
    ws_opcode opcode;      // Operation code
    bool mask;             // Mask flag
    uint64_t payload_len;  // Payload length
    std::array<uint8_t, 4> masking_key; // Masking key
};

// Frame encoder/decoder
class websocket_frame {
public:
    // Encode data frame (move semantics for zero-copy)
    static std::vector<uint8_t> encode_frame(
        ws_opcode opcode,
        std::vector<uint8_t>&& payload,
        bool fin = true,
        bool mask = false
    );

    // Decode frame header
    static std::optional<ws_frame_header> decode_header(
        const std::vector<uint8_t>& data
    );

    // Decode frame payload
    static std::vector<uint8_t> decode_payload(
        const ws_frame_header& header,
        const std::vector<uint8_t>& data
    );

    // Apply/remove masking
    static void apply_mask(
        std::vector<uint8_t>& data,
        const std::array<uint8_t, 4>& mask
    );

    // Generate random masking key
    static std::array<uint8_t, 4> generate_mask();
};

} // namespace network_system::internal
```

**Key Features:**
- RFC 6455 compliant frame encoding/decoding
- Support for all frame types (text, binary, control)
- Masking/unmasking for client frames
- Fragmentation support
- Efficient buffer operations

**Testing:**
- Unit tests for frame encoding/decoding
- Test all opcode types
- Test various payload sizes (0, 125, 126, 127, 65536 bytes)
- Test masking/unmasking
- Edge cases (invalid frames, truncated data)

---

### Phase 2: WebSocket Handshake Layer
**Duration**: 2-3 days
**Priority**: High

#### Components

**2.1 websocket_handshake.h/cpp**
```cpp
namespace network_system::internal {

// WebSocket handshake result
struct ws_handshake_result {
    bool success;
    std::string error_message;
    std::map<std::string, std::string> headers;
};

// HTTP request/response builder
class websocket_handshake {
public:
    // Client: Generate handshake request
    static std::string create_client_handshake(
        std::string_view host,
        std::string_view path,
        uint16_t port,
        const std::map<std::string, std::string>& extra_headers = {}
    );

    // Client: Validate handshake response
    static ws_handshake_result validate_server_response(
        const std::string& response,
        const std::string& expected_key
    );

    // Server: Parse handshake request
    static ws_handshake_result parse_client_request(
        const std::string& request
    );

    // Server: Generate handshake response
    static std::string create_server_response(
        const std::string& client_key
    );

    // Generate Sec-WebSocket-Key
    static std::string generate_websocket_key();

    // Calculate Sec-WebSocket-Accept
    static std::string calculate_accept_key(
        const std::string& client_key
    );
};

} // namespace network_system::internal
```

**Key Features:**
- HTTP/1.1 upgrade handshake
- Sec-WebSocket-Key generation and validation
- SHA-1 based accept key calculation
- Header parsing and validation
- Support for custom headers

**Testing:**
- Test handshake request generation
- Test handshake response validation
- Test Sec-WebSocket-Accept calculation
- Test invalid handshake scenarios
- Test header parsing

---

### Phase 3: WebSocket Protocol Layer
**Duration**: 4-5 days
**Priority**: High

#### Components

**3.1 websocket_protocol.h/cpp**
```cpp
namespace network_system::internal {

// Message type
enum class ws_message_type {
    text,
    binary
};

// WebSocket message
struct ws_message {
    ws_message_type type;
    std::vector<uint8_t> data;

    // Convenience methods
    std::string as_text() const;
    const std::vector<uint8_t>& as_binary() const;
};

// Close code (RFC 6455 Section 7.4)
enum class ws_close_code : uint16_t {
    normal               = 1000,
    going_away           = 1001,
    protocol_error       = 1002,
    unsupported_data     = 1003,
    invalid_frame        = 1007,
    policy_violation     = 1008,
    message_too_big      = 1009,
    internal_error       = 1011
};

// WebSocket protocol handler
class websocket_protocol {
public:
    // Constructor
    websocket_protocol(bool is_client);

    // Process incoming data
    void process_data(const std::vector<uint8_t>& data);

    // Send text message (move semantics for zero-copy)
    std::vector<uint8_t> create_text_message(
        std::string&& text
    );

    // Send binary message (move semantics for zero-copy)
    std::vector<uint8_t> create_binary_message(
        std::vector<uint8_t>&& data
    );

    // Send ping (move semantics for zero-copy)
    std::vector<uint8_t> create_ping(
        std::vector<uint8_t>&& payload = {}
    );

    // Send pong (move semantics for zero-copy)
    std::vector<uint8_t> create_pong(
        std::vector<uint8_t>&& payload = {}
    );

    // Send close frame
    std::vector<uint8_t> create_close(
        ws_close_code code,
        std::string&& reason = ""
    );

    // Callbacks
    void set_message_callback(
        std::function<void(const ws_message&)> callback
    );

    void set_ping_callback(
        std::function<void(const std::vector<uint8_t>&)> callback
    );

    void set_pong_callback(
        std::function<void(const std::vector<uint8_t>&)> callback
    );

    void set_close_callback(
        std::function<void(ws_close_code, const std::string&)> callback
    );

private:
    bool is_client_;
    std::vector<uint8_t> buffer_;
    std::vector<uint8_t> fragmented_message_;
    ws_opcode fragmented_type_;

    // Callbacks
    std::function<void(const ws_message&)> message_callback_;
    std::function<void(const std::vector<uint8_t>&)> ping_callback_;
    std::function<void(const std::vector<uint8_t>&)> pong_callback_;
    std::function<void(ws_close_code, const std::string&)> close_callback_;

    // Internal handlers
    void handle_data_frame(const ws_frame_header& header, const std::vector<uint8_t>& payload);
    void handle_control_frame(const ws_frame_header& header, const std::vector<uint8_t>& payload);
    void handle_ping(const std::vector<uint8_t>& payload);
    void handle_pong(const std::vector<uint8_t>& payload);
    void handle_close(const std::vector<uint8_t>& payload);
};

} // namespace network_system::internal
```

**Key Features:**
- Complete frame processing state machine
- Message fragmentation and reassembly
- Control frame handling (ping/pong/close)
- Text/binary message support
- UTF-8 validation for text messages
- Automatic pong response to ping

**Testing:**
- Test message fragmentation/reassembly
- Test control frame handling
- Test ping/pong mechanism
- Test close handshake
- Test invalid frame sequences
- Test large message handling

**3.2 Data Flow and Pipeline Order**

**Critical Design Decision**: Define the exact order of operations for text/binary conversion, UTF-8 validation, and Pipeline application.

#### Sending Data Flow (Client/Server → Network)

```
User Code
    ↓
send_text(string) or send_binary(vector<uint8_t>)
    ↓
[1] TEXT CONVERSION & VALIDATION
    - If send_text: Convert std::string to std::vector<uint8_t> (no conversion, just wrap)
    - If send_text: UTF-8 validation (MUST be valid UTF-8 per RFC 6455)
    - If send_binary: No validation
    ↓
[2] WEBSOCKET FRAMING
    - Add WebSocket frame header (opcode, length, masking key if client)
    - Apply masking (client only)
    - Result: std::vector<uint8_t> with WebSocket frame
    ↓
[3] PIPELINE APPLICATION (Optional)
    - Apply pipeline::compress (if configured)
    - Apply pipeline::encrypt (if configured)
    - Order: compress → encrypt (standard practice)
    - Pipeline operates on WebSocket frames (NOT raw payload)
    - This ensures compression/encryption covers entire frame
    ↓
[4] TCP TRANSMISSION
    - Send via tcp_socket->async_send()
    - Actual network I/O
```

#### Receiving Data Flow (Network → Client/Server)

```
TCP Socket
    ↓
Raw bytes received via tcp_socket receive callback
    ↓
[1] PIPELINE APPLICATION (Optional, reverse order)
    - Apply pipeline::decrypt (if configured)
    - Apply pipeline::decompress (if configured)
    - Order: decrypt → decompress (reverse of send)
    - Result: Raw WebSocket frames
    ↓
[2] WEBSOCKET FRAME DECODING
    - Parse frame header (opcode, length, mask)
    - Unmask payload (if masked)
    - Reassemble fragmented messages
    - Result: Complete message payload
    ↓
[3] TEXT/BINARY INTERPRETATION
    - If opcode = TEXT: Validate UTF-8
    - If opcode = BINARY: No validation
    - Store in ws_message{type, data}
    ↓
User Callback
    - message_callback(ws_message)
    - or text_message_callback(string)
    - or binary_message_callback(vector<uint8_t>)
```

**Key Design Decisions:**

1. **Pipeline operates on WebSocket frames, not raw payloads**
   - Rationale: Compression/encryption should cover the entire frame for security
   - Alternative rejected: Applying pipeline before framing would expose frame structure
   - Implementation: Use `pipeline->compress` then `pipeline->encrypt` (send)
   - Implementation: Use `pipeline->decrypt` then `pipeline->decompress` (receive)

2. **UTF-8 validation happens AFTER framing for send, BEFORE user callback for receive**
   - Send: Validate before framing to reject invalid input early
   - Receive: Validate after de-framing to ensure RFC compliance
   - Invalid UTF-8 in TEXT frame → Close connection with code 1007 (invalid_frame)

3. **Text-to-binary conversion is zero-copy where possible**
   - std::string and std::vector<uint8_t> share underlying buffer via move semantics
   - No memory allocation for conversion

**Example Implementation:**

```cpp
// Send text flow
auto websocket_socket::async_send_text(
    std::string&& message,
    std::function<void(std::error_code, std::size_t)> handler
) -> VoidResult {
    // [1] UTF-8 validation
    if (!is_valid_utf8(message)) {
        return VoidResult::error(error_codes::common::invalid_argument, "Invalid UTF-8");
    }

    // [2] WebSocket framing
    auto frame_data = protocol_.create_text_frame(std::move(message));

    // [3] Pipeline (if configured)
    // Note: pipeline struct has compress/decompress/encrypt/decrypt functions
    // Apply in order: compress then encrypt (if both configured)
    if (pipeline_) {
        if (pipeline_->compress) {
            frame_data = pipeline_->compress(frame_data);
        }
        if (pipeline_->encrypt) {
            frame_data = pipeline_->encrypt(frame_data);
        }
    }

    // [4] TCP transmission
    tcp_socket_->async_send(std::move(frame_data), std::move(handler));

    return {};
}

// Receive flow
void websocket_socket::on_tcp_receive(const std::vector<uint8_t>& data) {
    // [1] Pipeline (reverse order: decrypt then decompress)
    auto decoded_data = data;
    if (pipeline_) {
        if (pipeline_->decrypt) {
            decoded_data = pipeline_->decrypt(decoded_data);
        }
        if (pipeline_->decompress) {
            decoded_data = pipeline_->decompress(decoded_data);
        }
    }

    // [2] Frame decoding
    auto messages = protocol_.decode_frames(decoded_data);

    // [3] Process each message
    for (const auto& msg : messages) {
        if (msg.type == ws_message_type::text) {
            // UTF-8 validation for text messages
            if (!is_valid_utf8(msg.data)) {
                // RFC 6455: Close with code 1007
                async_close(ws_close_code::invalid_frame, "Invalid UTF-8", nullptr);
                return;
            }
        }

        // Dispatch to user callback
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (message_callback_) {
            message_callback_(msg);
        }
    }
}
```

---

### Phase 4: WebSocket Socket Layer ✅ **COMPLETED**
**Duration**: 3-4 days
**Priority**: High
**Status**: Implementation complete (2025-10-26)

#### Components

**4.1 websocket_socket.h/cpp**

**Design Principle**: Reuse existing `tcp_socket` infrastructure instead of duplicating functionality.

The `websocket_socket` wraps an existing `tcp_socket` and adds WebSocket framing on top of it. This approach:
- Avoids code duplication (tcp_socket already handles async I/O, error callbacks)
- Maintains consistency with existing network_system patterns
- Separates concerns: tcp_socket handles transport, websocket_socket handles framing

```cpp
namespace network_system::internal {

// Forward declaration
class tcp_socket;

// WebSocket connection state
enum class ws_state {
    connecting,
    open,
    closing,
    closed
};

/**
 * @class websocket_socket
 * @brief WebSocket framing layer built on top of tcp_socket
 *
 * Wraps an existing tcp_socket and provides WebSocket protocol framing.
 * The underlying tcp_socket handles all transport I/O, while this class
 * handles frame encoding/decoding and WebSocket-specific logic.
 */
class websocket_socket : public std::enable_shared_from_this<websocket_socket> {
public:
    // Constructor (wraps existing tcp_socket)
    websocket_socket(
        std::shared_ptr<tcp_socket> socket,
        bool is_client
    );

    // Destructor
    ~websocket_socket();

    // Perform WebSocket handshake
    auto async_handshake(
        const std::string& host,
        const std::string& path,
        uint16_t port,
        std::function<void(std::error_code)> handler
    ) -> void;

    // Accept WebSocket handshake (server)
    auto async_accept(
        std::function<void(std::error_code)> handler
    ) -> void;

    // Start reading frames
    auto start_read() -> void;

    // Send text message (move semantics for zero-copy)
    auto async_send_text(
        std::string&& message,
        std::function<void(std::error_code, std::size_t)> handler
    ) -> VoidResult;

    // Send binary message (move semantics for zero-copy)
    auto async_send_binary(
        std::vector<uint8_t>&& data,
        std::function<void(std::error_code, std::size_t)> handler
    ) -> VoidResult;

    // Send ping
    auto async_send_ping(
        std::vector<uint8_t> payload,
        std::function<void(std::error_code)> handler
    ) -> void;

    // Close connection
    auto async_close(
        ws_close_code code,
        const std::string& reason,
        std::function<void(std::error_code)> handler
    ) -> void;

    // Get connection state
    auto state() const -> ws_state;

    // Check if connected
    auto is_open() const -> bool;

    // Set callbacks
    void set_message_callback(
        std::function<void(const ws_message&)> callback
    );

    void set_ping_callback(
        std::function<void(const std::vector<uint8_t>&)> callback
    );

    void set_pong_callback(
        std::function<void(const std::vector<uint8_t>&)> callback
    );

    void set_close_callback(
        std::function<void(ws_close_code, const std::string&)> callback
    );

    void set_error_callback(
        std::function<void(std::error_code)> callback
    );

private:
    // Underlying TCP socket (reuses existing infrastructure)
    std::shared_ptr<tcp_socket> tcp_socket_;

    // WebSocket protocol state machine
    websocket_protocol protocol_;
    std::atomic<ws_state> state_{ws_state::connecting};

    // Callbacks
    std::mutex callback_mutex_;
    std::function<void(const ws_message&)> message_callback_;
    std::function<void(const std::vector<uint8_t>&)> ping_callback_;
    std::function<void(const std::vector<uint8_t>&)> pong_callback_;
    std::function<void(ws_close_code, const std::string&)> close_callback_;
    std::function<void(std::error_code)> error_callback_;

    // Internal methods
    void on_tcp_receive(const std::vector<uint8_t>& data);  // Called by tcp_socket
    void on_tcp_error(std::error_code ec);                   // Called by tcp_socket
    void handle_protocol_message(const ws_message& msg);
    void handle_protocol_ping(const std::vector<uint8_t>& payload);
    void handle_protocol_pong(const std::vector<uint8_t>& payload);
    void handle_protocol_close(ws_close_code code, const std::string& reason);
};

} // namespace network_system::internal
```

**Implementation Notes:**

1. **Constructor Integration**
   ```cpp
   websocket_socket::websocket_socket(std::shared_ptr<tcp_socket> socket, bool is_client)
       : tcp_socket_(std::move(socket))
       , protocol_(is_client)
   {
       // Set tcp_socket callbacks to feed data into WebSocket protocol layer
       tcp_socket_->set_receive_callback([this](const std::vector<uint8_t>& data) {
           on_tcp_receive(data);
       });

       tcp_socket_->set_error_callback([this](std::error_code ec) {
           on_tcp_error(ec);
       });

       // Start reading from TCP socket
       tcp_socket_->start_read();
   }
   ```

2. **Sending Messages**
   ```cpp
   auto websocket_socket::async_send_text(
       std::string&& message,
       std::function<void(std::error_code, std::size_t)> handler
   ) -> VoidResult {
       // Encode text message into WebSocket frame
       auto frame_data = protocol_.encode_text_frame(std::move(message));

       // Send via underlying tcp_socket
       tcp_socket_->async_send(std::move(frame_data), std::move(handler));

       return {};
   }
   ```

3. **Receiving Data**
   ```cpp
   void websocket_socket::on_tcp_receive(const std::vector<uint8_t>& data) {
       // Feed raw TCP data into WebSocket protocol decoder
       auto messages = protocol_.decode_frames(data);

       // Process each complete message
       for (const auto& msg : messages) {
           if (msg.is_control_frame) {
               handle_control_frame(msg);
           } else {
               std::lock_guard<std::mutex> lock(callback_mutex_);
               if (message_callback_) {
                   message_callback_(msg);
               }
           }
       }
   }
   ```

**Key Design Benefits:**
- **Reuses tcp_socket infrastructure**: No duplication of async I/O, error handling, buffering
- **Single responsibility**: websocket_socket focuses only on WebSocket framing
- **Consistent patterns**: Same tcp_socket used by messaging_client is reused here
- **Easier testing**: Can mock tcp_socket for unit testing websocket_socket
- **Simpler handshake**: Handshake logic just writes/reads raw bytes via tcp_socket
- **Thread safety**: Inherits tcp_socket's thread-safe I/O operations

**Testing:**
- Test handshake (client and server)
- Test message send/receive
- Test ping/pong
- Test close handshake
- Test connection state transitions
- Test error handling

---

### Phase 5: High-Level Client/Server API ✅ **COMPLETED**
**Duration**: 5-6 days
**Priority**: High
**Status**: Implementation complete (2025-10-26)

#### Components

**5.1 messaging_ws_client.h/cpp**
```cpp
namespace network_system::core {

// WebSocket client configuration
struct ws_client_config {
    std::string host;
    uint16_t port = 80;
    std::string path = "/";
    std::map<std::string, std::string> headers;
    std::chrono::milliseconds connect_timeout{10000};
    std::chrono::milliseconds ping_interval{30000};
    bool auto_pong = true;
    bool auto_reconnect = false;
    size_t max_message_size = 10 * 1024 * 1024; // 10MB
};

class messaging_ws_client : public std::enable_shared_from_this<messaging_ws_client> {
public:
    // Constructor
    explicit messaging_ws_client(const std::string& client_id);

    // Destructor
    ~messaging_ws_client();

    // Start client with config
    auto start_client(const ws_client_config& config) -> VoidResult;

    // Start client with simple parameters
    auto start_client(
        std::string_view host,
        uint16_t port,
        std::string_view path = "/"
    ) -> VoidResult;

    // Stop client
    auto stop_client() -> VoidResult;

    // Wait for client to stop
    auto wait_for_stop() -> void;

    // Check if running
    auto is_running() const -> bool;

    // Check if connected
    auto is_connected() const -> bool;

    // Send text message (move semantics for zero-copy)
    auto send_text(
        std::string&& message,
        std::function<void(std::error_code, std::size_t)> handler = nullptr
    ) -> VoidResult;

    // Send binary message (move semantics for zero-copy)
    auto send_binary(
        std::vector<uint8_t>&& data,
        std::function<void(std::error_code, std::size_t)> handler = nullptr
    ) -> VoidResult;

    // Send ping (move semantics for zero-copy)
    auto send_ping(
        std::vector<uint8_t>&& payload = {}
    ) -> VoidResult;

    // Close connection
    auto close(
        ws_close_code code = ws_close_code::normal,
        const std::string& reason = ""
    ) -> VoidResult;

    // Set callbacks
    void set_message_callback(
        std::function<void(const ws_message&)> callback
    );

    void set_text_message_callback(
        std::function<void(const std::string&)> callback
    );

    void set_binary_message_callback(
        std::function<void(const std::vector<uint8_t>&)> callback
    );

    void set_connected_callback(
        std::function<void()> callback
    );

    void set_disconnected_callback(
        std::function<void(ws_close_code, const std::string&)> callback
    );

    void set_error_callback(
        std::function<void(std::error_code)> callback
    );

    // Get client ID
    auto client_id() const -> const std::string&;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;

    // Note: Implementation will include:
    // - internal::pipeline for compression/encryption (same as TCP)
    // - integration::logger_integration_manager for logging
    // - Monitoring hooks compatible with existing network metrics
};

} // namespace network_system::core
```

**5.2 messaging_ws_server.h/cpp**
```cpp
namespace network_system::core {

// WebSocket server configuration
struct ws_server_config {
    uint16_t port = 8080;
    std::string path = "/";
    size_t max_connections = 1000;  // Managed via session_manager (reuses existing infrastructure)
    std::chrono::milliseconds ping_interval{30000};
    bool auto_pong = true;
    size_t max_message_size = 10 * 1024 * 1024; // 10MB
};

// WebSocket connection handle
class ws_connection {
public:
    // Send text message (move semantics for zero-copy)
    auto send_text(
        std::string&& message,
        std::function<void(std::error_code, std::size_t)> handler = nullptr
    ) -> VoidResult;

    // Send binary message (move semantics for zero-copy)
    auto send_binary(
        std::vector<uint8_t>&& data,
        std::function<void(std::error_code, std::size_t)> handler = nullptr
    ) -> VoidResult;

    // Close connection
    auto close(
        ws_close_code code = ws_close_code::normal,
        const std::string& reason = ""
    ) -> VoidResult;

    // Get connection ID
    auto connection_id() const -> const std::string&;

    // Get remote endpoint
    auto remote_endpoint() const -> std::string;
};

class messaging_ws_server : public std::enable_shared_from_this<messaging_ws_server> {
public:
    // Constructor
    explicit messaging_ws_server(const std::string& server_id);

    // Destructor
    ~messaging_ws_server();

    // Start server with config
    auto start_server(const ws_server_config& config) -> VoidResult;

    // Start server with simple parameters
    auto start_server(uint16_t port, std::string_view path = "/") -> VoidResult;

    // Stop server
    auto stop_server() -> VoidResult;

    // Wait for server to stop
    auto wait_for_stop() -> void;

    // Check if running
    auto is_running() const -> bool;

    // Broadcast text message to all connections
    auto broadcast_text(const std::string& message) -> void;

    // Broadcast binary message to all connections
    auto broadcast_binary(const std::vector<uint8_t>& data) -> void;

    // Get connection by ID
    auto get_connection(const std::string& connection_id) -> std::shared_ptr<ws_connection>;

    // Get all connection IDs
    auto get_all_connections() -> std::vector<std::string>;

    // Get connection count
    auto connection_count() const -> size_t;

    // Set callbacks
    void set_connection_callback(
        std::function<void(std::shared_ptr<ws_connection>)> callback
    );

    void set_disconnection_callback(
        std::function<void(const std::string& connection_id, ws_close_code, const std::string&)> callback
    );

    void set_message_callback(
        std::function<void(std::shared_ptr<ws_connection>, const ws_message&)> callback
    );

    void set_text_message_callback(
        std::function<void(std::shared_ptr<ws_connection>, const std::string&)> callback
    );

    void set_binary_message_callback(
        std::function<void(std::shared_ptr<ws_connection>, const std::vector<uint8_t>&)> callback
    );

    void set_error_callback(
        std::function<void(const std::string& connection_id, std::error_code)> callback
    );

    // Get server ID
    auto server_id() const -> const std::string&;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;

    // Note: Implementation will include:
    // - ws_session_manager for connection management (same pattern as session_manager)
    // - internal::pipeline for compression/encryption (same as TCP)
    // - integration::logger_integration_manager for logging
    // - Monitoring hooks compatible with existing network metrics
};

} // namespace network_system::core
```

**Key Features:**
- Simple, intuitive API
- Connection management
- Broadcast support
- Configurable options
- Event-driven callbacks
- Thread-safe operations
- Follows TCP/UDP patterns

**5.3 Integration with Existing Infrastructure**

**Pipeline Integration (Compression/Encryption)**
```cpp
// messaging_ws_client::impl private members (similar to messaging_client)
class messaging_ws_client::impl {
private:
    internal::pipeline pipeline_;
    bool compress_mode_{false};
    bool encrypt_mode_{false};

    // Process outgoing message through pipeline
    auto process_outgoing(std::vector<uint8_t>&& data) -> std::vector<uint8_t> {
        if (compress_mode_) {
            data = pipeline_.compress(std::move(data));
        }
        if (encrypt_mode_) {
            data = pipeline_.encrypt(std::move(data));
        }
        return data;
    }

    // Process incoming message through pipeline
    auto process_incoming(std::vector<uint8_t>&& data) -> std::vector<uint8_t> {
        if (encrypt_mode_) {
            data = pipeline_.decrypt(std::move(data));
        }
        if (compress_mode_) {
            data = pipeline_.decompress(std::move(data));
        }
        return data;
    }
};
```

**Logger Integration**
```cpp
// Use existing logger macros throughout WebSocket implementation
#include "network_system/integration/logger_integration.h"

// Example usage in messaging_ws_client
auto messaging_ws_client::start_client(const ws_client_config& config) -> VoidResult {
    NETWORK_LOG_INFO("Starting WebSocket client: " + client_id_);
    // ... implementation
}

auto messaging_ws_client::on_message(const ws_message& msg) -> void {
    NETWORK_LOG_DEBUG("Received message: " + std::to_string(msg.data.size()) + " bytes");
    // ... implementation
}
```

**Session Manager Integration (Server)**

**Note on session_manager Reuse**: The existing `session_manager` is designed for
`std::shared_ptr<network_system::session::messaging_session>` (TCP sessions). Since
`ws_connection` is a different type and `messaging_session` is not polymorphic, we'll
create a WebSocket-specific session manager with the same interface pattern.

```cpp
// New file: include/network_system/core/ws_session_manager.h
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>

#include "network_system/core/session_manager.h"  // Reuse session_config

namespace network_system::core {

// Forward declaration
class ws_connection;

/**
 * @class ws_session_manager
 * @brief WebSocket session manager (mirrors session_manager design)
 *
 * Provides the same connection management features as session_manager
 * but for WebSocket connections instead of TCP sessions.
 *
 * ### Thread Safety
 * All methods are thread-safe using shared_mutex for concurrent reads
 * and exclusive writes.
 */
class ws_session_manager {
public:
    using ws_connection_ptr = std::shared_ptr<ws_connection>;

    explicit ws_session_manager(const session_config& config = session_config())
        : config_(config)
        , connection_count_(0)
        , total_accepted_(0)
        , total_rejected_(0)
    {
    }

    /**
     * @brief Check if new connection can be accepted
     * @return true if under max_sessions limit
     */
    [[nodiscard]] bool can_accept_connection() const {
        return connection_count_.load(std::memory_order_acquire) < config_.max_sessions;
    }

    /**
     * @brief Check if backpressure should be applied
     * @return true if connection count exceeds backpressure threshold
     */
    [[nodiscard]] bool is_backpressure_active() const {
        if (!config_.enable_backpressure) {
            return false;
        }
        auto count = connection_count_.load(std::memory_order_acquire);
        auto threshold = static_cast<size_t>(config_.max_sessions * config_.backpressure_threshold);
        return count >= threshold;
    }

    /**
     * @brief Add connection to manager (thread-safe)
     * @param conn Connection to add
     * @param conn_id Optional connection ID (auto-generated if empty)
     * @return Connection ID that was used (useful when auto-generated)
     *
     * @note Returns empty string if connection was rejected due to limits
     */
    std::string add_connection(ws_connection_ptr conn, const std::string& conn_id = "") {
        if (!can_accept_connection()) {
            total_rejected_.fetch_add(1, std::memory_order_relaxed);
            return "";  // Rejected
        }

        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        std::string id = conn_id.empty() ? generate_connection_id() : conn_id;

        active_connections_[id] = conn;
        connection_count_.fetch_add(1, std::memory_order_release);
        total_accepted_.fetch_add(1, std::memory_order_relaxed);

        return id;  // Return the ID that was used
    }

    /**
     * @brief Remove connection (thread-safe)
     * @param conn_id Connection ID to remove
     * @return true if removed
     */
    bool remove_connection(const std::string& conn_id) {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);

        auto it = active_connections_.find(conn_id);
        if (it != active_connections_.end()) {
            active_connections_.erase(it);
            connection_count_.fetch_sub(1, std::memory_order_release);
            return true;
        }
        return false;
    }

    /**
     * @brief Get connection by ID (thread-safe)
     */
    [[nodiscard]] ws_connection_ptr get_connection(const std::string& conn_id) const {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = active_connections_.find(conn_id);
        return (it != active_connections_.end()) ? it->second : nullptr;
    }

    /**
     * @brief Get all active connections (thread-safe)
     */
    [[nodiscard]] std::vector<ws_connection_ptr> get_all_connections() const {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        std::vector<ws_connection_ptr> conns;
        conns.reserve(active_connections_.size());
        for (const auto& [id, conn] : active_connections_) {
            conns.push_back(conn);
        }
        return conns;
    }

    /**
     * @brief Get current connection count
     */
    [[nodiscard]] size_t get_connection_count() const {
        return connection_count_.load(std::memory_order_acquire);
    }

    /**
     * @brief Generate a unique connection ID
     * @return Generated connection ID
     *
     * @note This is public to allow external ID generation if needed
     */
    static std::string generate_connection_id() {
        static std::atomic<uint64_t> counter{0};
        return "ws_conn_" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
    }

private:
    session_config config_;
    mutable std::shared_mutex connections_mutex_;
    std::unordered_map<std::string, ws_connection_ptr> active_connections_;

    std::atomic<size_t> connection_count_;
    std::atomic<uint64_t> total_accepted_;
    std::atomic<uint64_t> total_rejected_;
};

} // namespace network_system::core
```

**Usage in messaging_ws_server**
```cpp
// messaging_ws_server::impl will use ws_session_manager
#include "network_system/core/ws_session_manager.h"

class messaging_ws_server::impl {
private:
    std::shared_ptr<ws_session_manager> session_mgr_;

    auto on_new_connection(asio::ip::tcp::socket socket) -> void {
        // Check session limits before accepting
        if (!session_mgr_->can_accept_connection()) {
            NETWORK_LOG_WARN("Connection rejected: max sessions reached");
            socket.close();
            return;
        }

        // Create WebSocket connection and add to manager
        auto ws_conn = std::make_shared<ws_connection>(std::move(socket));

        // add_connection returns the connection ID (auto-generated if not provided)
        auto conn_id = session_mgr_->add_connection(ws_conn);

        if (!conn_id.empty()) {
            NETWORK_LOG_INFO("WebSocket connection accepted: " + conn_id);
            // Perform handshake...
        } else {
            NETWORK_LOG_WARN("Connection rejected: add_connection failed");
            // Connection was rejected (should not happen after can_accept_connection check,
            // but handle race condition)
        }
    }

    auto on_connection_closed(const std::string& conn_id) -> void {
        session_mgr_->remove_connection(conn_id);
        NETWORK_LOG_INFO("WebSocket connection closed: " + conn_id);
    }
};
```

**Why a Separate Manager?**
- `session_manager` is tightly coupled to `messaging_session` (TCP sessions)
- `messaging_session` is not polymorphic (no virtual functions)
- Creating `ws_session_manager` maintains type safety and clear separation
- Both managers share the same `session_config` for consistent configuration
- Pattern is consistent: each protocol has its own session type and manager

**Monitoring & Metrics**
```cpp
// Leverage existing monitoring patterns
// WebSocket metrics will be compatible with TCP/UDP metrics

struct ws_metrics {
    std::atomic<uint64_t> messages_sent{0};
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> frames_sent{0};
    std::atomic<uint64_t> frames_received{0};
    std::atomic<uint64_t> ping_count{0};
    std::atomic<uint64_t> pong_count{0};
};

// Compatible with existing network monitoring tools
```

**Testing:**
- Test client connection/disconnection
- Test server accept/close
- Test message send/receive
- Test broadcast
- Test connection management
- Test error scenarios
- Integration tests

**5.3 API Compatibility Layer (compatibility.h extension)**

Extend `include/network_system/compatibility.h` (existing file) to provide protocol-agnostic messaging.

**Note**: Current file uses `network_system::compat` namespace, not `compatibility`. We extend this namespace.

```cpp
// compatibility.h extension (add to existing file)
namespace network_system::compat {

// Forward declarations (add to existing includes)
namespace core {
    class messaging_client;
    class messaging_udp_client;
    class messaging_ws_client;
}

//////////////////////////////////////////////////
// Protocol-agnostic send_message adapters
//////////////////////////////////////////////////

// TCP adapter (matches actual API - send_packet does NOT take handler)
inline auto send_message(
    std::shared_ptr<core::messaging_client> client,
    std::vector<uint8_t>&& data
) -> VoidResult {
    return client->send_packet(std::move(data));
}

// UDP adapter (matches actual API - send_packet DOES take handler)
inline auto send_message(
    std::shared_ptr<core::messaging_udp_client> client,
    std::vector<uint8_t>&& data,
    std::function<void(std::error_code, std::size_t)> handler = nullptr
) -> VoidResult {
    return client->send_packet(std::move(data), std::move(handler));
}

// WebSocket adapter - binary frame
inline auto send_message(
    std::shared_ptr<core::messaging_ws_client> client,
    std::vector<uint8_t>&& data,
    std::function<void(std::error_code, std::size_t)> handler = nullptr
) -> VoidResult {
    return client->send_binary(std::move(data), std::move(handler));
}

// WebSocket adapter - text frame
inline auto send_message(
    std::shared_ptr<core::messaging_ws_client> client,
    std::string&& text,
    std::function<void(std::error_code, std::size_t)> handler = nullptr
) -> VoidResult {
    return client->send_text(std::move(text), std::move(handler));
}

//////////////////////////////////////////////////
// Example usage in protocol-agnostic code
//////////////////////////////////////////////////

/*
// Example 1: TCP client (no handler)
auto tcp_client = std::make_shared<core::messaging_client>("tcp");
send_message(tcp_client, std::vector<uint8_t>{1,2,3});  // Calls send_packet

// Example 2: UDP client (with handler)
auto udp_client = std::make_shared<core::messaging_udp_client>("udp");
send_message(udp_client, std::vector<uint8_t>{1,2,3}, [](auto ec, auto len) {
    if (!ec) std::cout << "UDP sent " << len << " bytes\\n";
});

// Example 3: WebSocket binary (with handler)
auto ws_client = std::make_shared<core::messaging_ws_client>("ws");
send_message(ws_client, std::vector<uint8_t>{1,2,3}, [](auto ec, auto len) {
    if (!ec) std::cout << "WS sent " << len << " bytes\\n";
});

// Example 4: WebSocket text (with handler)
send_message(ws_client, std::string("hello"), [](auto ec, auto len) {
    if (!ec) std::cout << "WS sent text " << len << " bytes\\n";
});

// Note: TCP adapter does NOT support handler (TCP send_packet has no callback)
//       UDP/WebSocket adapters DO support optional handler parameter
*/

} // namespace network_system::compat
```

**Design Rationale:**
- **Overload resolution**: C++ overload resolution selects the correct adapter based on client type
- **Zero overhead**: All adapters are inline, compile-time dispatch
- **Backward compatible**: Existing TCP/UDP code unchanged
- **Opt-in**: Users who want WebSocket-specific API can still call `send_text`/`send_binary` directly
- **messaging_bridge compatible**: Works with existing messaging_bridge patterns

**File Changes:**
- Update `include/network_system/compatibility.h` (existing file at root level)
- Add forward declarations for WebSocket types within `network_system::compat` namespace
- Add WebSocket adapter overloads to `network_system::compat`
- Update includes to add WebSocket headers
- Document usage patterns in WEBSOCKET_SUPPORT.md

**5.4 messaging_bridge WebSocket Integration**

Extend `include/network_system/integration/messaging_bridge.h` to support WebSocket client/server creation.

**Current State**: messaging_bridge only provides `create_server()` and `create_client()` for TCP.

**Proposed Extension**:

```cpp
// Add to messaging_bridge.h
namespace network_system::integration {

class messaging_bridge {
public:
    // ... existing TCP methods ...

    #ifdef BUILD_WEBSOCKET_SUPPORT  // Conditional compilation for WebSocket
    /**
     * @brief Create WebSocket server
     * @param server_id Server identifier
     * @param port Port to listen on
     * @param path WebSocket endpoint path (default "/")
     * @return Shared pointer to WebSocket server
     */
    std::shared_ptr<core::messaging_ws_server> create_ws_server(
        const std::string& server_id,
        uint16_t port,
        const std::string& path = "/"
    );

    /**
     * @brief Create WebSocket client
     * @param client_id Client identifier
     * @return Shared pointer to WebSocket client
     */
    std::shared_ptr<core::messaging_ws_client> create_ws_client(
        const std::string& client_id
    );

    /**
     * @brief Set WebSocket message handler (bridges container to WebSocket)
     * @param handler Function to convert container to WebSocket message
     */
    #ifdef BUILD_WITH_CONTAINER_SYSTEM
    void set_ws_message_handler(
        std::function<void(
            std::shared_ptr<core::messaging_ws_client>,
            std::shared_ptr<container_module::value_container>
        )> handler
    );
    #endif

    #endif // BUILD_WEBSOCKET_SUPPORT
};

} // namespace network_system::integration
```

**Implementation Notes**:

1. **Conditional Compilation**: WebSocket support is optional via `BUILD_WEBSOCKET_SUPPORT` flag
2. **Factory Pattern**: Follows same pattern as TCP `create_server/create_client`
3. **Container Integration**: Optional WebSocket-to-container message handler (if container_system available)
4. **Backward Compatible**: Existing TCP code unaffected

**Usage Example**:

```cpp
#include "network_system/integration/messaging_bridge.h"

auto bridge = std::make_shared<network_system::integration::messaging_bridge>();

#ifdef BUILD_WEBSOCKET_SUPPORT
// Create WebSocket server
auto ws_server = bridge->create_ws_server("ws_srv", 8080, "/ws");

// Create WebSocket client
auto ws_client = bridge->create_ws_client("ws_cli");
ws_client->start_client("localhost", 8080, "/ws");
#endif
```

**File Changes**:
- Update `include/network_system/integration/messaging_bridge.h`
- Update `src/integration/messaging_bridge.cpp` (implementation)
- Add `BUILD_WEBSOCKET_SUPPORT` CMake option
- Document in WEBSOCKET_SUPPORT.md

---

### Phase 6: Documentation and Examples ✅ **COMPLETED**
**Duration**: 2-3 days
**Priority**: Medium
**Status**: Implementation complete (2025-10-26)

#### Documentation

**6.1 Update include/network_system/network_system.h**

Add WebSocket headers to the main public header:

```cpp
#pragma once

/**
 * @file network_system.h
 * @brief Main header for the Network System
 *
 * This header provides access to all core Network System functionality
 * including messaging clients, servers, and session management.
 *
 * @author kcenon
 * @date 2025-09-19
 */

// Core networking components
#include "network_system/core/messaging_client.h"
#include "network_system/core/messaging_server.h"

// WebSocket support (conditional - only if BUILD_WEBSOCKET_SUPPORT=ON)
#ifdef BUILD_WEBSOCKET_SUPPORT
#include "network_system/core/messaging_ws_client.h"
#include "network_system/core/messaging_ws_server.h"
#endif

// Session management
#include "network_system/session/messaging_session.h"

// Integration interfaces
#include "network_system/integration/messaging_bridge.h"
#include "network_system/integration/thread_integration.h"
#include "network_system/integration/container_integration.h"

// Compatibility layer
#include "network_system/compatibility.h"

/**
 * @namespace network_system
 * @brief Main namespace for all Network System components
 */
namespace network_system {

/**
 * @brief Initialize the network system
 * @return true if initialization successful, false otherwise
 */
bool initialize();

/**
 * @brief Shutdown the network system
 */
void shutdown();

} // namespace network_system
```

**6.2 Update README.md**

Add WebSocket section to existing README (currently shows WebSocket as available but not implemented):

```markdown
## WebSocket Support

The network_system library provides full RFC 6455 compliant WebSocket support for both client and server implementations.

### Features

- **RFC 6455 Compliant**: Full implementation of the WebSocket protocol
- **Text and Binary Messages**: Support for both text and binary frame types
- **Control Frames**: Ping/pong for keep-alive, graceful close handshake
- **Message Fragmentation**: Automatic handling of fragmented messages
- **Thread-Safe**: All operations are thread-safe
- **Zero-Copy**: Move semantics for efficient message handling
- **Integration**: Seamless integration with existing TCP/UDP infrastructure

### Quick Start

**WebSocket Client:**

```cpp
#include <network_system/network_system.h>

auto client = std::make_shared<network_system::core::messaging_ws_client>("ws-client");

// Set message callback
client->set_text_message_callback([](const std::string& message) {
    std::cout << "Received: " << message << std::n";
});

// Connect to server
auto result = client->start_client("localhost", 8080, "/");
if (!result) {
    std::cerr << "Failed to connect: " << result.error().message << "\n";
    return -1;
}

// Send message
client->send_text("Hello, WebSocket!");

// Cleanup
client->stop_client();
```

**WebSocket Server:**

```cpp
#include <network_system/network_system.h>

auto server = std::make_shared<network_system::core::messaging_ws_server>("ws-server");

// Set message callback
server->set_text_message_callback(
    [](std::shared_ptr<network_system::core::ws_connection> conn, const std::string& message) {
        std::cout << "Client says: " << message << "\n";
        // Echo back
        conn->send_text(std::string(message));
    }
);

// Start server
auto result = server->start_server(8080, "/");
if (!result) {
    std::cerr << "Failed to start server: " << result.error().message << "\n";
    return -1;
}

// Server runs until stopped
server->wait_for_stop();
```

See `samples/websocket_echo_demo.cpp` for complete examples.
```

**6.3 WEBSOCKET_SUPPORT.md**
- Overview of WebSocket support
- API reference
- Usage examples
- Configuration options
- Best practices
- Troubleshooting

**6.4 API Documentation**
- Doxygen comments for all public APIs
- Code examples in headers

#### Examples

**6.5 websocket_echo_demo.cpp**
```cpp
// Simple echo server and client
// Client connects to server
// Client sends messages
// Server echoes them back
```

**6.6 websocket_chat_demo.cpp**
```cpp
// Multi-client chat application
// Server broadcasts messages to all clients
// Demonstrates connection management
```

**6.7 websocket_broadcast_demo.cpp**
```cpp
// Broadcasting example
// Server sends periodic updates
// Multiple clients receive broadcasts
```

---


## 🧪 Testing Strategy

### Unit Tests
- Frame encoding/decoding
- Handshake generation/validation
- Protocol state machine
- Message fragmentation
- Control frame handling

### Integration Tests
- Client-server connection
- Message exchange
- Ping/pong mechanism
- Close handshake
- Error handling

### Interoperability Tests
- Test against standard WebSocket servers (ws, uWebSockets)
- Test against browser WebSocket API
- Test against other WebSocket libraries

### Performance Tests
- Message throughput
- Connection handling (1000+ concurrent)
- Large message handling
- Memory usage

### Stress Tests
- Rapid connect/disconnect
- Malformed frames
- Resource exhaustion
- Long-running connections

---

## 📊 Performance Targets

| Metric | Target |
|--------|--------|
| Connection throughput | 10,000+ connections/sec |
| Message throughput | 100,000+ messages/sec |
| Latency (localhost) | < 1ms (p99) |
| Memory per connection | < 50KB |
| Max concurrent connections | 10,000+ |
| Max message size | 10MB (configurable) |

---

## 🔒 Security Considerations

### Implemented
- Frame masking (client → server)
- Close handshake validation
- UTF-8 validation for text frames
- Max message size limits
- Connection limits

### Future (TLS/WSS)
- TLS 1.2/1.3 support
- Certificate validation
- Secure WebSocket (wss://)

---

## 📦 Dependencies

### OpenSSL Dependency Strategy

**Decision**: OpenSSL is **REQUIRED** for WebSocket support.

**Rationale**:
- WebSocket handshake requires SHA-1 hash computation (RFC 6455 Section 1.3)
- SHA-1 is needed for the `Sec-WebSocket-Accept` header validation
- Alternatives considered:
  1. **Custom SHA-1 implementation**: Rejected due to security audit burden
  2. **Optional OpenSSL**: Rejected because WebSocket handshake is non-functional without it
  3. **C++20 <crypto>**: Not standardized yet, no portable SHA-1

**Impact**:
- Users who build `network_system` with WebSocket support MUST have OpenSSL available
- vcpkg provides OpenSSL automatically, minimal burden
- CMake will fail gracefully if OpenSSL not found: "OpenSSL not found - WebSocket support will be limited"

**Build Behavior**:
```cmake
# In CMakeLists.txt
if(OPENSSL_FOUND)
    target_link_libraries(NetworkSystem PRIVATE OpenSSL::Crypto)
    target_compile_definitions(NetworkSystem PRIVATE HAVE_OPENSSL)
else()
    message(WARNING "Building without OpenSSL - WebSocket handshake will not work")
    # WebSocket classes will still compile, but handshake will fail at runtime
endif()
```

**Alternative for OpenSSL-averse environments**:
- Users can build without WebSocket support using CMake flag: `cmake -DBUILD_WEBSOCKET_SUPPORT=OFF ..`
- When disabled, WebSocket sources are excluded, OpenSSL is not required, and WebSocket headers are not included
- See "Build System Changes" section for detailed conditional compilation behavior

---

### Required
- **ASIO** (already present) - Managed via vcpkg.json
- **C++20** standard library
- **OpenSSL** (REQUIRED for WebSocket handshake SHA-1) - Added to vcpkg.json
- **fmt** (already present) - Managed via vcpkg.json

### Build/Test Dependencies
- **GTest** (already present) - For unit tests
- **Google Benchmark** (already present) - For performance tests

### Optional
- **Base64 encoding library** (can use custom implementation or OpenSSL's)
- **zlib** (for future permessage-deflate compression extension)

### System Integrations (Optional)
- **logger_system** - For advanced logging (already integrated)
- **thread_system** - For thread pool support (already integrated)
- **container_system** - For data structures (already integrated)
- **common_system** - For shared utilities (already integrated)

All core dependencies are managed through vcpkg.json and cmake/NetworkSystemDependencies.cmake.

---

## 🗓️ Timeline

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Phase 1: Frame Layer | 3-4 days | None |
| Phase 2: Handshake Layer (HTTP/1.1) | 2-3 days | OpenSSL |
| Phase 3: Protocol Layer | 4-5 days | Phase 1, 2 |
| Phase 4: Socket Layer ✅ | 3-4 days | Phase 3 |
| Phase 5: Client/Server API ✅ | 5-6 days | Phase 4 |
| Phase 6: Documentation ✅ | 2-3 days | Phase 5 |
| Phase 7: Network Load Testing | 11-15 days | Phase 5 |
| **Total** | **30-40 days** | |

---

## 🎯 Milestones

### M1: Frame Layer Complete
- [ ] Frame encoding/decoding implemented
- [ ] Unit tests passing
- [ ] Code review completed

### M2: Handshake Complete
- [ ] HTTP handshake implemented
- [ ] Key generation/validation working
- [ ] Unit tests passing

### M3: Protocol Layer Complete
- [ ] Frame processing implemented
- [ ] Fragmentation working
- [ ] Control frames handled
- [ ] Unit tests passing

### M4: Socket Layer Complete ✅
- [x] ASIO integration working
- [x] Async operations implemented
- [x] State management working
- [x] Unit tests passing

### M5: High-Level API Complete ✅
- [x] Client API implemented
- [x] Server API implemented
- [x] Connection management working
- [x] Integration tests passing

### M6: Documentation Complete ✅
- [x] API documentation written
- [x] Examples created
- [x] User guide completed

### M7: Network Load Testing Complete
- [ ] Memory profiling infrastructure ready
- [ ] TCP load tests implemented
- [ ] UDP load tests implemented
- [ ] WebSocket load tests implemented
- [ ] CI/CD automation configured
- [ ] BASELINE.md updated with real metrics
- [ ] Load test guide documentation complete

---

## 🚧 Known Limitations & Future Work

### Current Limitations
- No compression extension (permessage-deflate)
- No subprotocol negotiation
- No TLS/WSS support initially
- No automatic reconnection with backoff

### Future Enhancements
1. **HTTP/2 WebSocket Support (RFC 8441)**
   - WebSocket over HTTP/2 using extended CONNECT method
   - SETTINGS_ENABLE_CONNECT_PROTOCOL negotiation
   - Integration with nghttp2 library for HTTP/2 framing
   - Auto-negotiation (HTTP/2 → HTTP/1.1 fallback)
   - Note: Requires significant infrastructure (TLS/ALPN, stream management, HPACK)

2. **Compression Extension**
   - permessage-deflate (RFC 7692)
   - Configurable compression level

3. **Secure WebSocket (WSS)**
   - TLS integration
   - Certificate management
   - SNI support

4. **Advanced Features**
   - Subprotocol negotiation
   - Custom extensions
   - Connection pooling
   - Automatic reconnection

5. **Performance Optimizations**
   - Zero-copy where possible
   - Buffer pooling
   - SIMD for masking
   - Lock-free queues

6. **Session Manager Refactoring**
   - Template `session_manager` to support both `messaging_session` (TCP) and `ws_connection` (WebSocket)
   - Extract common session management logic into base template
   - Reduce code duplication between `session_manager` and `ws_session_manager`
   - Approach:
     ```cpp
     template<typename SessionType>
     class session_manager_base {
         // Common logic: add/remove, limits, backpressure, metrics
     };

     using tcp_session_manager = session_manager_base<messaging_session>;
     using ws_session_manager = session_manager_base<ws_connection>;
     ```
   - **Note**: Current implementation uses separate `ws_session_manager` to avoid breaking existing TCP code
   - **Migration**: Future version can unify both under templated design

---

## 📋 Build System Changes

### CMakeLists.txt Option Addition

Add WebSocket build option to root `CMakeLists.txt` (in Options section after existing options):

```cmake
##################################################
# Options
##################################################

# ... existing options ...

option(BUILD_WEBSOCKET_SUPPORT "Build WebSocket protocol support (RFC 6455)" ON)
```

**Decision**: Default to **ON** (enabled by default)

**Rationale**:
- WebSocket is a core feature, not an experimental addon
- Users who don't want it can explicitly disable: `-DBUILD_WEBSOCKET_SUPPORT=OFF`
- Simplifies default build experience (most users want WebSocket)

**Impact**:
- When `ON`: Compiles WebSocket sources, links OpenSSL, defines `BUILD_WEBSOCKET_SUPPORT` macro
- When `OFF`: Excludes WebSocket sources, no OpenSSL requirement, WebSocket headers not included

**Conditional Compilation**:
```cmake
# In src/CMakeLists.txt
if(BUILD_WEBSOCKET_SUPPORT)
    list(APPEND NETWORK_SOURCES ${WEBSOCKET_SOURCES})
    target_compile_definitions(NetworkSystem PUBLIC BUILD_WEBSOCKET_SUPPORT)

    if(OPENSSL_FOUND)
        target_link_libraries(NetworkSystem PRIVATE OpenSSL::Crypto)
    else()
        message(FATAL_ERROR "BUILD_WEBSOCKET_SUPPORT=ON requires OpenSSL")
    endif()
endif()
```

---

### vcpkg.json Changes

Add OpenSSL dependency for SHA-1 in WebSocket handshake:

```json
{
    "name": "network-system",
    "description": "High-performance asynchronous network messaging library",
    "license": "BSD-3-Clause",
    "dependencies": [
        "asio",
        "fmt",
        "gtest",
        "benchmark",
        "openssl"
    ],
    "features": {
        "tests": {
            "description": "Build unit tests (deprecated - dependencies now in main list)",
            "dependencies": []
        },
        "samples": {
            "description": "Build sample applications",
            "dependencies": []
        },
        "docs": {
            "description": "Build documentation",
            "dependencies": []
        }
    }
}
```

### cmake/NetworkSystemDependencies.cmake Changes

Add OpenSSL detection function:

```cmake
##################################################
# Find OpenSSL (for WebSocket handshake)
##################################################
function(find_openssl_library)
    message(STATUS "Looking for OpenSSL library...")

    # Try CMake config package first
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        message(STATUS "Found OpenSSL: ${OPENSSL_VERSION}")
        set(OPENSSL_FOUND TRUE PARENT_SCOPE)
        set(OPENSSL_TARGET OpenSSL::SSL PARENT_SCOPE)
        set(OPENSSL_CRYPTO_TARGET OpenSSL::Crypto PARENT_SCOPE)
        return()
    endif()

    message(WARNING "OpenSSL not found - WebSocket handshake will not work")
    set(OPENSSL_FOUND FALSE PARENT_SCOPE)
endfunction()

# In find_network_system_dependencies():
function(find_network_system_dependencies)
    message(STATUS "Finding NetworkSystem dependencies...")

    find_asio_library()
    find_fmt_library()
    find_openssl_library()   # WebSocket handshake (HTTP/1.1)
    find_container_system()
    find_thread_system()
    find_logger_system()
    find_common_system()

    # ... existing exports ...

    # Export OpenSSL variables
    set(OPENSSL_FOUND ${OPENSSL_FOUND} PARENT_SCOPE)
    set(OPENSSL_TARGET ${OPENSSL_TARGET} PARENT_SCOPE)
    set(OPENSSL_CRYPTO_TARGET ${OPENSSL_CRYPTO_TARGET} PARENT_SCOPE)

    # ... rest of function
endfunction()
```

### src/CMakeLists.txt Changes

Add WebSocket sources to NetworkSystem library:

```cmake
# Add WebSocket implementation sources
set(WEBSOCKET_SOURCES
    src/core/messaging_ws_client.cpp
    src/core/messaging_ws_server.cpp
    src/core/ws_session_manager.cpp      # WebSocket session manager
    src/internal/websocket_socket.cpp
    src/internal/websocket_protocol.cpp
    src/internal/websocket_frame.cpp
    src/internal/websocket_handshake.cpp
)

# Add to NetworkSystem library sources
add_library(NetworkSystem
    # ... existing sources ...
    ${WEBSOCKET_SOURCES}
)

# Link OpenSSL for SHA-1 (handshake)
if(OPENSSL_FOUND)
    target_link_libraries(NetworkSystem PRIVATE ${OPENSSL_CRYPTO_TARGET})
    target_compile_definitions(NetworkSystem PRIVATE HAVE_OPENSSL)
else()
    message(WARNING "Building without OpenSSL - WebSocket support will be limited")
endif()
```

### samples/CMakeLists.txt Changes

Add WebSocket samples to the existing sample programs list:

```cmake
# List of all sample programs
set(SAMPLE_PROGRAMS
    basic_usage
    tcp_server_client
    http_client_demo
    run_all_samples
    memory_profile_demo
    websocket_echo_demo      # Add this
    websocket_chat_demo      # Add this
    websocket_broadcast_demo # Add this
)

# (Rest of CMakeLists.txt remains the same - the foreach loop will handle the new samples)
```

### tests/CMakeLists.txt Changes

Add WebSocket **unit tests only** to existing test configuration:

```cmake
##################################################
# WebSocket Unit Tests
##################################################

if(GTest_FOUND OR GTEST_FOUND)
    # WebSocket frame tests
    add_executable(websocket_frame_test
        unit/websocket_frame_test.cpp
    )

    target_link_libraries(websocket_frame_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(websocket_frame_test)
    setup_fmt_integration(websocket_frame_test)

    set_target_properties(websocket_frame_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(websocket_frame_test)

    # WebSocket handshake tests
    add_executable(websocket_handshake_test
        unit/websocket_handshake_test.cpp
    )

    target_link_libraries(websocket_handshake_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(websocket_handshake_test)
    setup_fmt_integration(websocket_handshake_test)

    set_target_properties(websocket_handshake_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(websocket_handshake_test)

    # WebSocket protocol tests
    add_executable(websocket_protocol_test
        unit/websocket_protocol_test.cpp
    )

    target_link_libraries(websocket_protocol_test PRIVATE
        NetworkSystem
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
    )

    setup_asio_integration(websocket_protocol_test)
    setup_fmt_integration(websocket_protocol_test)

    set_target_properties(websocket_protocol_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    gtest_discover_tests(websocket_protocol_test)
endif()
```

### integration_tests/CMakeLists.txt Changes

Add WebSocket integration tests to the existing integration test suite.
These tests require `NETWORK_BUILD_INTEGRATION_TESTS=ON` to build.

**Note**: Integration tests go in `integration_tests/` directory, controlled by
the `NETWORK_BUILD_INTEGRATION_TESTS` CMake flag (network_system/CMakeLists.txt:214-217).

Add to `integration_tests/CMakeLists.txt`:

```cmake
##################################################
# WebSocket Integration Tests
##################################################

# WebSocket client-server integration test
add_executable(websocket_client_server_test
    websocket/websocket_client_server_test.cpp
)

target_link_libraries(websocket_client_server_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

# Setup ASIO and FMT integration
setup_asio_integration(websocket_client_server_test)
setup_fmt_integration(websocket_client_server_test)

set_target_properties(websocket_client_server_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

gtest_discover_tests(websocket_client_server_test)

# WebSocket interoperability test (test against standard WebSocket servers)
add_executable(websocket_interop_test
    websocket/websocket_interop_test.cpp
)

target_link_libraries(websocket_interop_test PRIVATE
    NetworkSystem
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
)

setup_asio_integration(websocket_interop_test)
setup_fmt_integration(websocket_interop_test)

set_target_properties(websocket_interop_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

gtest_discover_tests(websocket_interop_test)

message(STATUS "WebSocket integration tests configured")
```

---

## 🎓 References

### RFC & Standards
- [RFC 6455](https://tools.ietf.org/html/rfc6455) - The WebSocket Protocol
- [RFC 7692](https://tools.ietf.org/html/rfc7692) - Compression Extensions

### Implementation References
- [WebSocket++](https://github.com/zaphoyd/websocketpp)
- [uWebSockets](https://github.com/uNetworking/uWebSockets)
- [Boost.Beast](https://www.boost.org/doc/libs/master/libs/beast/doc/html/index.html)

### Test Resources
- [Autobahn Testsuite](https://github.com/crossbario/autobahn-testsuite)
- [WebSocket.org Echo Test](https://www.websocket.org/echo.html)

---

## ✅ Success Criteria Checklist

### Core Functionality
- [ ] RFC 6455 core features implemented
- [ ] Client can connect to standard WebSocket servers
- [ ] Server can accept standard WebSocket clients
- [ ] Text and binary messages work
- [ ] Ping/pong mechanism works
- [ ] Close handshake works

### API Consistency
- [ ] API consistent with TCP/UDP (VoidResult, move semantics)
- [ ] Thread-safe operations
- [ ] Zero-copy message handling with std::move

### Infrastructure Integration
- [ ] Pipeline integration for compression/encryption
- [ ] Logger integration (NETWORK_LOG_* macros)
- [ ] ws_session_manager implementation (mirrors session_manager pattern)
- [ ] Connection limits and backpressure via ws_session_manager
- [ ] Monitoring metrics compatible with TCP/UDP

### Testing & Quality
- [ ] Comprehensive tests (>80% coverage)
- [ ] Unit tests passing
- [ ] Integration tests passing
- [ ] Performance targets met
- [ ] Code review passed

### Documentation
- [ ] WEBSOCKET_SUPPORT.md complete
- [ ] network_system.h updated with WebSocket headers
- [ ] README.md updated with WebSocket section
- [ ] API documentation (Doxygen) complete
- [ ] Examples working (echo, chat, broadcast)

### Build System
- [ ] vcpkg.json updated with OpenSSL
- [ ] NetworkSystemDependencies.cmake updated (find_openssl_library)
- [ ] src/CMakeLists.txt updated (add WebSocket sources, link OpenSSL)
- [ ] samples/CMakeLists.txt updated (add WebSocket samples)
- [ ] tests/CMakeLists.txt updated (add unit tests only)
- [ ] integration_tests/CMakeLists.txt updated (add integration tests)

---

## 📞 Questions & Decisions

### Open Questions
1. Should we support WebSocket subprotocols in initial version?
   - **Decision**: No, add in future version

2. Should we implement compression extension initially?
   - **Decision**: No, add in Phase 2 after core is stable

3. Should we support WSS (secure WebSocket) initially?
   - **Decision**: No, but design API to support it later

4. Buffer size for frame reception?
   - **Decision**: 8KB initial buffer, dynamic expansion

5. Max concurrent connections for server?
   - **Decision**: 1000 default, configurable via ws_session_manager

6. How to handle session management given `session_manager` is TCP-specific?
   - **Decision**: Create `ws_session_manager` that mirrors `session_manager` design
   - **Rationale**:
     - `messaging_session` is not polymorphic (no virtual functions)
     - `ws_connection` is a different type than `messaging_session`
     - Creating a separate manager maintains type safety
     - Both share the same `session_config` for consistent configuration
     - Pattern is consistent: each protocol has its own session type and manager

---

## 🤝 Review & Approval

- [ ] Architecture review
- [ ] API design review
- [ ] Implementation plan approval
- [ ] Timeline approval
- [ ] Resource allocation

---

## Phase 7: Network Load Testing & Real Baseline Metrics

### Overview
**Duration**: 11-15 days (2-3 weeks)
**Priority**: High
**Status**: Planning Complete ✅
**Dependencies**: Phase 5 (WebSocket API Complete)

**Objective**: Replace synthetic CPU-only benchmarks with real network performance metrics for TCP, UDP, and WebSocket protocols. Collect throughput, latency (P50/P95/P99), and memory footprint data to populate BASELINE.md with verified measurements.

### Current State Analysis

#### What Exists ✅
- Performance test infrastructure (`integration_tests/performance/network_performance_test.cpp`)
- Test helpers (Statistics, timing, port management)
- Benchmark workflow (`.github/workflows/benchmarks.yml`)
- TCP partial coverage (latency, throughput tests exist)

#### What's Missing ⚠️
- Memory profiling utilities (RSS, heap tracking)
- UDP load tests (no protocol-specific tests)
- WebSocket load tests (implementation complete but no performance tests)
- Result persistence (JSON/CSV output automation)
- BASELINE.md auto-update pipeline

### Phase 7.1: Infrastructure Enhancement (3-4 days)

**File**: `integration_tests/framework/memory_profiler.h` (new)

**Memory Profiling API**:
```cpp
namespace network_system::integration_tests {

struct MemoryMetrics {
    size_t rss_bytes;       // Resident Set Size
    size_t heap_bytes;      // Heap allocation
    size_t vm_bytes;        // Virtual memory
};

class MemoryProfiler {
public:
    // Take memory snapshot
    auto snapshot() -> MemoryMetrics;

    // Calculate delta between snapshots
    auto delta(const MemoryMetrics& before, const MemoryMetrics& after) -> MemoryMetrics;

private:
    // Platform-specific implementations
    auto snapshot_linux() -> MemoryMetrics;   // Parse /proc/self/status
    auto snapshot_macos() -> MemoryMetrics;   // Use task_info()
    auto snapshot_windows() -> MemoryMetrics; // Use GetProcessMemoryInfo()
};

} // namespace
```

**File**: `integration_tests/framework/result_writer.h` (new)

**Result Persistence API**:
```cpp
namespace network_system::integration_tests {

struct PerformanceResult {
    std::string test_name;
    std::string protocol;      // "tcp", "udp", "websocket"
    Statistics latency_ms;     // P50, P95, P99, mean, stddev
    double throughput_msg_s;
    double bandwidth_mbps;
    MemoryMetrics memory;
    std::string timestamp;
    std::string platform;
    std::string compiler;
};

class ResultWriter {
public:
    // Write results to JSON
    void write_json(const std::string& path, const std::vector<PerformanceResult>& results);

    // Write results to CSV for spreadsheet analysis
    void write_csv(const std::string& path, const std::vector<PerformanceResult>& results);
};

} // namespace
```

**Deliverables**:
- [ ] `integration_tests/framework/memory_profiler.h` + `.cpp`
- [ ] `integration_tests/framework/result_writer.h` + `.cpp`
- [ ] Unit tests for memory profiler
- [ ] Unit tests for result writer

### Phase 7.2: Protocol Load Tests (5-6 days)

#### 7.2.1 TCP Load Tests Enhancement

**File**: `integration_tests/performance/tcp_load_test.cpp` (new)

**Tests to Implement**:
```cpp
class TCPLoadTest : public NetworkSystemFixture {
protected:
    MemoryProfiler memory_profiler_;
    ResultWriter result_writer_;
};

TEST_F(TCPLoadTest, SustainedThroughput_1K_Messages) {
    // Test: Send 10,000 x 1KB messages
    // Measure: throughput (msg/s), latency distribution, memory growth
    // Record: P50/P95/P99 latency, peak memory, average throughput
}

TEST_F(TCPLoadTest, LargePayload_Streaming) {
    // Test: Send 100 x 1MB messages
    // Measure: bandwidth (MB/s), memory efficiency, sustained throughput
}

TEST_F(TCPLoadTest, ConcurrentConnections_100) {
    // Test: 100 concurrent clients sending messages
    // Measure: connection rate, aggregate throughput, memory per connection
    // Record: total RSS, per-connection overhead
}

TEST_F(TCPLoadTest, SmallMessage_Latency_Distribution) {
    // Test: Send 1,000 x 64B messages
    // Measure: Fine-grained latency histogram
    // Record: P50, P95, P99, P99.9 latencies
}
```

#### 7.2.2 UDP Load Tests (New)

**File**: `integration_tests/performance/udp_load_test.cpp` (new)

**Prerequisites**: Verify `messaging_udp_server` and `messaging_udp_client` API exists

**Tests to Implement**:
```cpp
class UDPLoadTest : public NetworkSystemFixture {
    // Similar setup to TCPLoadTest
};

TEST_F(UDPLoadTest, SmallPacket_Throughput) {
    // Test: Send 10,000 x 64B packets
    // Measure: packet rate, packet loss percentage, latency
}

TEST_F(UDPLoadTest, BurstTraffic) {
    // Test: Send bursts of 1,000 packets with gaps
    // Measure: handling under load spikes, packet loss during bursts
}

TEST_F(UDPLoadTest, PacketLoss_Measurement) {
    // Test: Send 10,000 packets with sequence numbers
    // Measure: actual packet loss rate, out-of-order delivery
}
```

#### 7.2.3 WebSocket Load Tests (New)

**File**: `integration_tests/performance/websocket_load_test.cpp` (new)

**Dependencies**: `messaging_ws_server`, `messaging_ws_client` ✅ (Phase 5 complete)

**Tests to Implement**:
```cpp
class WebSocketLoadTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Find available port
        test_port_ = test_helpers::find_available_port();

        // Initialize WebSocket server
        server_ = std::make_shared<messaging_ws_server>("ws_test_server");

        // Initialize WebSocket client
        client_ = std::make_shared<messaging_ws_client>("ws_test_client");
    }

    std::shared_ptr<messaging_ws_server> server_;
    std::shared_ptr<messaging_ws_client> client_;
    MemoryProfiler memory_profiler_;
    ResultWriter result_writer_;
    uint16_t test_port_;
};

TEST_F(WebSocketLoadTest, Text_Message_Throughput) {
    // Test: Send 10,000 text messages (64B, 256B, 1KB, 8KB)
    // Measure: throughput (msg/s), latency (P50/P95/P99)
    // Record: Frame overhead, handshake time
}

TEST_F(WebSocketLoadTest, Binary_Message_Throughput) {
    // Test: Send 10,000 binary messages
    // Measure: throughput, latency
    // Compare: Text vs. binary performance
}

TEST_F(WebSocketLoadTest, Fragmented_Messages) {
    // Test: Send large messages requiring fragmentation
    // Measure: reassembly performance, memory usage during reassembly
}

TEST_F(WebSocketLoadTest, Concurrent_Connections_50) {
    // Test: 50 WebSocket clients
    // Measure: connection rate, broadcast throughput, total memory
    // Record: Per-connection memory overhead
}

TEST_F(WebSocketLoadTest, Ping_Pong_Latency) {
    // Test: Measure ping/pong round-trip time
    // Measure: Keepalive overhead, response latency
}

TEST_F(WebSocketLoadTest, Handshake_Performance) {
    // Test: 100 connection handshakes
    // Measure: Handshake latency (P50/P95/P99)
    // Record: Sec-WebSocket-Accept computation time
}
```

**Deliverables**:
- [ ] `integration_tests/performance/tcp_load_test.cpp`
- [ ] `integration_tests/performance/udp_load_test.cpp`
- [ ] `integration_tests/performance/websocket_load_test.cpp`
- [ ] Update `integration_tests/CMakeLists.txt` to build load tests
- [ ] JSON result output per protocol

### Phase 7.3: CI/CD Automation (2-3 days)

#### 7.3.1 GitHub Workflow

**File**: `.github/workflows/network-load-tests.yml` (new)

**Workflow Configuration**:
```yaml
name: Network Load Tests

on:
  # Weekly scheduled runs for baseline updates
  schedule:
    - cron: '0 0 * * 0'  # Every Sunday at midnight UTC

  # Manual trigger with options
  workflow_dispatch:
    inputs:
      protocols:
        description: 'Protocols to test (comma-separated: tcp,udp,websocket)'
        required: true
        default: 'tcp,udp,websocket'
      save_baseline:
        description: 'Save results as new baseline'
        required: false
        default: 'false'

jobs:
  load-tests:
    name: Run Network Load Tests
    runs-on: ubuntu-22.04
    timeout-minutes: 60

    steps:
      - name: Checkout code
        uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake ninja-build clang \
            libgtest-dev libfmt-dev libasio-dev libssl-dev \
            python3 python3-pip
          pip3 install scipy

      - name: Configure CMake
        run: |
          cmake -B build -S . -GNinja \
            -DCMAKE_BUILD_TYPE=Release \
            -DNETWORK_BUILD_INTEGRATION_TESTS=ON \
            -DBUILD_WITH_COMMON_SYSTEM=OFF \
            -DBUILD_WITH_LOGGER_SYSTEM=OFF

      - name: Build tests
        run: cmake --build build --target tcp_load_test udp_load_test websocket_load_test -j

      - name: Run TCP load tests
        run: |
          ./build/bin/integration_tests/tcp_load_test \
            --gtest_output=json:tcp_results.json

      - name: Run UDP load tests
        if: contains(github.event.inputs.protocols, 'udp')
        run: |
          ./build/bin/integration_tests/udp_load_test \
            --gtest_output=json:udp_results.json

      - name: Run WebSocket load tests
        if: contains(github.event.inputs.protocols, 'websocket')
        run: |
          ./build/bin/integration_tests/websocket_load_test \
            --gtest_output=json:websocket_results.json

      - name: Collect metrics
        run: |
          python3 scripts/collect_metrics.py \
            --tcp tcp_results.json \
            --udp udp_results.json \
            --websocket websocket_results.json \
            --output performance_baseline.json

      - name: Update BASELINE.md
        run: |
          python3 scripts/update_baseline.py \
            --results performance_baseline.json \
            --baseline BASELINE.md

      - name: Upload results
        uses: actions/upload-artifact@v4
        with:
          name: performance-results-${{ github.run_number }}
          path: |
            tcp_results.json
            udp_results.json
            websocket_results.json
            performance_baseline.json
          retention-days: 90

      - name: Create baseline update PR
        if: github.event.inputs.save_baseline == 'true'
        run: |
          git config user.name "github-actions[bot]"
          git config user.email "github-actions[bot]@users.noreply.github.com"
          git checkout -b baseline-update-${{ github.run_number }}
          git add BASELINE.md
          git commit -m "chore(baseline): update performance metrics from CI run #${{ github.run_number }}"
          git push origin baseline-update-${{ github.run_number }}
          gh pr create --title "Update Performance Baseline" \
            --body "Automated baseline update from network load tests run #${{ github.run_number }}"
```

#### 7.3.2 Metric Collection Script

**File**: `scripts/collect_metrics.py` (new)

**Functionality**:
- Parse GTest JSON output files
- Extract performance metrics from test stdout
- Aggregate into structured JSON format
- Calculate statistics (mean, stddev, percentiles)

**Output Format** (`performance_baseline.json`):
```json
{
  "timestamp": "2025-10-26T12:00:00Z",
  "platform": "ubuntu-22.04",
  "compiler": "gcc-11",
  "protocols": {
    "tcp": {
      "throughput": {
        "64B": { "msg_s": 450000, "stddev": 12000 },
        "1KB": { "msg_s": 180000, "stddev": 8000 },
        "8KB": { "msg_s": 35000, "stddev": 2000 }
      },
      "latency_ms": {
        "p50": 0.08,
        "p95": 0.35,
        "p99": 1.2
      },
      "memory": {
        "idle_mb": 8.5,
        "50_connections_mb": 42.3,
        "per_connection_kb": 676
      }
    },
    "websocket": {
      "throughput": {
        "text_64B": { "msg_s": 380000, "stddev": 11000 },
        "text_1KB": { "msg_s": 165000, "stddev": 7500 },
        "binary_8KB": { "msg_s": 32000, "stddev": 1800 }
      },
      "latency_ms": {
        "p50": 0.095,
        "p95": 0.42,
        "p99": 1.4
      },
      "memory": {
        "idle_mb": 9.2,
        "50_connections_mb": 48.1,
        "per_connection_kb": 794
      },
      "frame_overhead_bytes": {
        "min": 6,
        "max": 14
      }
    }
  }
}
```

#### 7.3.3 BASELINE Update Script

**File**: `scripts/update_baseline.py` (new)

**Functionality**:
- Read current BASELINE.md
- Parse `performance_baseline.json`
- Update metrics sections while preserving document structure
- Add timestamp and commit hash to updated sections

**Deliverables**:
- [ ] `.github/workflows/network-load-tests.yml`
- [ ] `scripts/collect_metrics.py`
- [ ] `scripts/update_baseline.py`
- [ ] README update with workflow badge

### Phase 7.4: Documentation (1-2 days)

#### Update BASELINE.md

Add new sections for real network metrics:

**Section**: TCP Real Network Performance
```markdown
## TCP Real Network Performance

**Last Updated**: 2025-11-15 (automated via CI)
**Platform**: Ubuntu 22.04, Intel i7-12700K
**Compiler**: GCC 11.4 with -O3

### Throughput

| Message Size | Throughput | P50 Latency | P95 Latency | P99 Latency |
|--------------|-----------|-------------|-------------|-------------|
| 64 bytes     | 450K msg/s | 80 µs      | 350 µs      | 1.2 ms     |
| 1 KB         | 180K msg/s | 120 µs     | 480 µs      | 2.1 ms     |
| 8 KB         | 35K msg/s  | 520 µs     | 1.8 ms      | 5.2 ms     |

### Bandwidth

| Payload Size | Bandwidth (MB/s) | Utilization |
|--------------|------------------|-------------|
| 1 KB         | 175              | ~18% of GigE |
| 8 KB         | 273              | ~27% of GigE |
| 64 KB        | 420              | ~42% of GigE |
```

**Section**: WebSocket Real Network Performance
```markdown
## WebSocket Real Network Performance

**Last Updated**: 2025-11-15
**Platform**: Ubuntu 22.04, Intel i7-12700K
**Compiler**: GCC 11.4 with -O3

### Throughput (Text Messages)

| Message Size | Throughput | P50 Latency | P95 Latency | Frame Overhead |
|--------------|-----------|-------------|-------------|----------------|
| 64 bytes     | 380K msg/s | 95 µs      | 420 µs      | 6-14 bytes    |
| 1 KB         | 165K msg/s | 145 µs     | 550 µs      | 6-14 bytes    |
| 8 KB         | 32K msg/s  | 580 µs     | 2.1 ms      | 6-14 bytes    |

### Throughput (Binary Messages)

| Message Size | Throughput | P50 Latency | P95 Latency |
|--------------|-----------|-------------|-------------|
| 64 bytes     | 385K msg/s | 92 µs      | 410 µs      |
| 1 KB         | 168K msg/s | 140 µs     | 540 µs      |
| 8 KB         | 33K msg/s  | 570 µs     | 2.0 ms      |

### Connection Metrics

| Metric | Value |
|--------|-------|
| Handshake Latency (P50) | 1.2 ms |
| Handshake Latency (P99) | 4.5 ms |
| Max Concurrent Connections | 1000+ |
| Connection Rate | 250 conn/s |
```

**Section**: Memory Footprint Analysis
```markdown
## Memory Footprint Analysis

**Methodology**: RSS (Resident Set Size) measured via `/proc/self/status` on Linux

### TCP Memory Usage

| Scenario | RSS (MB) | Heap (MB) | Per-Connection (KB) |
|----------|---------|-----------|---------------------|
| Idle Server | 8.5 | 4.2 | N/A |
| 10 Connections | 14.2 | 8.7 | 570 |
| 50 Connections | 42.3 | 28.7 | 676 |
| 100 Connections | 78.1 | 54.3 | 696 |

### WebSocket Memory Usage

| Scenario | RSS (MB) | Heap (MB) | Per-Connection (KB) |
|----------|---------|-----------|---------------------|
| Idle Server | 9.2 | 5.1 | N/A |
| 10 Connections | 16.8 | 10.2 | 760 |
| 50 Connections | 48.1 | 32.4 | 794 |
| 100 Connections | 88.6 | 62.7 | 810 |

**Note**: Per-connection overhead includes session state, send/receive buffers, and protocol overhead.
```

#### Create Load Test Guide

**File**: `docs/LOAD_TEST_GUIDE.md` (new)

**Contents**:
- Running load tests locally
- Interpreting performance results
- Adding new load tests
- Troubleshooting flaky tests
- Understanding baseline update process

**Deliverables**:
- [ ] Update BASELINE.md with real network sections
- [ ] Create `docs/LOAD_TEST_GUIDE.md`
- [ ] Update README.md to reference load test guide
- [ ] Add workflow badge for load tests

### Success Criteria

#### Metrics Captured

**TCP**:
- [x] Throughput: msg/s for 64B, 1KB, 8KB payloads
- [x] Latency: P50, P95, P99 in microseconds
- [x] Memory: RSS, heap, per-connection overhead
- [x] Concurrency: 50-100 simultaneous connections

**UDP**:
- [ ] Throughput: packets/s for 64B, 512B, 1KB
- [ ] Packet loss rate under load
- [ ] Latency: one-way and round-trip
- [ ] Memory: per-socket overhead

**WebSocket**:
- [ ] Throughput: msg/s for text and binary messages
- [ ] Latency: including handshake overhead
- [ ] Frame overhead: bytes per message size
- [ ] Memory: per-connection with session management
- [ ] Broadcast performance: 1-to-N distribution

#### Automation Goals
- [ ] Tests run automatically (weekly + on-demand)
- [ ] Results auto-collected to JSON
- [ ] BASELINE.md updates via script (PR review required)
- [ ] Regression detection (future: compare vs. previous baseline)

### Resource Requirements

**Development**:
- **Time**: 80-100 hours
- **Calendar**: 2-3 weeks
- **Team**: Network system developers

**CI Infrastructure**:
- **Runner**: ubuntu-22.04 (consistent environment)
- **Execution Time**: ~45 minutes per full run
- **Frequency**: Weekly scheduled + manual triggers
- **Storage**: ~10MB per baseline (retained 90 days)

**Dependencies**:
- ✅ GTest (integrated)
- ✅ messaging_ws_server/client (Phase 5 complete)
- ⚠️ Python 3.8+ (for metric collection scripts)
- ⚠️ scipy (for statistical analysis)

### Risk Mitigation

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| CI environment variance | High | Medium | Statistical aggregation (10 runs, median) |
| Port conflicts | Medium | Low | `find_available_port()` already used |
| Memory profiling overhead | Low | Medium | Snapshot before/after test blocks |
| UDP API unavailable | Low | High | Verify API exists before Phase 7.2.2 |
| Test flakiness | High | Medium | Retry logic (3 attempts), skip in CI if needed |

### Timeline

| Phase | Target Date | Status |
|-------|------------|--------|
| Phase 7.1 (Infrastructure) | 2025-11-01 | Planning |
| Phase 7.2 (Protocol Tests) | 2025-11-08 | Planning |
| Phase 7.3 (CI Automation) | 2025-11-12 | Planning |
| Phase 7.4 (Documentation) | 2025-11-15 | Planning |
| **Phase 7 Complete** | **2025-11-15** | Planning |

### Deliverables Checklist

**Phase 7.1**:
- [ ] `integration_tests/framework/memory_profiler.h` + `.cpp`
- [ ] `integration_tests/framework/result_writer.h` + `.cpp`
- [ ] Unit tests for profiler and writer

**Phase 7.2**:
- [ ] `integration_tests/performance/tcp_load_test.cpp`
- [ ] `integration_tests/performance/udp_load_test.cpp`
- [ ] `integration_tests/performance/websocket_load_test.cpp`
- [ ] CMakeLists.txt updates for load tests

**Phase 7.3**:
- [ ] `.github/workflows/network-load-tests.yml`
- [ ] `scripts/collect_metrics.py`
- [ ] `scripts/update_baseline.py`

**Phase 7.4**:
- [ ] BASELINE.md real network sections
- [ ] `docs/LOAD_TEST_GUIDE.md`
- [ ] README.md updates with load test info

### Next Steps

1. **Approve Phase 7 plan** (review this section)
2. **Create GitHub issues** for each sub-phase
3. **Assign to team members**
4. **Begin Phase 7.1 implementation** (infrastructure)

---

**Last Updated**: 2025-10-26
**Next Review**: Phase 7.1 kickoff (2025-11-01)
