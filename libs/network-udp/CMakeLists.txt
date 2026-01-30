#*****************************************************************************
# BSD 3-Clause License
#
# Copyright (c) 2025, kcenon
# All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.16)

project(network-udp
    VERSION 0.1.0
    DESCRIPTION "UDP protocol implementation for network_system"
    LANGUAGES CXX
)

##################################################
# network-udp Library
#
# This library provides UDP protocol implementation:
# - UDP socket wrapper (udp_socket)
# - Messaging clients and servers (messaging_udp_client, messaging_udp_server)
# - Secure messaging (secure_messaging_udp_client, secure_messaging_udp_server)
# - Unified interface adapters (udp_connection_adapter, udp_listener_adapter)
# - Protocol factory functions (create_connection, create_listener)
# - Experimental reliable UDP client
#
# Design Rationale:
# - Implements unified network interfaces from network-core
# - Supports both plain and DTLS secure connections
# - Depends on ASIO for async I/O and OpenSSL for secure connections
##################################################

add_library(network-udp STATIC
    src/udp.cpp
    src/udp_socket.cpp
    src/messaging_udp_client.cpp
    src/messaging_udp_server.cpp
    src/secure_messaging_udp_client.cpp
    src/secure_messaging_udp_server.cpp
    src/udp_connection_adapter.cpp
    src/udp_listener_adapter.cpp
)

# Integration sources handling (ODR Fix - same pattern as network-tcp)
#
# In integrated builds (part of network_system), we use the shared
# network-integration-objects OBJECT library to avoid ODR violations.
# In standalone builds, we include the sources directly.
if(TARGET network-integration-objects)
    # Integrated build: use shared OBJECT library (no symbol duplication)
    message(STATUS "network-udp: Using shared network-integration-objects (ODR-safe)")
    target_sources(network-udp PRIVATE $<TARGET_OBJECTS:network-integration-objects>)
else()
    # Standalone build: include sources directly
    message(STATUS "network-udp: Including integration sources (standalone build)")
    target_sources(network-udp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/integration/io_context_thread_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/integration/logger_integration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/integration/thread_integration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/integration/monitoring_integration.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/integration/thread_system_adapter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/network_context.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/session/messaging_session.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/metrics/network_metrics.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/metrics/sliding_histogram.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/metrics/histogram.cpp
    )
endif()

add_library(kcenon::network-udp ALIAS network-udp)

# C++20 requirement
target_compile_features(network-udp PUBLIC cxx_std_20)

# Include directories
# When built standalone, use local includes
# When part of network_system, use parent includes
target_include_directories(network-udp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

##################################################
# Dependencies
#
# network-udp requires:
# - ASIO (header-only) for async I/O
# - OpenSSL for DTLS support
# - network-core for unified interfaces
# - Main project headers for unified_udp_messaging_client/server
##################################################

# Find ASIO (compatible with main project's ASIO setup)
# Try CMake config package first (vcpkg)
find_package(asio QUIET CONFIG)
if(TARGET asio::asio)
    message(STATUS "network-udp: Found ASIO via CMake package")
    target_link_libraries(network-udp PUBLIC asio::asio)
else()
    # Fallback: find ASIO headers directly
    find_path(ASIO_INCLUDE_DIR
        NAMES asio.hpp
        PATHS
            /opt/homebrew/include
            /usr/local/include
            /usr/include
        DOC "Path to standalone ASIO headers"
    )

    if(ASIO_INCLUDE_DIR)
        message(STATUS "network-udp: Found standalone ASIO at ${ASIO_INCLUDE_DIR}")
        target_include_directories(network-udp SYSTEM PUBLIC
            $<BUILD_INTERFACE:${ASIO_INCLUDE_DIR}>
        )
        target_compile_definitions(network-udp PUBLIC ASIO_STANDALONE)
    else()
        message(FATAL_ERROR "network-udp: ASIO not found. Please install standalone ASIO.")
    endif()
endif()

# Find OpenSSL
find_package(OpenSSL REQUIRED)
target_link_libraries(network-udp PUBLIC OpenSSL::SSL OpenSSL::Crypto)

##################################################
# Compiler Options
##################################################

if(MSVC)
    target_compile_options(network-udp PRIVATE /W4)
else()
    target_compile_options(network-udp PRIVATE -Wall -Wextra -Wpedantic)
endif()

##################################################
# Installation
##################################################

include(GNUInstallDirs)

# Determine export set based on build context
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build - use own export set
    set(NETWORK_UDP_EXPORT_SET network-udp-targets)
else()
    # Integrated build - use parent's export set
    set(NETWORK_UDP_EXPORT_SET NetworkSystemTargets)
endif()

install(TARGETS network-udp
    EXPORT ${NETWORK_UDP_EXPORT_SET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/network_udp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.inl"
)

# Standalone build config file
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(EXPORT network-udp-targets
        FILE network-udp-config.cmake
        NAMESPACE kcenon::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network-udp
    )
endif()

##################################################
# Summary
##################################################

message(STATUS "network-udp: UDP protocol library configured")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Include path: ${CMAKE_CURRENT_SOURCE_DIR}/include")
