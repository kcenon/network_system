---
doc_id: "NET-API-002"
doc_title: "API Reference"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "API"
---

# API Reference

> **SSOT**: This document is the single source of truth for **API Reference**.

> **Language:** **English** | [한국어](API_REFERENCE.kr.md)

Comprehensive API documentation for the Network System library. The reference is split into two documents for easier navigation:

## Documents

### [API Reference - Facades](API_REFERENCE_FACADES.md)

All facade API documentation covering the simplified, high-level interfaces:

- **Overview** -- Namespace structure and header files
- **C++20 Concepts** -- Compile-time type validation concepts
- **Core Classes** -- NetworkServer, NetworkClient
- **Networking Components** -- Connection, ConnectionPool
- **Utility Classes** -- Message, Buffer
- **Error Handling** -- Exception types, Result type
- **Configuration** -- ServerConfig, ClientConfig
- **Examples** -- TCP server/client, HTTP server, WebSocket server, SSL/TLS, async client
- **Thread Safety** -- Concurrency guarantees
- **Performance Considerations** -- Zero-copy, memory pooling, batch operations
- **Platform-Specific Notes** -- Linux, Windows, macOS

### [API Reference - Protocols](API_REFERENCE_PROTOCOLS.md)

All protocol-level API documentation covering low-level protocol classes, unified interface, and gRPC:

- **Protocol Handlers** -- HTTP handler (HttpServer, Request, Response), WebSocket handler (WebSocketServer, WebSocketConnection)
- **Thread Integration API** -- thread_pool_interface, basic_thread_pool, thread_integration_manager, thread_system_pool_adapter
- **Version History** and **License**
