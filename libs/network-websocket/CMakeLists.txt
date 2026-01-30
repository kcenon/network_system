#*****************************************************************************
# BSD 3-Clause License
#
# Copyright (c) 2025, kcenon
# All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.16)

project(network-websocket
    VERSION 0.1.0
    DESCRIPTION "WebSocket protocol implementation for network_system"
    LANGUAGES CXX
)

##################################################
# network-websocket Library
#
# This library provides WebSocket protocol implementation:
# - WebSocket frame encoding/decoding (RFC 6455)
# - HTTP/1.1 upgrade handshake
# - WebSocket protocol state machine
# - WebSocket socket wrapper on top of TCP
#
# Design Rationale:
# - Implements RFC 6455 compliant WebSocket protocol
# - Builds on top of network-tcp for transport layer
# - Depends on OpenSSL for SHA-1 in handshake
##################################################

add_library(network-websocket
    src/websocket_frame.cpp
    src/websocket_handshake.cpp
    src/websocket_protocol.cpp
    src/websocket_socket.cpp
)

add_library(kcenon::network-websocket ALIAS network-websocket)

# C++20 requirement
target_compile_features(network-websocket PUBLIC cxx_std_20)

# Include directories
target_include_directories(network-websocket
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../network-core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../network-tcp/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

##################################################
# Dependencies
#
# network-websocket requires:
# - OpenSSL for SHA-1 hashing in WebSocket handshake
# - network-core for interfaces and result types
# - network-tcp for underlying TCP transport
##################################################

# Find OpenSSL (for SHA-1 in handshake)
find_package(OpenSSL REQUIRED)
target_link_libraries(network-websocket PUBLIC OpenSSL::SSL OpenSSL::Crypto)

# Link to network-tcp if available
if(TARGET network-tcp)
    target_link_libraries(network-websocket PUBLIC network-tcp)
endif()

##################################################
# Compiler Options
##################################################

if(MSVC)
    target_compile_options(network-websocket PRIVATE /W4)
else()
    target_compile_options(network-websocket PRIVATE -Wall -Wextra -Wpedantic)
endif()

##################################################
# Installation
##################################################

include(GNUInstallDirs)

# Determine export set based on build context
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build - use own export set
    set(NETWORK_WEBSOCKET_EXPORT_SET network-websocket-targets)
else()
    # Integrated build - use parent's export set
    set(NETWORK_WEBSOCKET_EXPORT_SET NetworkSystemTargets)
endif()

install(TARGETS network-websocket
    EXPORT ${NETWORK_WEBSOCKET_EXPORT_SET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/network_websocket
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Standalone build config file
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(EXPORT network-websocket-targets
        FILE network-websocket-config.cmake
        NAMESPACE kcenon::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network-websocket
    )
endif()

##################################################
# Summary
##################################################

message(STATUS "network-websocket: WebSocket protocol library configured")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Include path: ${CMAKE_CURRENT_SOURCE_DIR}/include")
