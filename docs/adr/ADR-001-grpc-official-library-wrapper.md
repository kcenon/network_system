# ADR-001: gRPC Official Library Wrapper Strategy

**Status:** Accepted
**Date:** 2024-12-28
**Decision Makers:** Development Team
**Related Issues:** #360, #361

---

## Context

The current gRPC implementation in network_system is at prototype stage. The code explicitly states:

> "This is a prototype implementation. For production use, consider wrapping the official gRPC library."

### Current State Analysis

**What Exists:**
- Basic client/server structure with PIMPL pattern
- Unary, Server streaming, Client streaming, Bidirectional streaming APIs defined
- HTTP/2 transport layer is complete (custom implementation)
- Clean interfaces: `grpc_server`, `grpc_client`, handler types
- Integration with `Result<T>` error handling from common_system

**Key Gaps:**
1. No official gRPC library integration
2. Limited error code mapping (basic status codes only)
3. No deadline/timeout propagation to transport layer
4. No interceptor/middleware support
5. No load balancing or service discovery
6. Missing advanced features (health checking, reflection, compression)

### Problem Statement

We need to decide between two approaches:
1. **Complete Custom Implementation:** Fully implement gRPC protocol over HTTP/2
2. **Official Library Wrapper:** Wrap the official grpc++ library while maintaining current API

---

## Decision

**We will wrap the official gRPC library (grpc++) while maintaining API compatibility with the existing prototype.**

---

## Evaluation Criteria

| Criteria | Weight | Custom Implementation | Official Wrapper |
|----------|--------|----------------------|------------------|
| **Production Readiness** | High | Low (requires extensive testing) | High (battle-tested) |
| **Development Effort** | High | Very High (months) | Medium (weeks) |
| **Maintenance Burden** | High | High (protocol updates) | Low (library updates) |
| **Feature Completeness** | Medium | Depends on effort | Full gRPC features |
| **API Compatibility** | High | Full control | Good (adapter pattern) |
| **Performance** | Medium | Unknown | Well-optimized |
| **Dependencies** | Low | Minimal | grpc++, protobuf, abseil |
| **Cross-Platform** | Medium | Already achieved | Supported |

---

## Rationale

### Why Official Library Wrapper

1. **Production Quality**
   - Official grpc++ is used in production at scale (Google, Netflix, etc.)
   - Years of performance optimization and bug fixes
   - Well-documented edge cases and failure modes

2. **Feature Completeness**
   - Deadline propagation across service boundaries
   - Interceptor/middleware support for logging, auth, metrics
   - Load balancing (round-robin, pick-first, custom)
   - Service discovery integration (DNS, xDS)
   - Health checking protocol
   - Reflection for debugging tools
   - Compression (gzip, deflate)

3. **Maintenance Efficiency**
   - Protocol changes handled by library updates
   - Security patches from upstream
   - Community support and documentation

4. **API Compatibility Strategy**
   - Existing API (`grpc_server`, `grpc_client`) remains unchanged
   - Internal implementation switches to official library
   - `Result<T>` integration via adapter layer
   - Gradual migration path for existing users

### Why Not Complete Custom Implementation

1. **Effort vs Value**
   - Estimated 3-6 months for production-quality implementation
   - Duplicates work already done by gRPC team
   - Risk of subtle protocol bugs

2. **Maintenance Burden**
   - gRPC protocol evolves (health checking, xDS, etc.)
   - Each update requires manual implementation
   - Testing matrix grows with protocol versions

3. **Feature Gap**
   - Many advanced features require significant effort
   - Interceptors, load balancing, service mesh integration
   - Would remain incomplete for extended period

---

## Implementation Strategy

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Code                          │
├─────────────────────────────────────────────────────────────┤
│              Existing API (grpc_server, grpc_client)         │
│              Result<T>, VoidResult error handling            │
├─────────────────────────────────────────────────────────────┤
│                   Adapter Layer (NEW)                        │
│   ┌─────────────────────┐  ┌─────────────────────────────┐  │
│   │ grpc_server_wrapper │  │ grpc_client_wrapper         │  │
│   │ - ::grpc::Server    │  │ - ::grpc::Channel           │  │
│   │ - Service adapter   │  │ - Stub management           │  │
│   └─────────────────────┘  └─────────────────────────────┘  │
│   ┌─────────────────────────────────────────────────────┐   │
│   │            Status/Error Conversion                   │   │
│   │  to_grpc_status() / from_grpc_status()              │   │
│   └─────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────┤
│                Official gRPC Library (grpc++)                │
│                        Protobuf                              │
│                        Abseil                                │
└─────────────────────────────────────────────────────────────┘
```

### CMake Configuration

```cmake
option(NETWORK_ENABLE_GRPC_OFFICIAL "Use official gRPC library" OFF)

if(NETWORK_ENABLE_GRPC_OFFICIAL)
    find_package(gRPC CONFIG REQUIRED)
    find_package(Protobuf CONFIG REQUIRED)

    target_compile_definitions(network_system_grpc PRIVATE
        NETWORK_GRPC_OFFICIAL=1
    )

    target_link_libraries(network_system_grpc PRIVATE
        gRPC::grpc++
        gRPC::grpc++_reflection
        protobuf::libprotobuf
    )
endif()
```

### Error Mapping

```cpp
namespace kcenon::network::protocols::grpc {

// Convert Result<T> to grpc::Status
template<typename T>
auto to_grpc_status(const Result<T>& result) -> ::grpc::Status {
    if (result.is_ok()) {
        return ::grpc::Status::OK;
    }

    const auto& err = result.error();
    return ::grpc::Status(
        map_error_code(err.code),
        err.message
    );
}

// Convert grpc::Status to Result<T>
template<typename T>
auto from_grpc_status(::grpc::Status status, T&& value) -> Result<T> {
    if (status.ok()) {
        return ok(std::forward<T>(value));
    }

    return error<T>(
        map_grpc_code(status.error_code()),
        std::string(status.error_message()),
        "grpc",
        std::string(status.error_details())
    );
}

}
```

### Backward Compatibility

| Scenario | Behavior |
|----------|----------|
| `NETWORK_ENABLE_GRPC_OFFICIAL=OFF` | Uses existing prototype implementation |
| `NETWORK_ENABLE_GRPC_OFFICIAL=ON` | Uses official library via wrapper |
| API compatibility | 100% - existing code compiles unchanged |
| Handler signatures | Unchanged - same function types |
| Result types | Unchanged - Result<T>, VoidResult |

---

## Consequences

### Positive

- Production-ready gRPC implementation with minimal effort
- Access to full gRPC feature set
- Reduced maintenance burden for protocol updates
- Community support and documentation
- Performance optimizations from upstream

### Negative

- Additional dependencies (grpc++, protobuf, abseil)
- Larger binary size when enabled
- Build complexity (gRPC build can be slow)
- Dependency version management

### Neutral

- CMake option makes it opt-in
- Existing prototype remains available as fallback
- No breaking API changes

---

## Alternatives Considered

### Alternative 1: Complete Custom Implementation

**Rejected** because the effort required (3-6 months) outweighs the benefits. The existing HTTP/2 layer is good, but implementing all gRPC features properly would duplicate significant work.

### Alternative 2: Different gRPC Library (e.g., grpc-cpp-ng)

**Rejected** because official grpc++ has the largest community, best documentation, and longest track record. Alternative libraries may have compatibility issues or less active maintenance.

### Alternative 3: Keep Prototype Only

**Rejected** because the prototype is explicitly documented as unsuitable for production use. Users need a production-ready option.

---

## References

- [gRPC Official Documentation](https://grpc.io/docs/)
- [grpc++ GitHub Repository](https://github.com/grpc/grpc)
- [gRPC Performance Benchmarks](https://grpc.io/docs/guides/benchmarking/)
- [Issue #360: Complete gRPC Implementation](https://github.com/kcenon/network_system/issues/360)

---

## Changelog

| Date | Change |
|------|--------|
| 2024-12-28 | Initial decision documented |
