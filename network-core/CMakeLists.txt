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

# Source files (currently header-only for phase 1)
set(NETWORK_CORE_SOURCES
    # Phase 1: Header-only interfaces
    # Future phases will add implementation files:
    # - Metrics (network_metrics.cpp, histogram.cpp, etc.)
    # - Tracing (trace_context.cpp, span.cpp)
    # - Integration (logger_integration.cpp, etc.)
)

# Header files (Phase 1: Core interfaces only)
set(NETWORK_CORE_HEADERS
    # Interfaces (Phase 1)
    include/kcenon/network-core/interfaces/i_network_component.h
    include/kcenon/network-core/interfaces/i_client.h
    include/kcenon/network-core/interfaces/i_server.h
    include/kcenon/network-core/interfaces/i_session.h
    include/kcenon/network-core/interfaces/connection_observer.h

    # Utils (Phase 1)
    include/kcenon/network-core/utils/result_types.h

    # Config (Phase 1)
    include/kcenon/network-core/config/feature_flags.h

    # Future phases will add:
    # - Core session management (session_handle, session_model, etc.)
    # - Unified interfaces (i_transport, i_connection, i_listener)
    # - Concepts (network_concepts, etc.)
    # - Metrics framework
    # - Tracing infrastructure
    # - Integration adapters
)

# Create library target (INTERFACE for header-only in Phase 1)
add_library(network-core INTERFACE)

# Include directories (INTERFACE library)
target_include_directories(network-core
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Compiler options (INTERFACE library)
target_compile_features(network-core INTERFACE cxx_std_20)

# Add compile definitions (INTERFACE library)
if(KCENON_WITH_COMMON_SYSTEM)
    target_compile_definitions(network-core INTERFACE KCENON_WITH_COMMON_SYSTEM)
endif()

# Dependencies (INTERFACE library)
find_package(Threads REQUIRED)
target_link_libraries(network-core INTERFACE Threads::Threads)

# Optional: OpenTelemetry for tracing (future phase)
if(KCENON_WITH_OPENTELEMETRY)
    find_package(opentelemetry-cpp CONFIG)
    if(opentelemetry-cpp_FOUND)
        target_link_libraries(network-core INTERFACE opentelemetry-cpp::api)
        target_compile_definitions(network-core INTERFACE KCENON_WITH_OPENTELEMETRY)
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
