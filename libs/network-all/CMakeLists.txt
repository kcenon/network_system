#*****************************************************************************
# BSD 3-Clause License
#
# Copyright (c) 2025, kcenon
# All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.16)

project(network-all
    VERSION 0.1.0
    DESCRIPTION "Umbrella package aggregating all network protocol libraries"
    LANGUAGES CXX
)

##################################################
# network-all Umbrella Library
#
# This is an INTERFACE library that aggregates all protocol libraries:
# - network-core (header-only interfaces)
# - network-tcp (TCP protocol)
# - network-udp (UDP protocol)
# - network-websocket (WebSocket protocol)
# - network-http2 (HTTP/2 protocol)
# - network-quic (QUIC protocol)
# - network-grpc (gRPC protocol)
#
# Design Rationale:
# - Single dependency for applications needing all protocols
# - Version synchronization across protocol libraries
# - Simplified migration path from monolithic network_system
##################################################

add_library(network-all INTERFACE)
add_library(kcenon::network-all ALIAS network-all)

# C++20 requirement
target_compile_features(network-all INTERFACE cxx_std_20)

# Include directories for umbrella header
target_include_directories(network-all INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

##################################################
# Protocol Library Dependencies
#
# All protocol libraries are linked as INTERFACE dependencies,
# meaning consumers of network-all get all protocol libraries
# transitively.
##################################################

# Core interfaces (always available)
if(TARGET network-core)
    target_link_libraries(network-all INTERFACE network-core)
elseif(TARGET kcenon::network-core)
    target_link_libraries(network-all INTERFACE kcenon::network-core)
endif()

# TCP protocol
if(TARGET network-tcp)
    target_link_libraries(network-all INTERFACE network-tcp)
elseif(TARGET kcenon::network-tcp)
    target_link_libraries(network-all INTERFACE kcenon::network-tcp)
endif()

# UDP protocol
if(TARGET network-udp)
    target_link_libraries(network-all INTERFACE network-udp)
elseif(TARGET kcenon::network-udp)
    target_link_libraries(network-all INTERFACE kcenon::network-udp)
endif()

# WebSocket protocol (optional, depends on BUILD_WEBSOCKET_SUPPORT)
if(TARGET network-websocket)
    target_link_libraries(network-all INTERFACE network-websocket)
elseif(TARGET kcenon::network-websocket)
    target_link_libraries(network-all INTERFACE kcenon::network-websocket)
endif()

# HTTP/2 protocol
if(TARGET network-http2)
    target_link_libraries(network-all INTERFACE network-http2)
elseif(TARGET kcenon::network-http2)
    target_link_libraries(network-all INTERFACE kcenon::network-http2)
endif()

# QUIC protocol
if(TARGET network-quic)
    target_link_libraries(network-all INTERFACE network-quic)
elseif(TARGET kcenon::network-quic)
    target_link_libraries(network-all INTERFACE kcenon::network-quic)
endif()

# gRPC protocol
if(TARGET network-grpc)
    target_link_libraries(network-all INTERFACE network-grpc)
elseif(TARGET kcenon::network-grpc)
    target_link_libraries(network-all INTERFACE kcenon::network-grpc)
endif()

##################################################
# Installation
##################################################

include(GNUInstallDirs)

# Determine export set based on build context
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build - use own export set
    set(NETWORK_ALL_EXPORT_SET network-all-targets)
else()
    # Integrated build - use parent's export set
    set(NETWORK_ALL_EXPORT_SET NetworkSystemTargets)
endif()

install(TARGETS network-all
    EXPORT ${NETWORK_ALL_EXPORT_SET}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install umbrella header
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/network_all
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Standalone build config file
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Configure and install the config file
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/network-all-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/network-all-config.cmake
        @ONLY
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/network-all-config.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network-all
    )

    install(EXPORT network-all-targets
        FILE network-all-targets.cmake
        NAMESPACE kcenon::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network-all
    )
endif()

##################################################
# Summary
##################################################

# Collect available protocols for summary
set(_PROTOCOLS_ENABLED "")

if(TARGET network-core OR TARGET kcenon::network-core)
    list(APPEND _PROTOCOLS_ENABLED "core")
endif()
if(TARGET network-tcp OR TARGET kcenon::network-tcp)
    list(APPEND _PROTOCOLS_ENABLED "tcp")
endif()
if(TARGET network-udp OR TARGET kcenon::network-udp)
    list(APPEND _PROTOCOLS_ENABLED "udp")
endif()
if(TARGET network-websocket OR TARGET kcenon::network-websocket)
    list(APPEND _PROTOCOLS_ENABLED "websocket")
endif()
if(TARGET network-http2 OR TARGET kcenon::network-http2)
    list(APPEND _PROTOCOLS_ENABLED "http2")
endif()
if(TARGET network-quic OR TARGET kcenon::network-quic)
    list(APPEND _PROTOCOLS_ENABLED "quic")
endif()
if(TARGET network-grpc OR TARGET kcenon::network-grpc)
    list(APPEND _PROTOCOLS_ENABLED "grpc")
endif()

string(REPLACE ";" ", " _PROTOCOLS_STRING "${_PROTOCOLS_ENABLED}")

message(STATUS "network-all: Umbrella package configured")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Protocols: ${_PROTOCOLS_STRING}")
