# Network System Documentation

> **Language:** **English** | [한국어](README_KO.md)

**Version:** 0.2.0
**Last Updated:** 2025-11-28
**Status:** Comprehensive

Welcome to the network_system documentation! This is a high-performance C++20 asynchronous network library providing reusable transport primitives for distributed systems and messaging applications.

---

## Quick Navigation

| I want to... | Document |
|--------------|----------|
| Get started quickly | [Build Guide](guides/BUILD.md) |
| Understand the architecture | [Architecture](ARCHITECTURE.md) |
| Learn the API | [API Reference](API_REFERENCE.md) |
| Use C++20 Concepts | [Concepts Guide](advanced/CONCEPTS.md) |
| Configure TLS/SSL | [TLS Setup Guide](guides/TLS_SETUP_GUIDE.md) |
| Troubleshoot an issue | [Troubleshooting](guides/TROUBLESHOOTING.md) |
| Tune performance | [Performance Tuning](advanced/PERFORMANCE_TUNING.md) |
| Integrate with other systems | [Integration Guide](INTEGRATION.md) |

---

## Table of Contents

- [Documentation Structure](#documentation-structure)
- [Documentation by Role](#documentation-by-role)
- [By Feature](#by-feature)
- [Contributing to Documentation](#contributing-to-documentation)

---

## Documentation Structure

### Core Documentation

Essential documents for understanding the system:

| Document | Description | Korean |
|----------|-------------|--------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | System design, ASIO integration, protocol layers | [KO](ARCHITECTURE_KO.md) |
| [API_REFERENCE.md](API_REFERENCE.md) | Complete API documentation with examples | [KO](API_REFERENCE_KO.md) |
| [FEATURES.md](FEATURES.md) | Detailed feature descriptions and capabilities | [KO](FEATURES_KO.md) |
| [BENCHMARKS.md](BENCHMARKS.md) | Performance metrics and methodology | [KO](BENCHMARKS_KO.md) |
| [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) | Codebase organization and directory layout | [KO](PROJECT_STRUCTURE_KO.md) |
| [PRODUCTION_QUALITY.md](PRODUCTION_QUALITY.md) | Quality metrics and CI/CD status | [KO](PRODUCTION_QUALITY_KO.md) |
| [CHANGELOG.md](CHANGELOG.md) | Version history and changes | [KO](CHANGELOG_KO.md) |

### User Guides

Step-by-step guides for users:

| Document | Description | Korean |
|----------|-------------|--------|
| [guides/BUILD.md](guides/BUILD.md) | Build instructions for all platforms | [KO](guides/BUILD_KO.md) |
| [guides/TLS_SETUP_GUIDE.md](guides/TLS_SETUP_GUIDE.md) | TLS/SSL configuration and certificates | [KO](guides/TLS_SETUP_GUIDE_KO.md) |
| [guides/TROUBLESHOOTING.md](guides/TROUBLESHOOTING.md) | Common problems and solutions | [KO](guides/TROUBLESHOOTING_KO.md) |
| [INTEGRATION.md](INTEGRATION.md) | Integration with ecosystem systems | [KO](INTEGRATION_KO.md) |
| [advanced/OPERATIONS.md](advanced/OPERATIONS.md) | Operational guidelines and deployment | [KO](advanced/OPERATIONS_KO.md) |
| [guides/LOAD_TEST_GUIDE.md](guides/LOAD_TEST_GUIDE.md) | Load testing methodology and tools | - |

### Advanced Topics

For experienced users and performance optimization:

| Document | Description |
|----------|-------------|
| [advanced/CONCEPTS.md](advanced/CONCEPTS.md) | C++20 Concepts for compile-time type validation |
| [advanced/PERFORMANCE_TUNING.md](advanced/PERFORMANCE_TUNING.md) | Performance optimization techniques |
| [advanced/CONNECTION_POOLING.md](advanced/CONNECTION_POOLING.md) | Connection pool architecture and usage |
| [advanced/UDP_RELIABILITY.md](advanced/UDP_RELIABILITY.md) | UDP reliability patterns and implementation |
| [advanced/HTTP_ADVANCED.md](advanced/HTTP_ADVANCED.md) | Advanced HTTP features and configuration |
| [advanced/MEMORY_PROFILING.md](advanced/MEMORY_PROFILING.md) | Memory analysis and optimization |
| [advanced/STATIC_ANALYSIS.md](advanced/STATIC_ANALYSIS.md) | Static analysis configuration and results |

### Contributing

| Document | Description |
|----------|-------------|
| [contributing/CONTRIBUTING.md](contributing/CONTRIBUTING.md) | Contribution guidelines |

### Implementation Details

Technical implementation documentation:

| Document | Description |
|----------|-------------|
| [implementation/README.md](implementation/README.md) | Implementation overview |
| [01-architecture-and-components.md](implementation/01-architecture-and-components.md) | Component architecture |
| [02-dependency-and-testing.md](implementation/02-dependency-and-testing.md) | Dependencies and test strategy |
| [03-performance-and-resources.md](implementation/03-performance-and-resources.md) | Performance characteristics |
| [04-error-handling.md](implementation/04-error-handling.md) | Error handling patterns |

### Integration Guides

System integration documentation:

| Document | Description |
|----------|-------------|
| [integration/README.md](integration/README.md) | Integration overview |
| [integration/with-common-system.md](integration/with-common-system.md) | Common system integration |
| [integration/with-logger.md](integration/with-logger.md) | Logger system integration |

---

## Documentation by Role

### For New Users

**Getting Started Path**:
1. **Build** - [Build Guide](guides/BUILD.md) to compile the library
2. **Architecture** - [Architecture](ARCHITECTURE.md) for system overview
3. **API** - [API Reference](API_REFERENCE.md) for basic usage
4. **Examples** - Check `samples/` directory for working examples

**When You Have Issues**:
- Check [Troubleshooting](guides/TROUBLESHOOTING.md) first
- Review [TLS Setup Guide](guides/TLS_SETUP_GUIDE.md) for SSL issues
- Search [GitHub Issues](https://github.com/kcenon/network_system/issues)

### For Experienced Developers

**Advanced Usage Path**:
1. **Architecture** - Deep dive into [Architecture](ARCHITECTURE.md)
2. **Performance** - Learn [Performance Tuning](advanced/PERFORMANCE_TUNING.md)
3. **Protocols** - Study protocol-specific docs (HTTP, WebSocket, UDP)
4. **Integration** - Review [Integration Guide](INTEGRATION.md)

**Deep Dive Topics**:
- [Connection Pooling](advanced/CONNECTION_POOLING.md) - Pool architecture
- [UDP Reliability](advanced/UDP_RELIABILITY.md) - Reliable UDP patterns
- [Memory Profiling](advanced/MEMORY_PROFILING.md) - Memory optimization

### For System Integrators

**Integration Path**:
1. **Integration Guide** - [System integration](INTEGRATION.md)
2. **Common System** - [Common system integration](integration/with-common-system.md)
3. **Logger** - [Logger integration](integration/with-logger.md)
4. **Operations** - [Deployment guide](advanced/OPERATIONS.md)

---

## By Feature

### TCP/UDP Networking

| Topic | Document |
|-------|----------|
| Architecture | [Architecture](ARCHITECTURE.md) |
| API | [API Reference](API_REFERENCE.md) |
| UDP Reliability | [UDP Reliability](advanced/UDP_RELIABILITY.md) |

### TLS/SSL Security

| Topic | Document |
|-------|----------|
| Setup | [TLS Setup Guide](guides/TLS_SETUP_GUIDE.md) |
| Configuration | [Architecture](ARCHITECTURE.md#tls-configuration) |
| Troubleshooting | [Troubleshooting](guides/TROUBLESHOOTING.md#tls-issues) |

### HTTP Protocol

| Topic | Document |
|-------|----------|
| Basic Usage | [API Reference](API_REFERENCE.md#http) |
| Advanced | [HTTP Advanced](advanced/HTTP_ADVANCED.md) |
| Performance | [Benchmarks](BENCHMARKS.md#http-performance) |

### WebSocket Protocol

| Topic | Document |
|-------|----------|
| Usage | [API Reference](API_REFERENCE.md#websocket) |
| Features | [Features](FEATURES.md#websocket) |
| Performance | [Benchmarks](BENCHMARKS.md#websocket-performance) |

### Performance

| Topic | Document |
|-------|----------|
| Benchmarks | [Benchmarks](BENCHMARKS.md) |
| Tuning | [Performance Tuning](advanced/PERFORMANCE_TUNING.md) |
| Profiling | [Memory Profiling](advanced/MEMORY_PROFILING.md) |
| Load Testing | [Load Test Guide](guides/LOAD_TEST_GUIDE.md) |

---

## Project Information

### Current Status
- **Version**: 0.2.0
- **Type**: Static Library
- **C++ Standard**: C++20 (C++17 compatible for some features)
- **License**: BSD 3-Clause
- **Test Status**: Production-ready

### Supported Protocols
- TCP with lifecycle management and health monitoring
- UDP with reliability options
- TLS 1.2/1.3 with modern cipher suites
- WebSocket (RFC 6455) with framing and keepalive
- HTTP/1.1 with routing, cookies, multipart support
- HTTP/2 frame and HPACK implementation
- DTLS support

### Key Features
- ASIO-based non-blocking event loop
- ~769K msg/sec for small payloads
- Move-semantics friendly APIs (zero-copy)
- C++20 coroutine support
- Pipeline architecture for message transformation
- Comprehensive error handling (Result<> types)

### Platform Support
- **Linux**: Ubuntu 22.04+, GCC 9+, Clang 10+
- **macOS**: 12+, Apple Clang 14+
- **Windows**: 11, MSVC 2019+

---

## Contributing to Documentation

### Documentation Standards
- Front matter on all documents (version, date, status)
- Code examples must compile
- Bilingual support (English/Korean)
- Cross-references with relative links

### Submission Process
1. Edit markdown files
2. Test all code examples
3. Update Korean translations if applicable
4. Submit pull request

---

## Getting Help

### Documentation Issues
- **Missing info**: Open documentation issue on GitHub
- **Incorrect examples**: Report with details
- **Unclear instructions**: Suggest improvements

### Technical Support
1. Check [Troubleshooting](guides/TROUBLESHOOTING.md)
2. Search [GitHub Issues](https://github.com/kcenon/network_system/issues)
3. Ask on GitHub Discussions

---

## External Resources

- **GitHub Repository**: [kcenon/network_system](https://github.com/kcenon/network_system)
- **Issue Tracker**: [GitHub Issues](https://github.com/kcenon/network_system/issues)
- **Main README**: [../README.md](../README.md)
- **Ecosystem Overview**: [../../docs/ECOSYSTEM_OVERVIEW.md](../../docs/ECOSYSTEM_OVERVIEW.md)

---

**Network System Documentation** - High-Performance C++20 Async Networking

**Last Updated**: 2025-12-10
