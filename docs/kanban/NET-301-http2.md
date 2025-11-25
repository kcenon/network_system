# NET-301: HTTP/2 Protocol Support Design

| Field | Value |
|-------|-------|
| **ID** | NET-301 |
| **Title** | HTTP/2 Protocol Support Design |
| **Category** | FUTURE |
| **Priority** | LOW |
| **Status** | DONE |
| **Est. Duration** | 10-14 days |
| **Dependencies** | None |
| **Target Version** | v2.0.0 |

---

## What (Problem Statement)

### Current Problem
The network system only supports HTTP/1.1:
- No multiplexing support (one request per connection at a time)
- No server push capability
- No header compression
- Higher latency for multiple requests

### Goal
Design and implement HTTP/2 protocol support:
- Full HTTP/2 protocol implementation (RFC 7540)
- Stream multiplexing over single connection
- HPACK header compression (RFC 7541)
- Server push support
- TLS integration with ALPN negotiation
- Backward compatibility with HTTP/1.1

### Benefits
- Better performance for modern web applications
- Reduced latency through multiplexing
- Lower bandwidth through header compression
- Enable server push for proactive content delivery

---

## How (Implementation Plan)

### Phase 1: Core Frame Layer (4-5 days)

#### Frame Types Implementation
```cpp
// include/network_system/protocols/http2/frame.h

namespace network_system::protocols::http2 {

enum class frame_type : uint8_t {
    data          = 0x0,
    headers       = 0x1,
    priority      = 0x2,
    rst_stream    = 0x3,
    settings      = 0x4,
    push_promise  = 0x5,
    ping          = 0x6,
    goaway        = 0x7,
    window_update = 0x8,
    continuation  = 0x9
};

struct frame_header {
    uint32_t length;     // 24 bits
    frame_type type;
    uint8_t flags;
    uint32_t stream_id;  // 31 bits (MSB reserved)
};

class frame {
public:
    static auto parse(std::span<const uint8_t> data) -> Result<frame>;
    auto serialize() const -> std::vector<uint8_t>;

    auto header() const -> const frame_header&;
    auto payload() const -> std::span<const uint8_t>;

private:
    frame_header header_;
    std::vector<uint8_t> payload_;
};

// Specific frame types
class data_frame : public frame {
public:
    auto is_end_stream() const -> bool;
    auto is_padded() const -> bool;
    auto data() const -> std::span<const uint8_t>;
};

class headers_frame : public frame {
public:
    auto is_end_stream() const -> bool;
    auto is_end_headers() const -> bool;
    auto header_block() const -> std::span<const uint8_t>;
};

class settings_frame : public frame {
public:
    struct setting {
        uint16_t identifier;
        uint32_t value;
    };

    auto settings() const -> std::vector<setting>;
    auto is_ack() const -> bool;
};

} // namespace network_system::protocols::http2
```

### Phase 2: HPACK Implementation (3-4 days)

#### Header Compression
```cpp
// include/network_system/protocols/http2/hpack.h

namespace network_system::protocols::http2 {

class hpack_encoder {
public:
    explicit hpack_encoder(size_t max_table_size = 4096);

    auto encode(const std::vector<http_header>& headers) -> std::vector<uint8_t>;
    auto set_max_table_size(size_t size) -> void;

private:
    auto encode_integer(uint64_t value, uint8_t prefix_bits) -> std::vector<uint8_t>;
    auto encode_string(std::string_view str, bool huffman = true) -> std::vector<uint8_t>;

    dynamic_table table_;
};

class hpack_decoder {
public:
    explicit hpack_decoder(size_t max_table_size = 4096);

    auto decode(std::span<const uint8_t> data) -> Result<std::vector<http_header>>;
    auto set_max_table_size(size_t size) -> void;

private:
    auto decode_integer(std::span<const uint8_t>& data, uint8_t prefix_bits) -> Result<uint64_t>;
    auto decode_string(std::span<const uint8_t>& data) -> Result<std::string>;

    dynamic_table table_;
};

class dynamic_table {
public:
    auto insert(std::string_view name, std::string_view value) -> void;
    auto lookup(size_t index) const -> std::optional<http_header>;
    auto find(std::string_view name, std::string_view value) const -> std::optional<size_t>;
    auto evict_to_size(size_t target_size) -> void;

private:
    std::deque<http_header> entries_;
    size_t current_size_ = 0;
    size_t max_size_ = 4096;
};

} // namespace network_system::protocols::http2
```

### Phase 3: Stream Management (2-3 days)

#### Stream State Machine
```cpp
// include/network_system/protocols/http2/stream.h

namespace network_system::protocols::http2 {

enum class stream_state {
    idle,
    reserved_local,
    reserved_remote,
    open,
    half_closed_local,
    half_closed_remote,
    closed
};

class stream {
public:
    explicit stream(uint32_t id);

    auto id() const -> uint32_t;
    auto state() const -> stream_state;

    // State transitions
    auto send_headers(bool end_stream) -> Result<void>;
    auto receive_headers(bool end_stream) -> Result<void>;
    auto send_data(bool end_stream) -> Result<void>;
    auto receive_data(bool end_stream) -> Result<void>;
    auto send_rst_stream(uint32_t error_code) -> Result<void>;
    auto receive_rst_stream() -> Result<void>;

    // Flow control
    auto send_window_size() const -> int32_t;
    auto receive_window_size() const -> int32_t;
    auto update_send_window(int32_t delta) -> void;
    auto update_receive_window(int32_t delta) -> void;

private:
    uint32_t id_;
    stream_state state_ = stream_state::idle;
    int32_t send_window_ = 65535;
    int32_t receive_window_ = 65535;
};

class stream_manager {
public:
    auto create_stream() -> Result<stream&>;
    auto get_stream(uint32_t id) -> std::optional<std::reference_wrapper<stream>>;
    auto close_stream(uint32_t id) -> void;

    auto set_max_concurrent_streams(uint32_t max) -> void;
    auto active_stream_count() const -> size_t;

private:
    std::unordered_map<uint32_t, stream> streams_;
    uint32_t next_stream_id_ = 1;  // Client: odd, Server: even
    uint32_t max_concurrent_streams_ = 100;
};

} // namespace network_system::protocols::http2
```

### Phase 4: Client/Server API (2-3 days)

#### HTTP/2 Client
```cpp
// include/network_system/protocols/http2_client.h

namespace network_system::protocols {

class http2_client {
public:
    struct config {
        bool use_tls = true;
        size_t max_concurrent_streams = 100;
        size_t initial_window_size = 65535;
        size_t max_frame_size = 16384;
        size_t header_table_size = 4096;
    };

    explicit http2_client(const config& cfg = {});
    ~http2_client();

    // Connection management
    auto connect(const std::string& host, uint16_t port) -> VoidResult;
    auto disconnect() -> void;
    auto is_connected() const -> bool;

    // HTTP operations
    auto get(const std::string& path,
            const http_headers& headers = {}) -> Result<http_response>;

    auto post(const std::string& path,
             std::span<const uint8_t> body,
             const http_headers& headers = {}) -> Result<http_response>;

    auto request(const http_request& req) -> Result<http_response>;

    // Async operations
    auto request_async(const http_request& req,
                       std::function<void(Result<http_response>)> callback) -> void;

    // Streaming
    auto create_stream() -> Result<std::shared_ptr<http2_stream>>;

    // Server push handling
    using push_handler = std::function<void(const http_request& promised,
                                           std::shared_ptr<http2_stream> stream)>;
    auto set_push_handler(push_handler handler) -> void;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

class http2_stream {
public:
    auto send_headers(const http_headers& headers, bool end_stream = false) -> VoidResult;
    auto send_data(std::span<const uint8_t> data, bool end_stream = false) -> VoidResult;
    auto receive_headers() -> Result<http_headers>;
    auto receive_data() -> Result<std::vector<uint8_t>>;
    auto cancel(uint32_t error_code = 0x8) -> void;  // CANCEL

    auto id() const -> uint32_t;
    auto is_open() const -> bool;
};

} // namespace network_system::protocols
```

#### HTTP/2 Server
```cpp
// include/network_system/protocols/http2_server.h

namespace network_system::protocols {

class http2_server {
public:
    struct config {
        size_t max_concurrent_streams = 100;
        size_t initial_window_size = 65535;
        size_t max_frame_size = 16384;
        bool enable_push = true;
    };

    explicit http2_server(const config& cfg = {});
    ~http2_server();

    // Server management
    auto start(uint16_t port) -> VoidResult;
    auto start_tls(uint16_t port,
                   const std::string& cert_path,
                   const std::string& key_path) -> VoidResult;
    auto stop() -> void;

    // Routing
    auto route(const std::string& method,
               const std::string& path,
               std::function<http_response(const http_request&)> handler) -> void;

    // Server push
    auto push(http2_stream& stream,
              const http_request& promised_request,
              const http_response& response) -> VoidResult;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace network_system::protocols
```

---

## Test (Verification Plan)

### Unit Tests
```cpp
// Frame parsing
TEST(Http2Frame, ParsesDataFrame) {
    std::vector<uint8_t> raw = {0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
                                'h', 'e', 'l', 'l', 'o'};
    auto frame = http2::frame::parse(raw);

    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->header().type, http2::frame_type::data);
    EXPECT_EQ(frame->header().length, 5);
}

// HPACK encoding/decoding
TEST(Hpack, EncodesHeaders) {
    hpack_encoder encoder;
    std::vector<http_header> headers = {
        {":method", "GET"},
        {":path", "/"},
        {"host", "example.com"}
    };

    auto encoded = encoder.encode(headers);
    EXPECT_FALSE(encoded.empty());

    hpack_decoder decoder;
    auto decoded = decoder.decode(encoded);

    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->size(), 3);
}

// Stream state machine
TEST(Http2Stream, StateTransitions) {
    http2::stream stream(1);

    EXPECT_EQ(stream.state(), http2::stream_state::idle);

    stream.send_headers(false);
    EXPECT_EQ(stream.state(), http2::stream_state::open);

    stream.send_data(true);
    EXPECT_EQ(stream.state(), http2::stream_state::half_closed_local);
}
```

### Integration Tests
```cpp
TEST(Http2Client, SimpleRequest) {
    http2_server server;
    server.route("GET", "/", [](const auto& req) {
        return http_response::ok("Hello HTTP/2!");
    });
    server.start(8443);

    http2_client client;
    client.connect("localhost", 8443);

    auto response = client.get("/");

    ASSERT_TRUE(response.has_value());
    EXPECT_EQ(response->status_code(), 200);
}

TEST(Http2Client, MultiplexedRequests) {
    http2_server server;
    server.route("GET", "/slow", [](const auto& req) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return http_response::ok("slow");
    });
    server.route("GET", "/fast", [](const auto& req) {
        return http_response::ok("fast");
    });
    server.start(8443);

    http2_client client;
    client.connect("localhost", 8443);

    // Send both requests simultaneously
    std::future<Result<http_response>> slow_future;
    std::future<Result<http_response>> fast_future;

    auto start = std::chrono::steady_clock::now();

    slow_future = std::async([&]() { return client.get("/slow"); });
    fast_future = std::async([&]() { return client.get("/fast"); });

    auto fast_response = fast_future.get();
    auto fast_time = std::chrono::steady_clock::now() - start;

    // Fast request should complete before slow request finishes
    EXPECT_LT(fast_time, std::chrono::milliseconds(50));
}
```

### Compatibility Testing
- Test against `nghttp2` client/server
- Test against `curl --http2`
- Test against major browsers (Chrome, Firefox)

---

## Acceptance Criteria

- [x] Frame layer implementation complete
- [x] HPACK encoder/decoder working
- [ ] Stream management with proper state machine (deferred to implementation phase)
- [ ] Client API functional (deferred to implementation phase)
- [ ] Server API functional (deferred to implementation phase)
- [ ] TLS with ALPN negotiation working (deferred to implementation phase)
- [ ] Server push implementation (deferred to implementation phase)
- [x] All unit tests passing
- [ ] Integration tests passing (deferred to implementation phase)
- [ ] Interoperability with standard tools (deferred to implementation phase)

---

## Notes

- Consider using nghttp2 library for initial implementation
- HTTP/2 requires TLS in browsers (h2c is rarely used)
- Prioritization (RFC 7540 Section 5.3) can be deferred
- Connection preface must be sent/validated
- SETTINGS frames must be acknowledged

## Completion Notes

**Design phase completed (2025-11-25):**
- Frame layer implementation with all HTTP/2 frame types (DATA, HEADERS, SETTINGS, RST_STREAM, PING, GOAWAY, WINDOW_UPDATE)
- HPACK encoder/decoder with static/dynamic table support
- Huffman coding support for header compression
- 26 unit tests passing (13 frame tests + 13 HPACK tests)
- Full implementation (Stream management, Client/Server API) deferred to v2.0.0 implementation phase
