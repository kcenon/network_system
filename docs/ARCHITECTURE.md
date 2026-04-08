---
doc_id: "NET-ARCH-002"
doc_title: "Network System Architecture"
doc_version: "1.0.0"
doc_date: "2026-04-04"
doc_status: "Released"
project: "network_system"
category: "ARCH"
---

# Network System Architecture

> **SSOT**: This document is the single source of truth for **Network System Architecture**.

> **Language:** **English** | [한국어](ARCHITECTURE.kr.md)

> **Cross-reference**:
> [API Reference](./API_REFERENCE.md) -- Public API details for all network components
> [Integration Guide](./INTEGRATION.md) -- Ecosystem integration patterns and adapters
> [Facade Guide](./FACADE_GUIDE.md) -- Simplified protocol facade API documentation
> [Design Decisions](./DESIGN_DECISIONS.md) -- ADRs and rationale for architectural choices

Comprehensive architectural documentation for network_system. The architecture reference is split into two documents for easier navigation:

## Documents

### [Architecture - Overview](ARCHITECTURE_OVERVIEW.md)

High-level architecture, design principles, and component overview:

- **Purpose & Design Goals** -- Primary purpose, design goals and status
- **Architecture Layers** -- Layered architecture diagram, facade layer, core layer, session management, internal implementation, integration layer, platform layer
- **Core Components** -- Composition-based architecture, interface hierarchy, composition utilities
- **Integration Architecture** -- Thread system, logger system, container system, common system integration
- **Build Configuration** -- CMake options, integration topology
- **Design Patterns** -- Proactor, adapter, observer, object pool patterns
- **Ecosystem Dependencies** -- Tier positioning and dependency graph
- **References** -- External documentation links

### [Architecture - Protocols](ARCHITECTURE_PROTOCOLS.md)

Protocol-specific architecture details and implementation notes:

- **Protocol Components** -- MessagingServer, MessagingClient, MessagingSession, Pipeline (zero-copy buffer)
- **Threading Model** -- ASIO event loop, thread pool integration
- **Memory Management** -- Zero-copy pipeline, buffer management strategy, memory profiling
- **Performance Characteristics** -- Throughput benchmarks, optimization techniques, scalability
- **Future Enhancements** -- Planned features and roadmap
