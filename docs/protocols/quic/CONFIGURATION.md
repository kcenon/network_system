# QUIC Configuration Guide

## Build Configuration

### Enabling QUIC Support

QUIC support is optional and must be enabled at compile time:

```bash
cmake -DBUILD_QUIC_SUPPORT=ON -DCMAKE_BUILD_TYPE=Release ..
make
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_QUIC_SUPPORT` | OFF | Enable QUIC protocol support |

### Dependencies

When `BUILD_QUIC_SUPPORT=ON`:
- OpenSSL 1.1.1+ is required for TLS 1.3 cryptography
- The `BUILD_WITH_OPENSSL` macro is automatically defined

---

## Client Configuration

### quic_client_config Options

```cpp
quic_client_config config;
```

#### TLS Configuration

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `ca_cert_file` | `optional<string>` | none | Path to CA certificate (PEM) |
| `client_cert_file` | `optional<string>` | none | Path to client certificate (PEM) |
| `client_key_file` | `optional<string>` | none | Path to client private key (PEM) |
| `verify_server` | `bool` | `true` | Verify server certificate |

**Basic TLS:**
```cpp
quic_client_config config;
config.ca_cert_file = "/etc/ssl/certs/ca-certificates.crt";
config.verify_server = true;
```

**Mutual TLS (mTLS):**
```cpp
quic_client_config config;
config.ca_cert_file = "/path/to/ca.pem";
config.client_cert_file = "/path/to/client.pem";
config.client_key_file = "/path/to/client-key.pem";
```

**Development/Testing (insecure):**
```cpp
quic_client_config config;
config.verify_server = false;  // WARNING: Only for testing!
```

#### ALPN Protocol Negotiation

```cpp
config.alpn_protocols = {"h3", "h3-29", "hq-interop"};
```

Common ALPN identifiers:
- `h3`: HTTP/3
- `h3-29`: HTTP/3 draft-29
- `hq-interop`: QUIC interop testing

#### Connection Parameters

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `max_idle_timeout_ms` | `uint64_t` | 30000 | Max idle timeout (ms) |
| `initial_max_data` | `uint64_t` | 1048576 | Connection-level flow control (1 MB) |
| `initial_max_stream_data` | `uint64_t` | 65536 | Stream-level flow control (64 KB) |
| `initial_max_streams_bidi` | `uint64_t` | 100 | Max bidirectional streams |
| `initial_max_streams_uni` | `uint64_t` | 100 | Max unidirectional streams |

**High-throughput configuration:**
```cpp
quic_client_config config;
config.initial_max_data = 16 * 1024 * 1024;        // 16 MB
config.initial_max_stream_data = 1024 * 1024;      // 1 MB per stream
config.initial_max_streams_bidi = 256;
```

**Low-latency configuration:**
```cpp
quic_client_config config;
config.max_idle_timeout_ms = 10000;  // 10 seconds
config.initial_max_data = 262144;    // 256 KB
config.initial_max_stream_data = 16384;  // 16 KB
```

#### 0-RTT Early Data

```cpp
config.enable_early_data = true;
config.session_ticket = previous_ticket;  // From prior connection
```

---

## Server Configuration

### quic_server_config Options

```cpp
quic_server_config config;
```

#### TLS Configuration (Required)

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `cert_file` | `string` | empty | Server certificate path (required) |
| `key_file` | `string` | empty | Server private key path (required) |
| `ca_cert_file` | `optional<string>` | none | CA cert for client verification |
| `require_client_cert` | `bool` | `false` | Require client certificate |

**Basic server TLS:**
```cpp
quic_server_config config;
config.cert_file = "/path/to/server.pem";
config.key_file = "/path/to/server-key.pem";
```

**Mutual TLS server:**
```cpp
quic_server_config config;
config.cert_file = "/path/to/server.pem";
config.key_file = "/path/to/server-key.pem";
config.ca_cert_file = "/path/to/ca.pem";
config.require_client_cert = true;
```

#### Connection Limits

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `max_connections` | `size_t` | 10000 | Maximum concurrent connections |
| `enable_retry` | `bool` | `true` | Enable retry for DoS protection |
| `retry_key` | `vector<uint8_t>` | empty | Key for retry token validation |

**High-capacity server:**
```cpp
quic_server_config config;
config.max_connections = 50000;
config.max_idle_timeout_ms = 60000;
config.initial_max_streams_bidi = 500;
```

**Security-focused configuration:**
```cpp
quic_server_config config;
config.enable_retry = true;
config.require_client_cert = true;
config.max_connections = 1000;  // Limited for security
```

---

## Performance Tuning

### Buffer Sizes

Adjust flow control limits based on expected data patterns:

```cpp
// Large file transfers
config.initial_max_data = 64 * 1024 * 1024;  // 64 MB
config.initial_max_stream_data = 16 * 1024 * 1024;  // 16 MB

// Small message protocols
config.initial_max_data = 1024 * 1024;  // 1 MB
config.initial_max_stream_data = 65536;  // 64 KB
```

### Stream Limits

Match stream limits to application needs:

```cpp
// Multiplexed RPC (many concurrent requests)
config.initial_max_streams_bidi = 1000;

// Simple request-response
config.initial_max_streams_bidi = 10;
```

### Timeout Configuration

```cpp
// Long-lived connections
config.max_idle_timeout_ms = 300000;  // 5 minutes

// Short-lived connections
config.max_idle_timeout_ms = 10000;  // 10 seconds
```

---

## Environment-Specific Configurations

### Development

```cpp
quic_client_config dev_config;
dev_config.verify_server = false;
dev_config.max_idle_timeout_ms = 60000;
```

### Production

```cpp
quic_client_config prod_config;
prod_config.ca_cert_file = "/etc/ssl/certs/ca-certificates.crt";
prod_config.verify_server = true;
prod_config.enable_early_data = true;
prod_config.max_idle_timeout_ms = 30000;
```

### High-Security

```cpp
quic_server_config secure_config;
secure_config.require_client_cert = true;
secure_config.ca_cert_file = "/path/to/trusted-ca.pem";
secure_config.enable_retry = true;
secure_config.max_connections = 1000;
```

---

## Certificate Generation

### Self-Signed Certificate (Development)

```bash
# Generate private key
openssl ecparam -genkey -name prime256v1 -out server-key.pem

# Generate self-signed certificate
openssl req -new -x509 -key server-key.pem -out server.pem -days 365 \
    -subj "/CN=localhost"
```

### CA-Signed Certificate (Production)

```bash
# Generate CSR
openssl req -new -key server-key.pem -out server.csr \
    -subj "/CN=example.com"

# Sign with CA (varies by CA provider)
# ...
```

---

## Troubleshooting

### Connection Failures

1. **Certificate verification failed**
   - Ensure `ca_cert_file` contains the correct CA
   - Check certificate chain is complete
   - Verify hostname matches certificate CN/SAN

2. **Handshake timeout**
   - Check firewall allows UDP traffic on the port
   - Verify server is listening
   - Check for NAT traversal issues

3. **Stream creation fails**
   - Increase `initial_max_streams_bidi/uni`
   - Check flow control limits

### Performance Issues

1. **Low throughput**
   - Increase `initial_max_data` and `initial_max_stream_data`
   - Check for packet loss (congestion control will reduce throughput)

2. **High latency**
   - Check RTT via `stats().smoothed_rtt`
   - Consider enabling 0-RTT for reconnections

3. **Connection drops**
   - Increase `max_idle_timeout_ms`
   - Implement keep-alive pings in application layer
