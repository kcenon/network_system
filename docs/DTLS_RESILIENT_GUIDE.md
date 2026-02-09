# DTLS Socket and Resilient Client Guide

> **Language:** **English**

A comprehensive guide to two internal utility components in the `network_system` library: the DTLS (Datagram Transport Layer Security) socket for encrypted UDP communication, and the resilient client with automatic reconnection and circuit breaker support.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [DTLS Socket](#dtls-socket)
  - [What is DTLS](#what-is-dtls)
  - [Memory BIO Architecture](#memory-bio-architecture)
  - [DTLS Handshake](#dtls-handshake)
  - [Sending Data](#sending-data)
  - [Receiving Data](#receiving-data)
  - [DTLS Socket API Reference](#dtls-socket-api-reference)
  - [Thread Safety Model](#thread-safety-model)
- [Resilient Client](#resilient-client)
  - [Overview](#overview)
  - [Construction and Configuration](#construction-and-configuration)
  - [Connection with Retry](#connection-with-retry)
  - [Send with Automatic Reconnection](#send-with-automatic-reconnection)
  - [Exponential Backoff Algorithm](#exponential-backoff-algorithm)
  - [Circuit Breaker Integration](#circuit-breaker-integration)
  - [Resilient Client API Reference](#resilient-client-api-reference)
- [Combined Usage Patterns](#combined-usage-patterns)
- [Production Deployment Patterns](#production-deployment-patterns)

## Architecture Overview

### Component Relationship

```
┌─────────────────────────────────────────────────────────┐
│                  User Application                       │
├─────────────────────────────────────────────────────────┤
│  resilient_client                                       │  ← Auto-reconnection
│    ├── messaging_client (TCP)                           │     + Circuit Breaker
│    ├── exponential backoff                              │
│    └── circuit_breaker (optional, WITH_COMMON_SYSTEM)   │
├─────────────────────────────────────────────────────────┤
│  dtls_socket                                            │  ← DTLS Encryption
│    ├── asio::ip::udp::socket (transport)                │     over UDP
│    ├── OpenSSL SSL/BIO (encryption)                     │
│    └── async handshake + send/receive                   │
├─────────────────────────────────────────────────────────┤
│              UDP / TCP (asio)                            │  ← Transport
└─────────────────────────────────────────────────────────┘
```

### Namespace and Location

| Component | Namespace | Header | Implementation |
|-----------|-----------|--------|----------------|
| `dtls_socket` | `kcenon::network::internal` | `src/internal/tcp/dtls_socket.h` | `src/internal/dtls_socket.cpp` |
| `resilient_client` | `kcenon::network::utils` | `src/internal/utils/resilient_client.h` | `src/internal/utils/resilient_client.cpp` |

Both are internal components (`src/internal/`) that affect user-facing behavior but are not directly exposed in the public API.

---

## DTLS Socket

**Header:** `src/internal/tcp/dtls_socket.h`
**Implementation:** `src/internal/dtls_socket.cpp`

### What is DTLS

DTLS (Datagram Transport Layer Security) provides TLS-equivalent security guarantees over UDP transport. Unlike TLS which operates over reliable TCP streams, DTLS must handle:

| Challenge | How DTLS Handles It |
|-----------|---------------------|
| Packet loss | Handshake retransmission timers |
| Packet reordering | Sequence numbers in records |
| No stream semantics | Message boundaries preserved |
| DoS amplification | Cookie exchange (HelloVerifyRequest) |

**When to use DTLS vs TLS:**

| Scenario | Protocol | Reason |
|----------|----------|--------|
| IoT sensor data | DTLS | Low latency, small messages |
| VoIP/real-time media | DTLS | UDP transport required |
| DNS over encryption | DTLS | Datagram-based protocol |
| Game server communication | DTLS | Low latency critical |
| REST API calls | TLS | Reliable delivery needed |
| File transfers | TLS | Ordered stream required |
| WebSocket connections | TLS | Stream-based protocol |

### Memory BIO Architecture

The `dtls_socket` uses OpenSSL memory BIOs (Basic I/O) to decouple encryption from transport, enabling non-blocking async I/O with Asio:

```
                      dtls_socket
┌──────────────────────────────────────────┐
│                                          │
│  Plaintext          OpenSSL              │  Encrypted
│  ──────────►  ┌──────────────┐  ──────── │ ──────────►
│  (user data)  │  SSL object  │  (wbio_)  │  (UDP send)
│               │              │           │
│  ◄──────────  │   rbio_ ←    │  ◄─────── │ ◄──────────
│  (callback)   │   wbio_ →    │  (read)   │  (UDP recv)
│               └──────────────┘           │
│                                          │
│  rbio_ = Read BIO (memory): network → SSL│
│  wbio_ = Write BIO (memory): SSL → network│
└──────────────────────────────────────────┘
```

**How it works:**

1. **Receiving:** Encrypted UDP datagram arrives → written to `rbio_` → `SSL_read()` decrypts → plaintext delivered via callback
2. **Sending:** Plaintext from user → `SSL_write()` encrypts → encrypted data read from `wbio_` → sent via UDP socket
3. **Handshake:** `SSL_do_handshake()` reads/writes through BIOs → BIO data flushed to/from UDP socket

### DTLS Handshake

The DTLS handshake is similar to TLS but adapted for unreliable transport:

```
Client                              Server
  │                                    │
  │──── ClientHello ──────────────►    │
  │                                    │
  │◄─── HelloVerifyRequest ─────────   │  (Cookie for DoS prevention)
  │                                    │
  │──── ClientHello + Cookie ─────►    │
  │                                    │
  │◄─── ServerHello ────────────────   │
  │◄─── Certificate ────────────────   │
  │◄─── ServerHelloDone ───────────    │
  │                                    │
  │──── ClientKeyExchange ────────►    │
  │──── ChangeCipherSpec ─────────►    │
  │──── Finished ─────────────────►    │
  │                                    │
  │◄─── ChangeCipherSpec ───────────   │
  │◄─── Finished ───────────────────   │
  │                                    │
  │     (Encrypted communication)      │
```

#### Client-Side Handshake

```cpp
#include <asio.hpp>
#include "internal/tcp/dtls_socket.h"
#include "internal/utils/openssl_compat.h"

using namespace kcenon::network::internal;

// 1. Set up OpenSSL DTLS context
SSL_CTX* ctx = SSL_CTX_new(DTLS_client_method());
SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
SSL_CTX_load_verify_locations(ctx, "ca-cert.pem", nullptr);

// 2. Create and connect UDP socket
asio::io_context io;
asio::ip::udp::socket udp_sock(io, asio::ip::udp::v4());
auto endpoint = asio::ip::udp::endpoint(
    asio::ip::make_address("192.168.1.100"), 4433);

// 3. Create DTLS socket
auto dtls = std::make_shared<dtls_socket>(std::move(udp_sock), ctx);
dtls->set_peer_endpoint(endpoint);

// 4. Perform async handshake
dtls->async_handshake(
    dtls_socket::handshake_type::client,
    [dtls](std::error_code ec) {
        if (ec) {
            std::cerr << "Handshake failed: " << ec.message() << "\n";
            return;
        }
        std::cout << "DTLS handshake complete!\n";
        // Now safe to send/receive encrypted data
    });

io.run();
```

#### Server-Side Handshake

```cpp
// 1. Set up OpenSSL DTLS server context
SSL_CTX* ctx = SSL_CTX_new(DTLS_server_method());
SSL_CTX_use_certificate_file(ctx, "server-cert.pem", SSL_FILETYPE_PEM);
SSL_CTX_use_PrivateKey_file(ctx, "server-key.pem", SSL_FILETYPE_PEM);

// 2. Create bound UDP socket
asio::io_context io;
asio::ip::udp::socket udp_sock(io,
    asio::ip::udp::endpoint(asio::ip::udp::v4(), 4433));

// 3. Create DTLS socket
auto dtls = std::make_shared<dtls_socket>(std::move(udp_sock), ctx);

// 4. Perform server-side handshake
dtls->async_handshake(
    dtls_socket::handshake_type::server,
    [dtls](std::error_code ec) {
        if (ec) {
            std::cerr << "Server handshake failed: " << ec.message() << "\n";
            return;
        }
        std::cout << "Client connected via DTLS\n";
    });

io.run();
```

### Sending Data

After handshake completion, data can be sent encrypted:

```cpp
// Send to connected peer
std::vector<uint8_t> payload = {'H', 'e', 'l', 'l', 'o'};
dtls->async_send(std::move(payload),
    [](std::error_code ec, std::size_t bytes_sent) {
        if (ec) {
            std::cerr << "Send failed: " << ec.message() << "\n";
            return;
        }
        std::cout << "Sent " << bytes_sent << " bytes (plaintext size)\n";
    });

// Send to specific endpoint (for server responding to different clients)
asio::ip::udp::endpoint client_ep(
    asio::ip::make_address("10.0.0.5"), 12345);

std::vector<uint8_t> response = {0x01, 0x02, 0x03};
dtls->async_send_to(std::move(response), client_ep,
    [](std::error_code ec, std::size_t bytes_sent) {
        if (ec) {
            std::cerr << "Send to failed: " << ec.message() << "\n";
            return;
        }
        // bytes_sent reflects original plaintext size, not encrypted size
    });
```

**Important:** `async_send()` and `async_send_to()` will fail with `std::errc::not_connected` if the handshake has not completed.

### Receiving Data

```cpp
// Set up receive callback (called for each decrypted datagram)
dtls->set_receive_callback(
    [](const std::vector<uint8_t>& data,
       const asio::ip::udp::endpoint& sender) {
        std::cout << "Received " << data.size()
                  << " bytes from " << sender << "\n";
        // Process decrypted data...
    });

// Set up error callback
dtls->set_error_callback(
    [](std::error_code ec) {
        std::cerr << "Socket error: " << ec.message() << "\n";
    });

// Start the continuous receive loop
dtls->start_receive();

// Later, stop receiving
dtls->stop_receive();
```

The receive loop is continuous: once started, it repeatedly receives encrypted datagrams, decrypts them, and invokes the callback until `stop_receive()` is called.

### DTLS Socket API Reference

#### Types

| Type | Definition | Description |
|------|------------|-------------|
| `handshake_type` | `enum class { client, server }` | Handshake role selection |

#### Constructor and Lifecycle

| Method | Signature | Description |
|--------|-----------|-------------|
| Constructor | `(asio::ip::udp::socket, SSL_CTX*)` | Create DTLS socket with UDP socket and OpenSSL context |
| Destructor | `~dtls_socket()` | Stops receive loop, performs SSL_shutdown, frees SSL resources |

The `dtls_socket` is **non-copyable** and **non-movable** (contains mutexes and atomics).

#### Handshake

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `async_handshake` | `(handshake_type, handler)` | `void` | Perform async DTLS handshake |
| `is_handshake_complete` | `() const` | `bool` | Check if handshake succeeded |

Handshake handler signature: `void(std::error_code)`

#### Data Transfer

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `async_send` | `(vector<uint8_t>&&, handler)` | `void` | Encrypt and send to connected peer |
| `async_send_to` | `(vector<uint8_t>&&, endpoint, handler)` | `void` | Encrypt and send to specific endpoint |
| `start_receive` | `()` | `void` | Begin continuous async receive loop |
| `stop_receive` | `()` | `void` | Stop the receive loop |

Send handler signature: `void(std::error_code, std::size_t)`

#### Callbacks

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `set_receive_callback` | `(function<void(const vector<uint8_t>&, const endpoint&)>)` | `void` | Set decrypted data handler |
| `set_error_callback` | `(function<void(std::error_code)>)` | `void` | Set error handler |

#### Endpoint Management

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `set_peer_endpoint` | `(const endpoint&)` | `void` | Set peer for connected mode |
| `peer_endpoint` | `() const` | `endpoint` | Get configured peer endpoint |
| `socket` | `()` | `udp::socket&` | Access underlying UDP socket |

### Thread Safety Model

The `dtls_socket` uses three separate mutexes for fine-grained locking:

| Mutex | Protects | Used By |
|-------|----------|---------|
| `ssl_mutex_` | OpenSSL SSL object operations | `SSL_read`, `SSL_write`, `SSL_do_handshake`, `BIO_read`, `BIO_write` |
| `callback_mutex_` | Callback registration and invocation | `set_receive_callback`, `set_error_callback`, callback dispatch |
| `endpoint_mutex_` | Peer endpoint access | `set_peer_endpoint`, `peer_endpoint`, `flush_bio_output` |

Atomic flags provide lock-free state queries:

| Atomic | Type | Purpose |
|--------|------|---------|
| `is_receiving_` | `atomic<bool>` | Receive loop active flag |
| `handshake_complete_` | `atomic<bool>` | Handshake success state |
| `handshake_in_progress_` | `atomic<bool>` | Handshake ongoing state |

**Callback invocation:** Callbacks are invoked on an Asio worker thread. Ensure callback logic is thread-safe if it accesses shared data.

---

## Resilient Client

**Header:** `src/internal/utils/resilient_client.h`
**Implementation:** `src/internal/utils/resilient_client.cpp`

### Overview

The `resilient_client` wraps `messaging_client` (TCP) to add:

1. **Automatic reconnection** on connection loss
2. **Exponential backoff** to prevent connection storms
3. **Circuit breaker pattern** to avoid cascade failures (when `WITH_COMMON_SYSTEM` is defined)
4. **Callback notifications** for reconnection and disconnection events

### Construction and Configuration

```cpp
#include "internal/utils/resilient_client.h"
using namespace kcenon::network::utils;

// Basic construction
auto client = std::make_shared<resilient_client>(
    "sensor-client",       // client_id: identifier for logging
    "192.168.1.100",       // host: server address
    8080,                  // port: server port
    5,                     // max_retries: max reconnection attempts (default: 3)
    std::chrono::seconds(2) // initial_backoff: starting delay (default: 1s)
);

// With circuit breaker (requires WITH_COMMON_SYSTEM)
#ifdef WITH_COMMON_SYSTEM
auto client = std::make_shared<resilient_client>(
    "sensor-client",
    "192.168.1.100",
    8080,
    5,                              // max retries
    std::chrono::seconds(1),        // initial backoff
    common::resilience::circuit_breaker_config{
        .failure_threshold = 5,     // open circuit after 5 failures
        .timeout = std::chrono::seconds(30)  // half-open after 30s
    }
);
#endif
```

### Constructor Parameters

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `client_id` | `const std::string&` | (required) | Unique identifier for logging |
| `host` | `const std::string&` | (required) | Server hostname or IP |
| `port` | `unsigned short` | (required) | Server port number |
| `max_retries` | `size_t` | `3` | Maximum reconnection attempts |
| `initial_backoff` | `std::chrono::milliseconds` | `1000ms` | Initial backoff duration |
| `cb_config` | `circuit_breaker_config` | `{}` | Circuit breaker config (WITH_COMMON_SYSTEM only) |

### Connection with Retry

```cpp
// Connect with automatic retry
auto result = client->connect();
if (!result) {
    std::cerr << "Failed after all retries: " << result.error().message << "\n";
    return;
}

// Check connection status
if (client->is_connected()) {
    std::cout << "Connected successfully\n";
}

// Disconnect
client->disconnect();
```

The `connect()` method:
1. Checks if already connected (returns `ok()` immediately)
2. Attempts connection up to `max_retries` times
3. Calls `reconnect_callback_` before each attempt
4. Applies exponential backoff between failed attempts
5. Returns error after all retries exhausted

### Send with Automatic Reconnection

```cpp
// Prepare data
std::vector<uint8_t> sensor_data = {0x01, 0x02, 0x03, 0x04};

// Send with automatic retry and reconnection
auto send_result = client->send_with_retry(std::move(sensor_data));
if (!send_result) {
    std::cerr << "Send failed: " << send_result.error().message << "\n";
}
```

The `send_with_retry()` method follows this flow:

```
send_with_retry(data)
     │
     ▼
┌────────────────────┐
│ Circuit breaker     │──── OPEN ────► Fail fast
│ allows request?     │               (circuit_open error)
└────────┬───────────┘
         │ YES
         ▼
┌────────────────────┐
│ Connected?         │──── NO ────► reconnect()
│                    │              │
└────────┬───────────┘              │
         │ YES                      │
         ▼                          ▼
┌────────────────────┐     ┌────────────────┐
│ send_packet()      │     │ Reconnected?   │──── NO ──► backoff + retry
│                    │     │                │
└────────┬───────────┘     └────────┬───────┘
         │                          │ YES
    ┌────┴────┐                     │
    │         │                     ▼
  SUCCESS   FAIL            Retry send_packet()
    │         │
    ▼         ▼
 record    record failure
 success   + disconnect
           + backoff + retry
```

### Callback Configuration

```cpp
// Reconnection event callback (called before each retry attempt)
client->set_reconnect_callback([](size_t attempt) {
    std::cout << "Reconnecting... attempt " << attempt << "\n";
    // Example: log to monitoring system, update UI status
});

// Disconnection event callback (called when connection is lost)
client->set_disconnect_callback([]() {
    std::cerr << "Connection lost!\n";
    // Example: switch to offline mode, queue messages
});

// Access underlying messaging_client for advanced configuration
auto inner = client->get_client();
// Configure protocol-specific settings on inner client...
```

### Exponential Backoff Algorithm

The backoff duration doubles with each attempt, capped at 30 seconds:

```
backoff = min(initial_backoff × 2^(attempt - 1), 30 seconds)
```

#### Backoff Table (initial_backoff = 1 second)

| Attempt | Formula | Backoff Duration |
|---------|---------|-----------------|
| 1 | 1s × 2^0 | **1 second** |
| 2 | 1s × 2^1 | **2 seconds** |
| 3 | 1s × 2^2 | **4 seconds** |
| 4 | 1s × 2^3 | **8 seconds** |
| 5 | 1s × 2^4 | **16 seconds** |
| 6 | 1s × 2^5 | **30 seconds** (capped) |
| 7+ | - | **30 seconds** (capped) |

#### Backoff Table (initial_backoff = 2 seconds)

| Attempt | Formula | Backoff Duration |
|---------|---------|-----------------|
| 1 | 2s × 2^0 | **2 seconds** |
| 2 | 2s × 2^1 | **4 seconds** |
| 3 | 2s × 2^2 | **8 seconds** |
| 4 | 2s × 2^3 | **16 seconds** |
| 5 | 2s × 2^4 | **30 seconds** (capped) |

**Key behavior:** No backoff sleep occurs after the last failed attempt (attempt == max_retries).

### Circuit Breaker Integration

When built with `WITH_COMMON_SYSTEM`, the resilient client integrates a circuit breaker pattern from the `common_system` library:

```
          ┌───────────┐
          │  CLOSED   │ ◄── Normal operation
          │           │     All requests pass through
          └─────┬─────┘
                │ failure_threshold reached
                ▼
          ┌───────────┐
          │   OPEN    │ ◄── Fail fast
          │           │     Reject all requests immediately
          └─────┬─────┘
                │ timeout expires
                ▼
          ┌───────────┐
          │ HALF_OPEN │ ◄── Probe
          │           │     Allow one request to test
          └─────┬─────┘
           ┌────┴────┐
         SUCCESS    FAIL
           │          │
           ▼          ▼
        CLOSED      OPEN
```

#### Circuit Breaker Behavior in send_with_retry()

| Circuit State | Behavior |
|---------------|----------|
| `CLOSED` | Normal: attempts send, records success/failure |
| `OPEN` | Fail fast: returns `circuit_open` error immediately |
| `HALF_OPEN` | Allows one probe request to test connectivity |

#### Checking Circuit State

```cpp
#ifdef WITH_COMMON_SYSTEM
auto state = client->circuit_state();
switch (state) {
    case common::resilience::circuit_state::CLOSED:
        std::cout << "Circuit healthy\n";
        break;
    case common::resilience::circuit_state::OPEN:
        std::cout << "Circuit open - backend unavailable\n";
        break;
    case common::resilience::circuit_state::HALF_OPEN:
        std::cout << "Circuit probing - testing connectivity\n";
        break;
}
#endif
```

### Resilient Client API Reference

#### Constructor and Lifecycle

| Method | Signature | Description |
|--------|-----------|-------------|
| Constructor | `(client_id, host, port, max_retries=3, initial_backoff=1s [, cb_config])` | Create with reconnection config |
| Destructor | `~resilient_client() noexcept` | Disconnects if connected |

#### Connection Management

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `connect` | `()` | `VoidResult` | Connect with retry logic |
| `disconnect` | `()` | `VoidResult` | Graceful disconnect |
| `is_connected` | `() const noexcept` | `bool` | Check connection (both internal state and client) |
| `get_client` | `() const` | `shared_ptr<messaging_client>` | Access underlying client |

#### Data Transfer

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `send_with_retry` | `(vector<uint8_t>&&)` | `VoidResult` | Send with auto-reconnect and circuit breaker |

#### Callbacks

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `set_reconnect_callback` | `(function<void(size_t)>)` | `void` | Called before each retry (1-based attempt) |
| `set_disconnect_callback` | `(function<void()>)` | `void` | Called when connection is lost |

#### Circuit Breaker (WITH_COMMON_SYSTEM only)

| Method | Signature | Returns | Description |
|--------|-----------|---------|-------------|
| `circuit_state` | `() const` | `circuit_state` | Get current circuit state |

---

## Combined Usage Patterns

### IoT Sensor with DTLS and Resilience

While `dtls_socket` and `resilient_client` serve different transport protocols (UDP vs TCP), both embody the same reliability principles. Here is a pattern combining DTLS security with reconnection resilience:

```cpp
// Resilient TCP client for command/control channel
auto control = std::make_shared<resilient_client>(
    "iot-controller", "server.example.com", 8080,
    10,                          // 10 retries for IoT reliability
    std::chrono::seconds(5)      // 5s initial backoff
);

control->set_reconnect_callback([](size_t attempt) {
    // Log reconnection attempt to local storage
    log_local("Control channel reconnecting, attempt: " + std::to_string(attempt));
});

control->set_disconnect_callback([]() {
    // Switch to buffered mode until reconnected
    enable_local_buffering();
});

auto result = control->connect();
if (result) {
    // Send buffered sensor readings
    std::vector<uint8_t> readings = collect_sensor_data();
    control->send_with_retry(std::move(readings));
}
```

### DTLS for Real-Time Encrypted Communication

```cpp
// Set up DTLS for time-sensitive encrypted datagrams
SSL_CTX* ctx = SSL_CTX_new(DTLS_client_method());
SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
SSL_CTX_load_verify_locations(ctx, "ca-cert.pem", nullptr);

asio::io_context io;
asio::ip::udp::socket sock(io, asio::ip::udp::v4());

auto dtls = std::make_shared<dtls_socket>(std::move(sock), ctx);
dtls->set_peer_endpoint(
    asio::ip::udp::endpoint(asio::ip::make_address("10.0.0.1"), 4433));

// Set up data handler before handshake
dtls->set_receive_callback(
    [](const std::vector<uint8_t>& data,
       const asio::ip::udp::endpoint& sender) {
        process_realtime_data(data);
    });

dtls->set_error_callback([](std::error_code ec) {
    handle_dtls_error(ec);
});

// Handshake, then start sending
dtls->async_handshake(
    dtls_socket::handshake_type::client,
    [dtls](std::error_code ec) {
        if (!ec) {
            // Start periodic data transmission
            start_sensor_stream(dtls);
        }
    });

io.run();
```

---

## Production Deployment Patterns

### Resilient Client: Recommended Settings

| Scenario | max_retries | initial_backoff | Notes |
|----------|-------------|-----------------|-------|
| Interactive API | 3 | 500ms | Fast fail for user-facing requests |
| Background worker | 10 | 2s | Persistent retry for batch jobs |
| IoT device | 20 | 5s | Long-lived connections, expect outages |
| Microservice mesh | 5 | 1s | Balance between fast recovery and backoff |

### DTLS: Certificate Configuration

| Configuration | Client | Server |
|---------------|--------|--------|
| Server cert verification | `SSL_CTX_load_verify_locations(ctx, "ca.pem", nullptr)` | N/A |
| Server certificate | N/A | `SSL_CTX_use_certificate_file(ctx, "cert.pem", SSL_FILETYPE_PEM)` |
| Server private key | N/A | `SSL_CTX_use_PrivateKey_file(ctx, "key.pem", SSL_FILETYPE_PEM)` |
| Mutual TLS (mTLS) | Both cert + key | `SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr)` |

### OpenSSL Version Requirement

The DTLS implementation requires **OpenSSL 3.x** (OpenSSL 1.1.x reached End-of-Life on September 11, 2023). The `openssl_compat.h` header enforces this at compile time:

```cpp
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    #define NETWORK_OPENSSL_VERSION_3_X 1
#else
    #error "OpenSSL 3.x or newer is required."
#endif
```

### Monitoring Checklist

| Metric | Source | Purpose |
|--------|--------|---------|
| Reconnection attempts | `set_reconnect_callback` | Track connection stability |
| Disconnection events | `set_disconnect_callback` | Alert on connection loss |
| Circuit breaker state | `circuit_state()` | Monitor backend health |
| Backoff duration | Log messages (`[resilient_client]`) | Diagnose retry storms |
| DTLS handshake success | `async_handshake` callback | Monitor TLS negotiation |
| DTLS errors | `set_error_callback` | Track encryption failures |

---

## Related Documentation

- [Facade API Guide](FACADE_GUIDE.md) - High-level protocol facades (including TCP facade used by resilient client)
- [Unified API Guide](UNIFIED_API_GUIDE.md) - Protocol-agnostic connection interfaces
- [HTTP/2 Guide](HTTP2_GUIDE.md) - HTTP/2 protocol with TLS support
