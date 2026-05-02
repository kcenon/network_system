##################################################
# network_system_targets.cmake
#
# Library target rules for the main network_system library.
#
# This module owns:
#   - network_system (the main library + ALIAS)
#   - All optional feature wiring on the main target:
#     TLS/SSL, WebSocket, LZ4, ZLIB, official gRPC,
#     verify_build, messaging bridge, integration setup.
#
# Preconditions (must be satisfied before include()ing):
#   - find_network_system_dependencies() has been called
#     (provides ASIO discovery state, GRPC_FOUND, etc.)
#   - check_network_system_features() has been called
#   - network_system_integration_objects.cmake has already
#     defined network-integration-objects.
#   - libs/network-core, libs/network-tcp, libs/network-udp,
#     libs/network-* protocol subdirectories have already
#     been add_subdirectory()'d so that the TARGET <name>
#     checks below match (Issue #538/554/618).
#
# Why ordering matters: network_system links PUBLIC against
# network-core / network-tcp / network-udp to inherit the
# include paths exposed by network-integration-objects (most
# notably the root-level network-core/include path that
# resolves `kcenon/network-core/...` headers). Skip the
# preceding subdirectories and the link calls below silently
# become no-ops, leading to "file not found" errors.
##################################################

##################################################
# Create Main Library
##################################################

add_library(network_system
    # Main API implementation
    src/network_system.cpp

    # Core implementation (stable TCP/UDP)
    # Note: network_context.cpp is in network-integration-objects to avoid ODR violations
    # UDP messaging moved to libs/network-udp (Issue #561)
    src/core/messaging_client.cpp
    src/core/messaging_server.cpp
    src/core/messaging_udp_client.cpp
    src/core/messaging_udp_server.cpp
    src/core/connection_pool.cpp

    # HTTP layer (stable)
    src/http/http_client.cpp
    src/http/http_server.cpp

    # Experimental protocols
    # reliable_udp_client moved to libs/network-udp (Issue #561)
    src/experimental/quic_client.cpp
    src/experimental/quic_server.cpp

    # Session management
    # Note: messaging_session.cpp is in network-integration-objects to avoid ODR violations
    src/session/quic_session.cpp

    # Core type-erased session management (Issue #522)
    src/core/unified_session_manager.cpp

    # Unified interface adapters (Issue #521 Phase 2)
    # TCP adapters moved to libs/network-tcp (Issue #554)
    # UDP adapters moved to libs/network-udp (Issue #561)
    src/unified/adapters/ws_connection_adapter.cpp
    src/unified/adapters/ws_listener_adapter.cpp
    src/unified/adapters/quic_connection_adapter.cpp
    src/unified/adapters/quic_listener_adapter.cpp

    # Protocol factory functions (Issue #521 Phase 2)
    # TCP protocol moved to libs/network-tcp (Issue #554)
    # UDP protocol moved to libs/network-udp (Issue #561)
    src/protocol/websocket.cpp
    src/protocol/quic.cpp

    # Internal implementation
    # TCP socket moved to libs/network-tcp (Issue #554)
    # UDP socket moved to libs/network-udp (Issue #561)
    src/internal/http_types.cpp
    src/internal/http_parser.cpp
    src/internal/http_error.cpp

    # Utility implementations
    src/internal/utils/resilient_client.cpp
    src/internal/utils/health_monitor.cpp
    src/internal/utils/buffer_pool.cpp
    src/internal/utils/compression_pipeline.cpp

    # Facade layer - simplified protocol creation APIs (Issue #596)
    src/facade/tcp_facade.cpp
    src/facade/udp_facade.cpp
    src/facade/http_facade.cpp
    src/facade/websocket_facade.cpp
    src/facade/quic_facade.cpp

    # Protocol adapters - bridge legacy implementations to unified interfaces
    src/adapters/http_client_adapter.cpp
    src/adapters/http_server_adapter.cpp
    src/adapters/quic_client_adapter.cpp
    src/adapters/quic_server_adapter.cpp
    src/adapters/tcp_server_adapter.cpp
    src/adapters/udp_client_adapter.cpp
    src/adapters/udp_server_adapter.cpp
    src/adapters/ws_client_adapter.cpp
    src/adapters/ws_server_adapter.cpp

    # Basic integration layer - container_integration and messaging_bridge
    # (Other integration sources are in network-integration-objects to avoid ODR violations)
    # messaging_bridge.cpp is here because it depends on messaging_client/server
    src/integration/container_integration.cpp
    src/integration/messaging_bridge.cpp

    # Tracing implementation (OpenTelemetry-compatible)
    src/tracing/trace_context.cpp
    src/tracing/span.cpp
    src/tracing/exporters.cpp

    # HTTP/2 protocol implementation (frame and hpack are TLS-independent)
    src/protocols/http2/frame.cpp
    src/protocols/http2/hpack.cpp

    # gRPC protocol implementation (prototype)
    src/protocols/grpc/frame.cpp
    src/protocols/grpc/client.cpp
    src/protocols/grpc/server.cpp
    src/protocols/grpc/service_registry.cpp

    # QUIC protocol implementation (RFC 9000)
    src/protocols/quic/varint.cpp
    src/protocols/quic/frame.cpp
    src/protocols/quic/connection_id.cpp
    src/protocols/quic/packet.cpp
    src/protocols/quic/keys.cpp
    src/protocols/quic/crypto.cpp
    src/protocols/quic/session_ticket_store.cpp
    src/protocols/quic/stream.cpp
    src/protocols/quic/stream_manager.cpp
    src/protocols/quic/flow_control.cpp
    src/protocols/quic/transport_params.cpp
    src/protocols/quic/connection.cpp
    src/protocols/quic/connection_id_manager.cpp
    src/protocols/quic/rtt_estimator.cpp
    src/protocols/quic/loss_detector.cpp
    src/protocols/quic/congestion_controller.cpp
    src/protocols/quic/ecn_tracker.cpp
    src/protocols/quic/pmtud_controller.cpp

    # QUIC socket (UDP wrapper with packet protection)
    src/internal/quic_socket.cpp
)

# Alias for build-tree consumers (FetchContent / add_subdirectory)
add_library(network_system::network_system ALIAS network_system)

# Suppress warnings inherited from parent project (especially from ASIO)
# WARNING: Suppressions removed for strict code quality check.
# Fix the code instead of suppressing warnings!
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # target_compile_options(network_system PRIVATE
    #     -Wno-sign-conversion
    #     -Wno-conversion
    #     -Wno-implicit-fallthrough
    #     -Wno-shadow
    #     -Wno-unused-parameter
    #     -Wno-unused-but-set-variable
    #     -Wno-unused-variable
    # )
elseif(MSVC)
    # target_compile_options(network_system PRIVATE
    #     /wd4244  # conversion from 'type1' to 'type2', possible loss of data
    #     /wd4267  # conversion from 'size_t' to 'type', possible loss of data
    #     /wd4100  # unreferenced formal parameter
    #     /wd4101  # unreferenced local variable
    #     /wd4456  # declaration hides previous local declaration
    #     /wd4457  # declaration hides function parameter
    # )
endif()

# TLS/SSL support (conditional)
option(BUILD_TLS_SUPPORT "Build with TLS/SSL support for secure messaging" ON)
option(BUILD_LZ4_COMPRESSION "Build with LZ4 compression support" ON)

if(BUILD_TLS_SUPPORT)
    target_sources(network_system PRIVATE
        # secure_tcp_socket moved to libs/network-tcp (Issue #554)
        # secure_messaging_udp moved to libs/network-udp (Issue #561)
        src/internal/dtls_socket.cpp
        src/session/secure_session.cpp
        src/core/secure_messaging_server.cpp
        src/core/secure_messaging_client.cpp
        # HTTP/2 client requires TLS (ALPN negotiation)
        src/protocols/http2/http2_client.cpp
        # HTTP/2 server requires TLS (ALPN negotiation)
        src/protocols/http2/http2_server.cpp
        src/protocols/http2/http2_server_stream.cpp
    )
    target_compile_definitions(network_system PUBLIC BUILD_TLS_SUPPORT)

    # Find OpenSSL for TLS/SSL
    # Minimum requirement: OpenSSL 3.0.0
    # OpenSSL 1.1.x reached EOL on September 11, 2023 and is no longer supported
    find_package(OpenSSL 3.0.0 REQUIRED)
    target_link_libraries(network_system PUBLIC OpenSSL::SSL OpenSSL::Crypto)

    message(STATUS "TLS/SSL support enabled (TLS 1.2/1.3)")
    message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")
endif()

# WebSocket support (conditional) - part of HTTP layer
if(BUILD_WEBSOCKET_SUPPORT)
    target_sources(network_system PRIVATE
        src/internal/websocket_frame.cpp
        src/internal/websocket_handshake.cpp
        src/internal/websocket_protocol.cpp
        src/internal/websocket_socket.cpp
        src/http/websocket_client.cpp
        src/http/websocket_server.cpp
    )
    target_compile_definitions(network_system PUBLIC BUILD_WEBSOCKET_SUPPORT)

    # Find OpenSSL for SHA-1 hashing in handshake (if not already found)
    if(NOT BUILD_TLS_SUPPORT)
        find_package(OpenSSL 3.0.0 REQUIRED)
        target_link_libraries(network_system PRIVATE OpenSSL::Crypto)
        message(STATUS "  OpenSSL version: ${OPENSSL_VERSION}")
    endif()

    message(STATUS "WebSocket support enabled")
endif()

# LZ4 compression support (conditional)
if(BUILD_LZ4_COMPRESSION)
    # Try to find lz4 library directly (works better on both macOS and Linux)
    find_library(LZ4_LIBRARY NAMES lz4)
    find_path(LZ4_INCLUDE_DIR NAMES lz4.h)

    if(LZ4_LIBRARY AND LZ4_INCLUDE_DIR)
        target_compile_definitions(network_system PUBLIC BUILD_LZ4_COMPRESSION)
        target_include_directories(network_system PRIVATE ${LZ4_INCLUDE_DIR})
        target_link_libraries(network_system PRIVATE ${LZ4_LIBRARY})
        message(STATUS "LZ4 compression enabled")
        message(STATUS "  LZ4 library: ${LZ4_LIBRARY}")
        message(STATUS "  LZ4 include: ${LZ4_INCLUDE_DIR}")
    else()
        # Fallback to pkg-config
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LZ4 QUIET liblz4)
            if(LZ4_FOUND)
                target_compile_definitions(network_system PUBLIC BUILD_LZ4_COMPRESSION)
                target_include_directories(network_system PRIVATE ${LZ4_INCLUDE_DIRS})
                target_link_libraries(network_system PRIVATE ${LZ4_LIBRARIES})
                message(STATUS "LZ4 compression enabled (via pkg-config)")
                message(STATUS "  LZ4 version: ${LZ4_VERSION}")
            else()
                message(WARNING "LZ4 library not found. Compression support will be disabled.")
                message(STATUS "Install LZ4: brew install lz4 (macOS) or apt-get install liblz4-dev (Linux)")
            endif()
        else()
            message(WARNING "LZ4 library not found. Compression support will be disabled.")
            message(STATUS "Install LZ4: brew install lz4 (macOS) or apt-get install liblz4-dev (Linux)")
        endif()
    endif()
endif()

# ZLIB compression support (for gzip/deflate)
option(BUILD_ZLIB_COMPRESSION "Build with ZLIB compression support (gzip/deflate)" ON)

if(BUILD_ZLIB_COMPRESSION)
    find_package(ZLIB QUIET)

    if(ZLIB_FOUND)
        target_compile_definitions(network_system PUBLIC BUILD_ZLIB_COMPRESSION)
        target_link_libraries(network_system PRIVATE ZLIB::ZLIB)
        message(STATUS "ZLIB compression enabled (gzip/deflate)")
        message(STATUS "  ZLIB version: ${ZLIB_VERSION_STRING}")
    else()
        message(WARNING "ZLIB library not found. gzip/deflate compression will be disabled.")
        message(STATUS "Install ZLIB: vcpkg install zlib or system package manager")
    endif()
endif()

# Official gRPC library support (conditional)
# See ADR-001 for architecture decision details
# gRPC detection is done in NetworkSystemDependencies.cmake via find_grpc_library()
if(NETWORK_ENABLE_GRPC_OFFICIAL AND GRPC_FOUND)
    message(STATUS "")
    message(STATUS "========================================")
    message(STATUS "Official gRPC Integration (Issue #360)")
    message(STATUS "========================================")

    # Add gRPC wrapper sources
    target_sources(network_system PRIVATE
        src/protocols/grpc/grpc_official_wrapper.cpp
    )

    if(GRPC_CMAKE_CONFIG)
        # Use CMake config targets (preferred)
        target_link_libraries(network_system PRIVATE
            gRPC::grpc++
            gRPC::grpc++_reflection
            protobuf::libprotobuf
        )

        # Link abseil if found separately (gRPC 1.50+ requirement)
        if(ABSEIL_FOUND)
            # abseil is typically linked transitively through gRPC::grpc++
            # but explicit linking may be needed for some configurations
            message(STATUS "  Abseil: Linked via gRPC")
        endif()

        message(STATUS "  gRPC++: Enabled (CMake config)")
        message(STATUS "  Reflection: Enabled")
        if(DEFINED Protobuf_VERSION)
            message(STATUS "  Protobuf: ${Protobuf_VERSION}")
        endif()
    elseif(GRPC_PKGCONFIG)
        # Use pkg-config libraries
        target_include_directories(network_system PRIVATE
            ${GRPCPP_INCLUDE_DIRS}
            ${PROTOBUF_INCLUDE_DIRS}
        )

        target_link_libraries(network_system PRIVATE
            ${GRPCPP_LIBRARIES}
            ${PROTOBUF_LIBRARIES}
        )

        message(STATUS "  gRPC++: Enabled (pkg-config)")
        message(STATUS "  Reflection: Manual linking required")
    endif()

    # Define compile-time flag
    target_compile_definitions(network_system PUBLIC NETWORK_GRPC_OFFICIAL=1)

    message(STATUS "")
    message(STATUS "Note: Existing gRPC API (grpc_server, grpc_client) remains unchanged.")
    message(STATUS "      Internal implementation now uses official gRPC library.")
    message(STATUS "========================================")
elseif(NETWORK_ENABLE_GRPC_OFFICIAL AND NOT GRPC_FOUND)
    # gRPC was requested but not found - warning already printed by find_grpc_library()
    message(STATUS "Official gRPC: Disabled (gRPC not found, using prototype)")
else()
    message(STATUS "Official gRPC: Disabled (using prototype implementation)")
endif()

# Set target properties
set_target_properties(network_system PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Include directories
target_include_directories(network_system
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/internal_include
)

# Link to network-core for shared interfaces (Issue #538, #539)
if(TARGET network-core)
    target_link_libraries(network_system PUBLIC network-core)
endif()

# Link to network-tcp for TCP protocol support (Issue #554)
if(TARGET network-tcp)
    target_link_libraries(network_system PUBLIC network-tcp)
endif()

# Link to network-udp for UDP protocol support (Issue #618)
if(TARGET network-udp)
    target_link_libraries(network_system PUBLIC network-udp)
endif()

# Note: Integration objects are provided by network-tcp (ODR fix - Issue #555)
# network_system gets these symbols through linking with network-tcp

##################################################
# Configure Integrations
##################################################

setup_network_system_integrations(network_system)

# Messaging bridge (optional - requires external systems)
if(BUILD_MESSAGING_BRIDGE)
    target_sources(network_system PRIVATE
        src/integration/messaging_bridge.cpp
    )
    target_compile_definitions(network_system PRIVATE BUILD_MESSAGING_BRIDGE)
endif()

##################################################
# Build Verification Test
##################################################

# Only build verify_build if all required libraries are available
# Disabled by default as it requires external system libraries
option(BUILD_VERIFY_BUILD "Build verify_build executable" OFF)

if(BUILD_VERIFY_BUILD)
    add_executable(verify_build verify_build.cpp)
    target_link_libraries(verify_build PRIVATE network_system)

    # Add system include paths to verify_build
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(verify_build PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(verify_build PRIVATE WITH_CONTAINER_SYSTEM)
    endif()

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(verify_build PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(verify_build PRIVATE WITH_THREAD_SYSTEM)
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(verify_build PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        target_compile_definitions(verify_build PRIVATE WITH_LOGGER_SYSTEM)
    endif()

    if(BUILD_MESSAGING_BRIDGE)
        target_compile_definitions(verify_build PRIVATE BUILD_MESSAGING_BRIDGE)
    endif()

    # Configure ASIO for verify_build
    setup_asio_integration(verify_build)

    set_target_properties(verify_build PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )
endif()
