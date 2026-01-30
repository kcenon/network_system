#*****************************************************************************
# BSD 3-Clause License
#
# Copyright (c) 2025, kcenon
# All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.16)

project(network-quic
    VERSION 0.1.0
    DESCRIPTION "QUIC protocol implementation for network_system"
    LANGUAGES CXX
)

##################################################
# network-quic Library
#
# This library provides QUIC protocol implementation:
# - QUIC connection management (RFC 9000)
# - TLS 1.3 integration (RFC 9001)
# - Stream multiplexing
# - Flow control and congestion control
# - Connection migration
#
# Design Rationale:
# - Implements RFC 9000 compliant QUIC protocol
# - Builds on top of network-udp for transport layer
# - Requires OpenSSL for TLS 1.3 support
##################################################

add_library(network-quic
    src/quic_client.cpp
    src/quic_server.cpp
    src/quic_connection.cpp
)

add_library(kcenon::network-quic ALIAS network-quic)

# C++20 requirement
target_compile_features(network-quic PUBLIC cxx_std_20)

# Include directories
target_include_directories(network-quic
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../network-core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../network-udp/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

##################################################
# Dependencies
#
# network-quic requires:
# - OpenSSL for TLS 1.3 support
# - ASIO for async I/O
# - network-core for interfaces and result types
# - network-udp for underlying UDP transport
##################################################

# Find OpenSSL (for TLS 1.3)
find_package(OpenSSL REQUIRED)
target_link_libraries(network-quic PUBLIC OpenSSL::SSL OpenSSL::Crypto)

# Link to network-udp if available
if(TARGET network-udp)
    target_link_libraries(network-quic PUBLIC network-udp)
endif()

##################################################
# Compiler Options
##################################################

if(MSVC)
    target_compile_options(network-quic PRIVATE /W4)
else()
    target_compile_options(network-quic PRIVATE -Wall -Wextra -Wpedantic)
endif()

##################################################
# Installation
##################################################

include(GNUInstallDirs)

# Determine export set based on build context
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build - use own export set
    set(NETWORK_QUIC_EXPORT_SET network-quic-targets)
else()
    # Integrated build - use parent's export set
    set(NETWORK_QUIC_EXPORT_SET NetworkSystemTargets)
endif()

install(TARGETS network-quic
    EXPORT ${NETWORK_QUIC_EXPORT_SET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/network_quic
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Standalone build config file
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(EXPORT network-quic-targets
        FILE network-quic-config.cmake
        NAMESPACE kcenon::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network-quic
    )
endif()

##################################################
# Summary
##################################################

message(STATUS "network-quic: QUIC protocol library configured")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Include path: ${CMAKE_CURRENT_SOURCE_DIR}/include")
