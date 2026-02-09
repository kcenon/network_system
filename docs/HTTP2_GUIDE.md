# HTTP/2 Advanced Features Guide

> **Language:** **English**

A comprehensive guide to the HTTP/2 protocol implementation in the `network_system` library, covering HPACK header compression, server-side streaming, client API, frame internals, flow control, and performance characteristics.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [HTTP/2 Client](#http2-client)
- [HTTP/2 Server](#http2-server)
- [Server-Side Streaming](#server-side-streaming)
- [Client-Side Streaming](#client-side-streaming)
- [HPACK Header Compression](#hpack-header-compression)
- [Frame Layer Internals](#frame-layer-internals)
- [HTTP/2 Request Structure](#http2-request-structure)
- [Flow Control](#flow-control)
- [Settings and Tuning](#settings-and-tuning)
- [Error Handling](#error-handling)
- [Performance Characteristics](#performance-characteristics)

## Architecture Overview

### Protocol Stack

```
┌─────────────────────────────────────────────────┐
│               User Application                  │
├─────────────────────────────────────────────────┤
│   http2_client            http2_server          │  ← High-Level API
│                  http2_server_stream             │
├─────────────────────────────────────────────────┤
│        hpack_encoder / hpack_decoder            │  ← HPACK Compression
│     static_table    dynamic_table    huffman     │     (RFC 7541)
├─────────────────────────────────────────────────┤
│   frame: data | headers | settings | goaway     │  ← Frame Layer
│          rst_stream | ping | window_update      │     (RFC 7540)
├─────────────────────────────────────────────────┤
│            TLS 1.3 (ALPN "h2")                  │  ← Transport Security
│            asio::ssl::stream                     │
├─────────────────────────────────────────────────┤
│              TCP (asio)                          │  ← Transport
└─────────────────────────────────────────────────┘
```

### Namespace

All HTTP/2 types reside in the `kcenon::network::protocols::http2` namespace. The public umbrella header re-exports into `network_http2`:

```cpp
#include <network_http2/http2.h>

// Types available via either namespace:
using namespace kcenon::network::protocols::http2;
// or
using namespace network_http2;
```

### Key Design Decisions

| Aspect | Choice | Rationale |
|--------|--------|-----------|
| I/O model | Asio-based async | Non-blocking, high-throughput |
| TLS | Asio SSL with ALPN "h2" | Standard HTTP/2 negotiation |
| Shared ownership | `std::enable_shared_from_this` | Safe async callback lifetimes |
| Copyability | Non-copyable, non-movable | Contains atomics and mutexes |
| Error handling | `Result<T>` / `VoidResult` | Type-safe, no exceptions |

---

## HTTP/2 Client

**Header:** `src/internal/protocols/http2/http2_client.h`

The `http2_client` class provides a full-featured HTTP/2 client with TLS support, stream multiplexing, and HPACK compression.

### Construction and Connection

```cpp
#include <network_http2/http2.h>
using namespace kcenon::network::protocols::http2;

// Create client (must be shared_ptr for async safety)
auto client = std::make_shared<http2_client>("my-client");

// Connect to server (TLS with ALPN "h2")
auto result = client->connect("api.example.com", 443);
if (!result) {
    std::cerr << "Connection failed: " << result.error().message << "\n";
    return;
}

// Check connection status
if (client->is_connected()) {
    // Ready for requests
}

// Graceful disconnect (sends GOAWAY frame)
client->disconnect();
```

### Request Methods

All request methods return `Result<http2_response>`:

```cpp
// GET request
auto response = client->get("/api/users");
if (response) {
    std::cout << "Status: " << response->status_code << "\n";
    std::cout << "Body: " << response->get_body_string() << "\n";

    // Access individual headers
    auto content_type = response->get_header("content-type");
    if (content_type) {
        std::cout << "Content-Type: " << *content_type << "\n";
    }
}

// POST with string body
std::vector<http_header> headers = {
    {"content-type", "application/json"},
    {"accept", "application/json"}
};
auto post_result = client->post("/api/users", R"({"name": "Alice"})", headers);

// POST with binary body
std::vector<uint8_t> binary_data = {0x01, 0x02, 0x03};
auto bin_result = client->post("/api/upload", binary_data, {
    {"content-type", "application/octet-stream"}
});

// PUT request
auto put_result = client->put("/api/users/1", R"({"name": "Bob"})", {
    {"content-type", "application/json"}
});

// DELETE request
auto del_result = client->del("/api/users/1");
```

### Client API Reference

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `connect` | `(host, port=443)` | `VoidResult` | Establish TLS connection with ALPN "h2" |
| `disconnect` | `()` | `VoidResult` | Send GOAWAY and close connection |
| `is_connected` | `() const` | `bool` | Check connection status |
| `get` | `(path, headers={})` | `Result<http2_response>` | HTTP GET request |
| `post` | `(path, body_str, headers={})` | `Result<http2_response>` | POST with string body |
| `post` | `(path, body_bytes, headers={})` | `Result<http2_response>` | POST with binary body |
| `put` | `(path, body, headers={})` | `Result<http2_response>` | HTTP PUT request |
| `del` | `(path, headers={})` | `Result<http2_response>` | HTTP DELETE request |
| `set_timeout` | `(milliseconds)` | `void` | Set request timeout (default: 30s) |
| `get_timeout` | `() const` | `milliseconds` | Get current timeout |
| `get_settings` | `() const` | `http2_settings` | Get local HTTP/2 settings |
| `set_settings` | `(settings)` | `void` | Update local settings |

### http2_response Structure

| Field | Type | Description |
|-------|------|-------------|
| `status_code` | `int` | HTTP status code (e.g., 200, 404, 500) |
| `headers` | `std::vector<http_header>` | Response headers |
| `body` | `std::vector<uint8_t>` | Response body bytes |

| Method | Returns | Description |
|--------|---------|-------------|
| `get_header(name)` | `std::optional<std::string>` | Case-insensitive header lookup |
| `get_body_string()` | `std::string` | Body as UTF-8 string |

---

## HTTP/2 Server

**Header:** `src/internal/protocols/http2/http2_server.h`

The `http2_server` class provides an HTTP/2 server supporting both TLS (h2) and cleartext (h2c) modes.

### Basic Server Setup

```cpp
#include <network_http2/http2.h>
using namespace kcenon::network::protocols::http2;

auto server = std::make_shared<http2_server>("my-server");

// Set request handler
server->set_request_handler([](http2_server_stream& stream,
                               const http2_request& request) {
    if (request.method == "GET" && request.path == "/api/health") {
        stream.send_headers(200, {{"content-type", "application/json"}});
        stream.send_data(R"({"status": "ok"})", true);
    } else if (request.method == "POST" && request.path == "/api/data") {
        auto body = request.get_body_string();
        stream.send_headers(201, {{"content-type", "text/plain"}});
        stream.send_data("Created: " + std::string(body.begin(), body.end()), true);
    } else {
        stream.send_headers(404, {});
        stream.send_data("Not Found", true);
    }
});

// Set error handler
server->set_error_handler([](const std::string& error) {
    std::cerr << "Server error: " << error << "\n";
});

// Start with TLS (recommended for production)
tls_config config;
config.cert_file = "/path/to/cert.pem";
config.key_file  = "/path/to/key.pem";
config.ca_file   = "/path/to/ca.pem";      // Optional: CA for client certs
config.verify_client = false;               // Optional: require client certs

auto start_result = server->start_tls(8443, config);
if (!start_result) {
    std::cerr << "Failed to start: " << start_result.error().message << "\n";
    return;
}

// Wait for shutdown signal
server->wait();
server->stop();
```

### Server Start Modes

```cpp
// Mode 1: TLS (h2) - Recommended for production
tls_config config;
config.cert_file = "server.pem";
config.key_file  = "server-key.pem";
server->start_tls(8443, config);

// Mode 2: Cleartext (h2c) - Development only
server->start(8080);  // Note: h2c is not widely supported
```

### tls_config Structure

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `cert_file` | `std::string` | `""` | Path to PEM certificate file |
| `key_file` | `std::string` | `""` | Path to PEM private key file |
| `ca_file` | `std::string` | `""` | Path to CA certificate (optional) |
| `verify_client` | `bool` | `false` | Require client certificate verification |

### Server API Reference

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `start` | `(port)` | `VoidResult` | Start h2c server (cleartext) |
| `start_tls` | `(port, tls_config)` | `VoidResult` | Start h2 server (TLS) |
| `stop` | `()` | `VoidResult` | Stop server, send GOAWAY to all |
| `is_running` | `() const` | `bool` | Check if server is accepting connections |
| `wait` | `()` | `void` | Block until stop() is called |
| `set_request_handler` | `(handler)` | `void` | Set request callback |
| `set_error_handler` | `(handler)` | `void` | Set error callback |
| `set_settings` | `(settings)` | `void` | Configure HTTP/2 settings |
| `get_settings` | `() const` | `http2_settings` | Get current settings |
| `active_connections` | `() const` | `size_t` | Number of live connections |
| `active_streams` | `() const` | `size_t` | Total streams across connections |
| `server_id` | `() const` | `string_view` | Server identifier |

### Handler Function Types

| Type | Signature | Purpose |
|------|-----------|---------|
| `request_handler_t` | `void(http2_server_stream&, const http2_request&)` | Handle incoming request |
| `error_handler_t` | `void(const std::string&)` | Handle server errors |

---

## Server-Side Streaming

**Header:** `src/internal/protocols/http2/http2_server_stream.h`

The `http2_server_stream` class enables both simple request-response and chunked streaming responses.

### Simple Response (Non-Streaming)

```cpp
// Single response: headers + data in one shot
server->set_request_handler([](http2_server_stream& stream,
                               const http2_request& request) {
    // Headers with end_stream=false (body follows)
    stream.send_headers(200, {
        {"content-type", "application/json"},
        {"cache-control", "no-cache"}
    });

    // Data with end_stream=true (completes the response)
    stream.send_data(R"({"result": "success"})", true);
});
```

### Chunked Streaming Response

```cpp
// Streaming: start_response → write → write → ... → end_response
server->set_request_handler([](http2_server_stream& stream,
                               const http2_request& request) {
    if (request.path == "/api/stream") {
        // Start streaming (sends HEADERS without END_STREAM)
        auto result = stream.start_response(200, {
            {"content-type", "text/event-stream"},
            {"cache-control", "no-cache"}
        });
        if (!result) return;

        // Send multiple data chunks
        for (int i = 0; i < 10; ++i) {
            std::string chunk = "data: event " + std::to_string(i) + "\n\n";
            std::vector<uint8_t> data(chunk.begin(), chunk.end());

            auto write_result = stream.write(data);
            if (!write_result) break;  // Client may have disconnected
        }

        // End the response (sends empty DATA with END_STREAM)
        stream.end_response();
    }
});
```

### Stream Reset

```cpp
// Abort a stream with RST_STREAM
stream.reset();  // Default: error_code::cancel (0x8)

// Reset with specific error code
stream.reset(static_cast<uint32_t>(error_code::internal_error));
```

### http2_server_stream API Reference

#### Response Methods

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `send_headers` | `(status_code, headers, end_stream=false)` | `VoidResult` | Send response headers |
| `send_data` | `(vector<uint8_t>, end_stream=false)` | `VoidResult` | Send binary data |
| `send_data` | `(string_view, end_stream=false)` | `VoidResult` | Send string data |
| `start_response` | `(status_code, headers)` | `VoidResult` | Begin streaming response |
| `write` | `(vector<uint8_t>)` | `VoidResult` | Write streaming chunk |
| `end_response` | `()` | `VoidResult` | End streaming response |
| `reset` | `(err_code=cancel)` | `VoidResult` | Abort stream with RST_STREAM |

#### Query Methods

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `stream_id` | `() const` | `uint32_t` | Stream identifier |
| `method` | `() const` | `string_view` | HTTP method from request |
| `path` | `() const` | `string_view` | Request path |
| `headers` | `() const` | `const vector<http_header>&` | Request headers |
| `request` | `() const` | `const http2_request&` | Full request object |
| `is_open` | `() const` | `bool` | Can still send data |
| `headers_sent` | `() const` | `bool` | Headers already sent |
| `state` | `() const` | `stream_state` | Current stream state |

#### Flow Control Methods

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `update_window` | `(increment)` | `void` | Update flow control window |
| `window_size` | `() const` | `int32_t` | Available send window |

### Thread Safety

All `http2_server_stream` public methods are thread-safe:
- State transitions protected by internal `std::mutex`
- Non-copyable and non-movable (contains mutex)
- Default initial window size: **65,535 bytes** (RFC 7540 default)
- Default max frame size: **16,384 bytes**

---

## Client-Side Streaming

The `http2_client` supports streaming requests with callback-based data reception.

### Starting a Stream

```cpp
auto client = std::make_shared<http2_client>("stream-client");
client->connect("api.example.com", 443);

// Start a streaming POST request
auto stream_result = client->start_stream(
    "/api/upload",
    {{"content-type", "application/octet-stream"}},
    // on_data: called for each received DATA frame
    [](std::vector<uint8_t> data) {
        std::cout << "Received: " << data.size() << " bytes\n";
    },
    // on_headers: called when response headers arrive
    [](std::vector<http_header> headers) {
        for (const auto& h : headers) {
            std::cout << h.name << ": " << h.value << "\n";
        }
    },
    // on_complete: called when stream ends (with status code)
    [](int status_code) {
        std::cout << "Stream complete. Status: " << status_code << "\n";
    }
);

if (!stream_result) {
    std::cerr << "Failed to start stream: " << stream_result.error().message << "\n";
    return;
}

uint32_t stream_id = *stream_result;

// Send data chunks
for (int i = 0; i < 100; ++i) {
    std::vector<uint8_t> chunk(1024, static_cast<uint8_t>(i));
    auto write_result = client->write_stream(stream_id, chunk);
    if (!write_result) break;
}

// Signal end of client data
client->close_stream_writer(stream_id);
```

### Stream Cancellation

```cpp
// Cancel an in-progress stream (sends RST_STREAM)
client->cancel_stream(stream_id);
```

### Client Streaming API Reference

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `start_stream` | `(path, headers, on_data, on_headers, on_complete)` | `Result<uint32_t>` | Start streaming request, returns stream ID |
| `write_stream` | `(stream_id, data, end_stream=false)` | `VoidResult` | Send data chunk to stream |
| `close_stream_writer` | `(stream_id)` | `VoidResult` | Close write side (half-close local) |
| `cancel_stream` | `(stream_id)` | `VoidResult` | Cancel stream with RST_STREAM |

### Stream Callback Types

| Callback | Signature | When Called |
|----------|-----------|------------|
| `on_data` | `void(std::vector<uint8_t>)` | Each received DATA frame chunk |
| `on_headers` | `void(std::vector<http_header>)` | Response headers received |
| `on_complete` | `void(int)` | Stream ends (argument is status code) |

---

## HPACK Header Compression

**Header:** `src/internal/protocols/http2/hpack.h`

HPACK (RFC 7541) compresses HTTP headers to reduce overhead on HTTP/2 connections. The implementation includes static and dynamic tables, Huffman coding, and encoder/decoder classes.

### How HPACK Works

```
Request Headers                    Compressed Bytes
┌─────────────────┐               ┌─────────────┐
│ :method: GET     │  ─encoder──► │ 0x82        │  (indexed: static table #2)
│ :path: /api/data │              │ 0x04 ...    │  (literal with indexing)
│ :scheme: https   │              │ 0x87        │  (indexed: static table #7)
│ accept: app/json │              │ 0x40 ...    │  (literal with indexing)
└─────────────────┘               └─────────────┘
                                     (much smaller)
```

### http_header Structure

The basic unit for HPACK operations:

```cpp
struct http_header {
    std::string name;   // Header name (lowercase for HTTP/2)
    std::string value;  // Header value

    // RFC 7541: each entry has 32-byte overhead for table accounting
    auto size() const -> size_t {
        return name.size() + value.size() + 32;
    }
};
```

### Static Table (RFC 7541 Appendix A)

The static table contains 61 predefined common header fields that never change:

```cpp
// Lookup by index (1-based, indices 1-61)
auto header = static_table::get(2);   // → {:method, GET}
auto header = static_table::get(7);   // → {:scheme, https}

// Find index by name and optional value
size_t idx = static_table::find(":method", "GET");   // → 2
size_t idx = static_table::find(":method", "POST");  // → 3
size_t idx = static_table::find(":method");           // → 2 (name-only match)
size_t idx = static_table::find("x-custom");          // → 0 (not found)

// Table size is always 61
static_assert(static_table::size() == 61);
```

| Static Table Index | Name | Value |
|--------------------|------|-------|
| 1 | `:authority` | (empty) |
| 2 | `:method` | `GET` |
| 3 | `:method` | `POST` |
| 4 | `:path` | `/` |
| 5 | `:path` | `/index.html` |
| 6 | `:scheme` | `http` |
| 7 | `:scheme` | `https` |
| 8 | `:status` | `200` |
| ... | ... | ... |
| 61 | `www-authenticate` | (empty) |

### Dynamic Table

The dynamic table maintains recently used headers, enabling compression of repeated custom headers:

```cpp
// Create with max size (default 4096 bytes per RFC 7541)
dynamic_table table(4096);

// Insert headers (FIFO: newest at index 0)
table.insert("x-request-id", "abc-123");
table.insert("authorization", "Bearer token...");

// Lookup by index (0-based)
auto header = table.get(0);  // → most recently inserted

// Find by name + value
auto idx = table.find("x-request-id", "abc-123");

// Table management
table.set_max_size(8192);           // Resize (may evict entries)
size_t size    = table.current_size();  // Current bytes used
size_t max     = table.max_size();      // Maximum allowed
size_t entries = table.entry_count();   // Number of entries
table.clear();                      // Remove all entries
```

### Dynamic Table API

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| Constructor | `(max_size=4096)` | - | Create table with byte limit |
| `insert` | `(name, value)` | `void` | Add header (evicts old if full) |
| `get` | `(index) const` | `optional<http_header>` | Get by 0-based index |
| `find` | `(name, value="") const` | `optional<size_t>` | Find index by name/value |
| `set_max_size` | `(size)` | `void` | Resize table (may evict) |
| `current_size` | `() const` | `size_t` | Current byte usage |
| `max_size` | `() const` | `size_t` | Maximum byte capacity |
| `entry_count` | `() const` | `size_t` | Number of entries |
| `clear` | `()` | `void` | Remove all entries |

### HPACK Encoder

Encodes header lists into HPACK binary format:

```cpp
// Create encoder (max dynamic table size)
hpack_encoder encoder(4096);

// Encode headers
std::vector<http_header> headers = {
    {":method", "GET"},
    {":path", "/api/users"},
    {":scheme", "https"},
    {":authority", "api.example.com"},
    {"accept", "application/json"},
    {"x-request-id", "req-42"}
};

std::vector<uint8_t> encoded = encoder.encode(headers);
// encoded contains HPACK binary representation

// Manage table
encoder.set_max_table_size(8192);
size_t current = encoder.table_size();
```

### HPACK Decoder

Decodes HPACK binary data back into header lists:

```cpp
// Create decoder (max dynamic table size)
hpack_decoder decoder(4096);

// Decode binary data
std::span<const uint8_t> data = /* received HPACK data */;
auto result = decoder.decode(data);
if (result) {
    for (const auto& header : *result) {
        std::cout << header.name << ": " << header.value << "\n";
    }
} else {
    std::cerr << "Decode error: " << result.error().message << "\n";
}

// Table management
decoder.set_max_table_size(8192);
size_t current = decoder.table_size();
```

### Encoder/Decoder API

| Class | Method | Signature | Returns |
|-------|--------|-----------|---------|
| `hpack_encoder` | Constructor | `(max_table_size=4096)` | - |
| `hpack_encoder` | `encode` | `(vector<http_header>)` | `vector<uint8_t>` |
| `hpack_encoder` | `set_max_table_size` | `(size)` | `void` |
| `hpack_encoder` | `table_size` | `() const` | `size_t` |
| `hpack_decoder` | Constructor | `(max_table_size=4096)` | - |
| `hpack_decoder` | `decode` | `(span<const uint8_t>)` | `Result<vector<http_header>>` |
| `hpack_decoder` | `set_max_table_size` | `(size)` | `void` |
| `hpack_decoder` | `table_size` | `() const` | `size_t` |

### Huffman Coding

HPACK uses the static Huffman code from RFC 7541 Appendix B for string compression:

```cpp
namespace huffman {
    // Encode string to Huffman bytes
    auto encoded = huffman::encode("content-type");

    // Decode Huffman bytes to string
    auto result = huffman::decode(encoded);
    if (result) {
        assert(*result == "content-type");
    }

    // Check encoded size without encoding
    size_t size = huffman::encoded_size("content-type");
}
```

| Function | Signature | Returns | Description |
|----------|-----------|---------|-------------|
| `huffman::encode` | `(string_view)` | `vector<uint8_t>` | Huffman encode string |
| `huffman::decode` | `(span<const uint8_t>)` | `Result<string>` | Huffman decode bytes |
| `huffman::encoded_size` | `(string_view)` | `size_t` | Predicted encoded size |

---

## Frame Layer Internals

**Header:** `src/internal/protocols/http2/frame.h`

HTTP/2 is a binary, frame-based protocol. Every piece of communication is wrapped in a frame with a 9-byte header.

### Frame Header (9 bytes)

```
+-----------------------------------------------+
|                 Length (24)                    |
+---------------+---------------+---------------+
|   Type (8)    |   Flags (8)   |
+-+-------------+---------------+
|R|                 Stream ID (31)              |
+-+---------------------------------------------+
```

```cpp
struct frame_header {
    uint32_t length;     // Payload length (24 bits, max 16384 default)
    frame_type type;     // Frame type enum
    uint8_t flags;       // Frame flags
    uint32_t stream_id;  // Stream identifier (31 bits, MSB reserved)

    static auto parse(std::span<const uint8_t> data) -> Result<frame_header>;
    auto serialize() const -> std::vector<uint8_t>;
};
```

### Frame Types (RFC 7540 Section 6)

| Type | Value | Class | Purpose |
|------|-------|-------|---------|
| `data` | `0x0` | `data_frame` | Application data |
| `headers` | `0x1` | `headers_frame` | Header block (opens stream) |
| `priority` | `0x2` | - | Stream priority (deprecated) |
| `rst_stream` | `0x3` | `rst_stream_frame` | Immediate stream termination |
| `settings` | `0x4` | `settings_frame` | Connection configuration |
| `push_promise` | `0x5` | - | Server push (disabled by default) |
| `ping` | `0x6` | `ping_frame` | Keepalive / RTT measurement |
| `goaway` | `0x7` | `goaway_frame` | Connection shutdown |
| `window_update` | `0x8` | `window_update_frame` | Flow control increment |
| `continuation` | `0x9` | - | Continued header block |

### Frame Flags

```cpp
namespace frame_flags {
    constexpr uint8_t none        = 0x0;
    constexpr uint8_t end_stream  = 0x1;  // DATA, HEADERS: last frame in stream
    constexpr uint8_t ack         = 0x1;  // SETTINGS, PING: acknowledgment
    constexpr uint8_t end_headers = 0x4;  // HEADERS: no CONTINUATION follows
    constexpr uint8_t padded      = 0x8;  // DATA, HEADERS: padding present
    constexpr uint8_t priority    = 0x20; // HEADERS: priority info present
}
```

### Concrete Frame Classes

#### data_frame

Conveys application data on a stream:

```cpp
// Create DATA frame
data_frame df(stream_id, payload_bytes, /*end_stream=*/true);

// Query
df.is_end_stream();  // bool
df.is_padded();      // bool
df.data();           // span<const uint8_t> (without padding)
```

#### headers_frame

Opens a stream and carries compressed headers:

```cpp
// Create HEADERS frame
headers_frame hf(stream_id, hpack_encoded_bytes,
                 /*end_stream=*/false, /*end_headers=*/true);

// Query
hf.is_end_stream();   // bool
hf.is_end_headers();  // bool
hf.header_block();    // span<const uint8_t>
```

#### settings_frame

Exchanges connection configuration:

```cpp
// Create SETTINGS frame
std::vector<setting_parameter> params = {
    {static_cast<uint16_t>(setting_identifier::max_concurrent_streams), 200},
    {static_cast<uint16_t>(setting_identifier::initial_window_size), 131072}
};
settings_frame sf(params);

// Create SETTINGS ACK
settings_frame ack({}, /*ack=*/true);

// Query
sf.settings();  // const vector<setting_parameter>&
sf.is_ack();    // bool
```

#### rst_stream_frame

Immediately terminates a stream:

```cpp
rst_stream_frame rf(stream_id, static_cast<uint32_t>(error_code::cancel));
rf.error_code();  // uint32_t
```

#### ping_frame

Measures round-trip time and checks connection liveness:

```cpp
std::array<uint8_t, 8> data = {1, 2, 3, 4, 5, 6, 7, 8};
ping_frame pf(data);

// PING ACK (echo back same opaque data)
ping_frame ack(pf.opaque_data(), /*ack=*/true);

pf.opaque_data();  // const array<uint8_t, 8>&
pf.is_ack();       // bool
```

#### goaway_frame

Initiates graceful connection shutdown:

```cpp
goaway_frame gf(last_processed_stream_id,
                static_cast<uint32_t>(error_code::no_error),
                debug_data_bytes);

gf.last_stream_id();    // uint32_t
gf.error_code();        // uint32_t
gf.additional_data();   // span<const uint8_t>
```

#### window_update_frame

Adjusts flow control windows:

```cpp
// Connection-level update (stream_id = 0)
window_update_frame conn_wuf(0, 65535);

// Stream-level update
window_update_frame stream_wuf(stream_id, 32768);

stream_wuf.window_size_increment();  // uint32_t
```

### Frame Parsing

```cpp
// Parse any frame from raw bytes
auto result = frame::parse(raw_bytes);
if (result) {
    auto& f = *result;
    switch (f->header().type) {
        case frame_type::data:
            // Handle DATA frame
            break;
        case frame_type::headers:
            // Handle HEADERS frame
            break;
        // ... etc
    }
}
```

---

## HTTP/2 Request Structure

**Header:** `src/internal/protocols/http2/http2_request.h`

The `http2_request` struct represents a parsed HTTP/2 request with pseudo-headers separated from regular headers.

### Pseudo-Headers (RFC 7540 Section 8.1.2.3)

| Pseudo-Header | Field | Required | Description |
|---------------|-------|----------|-------------|
| `:method` | `method` | Yes* | HTTP method (GET, POST, etc.) |
| `:path` | `path` | Yes* | Request path |
| `:scheme` | `scheme` | Yes* | URI scheme (http/https) |
| `:authority` | `authority` | No | Authority (host:port) |

*Required for all methods except CONNECT, which requires only `:authority`.

### http2_request Structure

| Field | Type | Description |
|-------|------|-------------|
| `method` | `std::string` | HTTP method from `:method` |
| `path` | `std::string` | Request path from `:path` |
| `authority` | `std::string` | Authority from `:authority` |
| `scheme` | `std::string` | Scheme from `:scheme` |
| `headers` | `std::vector<http_header>` | Regular (non-pseudo) headers |
| `body` | `std::vector<uint8_t>` | Body from DATA frames |

### Request Methods

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `get_header` | `(name) const` | `optional<string>` | Case-insensitive header lookup |
| `content_type` | `() const` | `optional<string>` | Shorthand for `get_header("content-type")` |
| `content_length` | `() const` | `optional<size_t>` | Parsed content-length value |
| `get_body_string` | `() const` | `string` | Body as UTF-8 string |
| `is_valid` | `() const` | `bool` | Check required pseudo-headers present |
| `from_headers` | `(parsed_headers)` | `http2_request` | Construct from decoded HPACK headers |

### Usage in Request Handler

```cpp
server->set_request_handler([](http2_server_stream& stream,
                               const http2_request& request) {
    // Access pseudo-headers
    std::cout << request.method << " " << request.path << "\n";

    // Access regular headers
    auto auth = request.get_header("authorization");
    if (!auth) {
        stream.send_headers(401, {{"www-authenticate", "Bearer"}});
        stream.send_data("Unauthorized", true);
        return;
    }

    // Access body
    if (request.method == "POST") {
        auto content_type = request.content_type();
        auto body_str = request.get_body_string();
        // Process body...
    }

    // Validation
    if (!request.is_valid()) {
        stream.reset(static_cast<uint32_t>(error_code::protocol_error));
        return;
    }
});
```

---

## Flow Control

HTTP/2 implements flow control at both connection and stream levels to prevent fast senders from overwhelming slow receivers.

### How Flow Control Works

```
Initial Window: 65,535 bytes (per RFC 7540)

Sender                          Receiver
  │                                │
  │── DATA (1000 bytes) ──────►   │  window: 65535 → 64535
  │── DATA (2000 bytes) ──────►   │  window: 64535 → 62535
  │                                │
  │◄── WINDOW_UPDATE (+4096) ──   │  window: 62535 → 66631
  │                                │
  │── DATA (3000 bytes) ──────►   │  window: 66631 → 63631
  │                                │
```

### Connection-Level vs Stream-Level

| Level | Scope | Stream ID | Purpose |
|-------|-------|-----------|---------|
| Connection | Entire connection | `0` | Total data across all streams |
| Stream | Individual stream | Non-zero | Per-stream flow control |

Both levels must have available window for data to be sent.

### Server-Side Flow Control

```cpp
// In server stream
int32_t available = stream.window_size();  // Check available window
stream.update_window(32768);               // Grant more window to client
```

### Client-Side Flow Control

Flow control is managed automatically by the `http2_client`. The `initial_window_size` setting controls the starting window for each stream:

```cpp
http2_settings settings;
settings.initial_window_size = 131072;  // 128 KB per stream
client->set_settings(settings);
```

---

## Settings and Tuning

### http2_settings Structure

| Field | Type | Default | RFC Parameter | Description |
|-------|------|---------|---------------|-------------|
| `header_table_size` | `uint32_t` | `4096` | `SETTINGS_HEADER_TABLE_SIZE` | HPACK dynamic table size (bytes) |
| `enable_push` | `bool` | `false` | `SETTINGS_ENABLE_PUSH` | Server push (disabled) |
| `max_concurrent_streams` | `uint32_t` | `100` | `SETTINGS_MAX_CONCURRENT_STREAMS` | Max parallel streams |
| `initial_window_size` | `uint32_t` | `65535` | `SETTINGS_INITIAL_WINDOW_SIZE` | Initial flow control window |
| `max_frame_size` | `uint32_t` | `16384` | `SETTINGS_MAX_FRAME_SIZE` | Max frame payload (bytes) |
| `max_header_list_size` | `uint32_t` | `8192` | `SETTINGS_MAX_HEADER_LIST_SIZE` | Max uncompressed header size |

### setting_identifier Enum

| Identifier | Value | Description |
|------------|-------|-------------|
| `header_table_size` | `0x1` | HPACK dynamic table maximum |
| `enable_push` | `0x2` | Enable server push |
| `max_concurrent_streams` | `0x3` | Maximum simultaneous streams |
| `initial_window_size` | `0x4` | Initial flow control window |
| `max_frame_size` | `0x5` | Maximum frame payload size |
| `max_header_list_size` | `0x6` | Maximum header list size |

### Tuning for High Throughput

```cpp
http2_settings settings;
settings.max_concurrent_streams = 250;   // More parallel requests
settings.initial_window_size = 1048576;  // 1 MB flow control window
settings.max_frame_size = 65536;         // 64 KB frames (max: 16777215)
settings.header_table_size = 8192;       // Larger HPACK table
settings.max_header_list_size = 16384;   // Allow larger headers

// Apply to server
server->set_settings(settings);

// Apply to client
client->set_settings(settings);
```

### Tuning for Low Latency

```cpp
http2_settings settings;
settings.max_concurrent_streams = 50;    // Fewer streams to reduce contention
settings.initial_window_size = 32768;    // 32 KB - smaller but faster ACKs
settings.max_frame_size = 16384;         // Default: minimizes buffering

server->set_settings(settings);
```

---

## Error Handling

### HTTP/2 Error Codes (RFC 7540 Section 7)

| Error Code | Value | Description |
|------------|-------|-------------|
| `no_error` | `0x0` | Graceful shutdown |
| `protocol_error` | `0x1` | Protocol error detected |
| `internal_error` | `0x2` | Implementation fault |
| `flow_control_error` | `0x3` | Flow-control limits exceeded |
| `settings_timeout` | `0x4` | Settings not acknowledged in time |
| `stream_closed` | `0x5` | Frame received for closed stream |
| `frame_size_error` | `0x6` | Frame size incorrect |
| `refused_stream` | `0x7` | Stream not processed |
| `cancel` | `0x8` | Stream cancelled |
| `compression_error` | `0x9` | HPACK compression state error |
| `connect_error` | `0xa` | TCP connection error for CONNECT |
| `enhance_your_calm` | `0xb` | Peer generating excessive load |
| `inadequate_security` | `0xc` | TLS parameters not acceptable |
| `http_1_1_required` | `0xd` | Use HTTP/1.1 instead |

### Error Handling Patterns

```cpp
// Result-based error handling (client)
auto response = client->get("/api/data");
if (!response) {
    auto& err = response.error();
    std::cerr << "Request failed: " << err.message << "\n";
    // Handle specific error types...
}

// VoidResult error handling (server stream)
auto result = stream.send_data("Hello", true);
if (!result) {
    // Stream may have been reset by client
    std::cerr << "Send failed: " << result.error().message << "\n";
}

// Server error handler
server->set_error_handler([](const std::string& error_message) {
    // Log connection-level errors
    std::cerr << "Server error: " << error_message << "\n";
});
```

---

## Performance Characteristics

### Stream Multiplexing

HTTP/2 multiplexes multiple request/response exchanges over a single TCP connection, eliminating head-of-line blocking at the HTTP layer:

```
Single TCP Connection
┌──────────────────────────────────────┐
│ Stream 1: GET /api/users    ──────►  │
│ Stream 3: POST /api/data   ──────►   │  (interleaved frames)
│ Stream 5: GET /api/config  ──────►   │
│ Stream 1: ◄──── 200 OK              │
│ Stream 5: ◄──── 200 OK              │
│ Stream 3: ◄──── 201 Created         │
└──────────────────────────────────────┘
```

### HPACK Compression Benefits

Header compression is especially effective for:

| Scenario | Typical Savings |
|----------|-----------------|
| Repeated requests (same headers) | 85-95% after first request |
| Static table hits (common headers) | Single byte per header |
| Custom headers (added to dynamic table) | Indexed after first use |
| Huffman-encoded values | ~30% for ASCII strings |

### Stream States (RFC 7540 Section 5.1)

```
                          +--------+
                  send PP |        | recv PP
                 ,--------|  idle  |--------.
                /         |        |         \
               v          +--------+          v
        +----------+          |           +----------+
        |          |          | send H /  |          |
 ,------| reserved |          | recv H    | reserved |------.
 |      | (local)  |          |           | (remote) |      |
 |      +----------+          v           +----------+      |
 |          |             +--------+             |          |
 |          |     recv ES |        | send ES     |          |
 |   send H |     ,-------|  open  |-------.     | recv H   |
 |          |    /        |        |        \    |          |
 |          v   v         +--------+         v   v          |
 |      +----------+          |           +----------+      |
 |      |   half   |          |           |   half   |      |
 |      |  closed  |          | send R /  |  closed  |      |
 |      | (remote) |          | recv R    |  (local) |      |
 |      +----------+          |           +----------+      |
 |           |                |                 |           |
 |           | send ES /      |       recv ES / |           |
 |           | send R /       v        send R / |           |
 |           | recv R     +--------+   recv R   |           |
 | send R /  `----------->|        |<-----------'  send R / |
 | recv R                 | closed |               recv R   |
 `----------------------->|        |<-----------------------'
                          +--------+
```

**Legend:** H = HEADERS, ES = END_STREAM, R = RST_STREAM, PP = PUSH_PROMISE

### stream_state Enum

| State | Value | Description |
|-------|-------|-------------|
| `idle` | - | Stream not yet opened |
| `open` | - | Stream active, both sides can send |
| `half_closed_local` | - | Local side closed, remote can still send |
| `half_closed_remote` | - | Remote side closed, local can still send |
| `closed` | - | Stream fully closed |

### Connection Constants

| Constant | Value | Description |
|----------|-------|-------------|
| `CONNECTION_PREFACE` | `"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"` | Client connection preface |
| `FRAME_HEADER_SIZE` | `9` | Frame header size in bytes |
| `DEFAULT_WINDOW_SIZE` | `65535` | Default flow control window (bytes) |

### Concurrency Model

| Component | Thread Safety | Mechanism |
|-----------|---------------|-----------|
| `http2_client` | Thread-safe | `std::mutex` + `std::atomic` |
| `http2_server` | Thread-safe | `std::mutex` + `std::atomic` |
| `http2_server_stream` | Thread-safe | `std::mutex` per stream |
| `http2_server_connection` | Thread-safe | Per-connection mutex |
| HPACK encoder/decoder | Not thread-safe | One per connection |

The server uses Asio's I/O context for asynchronous operations. Request handlers are invoked from the I/O thread, so long-running operations should be dispatched to a separate thread pool.

---

## Related Documentation

- [Facade API Guide](FACADE_GUIDE.md) - High-level protocol facades including `http_facade`
- [Unified API Guide](UNIFIED_API_GUIDE.md) - Protocol-agnostic connection/listener interfaces
