# UDP Reliability Layer Guide

**Last Updated**: 2025-11-24
**Version**: 0.1.0

This document provides comprehensive guidance on using the UDP reliability layer, which offers configurable reliability guarantees on top of UDP.

---

## Table of Contents

- [Overview](#overview)
- [Quick Start](#quick-start)
- [Reliability Modes](#reliability-modes)
- [Configuration Options](#configuration-options)
- [Statistics and Monitoring](#statistics-and-monitoring)
- [Performance Characteristics](#performance-characteristics)
- [Use Cases](#use-cases)
- [Error Handling](#error-handling)
- [Best Practices](#best-practices)

---

## Overview

The UDP Reliability Layer provides the best of both worlds: UDP's low latency with TCP-like reliability when needed. It's ideal for applications that need control over the reliability vs. performance trade-off.

**Namespace**: `network_system::core`

**Key Features**:
- **Selective Acknowledgment (SACK)**: Efficient ACK mechanism
- **Packet Retransmission**: Automatic retry for lost packets
- **In-order Delivery**: Optional sequence guarantee
- **Congestion Control**: Sliding window flow control
- **Flexible Modes**: Choose reliability vs performance trade-off
- **Statistics**: Built-in monitoring for RTT, packet loss, etc.

---

## Quick Start

### Basic Client Setup

```cpp
#include "kcenon/network/core/reliable_udp_client.h"

using namespace network_system::core;

// Create a reliable UDP client with ordered delivery
auto client = std::make_shared<reliable_udp_client>(
    "my-client",
    reliability_mode::reliable_ordered
);

// Set receive callback
client->set_receive_callback([](const std::vector<uint8_t>& data) {
    std::cout << "Received " << data.size() << " bytes\n";
});

// Set error callback
client->set_error_callback([](std::error_code ec) {
    std::cerr << "Error: " << ec.message() << "\n";
});

// Connect to server
auto result = client->start_client("localhost", 5555);
if (!result.is_ok()) {
    std::cerr << "Failed to start: " << result.error().message << "\n";
    return -1;
}

// Send data - reliability is handled automatically
std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
client->send_packet(std::move(data));

// Wait for client to finish
client->wait_for_stop();
```

### Server with UDP Support

For server-side functionality, use `messaging_udp_server` (see [UDP_SUPPORT.md](../UDP_SUPPORT.md)):

```cpp
#include "kcenon/network/core/messaging_udp_server.h"

using namespace network_system::core;

auto server = std::make_shared<messaging_udp_server>("my-server");

server->set_receive_callback(
    [server](const std::vector<uint8_t>& data,
             const asio::ip::udp::endpoint& sender) {
        // Process incoming data
        process_data(data);

        // Send response
        std::vector<uint8_t> response = create_response(data);
        server->async_send_to(std::move(response), sender,
            [](std::error_code ec, std::size_t bytes) {
                if (!ec) {
                    std::cout << "Response sent\n";
                }
            });
    });

server->start_server(5555);
```

---

## Reliability Modes

The reliability layer offers four modes to balance reliability and performance:

### 1. Unreliable Mode

Pure UDP with no overhead - packets may be lost, duplicated, or reordered.

```cpp
auto client = std::make_shared<reliable_udp_client>(
    "client",
    reliability_mode::unreliable
);
```

**Characteristics**:
- Lowest latency
- No retransmission
- No acknowledgments
- No ordering guarantee

**Use for**: Real-time voice/video, position updates in games, sensor readings.

### 2. Reliable Ordered Mode

TCP-like reliability with guaranteed in-order delivery.

```cpp
auto client = std::make_shared<reliable_udp_client>(
    "client",
    reliability_mode::reliable_ordered
);
```

**Characteristics**:
- Guaranteed delivery
- In-order delivery
- Automatic retransmission
- Higher latency due to ordering

**Use for**: Chat messages, game commands, critical state updates.

### 3. Reliable Unordered Mode

Guaranteed delivery without ordering constraint.

```cpp
auto client = std::make_shared<reliable_udp_client>(
    "client",
    reliability_mode::reliable_unordered
);
```

**Characteristics**:
- Guaranteed delivery
- May arrive out of order
- Lower latency than reliable_ordered
- Less memory for buffering

**Use for**: File chunk downloads, asset loading, non-sequential data.

### 4. Sequenced Mode

Latest packet only - old packets are dropped.

```cpp
auto client = std::make_shared<reliable_udp_client>(
    "client",
    reliability_mode::sequenced
);
```

**Characteristics**:
- Only latest packet matters
- Old packets automatically dropped
- No retransmission
- Low latency

**Use for**: Player positions in games, real-time sensor data, live state where only current value matters.

---

## Mode Comparison

| Feature | Unreliable | Reliable Ordered | Reliable Unordered | Sequenced |
|---------|------------|------------------|-------------------|-----------|
| Delivery Guarantee | No | Yes | Yes | Latest only |
| Order Guarantee | No | Yes | No | No |
| Retransmission | No | Yes | Yes | No |
| Latency | Lowest | Higher | Medium | Low |
| Bandwidth Overhead | Lowest | Higher | Medium | Low |
| Memory Usage | Lowest | Higher | Medium | Low |

### Decision Flowchart

```
                    Is delivery critical?
                            │
              ┌─────────────┴─────────────┐
              │                           │
             No                          Yes
              │                           │
              ▼                           ▼
     ┌────────────────┐         ┌─────────────────────┐
     │   Unreliable   │         │ Is order important? │
     │   - Voice/Video│         └──────────┬──────────┘
     │   - Positions  │                    │
     └────────────────┘       ┌────────────┴────────────┐
                              │                         │
                             Yes                        No
                              │                         │
                              ▼                         ▼
                   ┌─────────────────┐       ┌───────────────────┐
                   │Reliable Ordered │       │Reliable Unordered │
                   │ - Chat messages │       │ - File downloads  │
                   │ - Commands      │       │ - Asset loading   │
                   └─────────────────┘       └───────────────────┘
```

---

## Configuration Options

### Congestion Window

Controls how many packets can be "in flight" (sent but not acknowledged).

```cpp
// Larger window = higher throughput, more memory
client->set_congestion_window(64);

// Smaller window = lower throughput, less memory
client->set_congestion_window(16);
```

**Recommendations**:
- High throughput: 64-128 packets
- Low latency gaming: 16-32 packets
- Constrained networks: 8-16 packets

### Retransmission Settings

Configure how aggressively packets are retried:

```cpp
// Maximum retry attempts before giving up
client->set_max_retries(5);  // Default: 5

// Initial retransmission timeout (dynamically adjusted)
client->set_retransmission_timeout(200);  // 200ms default
```

**Tips**:
- Low latency applications: fewer retries, shorter timeout
- High reliability needs: more retries, longer timeout

### Configuration Presets

**Low Latency (Gaming)**:
```cpp
client->set_congestion_window(16);
client->set_max_retries(2);
client->set_retransmission_timeout(50);
```

**High Reliability (File Transfer)**:
```cpp
client->set_congestion_window(128);
client->set_max_retries(10);
client->set_retransmission_timeout(500);
```

**Balanced (General Use)**:
```cpp
client->set_congestion_window(32);
client->set_max_retries(5);
client->set_retransmission_timeout(200);
```

---

## Statistics and Monitoring

### Getting Statistics

```cpp
reliable_udp_stats stats = client->get_stats();

std::cout << "Packets sent: " << stats.packets_sent << "\n";
std::cout << "Packets received: " << stats.packets_received << "\n";
std::cout << "Retransmissions: " << stats.packets_retransmitted << "\n";
std::cout << "Packets dropped: " << stats.packets_dropped << "\n";
std::cout << "ACKs sent: " << stats.acks_sent << "\n";
std::cout << "ACKs received: " << stats.acks_received << "\n";
std::cout << "Average RTT: " << stats.average_rtt_ms << " ms\n";
```

### Statistics Fields

| Field | Description |
|-------|-------------|
| `packets_sent` | Total packets transmitted |
| `packets_received` | Total packets successfully received |
| `packets_retransmitted` | Packets that required retransmission |
| `packets_dropped` | Packets dropped (sequenced mode) |
| `acks_sent` | ACK packets sent (reliable modes) |
| `acks_received` | ACK packets received (reliable modes) |
| `average_rtt_ms` | Average round-trip time in milliseconds |

### Monitoring Best Practices

```cpp
// Periodic monitoring
void monitor_connection() {
    auto stats = client->get_stats();

    // Calculate packet loss rate
    double total = stats.packets_sent + stats.packets_received;
    double loss_rate = (stats.packets_retransmitted / total) * 100.0;

    if (loss_rate > 5.0) {
        log_warning("High packet loss: " + std::to_string(loss_rate) + "%");
    }

    if (stats.average_rtt_ms > 100.0) {
        log_warning("High latency: " + std::to_string(stats.average_rtt_ms) + " ms");
    }
}
```

---

## Performance Characteristics

### Benchmark Results (Reference System)

Test environment: M1 Pro, 16GB RAM, localhost

| Mode | Throughput | Latency (P50) | Latency (P99) | Memory/Connection |
|------|------------|---------------|---------------|-------------------|
| Unreliable | ~500K msg/s | 8 us | 25 us | ~1 KB |
| Reliable Ordered | ~150K msg/s | 45 us | 200 us | ~8 KB |
| Reliable Unordered | ~200K msg/s | 30 us | 150 us | ~4 KB |
| Sequenced | ~400K msg/s | 12 us | 40 us | ~2 KB |

*Note: Actual performance varies based on packet size, network conditions, and system load.*

### Memory Usage

- **Per connection**: ~4 KB base + buffer sizes
- **Per pending message**: ~100 bytes + message size
- **Congestion window**: window_size x max_packet_size

---

## Use Cases

### Real-time Gaming

```cpp
// For player position updates (only latest matters)
auto position_client = std::make_shared<reliable_udp_client>(
    "position-client",
    reliability_mode::sequenced
);
position_client->set_congestion_window(16);

// For chat messages (must be reliable and ordered)
auto chat_client = std::make_shared<reliable_udp_client>(
    "chat-client",
    reliability_mode::reliable_ordered
);

// For game state snapshots (reliable but order doesn't matter)
auto state_client = std::make_shared<reliable_udp_client>(
    "state-client",
    reliability_mode::reliable_unordered
);
```

### IoT Sensor Network

```cpp
// Sensor readings - occasional loss acceptable
auto sensor_client = std::make_shared<reliable_udp_client>(
    "sensor-node-001",
    reliability_mode::unreliable
);

// Control commands - must be delivered
auto control_client = std::make_shared<reliable_udp_client>(
    "control-node",
    reliability_mode::reliable_ordered
);
```

### Video Streaming

```cpp
// Video frames - only latest matters
auto video_client = std::make_shared<reliable_udp_client>(
    "video-stream",
    reliability_mode::sequenced
);

// Metadata/subtitles - must be reliable
auto metadata_client = std::make_shared<reliable_udp_client>(
    "metadata-stream",
    reliability_mode::reliable_unordered
);
```

---

## Error Handling

### Using Callbacks

```cpp
client->set_error_callback([](std::error_code ec) {
    if (ec == std::errc::connection_refused) {
        std::cerr << "Server not available\n";
    } else if (ec == std::errc::timed_out) {
        std::cerr << "Connection timed out\n";
    } else {
        std::cerr << "Error: " << ec.message() << "\n";
    }
});
```

### Using Result Type

```cpp
auto result = client->start_client("localhost", 5555);
if (!result.is_ok()) {
    auto& error = result.error();
    std::cerr << "Code: " << error.code << "\n";
    std::cerr << "Message: " << error.message << "\n";
    std::cerr << "Source: " << error.source << "\n";
    return -1;
}

auto send_result = client->send_packet(std::move(data));
if (!send_result.is_ok()) {
    // Handle send failure
}
```

---

## Best Practices

### 1. Choose the Right Mode

Match the reliability mode to your data's requirements:
- Use `unreliable` for high-frequency, non-critical data
- Use `reliable_ordered` only when order matters
- Prefer `reliable_unordered` when order doesn't matter
- Use `sequenced` for state updates where only latest matters

### 2. Configure Appropriately

```cpp
// For high-bandwidth scenarios
client->set_congestion_window(64);

// For low-latency scenarios
client->set_congestion_window(16);
client->set_max_retries(2);
```

### 3. Monitor Performance

Regularly check statistics to detect issues:

```cpp
auto stats = client->get_stats();
if (stats.packets_retransmitted > stats.packets_sent * 0.1) {
    // More than 10% retransmission - investigate network
}
```

### 4. Handle Errors Gracefully

Always set error callbacks and handle failures:

```cpp
client->set_error_callback([this](std::error_code ec) {
    handle_connection_error(ec);
    attempt_reconnect();
});
```

### 5. Clean Shutdown

Always stop clients properly:

```cpp
if (client->is_running()) {
    auto result = client->stop_client();
    if (!result.is_ok()) {
        log_warning("Client shutdown error: " + result.error().message);
    }
}
```

---

## Related Documentation

- [UDP Support Overview](../UDP_SUPPORT.md) - Basic UDP support documentation
- [API Reference](API_REFERENCE.md) - Complete API documentation
- [Performance Tuning](OPERATIONS.md) - Production optimization
- [Troubleshooting](TROUBLESHOOTING.md) - Common issues and solutions

---

**Maintainer**: Network System Team
**Contact**: Use issue tracker for questions
