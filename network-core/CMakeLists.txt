cmake_minimum_required(VERSION 3.21)

project(network-core
    VERSION 1.0.0
    DESCRIPTION "Core interfaces and abstractions for network_system protocol libraries"
    LANGUAGES CXX
)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(KCENON_WITH_COMMON_SYSTEM "Enable integration with common_system" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(KCENON_NETWORK_CORE_BUILD_TESTS "Build network-core tests" ON)

# Source files
set(NETWORK_CORE_SOURCES
    # Metrics
    src/metrics/network_metrics.cpp
    src/metrics/histogram.cpp
    src/metrics/sliding_histogram.cpp

    # Tracing
    src/tracing/trace_context.cpp
    src/tracing/span.cpp

    # Integration
    src/integration/logger_integration.cpp
    src/integration/monitoring_integration.cpp
    src/integration/thread_integration.cpp
    src/integration/container_integration.cpp
    src/integration/thread_system_adapter.cpp
    src/integration/io_context_thread_manager.cpp
)

# Header files (for IDE support)
set(NETWORK_CORE_HEADERS
    # Interfaces
    include/kcenon/network-core/interfaces/i_network_component.h
    include/kcenon/network-core/interfaces/i_client.h
    include/kcenon/network-core/interfaces/i_server.h
    include/kcenon/network-core/interfaces/connection_observer.h

    # Core
    include/kcenon/network-core/core/session_concept.h
    include/kcenon/network-core/core/session_handle.h
    include/kcenon/network-core/core/session_model.h
    include/kcenon/network-core/core/session_traits.h
    include/kcenon/network-core/core/session_info.h
    include/kcenon/network-core/core/connection_pool.h

    # Unified
    include/kcenon/network-core/unified/types.h
    include/kcenon/network-core/unified/i_transport.h
    include/kcenon/network-core/unified/i_connection.h
    include/kcenon/network-core/unified/i_listener.h
    include/kcenon/network-core/unified/unified.h

    # Utils
    include/kcenon/network-core/utils/result_types.h
    include/kcenon/network-core/utils/buffer_pool.h
    include/kcenon/network-core/utils/connection_state.h
    include/kcenon/network-core/utils/lifecycle_manager.h
    include/kcenon/network-core/utils/callback_manager.h
    include/kcenon/network-core/utils/startable_base.h

    # Concepts
    include/kcenon/network-core/concepts/concepts.h
    include/kcenon/network-core/concepts/network_concepts.h

    # Config
    include/kcenon/network-core/config/feature_flags.h
    include/kcenon/network-core/config/network_config.h

    # Internal
    include/kcenon/network-core/internal/common_defs.h

    # Metrics
    include/kcenon/network-core/metrics/network_metrics.h
    include/kcenon/network-core/metrics/histogram.h
    include/kcenon/network-core/metrics/sliding_histogram.h

    # Events
    include/kcenon/network-core/events/network_metric_event.h

    # Tracing
    include/kcenon/network-core/tracing/trace_context.h
    include/kcenon/network-core/tracing/span.h
    include/kcenon/network-core/tracing/tracing_config.h

    # Integration
    include/kcenon/network-core/integration/logger_integration.h
    include/kcenon/network-core/integration/thread_integration.h
    include/kcenon/network-core/integration/monitoring_integration.h
    include/kcenon/network-core/integration/container_integration.h
    include/kcenon/network-core/integration/thread_pool_adapters.h
    include/kcenon/network-core/integration/io_context_thread_manager.h
    include/kcenon/network-core/integration/common_system_adapter.h

    # Protocol
    include/kcenon/network-core/protocol/protocol_tags.h
)

# Create library target
add_library(network-core ${NETWORK_CORE_SOURCES} ${NETWORK_CORE_HEADERS})

# Set target properties
set_target_properties(network-core PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    PUBLIC_HEADER "${NETWORK_CORE_HEADERS}"
)

# Include directories
target_include_directories(network-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Compiler options
target_compile_features(network-core PUBLIC cxx_std_20)

# Add compile definitions
if(KCENON_WITH_COMMON_SYSTEM)
    target_compile_definitions(network-core PUBLIC KCENON_WITH_COMMON_SYSTEM)
endif()

# Dependencies
find_package(Threads REQUIRED)
target_link_libraries(network-core PUBLIC Threads::Threads)

# Optional: OpenTelemetry for tracing
if(KCENON_WITH_OPENTELEMETRY)
    find_package(opentelemetry-cpp CONFIG)
    if(opentelemetry-cpp_FOUND)
        target_link_libraries(network-core PUBLIC opentelemetry-cpp::api)
        target_compile_definitions(network-core PUBLIC KCENON_WITH_OPENTELEMETRY)
    endif()
endif()

# Install rules
install(TARGETS network-core
    EXPORT network-core-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT network-core-targets
    FILE network-core-targets.cmake
    NAMESPACE kcenon::
    DESTINATION lib/cmake/network-core
)

# Create package config files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/network-core-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/network-core-config.cmake
    INSTALL_DESTINATION lib/cmake/network-core
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/network-core-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/network-core-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/network-core-config-version.cmake
    DESTINATION lib/cmake/network-core
)

# Tests (if enabled)
if(KCENON_NETWORK_CORE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
