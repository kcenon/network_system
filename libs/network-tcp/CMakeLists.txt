#*****************************************************************************
# BSD 3-Clause License
#
# Copyright (c) 2025, kcenon
# All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.16)

project(network-tcp
    VERSION 0.1.0
    DESCRIPTION "TCP protocol implementation for network_system"
    LANGUAGES CXX
)

##################################################
# network-tcp Library
#
# This library provides TCP protocol implementation:
# - TCP socket wrapper (tcp_socket, secure_tcp_socket)
# - Unified interface adapters (tcp_connection_adapter, tcp_listener_adapter)
# - Protocol factory functions (create_connection, create_listener)
#
# Design Rationale:
# - Implements unified network interfaces from network-core
# - Supports both plain and SSL/TLS connections
# - Depends on ASIO for async I/O and OpenSSL for secure connections
##################################################

add_library(network-tcp
    src/tcp.cpp
    src/tcp_socket.cpp
    src/secure_tcp_socket.cpp
    src/tcp_connection_adapter.cpp
    src/tcp_listener_adapter.cpp
    # Integration components needed by TCP adapters
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/integration/io_context_thread_manager.cpp
    # Core network_context needed by io_context_thread_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core/network_context.cpp
)

add_library(kcenon::network-tcp ALIAS network-tcp)

# C++20 requirement
target_compile_features(network-tcp PUBLIC cxx_std_20)

# Include directories
# When built standalone, use local includes
# When part of network_system, use parent includes
target_include_directories(network-tcp
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
# network-tcp requires:
# - ASIO (header-only) for async I/O
# - OpenSSL for SSL/TLS support
# - network-core for unified interfaces
# - Main project headers for unified_messaging_client/server
##################################################

# Find ASIO (compatible with main project's ASIO setup)
# Try CMake config package first (vcpkg)
find_package(asio QUIET CONFIG)
if(TARGET asio::asio)
    message(STATUS "network-tcp: Found ASIO via CMake package")
    target_link_libraries(network-tcp PUBLIC asio::asio)
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
        message(STATUS "network-tcp: Found standalone ASIO at ${ASIO_INCLUDE_DIR}")
        target_include_directories(network-tcp SYSTEM PUBLIC
            $<BUILD_INTERFACE:${ASIO_INCLUDE_DIR}>
        )
        target_compile_definitions(network-tcp PUBLIC ASIO_STANDALONE)
    else()
        message(FATAL_ERROR "network-tcp: ASIO not found. Please install standalone ASIO.")
    endif()
endif()

# Find OpenSSL
find_package(OpenSSL REQUIRED)
target_link_libraries(network-tcp PUBLIC OpenSSL::SSL OpenSSL::Crypto)

##################################################
# Compiler Options
##################################################

if(MSVC)
    target_compile_options(network-tcp PRIVATE /W4)
else()
    target_compile_options(network-tcp PRIVATE -Wall -Wextra -Wpedantic)
endif()

##################################################
# Installation
##################################################

include(GNUInstallDirs)

# Determine export set based on build context
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build - use own export set
    set(NETWORK_TCP_EXPORT_SET network-tcp-targets)
else()
    # Integrated build - use parent's export set
    set(NETWORK_TCP_EXPORT_SET NetworkSystemTargets)
endif()

install(TARGETS network-tcp
    EXPORT ${NETWORK_TCP_EXPORT_SET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/network_tcp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Standalone build config file
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(EXPORT network-tcp-targets
        FILE network-tcp-config.cmake
        NAMESPACE kcenon::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network-tcp
    )
endif()

##################################################
# Summary
##################################################

message(STATUS "network-tcp: TCP protocol library configured")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Include path: ${CMAKE_CURRENT_SOURCE_DIR}/include")
