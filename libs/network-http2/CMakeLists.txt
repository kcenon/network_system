#*****************************************************************************
# BSD 3-Clause License
#
# Copyright (c) 2025, kcenon
# All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.16)

project(network-http2
    VERSION 0.1.0
    DESCRIPTION "HTTP/2 protocol implementation for network_system"
    LANGUAGES CXX
)

##################################################
# network-http2 Library
#
# This library provides HTTP/2 protocol implementation:
# - HTTP/2 frame encoding/decoding (RFC 7540)
# - HPACK header compression (RFC 7541)
# - TLS 1.3 with ALPN "h2" negotiation
# - Stream multiplexing
# - Flow control
#
# Design Rationale:
# - Implements RFC 7540 compliant HTTP/2 protocol
# - Builds on top of network-tcp for transport layer
# - Requires OpenSSL for TLS support
##################################################

add_library(network-http2
    src/http2_client.cpp
    src/http2_server.cpp
    src/http2_server_stream.cpp
)

add_library(kcenon::network-http2 ALIAS network-http2)

# C++20 requirement
target_compile_features(network-http2 PUBLIC cxx_std_20)

# Include directories
target_include_directories(network-http2
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../network-core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../network-tcp/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

##################################################
# Dependencies
#
# network-http2 requires:
# - OpenSSL for TLS support
# - ASIO for async I/O
# - network-core for interfaces and result types
# - network-tcp for underlying TCP transport
##################################################

# Find OpenSSL (for TLS)
find_package(OpenSSL REQUIRED)
target_link_libraries(network-http2 PUBLIC OpenSSL::SSL OpenSSL::Crypto)

# Link to network-tcp if available
if(TARGET network-tcp)
    target_link_libraries(network-http2 PUBLIC network-tcp)
endif()

##################################################
# Compiler Options
##################################################

if(MSVC)
    target_compile_options(network-http2 PRIVATE /W4)
else()
    target_compile_options(network-http2 PRIVATE -Wall -Wextra -Wpedantic)
endif()

##################################################
# Installation
##################################################

include(GNUInstallDirs)

# Determine export set based on build context
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build - use own export set
    set(NETWORK_HTTP2_EXPORT_SET network-http2-targets)
else()
    # Integrated build - use parent's export set
    set(NETWORK_HTTP2_EXPORT_SET NetworkSystemTargets)
endif()

install(TARGETS network-http2
    EXPORT ${NETWORK_HTTP2_EXPORT_SET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/network_http2
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Standalone build config file
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(EXPORT network-http2-targets
        FILE network-http2-config.cmake
        NAMESPACE kcenon::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network-http2
    )
endif()

##################################################
# Summary
##################################################

message(STATUS "network-http2: HTTP/2 protocol library configured")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Include path: ${CMAKE_CURRENT_SOURCE_DIR}/include")
