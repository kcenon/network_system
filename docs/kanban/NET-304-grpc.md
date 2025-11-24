# NET-304: gRPC Integration Prototype

| Field | Value |
|-------|-------|
| **ID** | NET-304 |
| **Title** | gRPC Integration Prototype |
| **Category** | FUTURE |
| **Priority** | LOW |
| **Status** | TODO |
| **Est. Duration** | 7-10 days |
| **Dependencies** | NET-301 (HTTP/2 Support) |
| **Target Version** | v2.0.0 |

---

## What (Problem Statement)

### Current Problem
No support for gRPC protocol:
- Cannot communicate with gRPC services
- No RPC abstraction layer
- Missing Protocol Buffers integration
- No streaming RPC support

### Goal
Implement gRPC protocol support:
- gRPC over HTTP/2 transport
- Unary and streaming RPC calls
- Protocol Buffers serialization
- Metadata and deadline support
- Compatible with standard gRPC ecosystem

### Benefits
- Interoperability with gRPC services
- High-performance RPC communication
- Strong typing through Protocol Buffers
- Bi-directional streaming support

---

## How (Implementation Plan)

### Phase 1: Protocol Buffers Integration (2-3 days)

#### CMake Integration
```cmake
# cmake/FindProtobuf.cmake

find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG)

function(add_proto_library TARGET)
    set(PROTO_FILES ${ARGN})

    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)

        set(PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
        set(PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
        set(GRPC_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
        set(GRPC_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")

        add_custom_command(
            OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
            COMMAND protobuf::protoc
            ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                 --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                 --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
                 -I ${PROTO_DIR}
                 ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
        )

        list(APPEND ALL_SRCS ${PROTO_SRCS} ${GRPC_SRCS})
    endforeach()

    add_library(${TARGET} ${ALL_SRCS})
    target_link_libraries(${TARGET} PUBLIC protobuf::libprotobuf)
endfunction()
```

### Phase 2: gRPC Frame Layer (2 days)

#### gRPC Message Format
```cpp
// include/network_system/protocols/grpc/frame.h

namespace network_system::protocols::grpc {

// gRPC message format:
// | Compressed-Flag (1 byte) | Message-Length (4 bytes, big-endian) | Message |

struct grpc_message {
    bool compressed;
    std::vector<uint8_t> data;

    static auto parse(std::span<const uint8_t> data) -> Result<grpc_message>;
    auto serialize() const -> std::vector<uint8_t>;
};

// gRPC status codes
enum class status_code {
    ok = 0,
    cancelled = 1,
    unknown = 2,
    invalid_argument = 3,
    deadline_exceeded = 4,
    not_found = 5,
    already_exists = 6,
    permission_denied = 7,
    resource_exhausted = 8,
    failed_precondition = 9,
    aborted = 10,
    out_of_range = 11,
    unimplemented = 12,
    internal = 13,
    unavailable = 14,
    data_loss = 15,
    unauthenticated = 16
};

struct grpc_status {
    status_code code;
    std::string message;
    std::string details;  // google.rpc.Status in binary format
};

// gRPC trailers
struct grpc_trailers {
    status_code status;
    std::string status_message;
    std::optional<std::string> status_details;
};

} // namespace network_system::protocols::grpc
```

### Phase 3: Client Implementation (2-3 days)

#### gRPC Client API
```cpp
// include/network_system/protocols/grpc_client.h

namespace network_system::protocols {

// Channel configuration
struct grpc_channel_config {
    std::chrono::milliseconds default_timeout{30000};
    bool use_tls = true;
    std::string root_certificates;
    std::optional<std::string> client_certificate;
    std::optional<std::string> client_key;
    size_t max_message_size = 4 * 1024 * 1024;  // 4MB
};

// Metadata for requests/responses
using grpc_metadata = std::vector<std::pair<std::string, std::string>>;

// Call options
struct call_options {
    std::optional<std::chrono::system_clock::time_point> deadline;
    grpc_metadata metadata;
    bool wait_for_ready = false;
};

// Client interface
class grpc_client {
public:
    explicit grpc_client(const std::string& target,
                        const grpc_channel_config& config = {});
    ~grpc_client();

    // Connection management
    auto connect() -> VoidResult;
    auto disconnect() -> void;
    auto is_connected() const -> bool;
    auto wait_for_connected(std::chrono::milliseconds timeout) -> bool;

    // Unary RPC
    template<typename Request, typename Response>
    auto call(const std::string& method,
              const Request& request,
              const call_options& options = {}) -> Result<Response>;

    // Async unary RPC
    template<typename Request, typename Response>
    auto call_async(const std::string& method,
                    const Request& request,
                    std::function<void(Result<Response>)> callback,
                    const call_options& options = {}) -> void;

    // Server streaming RPC
    template<typename Request, typename Response>
    auto server_stream(const std::string& method,
                       const Request& request,
                       std::function<bool(const Response&)> handler,
                       const call_options& options = {}) -> Result<grpc_status>;

    // Client streaming RPC
    template<typename Request, typename Response>
    class client_stream {
    public:
        auto write(const Request& request) -> VoidResult;
        auto write_done() -> VoidResult;
        auto finish() -> Result<Response>;
    };

    template<typename Request, typename Response>
    auto client_stream(const std::string& method,
                       const call_options& options = {})
        -> Result<client_stream<Request, Response>>;

    // Bidirectional streaming RPC
    template<typename Request, typename Response>
    class bidi_stream {
    public:
        auto write(const Request& request) -> VoidResult;
        auto read() -> Result<Response>;
        auto write_done() -> VoidResult;
        auto finish() -> Result<grpc_status>;
    };

    template<typename Request, typename Response>
    auto bidi_stream(const std::string& method,
                     const call_options& options = {})
        -> Result<bidi_stream<Request, Response>>;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

} // namespace network_system::protocols
```

### Phase 4: Server Implementation (2 days)

#### gRPC Server API
```cpp
// include/network_system/protocols/grpc_server.h

namespace network_system::protocols {

struct grpc_server_config {
    size_t max_concurrent_streams = 100;
    size_t max_message_size = 4 * 1024 * 1024;
    std::chrono::milliseconds keepalive_time{7200000};
    std::chrono::milliseconds keepalive_timeout{20000};
};

// Server context for handling requests
class server_context {
public:
    auto client_metadata() const -> const grpc_metadata&;
    auto set_trailing_metadata(grpc_metadata metadata) -> void;
    auto is_cancelled() const -> bool;
    auto deadline() const -> std::optional<std::chrono::system_clock::time_point>;
    auto peer() const -> std::string;
};

class grpc_server {
public:
    explicit grpc_server(const grpc_server_config& config = {});
    ~grpc_server();

    // Server lifecycle
    auto start(uint16_t port) -> VoidResult;
    auto start_tls(uint16_t port,
                   const std::string& cert_path,
                   const std::string& key_path) -> VoidResult;
    auto stop() -> void;
    auto wait() -> void;

    // Register service handlers
    template<typename Service>
    auto register_service(Service* service) -> void;

    // Low-level method registration
    template<typename Request, typename Response>
    auto register_unary_method(
        const std::string& full_method_name,
        std::function<grpc_status(server_context&, const Request&, Response*)> handler)
        -> void;

    template<typename Request, typename Response>
    auto register_server_streaming_method(
        const std::string& full_method_name,
        std::function<grpc_status(server_context&, const Request&,
                                  std::function<bool(const Response&)>)> handler)
        -> void;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

// Service base class for code generation
class grpc_service {
public:
    virtual ~grpc_service() = default;
    virtual auto service_name() const -> std::string_view = 0;
};

} // namespace network_system::protocols
```

---

## Test (Verification Plan)

### Unit Tests
```cpp
// gRPC message serialization
TEST(GrpcMessage, SerializesCorrectly) {
    grpc_message msg{
        .compressed = false,
        .data = {1, 2, 3, 4, 5}
    };

    auto serialized = msg.serialize();

    EXPECT_EQ(serialized[0], 0);  // Not compressed
    EXPECT_EQ(serialized[1], 0);
    EXPECT_EQ(serialized[2], 0);
    EXPECT_EQ(serialized[3], 0);
    EXPECT_EQ(serialized[4], 5);  // Length
}

// Client call
TEST(GrpcClient, UnaryCall) {
    grpc_server server;
    server.register_unary_method<EchoRequest, EchoResponse>(
        "/test.Echo/Echo",
        [](auto& ctx, const auto& req, auto* resp) {
            resp->set_message(req.message());
            return grpc_status{status_code::ok};
        });
    server.start(50051);

    grpc_client client("localhost:50051", {.use_tls = false});
    client.connect();

    EchoRequest request;
    request.set_message("hello");

    auto result = client.call<EchoRequest, EchoResponse>("/test.Echo/Echo", request);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message(), "hello");
}
```

### Integration Tests
```cpp
TEST(GrpcIntegration, StreamingRpc) {
    // Server streaming test
    grpc_server server;
    server.register_server_streaming_method<NumberRequest, NumberResponse>(
        "/test.Numbers/GetNumbers",
        [](auto& ctx, const auto& req, auto writer) {
            for (int i = 0; i < req.count(); ++i) {
                NumberResponse resp;
                resp.set_value(i);
                if (!writer(resp)) break;
            }
            return grpc_status{status_code::ok};
        });
    server.start(50052);

    grpc_client client("localhost:50052", {.use_tls = false});
    client.connect();

    NumberRequest request;
    request.set_count(10);

    std::vector<int> received;
    auto status = client.server_stream<NumberRequest, NumberResponse>(
        "/test.Numbers/GetNumbers",
        request,
        [&](const auto& resp) {
            received.push_back(resp.value());
            return true;
        });

    EXPECT_EQ(status.code, status_code::ok);
    EXPECT_EQ(received.size(), 10);
}
```

### Compatibility Tests
```bash
# Test with grpcurl
grpcurl -plaintext localhost:50051 list
grpcurl -plaintext -d '{"message": "test"}' localhost:50051 test.Echo/Echo

# Test with grpc_cli
grpc_cli call localhost:50051 test.Echo.Echo "message: 'test'"
```

---

## Acceptance Criteria

- [ ] HTTP/2 transport layer working (depends on NET-301)
- [ ] gRPC message framing implemented
- [ ] Unary RPC calls working
- [ ] Server streaming RPC working
- [ ] Client streaming RPC working
- [ ] Bidirectional streaming RPC working
- [ ] Metadata support
- [ ] Deadline/timeout support
- [ ] Compatible with standard gRPC tools
- [ ] Documentation complete

---

## Notes

- Consider wrapping official gRPC library vs implementing from scratch
- HTTP/2 implementation (NET-301) is a prerequisite
- Protocol Buffers version compatibility (proto2 vs proto3)
- May need custom code generator for cleaner API
- Performance should be comparable to official gRPC-cpp
