# TLS/SSL Setup Guide for network_system

> **Language:** **English** | [한국어](TLS_SETUP_GUIDE_KO.md)

## Table of Contents

- [Overview](#overview)
- [⚠️ Current Status](#-current-status)
- [Configuration Structure](#configuration-structure)
  - [Basic TLS Configuration](#basic-tls-configuration)
  - [Client Configuration](#client-configuration)
- [Security Best Practices](#security-best-practices)
  - [✅ DO](#-do)
  - [❌ DON'T](#-dont)
- [Certificate Management](#certificate-management)
  - [Generating Self-Signed Certificates (Development)](#generating-self-signed-certificates-development)
  - [Production Certificates](#production-certificates)
- [Configuration Examples](#configuration-examples)
  - [High Security Server](#high-security-server)
  - [Client with Mutual TLS (mTLS)](#client-with-mutual-tls-mtls)
- [Troubleshooting](#troubleshooting)
  - [Certificate Verification Fails](#certificate-verification-fails)
  - [Handshake Timeout](#handshake-timeout)
  - [Cipher Suite Mismatch](#cipher-suite-mismatch)
- [Implementation Roadmap](#implementation-roadmap)
  - [✅ Phase 1: Infrastructure (COMPLETED)](#-phase-1-infrastructure-completed)
  - [🔄 Phase 2: Core Integration (IN PROGRESS)](#-phase-2-core-integration-in-progress)
  - [📋 Phase 3: Advanced Features (PLANNED)](#-phase-3-advanced-features-planned)
- [Integration with ASIO SSL](#integration-with-asio-ssl)
  - [Future API (Preview)](#future-api-preview)
- [Security Auditing](#security-auditing)
  - [Recommended Tools](#recommended-tools)
- [Compliance](#compliance)
- [Further Reading](#further-reading)
- [Support](#support)

## Overview

This guide provides instructions for configuring and using TLS/SSL encryption in the network_system library. TLS (Transport Layer Security) is essential for securing network communications in production environments.

## ⚠️ Current Status

**TLS Infrastructure: READY**
**Full Implementation: IN PROGRESS**

The TLS configuration structures and security guidelines are now in place. Full integration with ASIO SSL sockets is planned for a future release.

## Configuration Structure

### Basic TLS Configuration

```cpp
#include "network_system/internal/common_defs.h"

using namespace network_system::internal;

// Server configuration with TLS
tls_config server_tls = tls_config::secure_defaults();
server_tls.certificate_file = "/path/to/server.crt";
server_tls.private_key_file = "/path/to/server.key";
server_tls.ca_file = "/path/to/ca.crt";

// Verify configuration
if (!server_tls.is_valid()) {
    throw std::runtime_error("Invalid TLS configuration");
}
```

### Client Configuration

```cpp
// Client configuration with TLS
tls_config client_tls = tls_config::secure_defaults();
client_tls.ca_file = "/path/to/ca.crt";
client_tls.sni_hostname = "example.com";

// For testing only (INSECURE!)
// auto client_tls = tls_config::insecure_for_testing();
```

## Security Best Practices

### ✅ DO

1. **Use TLS 1.2 or 1.3** - Always set `min_version` to `tls_version::tls_1_2` or higher
2. **Verify Peer Certificates** - Use `certificate_verification::verify_peer` in production
3. **Protect Private Keys** - Set file permissions to 600 (read/write owner only)
4. **Use Strong Ciphers** - Prefer ECDHE with AES-GCM or ChaCha20-Poly1305
5. **Enable SNI** - Set `sni_hostname` for clients to support virtual hosting
6. **Rotate Certificates** - Replace certificates before expiration

### ❌ DON'T

1. **Never disable verification** - `certificate_verification::none` is for testing only
2. **Don't use TLS 1.0/1.1** - These versions have known vulnerabilities
3. **Don't hardcode passwords** - Use environment variables or secure key stores
4. **Don't commit private keys** - Add `*.key` and `*.pem` to `.gitignore`
5. **Don't skip validation** - Always call `is_valid()` before using config

## Certificate Management

### Generating Self-Signed Certificates (Development)

```bash
# Generate CA key and certificate
openssl genrsa -out ca.key 4096
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
    -subj "/CN=Test CA/O=Development"

# Generate server key and CSR
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
    -subj "/CN=localhost/O=Development"

# Sign server certificate with CA
openssl x509 -req -days 365 -in server.csr \
    -CA ca.crt -CAkey ca.key -CAcreateserial \
    -out server.crt

# Set proper permissions
chmod 600 server.key ca.key
chmod 644 server.crt ca.crt
```

### Production Certificates

For production, use certificates from a trusted Certificate Authority (CA):
- Let's Encrypt (free, automated)
- DigiCert
- GlobalSign
- Your organization's internal CA

## Configuration Examples

### High Security Server

```cpp
tls_config secure_server;
secure_server.enabled = true;
secure_server.min_version = tls_version::tls_1_3;
secure_server.certificate_file = "/etc/ssl/certs/server.crt";
secure_server.private_key_file = "/etc/ssl/private/server.key";
secure_server.ca_file = "/etc/ssl/certs/ca-bundle.crt";
secure_server.verify_mode = certificate_verification::verify_fail_if_no_peer_cert;
secure_server.cipher_list = "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-RSA-CHACHA20-POLY1305";
secure_server.enable_session_resumption = true;
secure_server.handshake_timeout_ms = 5000;
```

### Client with Mutual TLS (mTLS)

```cpp
tls_config mtls_client;
mtls_client.enabled = true;
mtls_client.min_version = tls_version::tls_1_2;
mtls_client.certificate_file = "/path/to/client.crt";
mtls_client.private_key_file = "/path/to/client.key";
mtls_client.ca_file = "/path/to/ca.crt";
mtls_client.verify_mode = certificate_verification::verify_peer;
mtls_client.sni_hostname = "api.example.com";
```

## Troubleshooting

### Certificate Verification Fails

**Problem**: Connection fails with certificate verification error

**Solutions**:
1. Check that CA certificate is correct and trusted
2. Verify certificate chain is complete
3. Ensure server certificate CN/SAN matches hostname
4. Check certificate expiration date

```bash
# Verify certificate
openssl x509 -in server.crt -text -noout

# Check certificate chain
openssl verify -CAfile ca.crt server.crt
```

### Handshake Timeout

**Problem**: TLS handshake takes too long or times out

**Solutions**:
1. Increase `handshake_timeout_ms`
2. Check network latency
3. Verify server is responding
4. Check firewall rules

### Cipher Suite Mismatch

**Problem**: No common cipher suites between client and server

**Solutions**:
1. Update cipher list to include compatible ciphers
2. Check OpenSSL version compatibility
3. Enable more cipher suites (carefully)

## Implementation Roadmap

### ✅ Phase 1: Infrastructure (COMPLETED)
- TLS configuration structures
- Security enums and constants
- Validation logic
- Documentation

### 🔄 Phase 2: Core Integration (IN PROGRESS)
- ASIO SSL context integration
- SSL socket wrapper
- Certificate loading
- Handshake handling

### 📋 Phase 3: Advanced Features (PLANNED)
- Session resumption
- OCSP stapling
- Certificate pinning
- SNI callbacks

## Integration with ASIO SSL

### Future API (Preview)

```cpp
// This API is planned for future implementation
#include <asio/ssl.hpp>

// Server example (future)
asio::ssl::context ssl_ctx(asio::ssl::context::tlsv13);
ssl_ctx.use_certificate_file(tls_cfg.certificate_file.value(),
                             asio::ssl::context::pem);
ssl_ctx.use_private_key_file(tls_cfg.private_key_file.value(),
                            asio::ssl::context::pem);

// Create SSL socket
asio::ssl::stream<asio::ip::tcp::socket> ssl_socket(io_context, ssl_ctx);

// Perform handshake
ssl_socket.async_handshake(asio::ssl::stream_base::server,
    [](const asio::error_code& ec) {
        if (!ec) {
            // Handshake successful
        }
    });
```

## Security Auditing

### Recommended Tools

1. **testssl.sh** - Comprehensive TLS/SSL scanner
   ```bash
   ./testssl.sh --protocols --ciphers --server-defaults localhost:8443
   ```

2. **OpenSSL s_client** - Manual testing
   ```bash
   openssl s_client -connect localhost:8443 -tls1_2
   ```

3. **Nmap SSL Scripts** - Automated scanning
   ```bash
   nmap --script ssl-enum-ciphers -p 8443 localhost
   ```

## Compliance

This TLS configuration supports:
- ✅ PCI DSS 3.2+ (TLS 1.2+)
- ✅ HIPAA Security Rule
- ✅ GDPR Article 32
- ✅ NIST SP 800-52 Rev. 2

## Further Reading

- [OWASP TLS Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Transport_Layer_Protection_Cheat_Sheet.html)
- [Mozilla SSL Configuration Generator](https://ssl-config.mozilla.org/)
- [RFC 8446 - TLS 1.3](https://tools.ietf.org/html/rfc8446)
- [ASIO SSL Documentation](https://think-async.com/Asio/asio-1.18.0/doc/asio/overview/ssl.html)

## Support

For questions or issues regarding TLS configuration:
1. Check this documentation first
2. Review ASIO SSL examples
3. Consult OpenSSL documentation
4. File an issue in the project repository

---

**Last Updated**: 2025-10-19
**Status**: Infrastructure Complete, Full Integration Pending
