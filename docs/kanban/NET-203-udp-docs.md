# NET-203: UDP Reliability Layer Documentation

| Field | Value |
|-------|-------|
| **ID** | NET-203 |
| **Title** | UDP Reliability Layer Documentation |
| **Category** | DOC |
| **Priority** | MEDIUM |
| **Status** | TODO |
| **Est. Duration** | 2-3 days |
| **Dependencies** | None |
| **Target Version** | v1.8.0 |

---

## What (Problem Statement)

### Current Problem
Documentation for the UDP reliability layer (implemented in v1.7.0) is insufficient:
- No API usage documentation
- Insufficient explanation of reliability modes
- No tuning guide
- Performance characteristics undocumented

### Goal
Create comprehensive documentation for the UDP reliability layer:
- API reference documentation
- Usage examples and tutorials
- Reliability mode comparison guide
- Performance tuning guide

### Deliverables
1. `docs/UDP_RELIABILITY.md` - Main documentation
2. `docs/UDP_RELIABILITY_KO.md` - Korean version
3. `samples/reliable_udp_example.cpp` - Example code
4. API reference updates

---

## How (Implementation Plan)

### Implementation Plan

#### Step 1: Create Main Documentation
```markdown
# UDP Reliability Layer Guide

## Overview

The UDP Reliability Layer provides configurable reliability on top of UDP,
offering the best of both worlds: UDP's low latency with TCP-like reliability
when needed.

## Quick Start

```cpp
#include "network_system/core/reliable_udp_client.h"
#include "network_system/core/reliable_udp_server.h"

// Server
reliable_udp_server server("my-server");
server.set_message_handler([](auto client_id, auto data) {
    // Handle received data
});
server.start_server(5555);

// Client
reliable_udp_client client("my-client");
client.start_client("localhost", 5555);
client.send_packet({1, 2, 3, 4, 5});
```

## Reliability Modes

### 1. Unreliable (Pure UDP)
- No guarantees
- Lowest latency
- Use for: Real-time voice/video, position updates

### 2. Reliable Ordered
- Guaranteed delivery
- In-order delivery
- Use for: Chat messages, commands

### 3. Reliable Unordered
- Guaranteed delivery
- May arrive out of order
- Use for: Asset downloads, non-sequential data

### 4. Sequenced
- Latest packet only
- Drops outdated packets
- Use for: Position/state updates where only latest matters
```

#### Step 2: Write Mode Comparison Section
```markdown
## Mode Comparison

| Feature | Unreliable | Reliable Ordered | Reliable Unordered | Sequenced |
|---------|------------|------------------|-------------------|-----------|
| Delivery Guarantee | No | Yes | Yes | Latest only |
| Order Guarantee | No | Yes | No | No |
| Retransmission | No | Yes | Yes | No |
| Latency | Lowest | Higher | Medium | Low |
| Bandwidth | Lowest | Higher | Medium | Low |

## Choosing the Right Mode

```
┌─────────────────────────────────────────────────────────────┐
│                    Is delivery critical?                      │
└───────────────────────────┬─────────────────────────────────┘
                            │
              ┌─────────────┴─────────────┐
              │                           │
             No                          Yes
              │                           │
              ▼                           ▼
        ┌──────────┐            ┌─────────────────────┐
        │Unreliable│            │ Is order important? │
        └──────────┘            └──────────┬──────────┘
                                           │
                              ┌────────────┴────────────┐
                              │                         │
                             Yes                        No
                              │                         │
                              ▼                         ▼
                   ┌─────────────────┐       ┌───────────────────┐
                   │Reliable Ordered │       │Reliable Unordered │
                   └─────────────────┘       └───────────────────┘
```
```

#### Step 3: Document Configuration Options
```markdown
## Configuration

### Client Configuration

```cpp
reliable_udp_client client("client-id", reliability_mode::reliable_ordered);

// Congestion control
client.set_congestion_window(64);        // Max packets in flight
client.set_max_retries(5);               // Retransmission attempts
client.set_rto_min(std::chrono::milliseconds(50));   // Min retransmit timeout
client.set_rto_max(std::chrono::milliseconds(3000)); // Max retransmit timeout

// Connection settings
client.set_heartbeat_interval(std::chrono::seconds(5));
client.set_connection_timeout(std::chrono::seconds(30));
```

### Server Configuration

```cpp
reliable_udp_server server("server-id");

server.set_max_clients(1000);
server.set_receive_buffer_size(65536);
server.set_send_buffer_size(65536);
```

### Tuning for Different Scenarios

#### Low Latency (Gaming)
```cpp
client.set_congestion_window(32);
client.set_rto_min(std::chrono::milliseconds(20));
client.set_max_retries(2);  // Quick failover
```

#### High Reliability (File Transfer)
```cpp
client.set_congestion_window(128);
client.set_max_retries(10);
client.set_rto_max(std::chrono::seconds(10));
```
```

#### Step 4: Add Performance Characteristics
```markdown
## Performance Characteristics

### Benchmark Results (M1 Pro, 16GB RAM)

| Mode | Throughput | Latency (P50) | Latency (P99) |
|------|------------|---------------|---------------|
| Unreliable | 500K msg/s | 8 μs | 25 μs |
| Reliable Ordered | 150K msg/s | 45 μs | 200 μs |
| Reliable Unordered | 200K msg/s | 30 μs | 150 μs |
| Sequenced | 400K msg/s | 12 μs | 40 μs |

### Memory Usage

- Per connection: ~4KB base + buffer sizes
- Per pending message: ~100 bytes + message size

### CPU Usage

- Unreliable: Minimal overhead
- Reliable modes: Additional CPU for ACK processing and retransmission
```

#### Step 5: Create Example Code
```cpp
// samples/reliable_udp_example.cpp

#include "network_system/core/reliable_udp_client.h"
#include "network_system/core/reliable_udp_server.h"
#include <iostream>
#include <thread>

using namespace network_system;

// Example 1: Basic Echo Server
void run_echo_server() {
    reliable_udp_server server("echo-server");

    server.set_message_handler([&](auto client_id, auto data) {
        std::cout << "Received from " << client_id << ": "
                  << data.size() << " bytes\n";
        server.send_to(client_id, std::move(data));
    });

    server.start_server(5555);
    std::cout << "Server running on port 5555\n";

    // Keep running
    std::this_thread::sleep_for(std::chrono::hours(1));
}

// Example 2: Reliable Client with Retry
void run_reliable_client() {
    reliable_udp_client client("my-client", reliability_mode::reliable_ordered);

    client.set_max_retries(5);
    client.set_connection_callback([](bool connected) {
        std::cout << (connected ? "Connected!" : "Disconnected!") << "\n";
    });

    client.set_message_handler([](auto data) {
        std::cout << "Received: " << data.size() << " bytes\n";
    });

    auto result = client.start_client("localhost", 5555);
    if (!result) {
        std::cerr << "Failed to connect: " << result.error().message() << "\n";
        return;
    }

    // Send messages
    for (int i = 0; i < 100; ++i) {
        std::vector<uint8_t> msg = {'H', 'e', 'l', 'l', 'o', ' ',
                                    static_cast<uint8_t>('0' + i % 10)};
        client.send_packet(std::move(msg));
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Example 3: Game State Updates (Sequenced Mode)
void run_game_client() {
    reliable_udp_client client("game-client", reliability_mode::sequenced);

    client.set_congestion_window(16);  // Low latency
    client.set_message_handler([](auto data) {
        // Only latest state matters
        auto state = deserialize_game_state(data);
        update_local_state(state);
    });

    client.start_client("game-server.example.com", 7777);

    // Send position updates
    while (game_running) {
        auto position = get_player_position();
        client.send_packet(serialize(position));
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // 60 FPS
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "server") {
        run_echo_server();
    } else {
        run_reliable_client();
    }
    return 0;
}
```

---

## Test (Verification Plan)

### Documentation Review
1. Technical accuracy review
2. Example code compilation and execution test
3. Document link validity check

### Example Code Testing
```bash
# Build examples
cmake --build build --target reliable_udp_example

# Run server
./build/samples/reliable_udp_example server &

# Run client
./build/samples/reliable_udp_example client

# Verify output
```

### User Testing
1. Test if features can be used by reading documentation only
2. Determine if FAQ items need to be added

---

## Acceptance Criteria

- [ ] Main documentation written (English)
- [ ] Korean documentation written
- [ ] All reliability modes explained
- [ ] All configuration options documented
- [ ] Performance characteristics documented
- [ ] 3+ working example codes
- [ ] Consistency with API reference maintained

---

## Notes

- Consolidate duplicate content with existing UDP_SUPPORT.md
- Documentation should be readable independently
- Real use case-based examples recommended
