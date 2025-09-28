Architecture Overview
=====================

Purpose
- network_system is an asynchronous networking library (ASIO‑based) extracted from messaging_system for reuse and modularity.
- It provides connection/session management, a zero‑copy pipeline, and coroutine‑friendly APIs.

Layers
- Core: messaging_client/server, connection/session management
- Internal: tcp_socket, pipeline, send_coroutine, common_defs
- Integration: thread_integration (thread pool abstraction), logger_integration (logging abstraction)

Integration Flags
- `BUILD_WITH_THREAD_SYSTEM`: use external thread pool via adapters
- `BUILD_WITH_LOGGER_SYSTEM`: use logger_system adapter (fallback: basic_logger)
- `BUILD_WITH_CONTAINER_SYSTEM`: enable container_system adapters for serialization

Integration Topology
```
thread_system (optional) ──► network_system ◄── logger_system (optional)
             provides threads        provides logging

container_system ──► serialization ◄── messaging_system/database_system
```

Threading
- Pluggable thread_pool_interface so applications can inject thread_system or use the built‑in basic_thread_pool.

Logging
- logger_integration defines a minimal logger_interface and adapters. In absence of logger_system, a basic console logger is provided.

Performance
- Zero‑copy buffers on the pipeline; connection pooling; coroutine send/recv helpers.

Build
- C++20, CMake. Optional feature flags: BUILD_WITH_LOGGER_SYSTEM, USE_THREAD_SYSTEM.
