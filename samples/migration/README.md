# Migration Examples

This directory contains examples demonstrating the migration from legacy protocol-specific interfaces to the new unified interface API.

## Files

| File | Description |
|------|-------------|
| `legacy_client_example.cpp` | Shows the OLD (deprecated) client patterns |
| `unified_client_example.cpp` | Shows the NEW unified client API |
| `unified_server_example.cpp` | Shows the NEW unified server (listener) API |

## Quick Comparison

### Client Code

**Legacy (Don't use in new code):**
```cpp
// Multiple protocol-specific interfaces
i_tcp_client* client;
i_udp_client* udp_client;
i_websocket_client* ws_client;

// Individual callback setters
client->set_receive_callback(...);
client->set_connected_callback(...);
client->set_disconnected_callback(...);
client->set_error_callback(...);

// Start with separate host/port
client->start("localhost", 8080);
```

**Unified (Recommended):**
```cpp
// Single interface for all protocols
auto tcp = protocol::tcp::connect({"localhost", 8080});
auto udp = protocol::udp::connect({"localhost", 5555});
auto ws = protocol::websocket::connect("ws://localhost:8080/ws");

// Clean callback structure
conn->set_callbacks({
    .on_connected = ...,
    .on_data = ...,
    .on_disconnected = ...,
    .on_error = ...
});

// Connect with endpoint_info
conn->connect({"remote.host", 9000});
```

### Server Code

**Legacy:**
```cpp
i_tcp_server* server;
server->set_connection_callback(...);
server->set_receive_callback(...);
server->start(8080);
```

**Unified:**
```cpp
auto listener = protocol::tcp::listen(8080);
listener->set_callbacks({
    .on_accept = ...,
    .on_data = ...,
    .on_disconnect = ...
});
listener->start(8080);
```

## Key Benefits

1. **Fewer interfaces to learn**: 3 core interfaces vs 12+ protocol-specific
2. **Protocol-agnostic code**: Write once, use with any protocol
3. **Cleaner callbacks**: Single `set_callbacks()` with aggregate initialization
4. **Modern types**: `std::span<std::byte>` instead of `std::vector<uint8_t>`
5. **Factory pattern**: Protocol selection at creation time

## Building Examples

These examples are for documentation purposes. To compile:

```bash
# From project root
cmake -B build
cmake --build build --target migration_examples
```

## See Also

- [Migration Guide](../../docs/guides/MIGRATION_UNIFIED_API.md)
- [Unified API Reference](../../docs/API_REFERENCE.md)
- [Compatibility Header](../../include/kcenon/network/compat/legacy_aliases.h)
