#*****************************************************************************
# BSD 3-Clause License
#
# Copyright (c) 2025, kcenon
# All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.16)

project(network-grpc
    VERSION 0.1.0
    DESCRIPTION "gRPC protocol implementation for network_system"
    LANGUAGES CXX
)

##################################################
# network-grpc Library
#
# This library provides gRPC protocol implementation:
# - gRPC client for making RPC calls
# - gRPC server for handling RPC requests
# - Service registry for service management
# - Support for unary and streaming RPC
# - Optional official gRPC library wrapper
#
# Design Rationale:
# - Builds on top of network-http2 for HTTP/2 transport
# - Supports both custom implementation and official gRPC wrapper
# - Requires network-core for interfaces and result types
##################################################

add_library(network-grpc
    src/grpc_frame.cpp
    src/grpc_client.cpp
    src/grpc_server.cpp
    src/grpc_service_registry.cpp
    src/grpc_official_wrapper.cpp
)

add_library(kcenon::network-grpc ALIAS network-grpc)

# C++20 requirement
target_compile_features(network-grpc PUBLIC cxx_std_20)

# Include directories
target_include_directories(network-grpc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../network-core/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../network-http2/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

##################################################
# Dependencies
#
# network-grpc requires:
# - network-core for interfaces and result types
# - network-http2 for HTTP/2 transport layer
# - OpenSSL for TLS support (optional)
# - Official gRPC library (optional, for wrapper mode)
##################################################

# Link to network-http2 if available
if(TARGET network-http2)
    target_link_libraries(network-grpc PUBLIC network-http2)
endif()

# Find OpenSSL (for TLS)
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
    target_link_libraries(network-grpc PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

# Optional: Link to official gRPC library if available
option(NETWORK_GRPC_USE_OFFICIAL "Use official gRPC library" OFF)
if(NETWORK_GRPC_USE_OFFICIAL)
    find_package(gRPC QUIET)
    if(gRPC_FOUND)
        target_link_libraries(network-grpc PUBLIC gRPC::grpc++ gRPC::grpc)
        target_compile_definitions(network-grpc PUBLIC NETWORK_GRPC_OFFICIAL=1)
    endif()
endif()

##################################################
# Compiler Options
##################################################

if(MSVC)
    target_compile_options(network-grpc PRIVATE /W4)
else()
    target_compile_options(network-grpc PRIVATE -Wall -Wextra -Wpedantic)
endif()

##################################################
# Installation
##################################################

include(GNUInstallDirs)

# Determine export set based on build context
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build - use own export set
    set(NETWORK_GRPC_EXPORT_SET network-grpc-targets)
else()
    # Integrated build - use parent's export set
    set(NETWORK_GRPC_EXPORT_SET NetworkSystemTargets)
endif()

install(TARGETS network-grpc
    EXPORT ${NETWORK_GRPC_EXPORT_SET}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/network_grpc
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Standalone build config file
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(EXPORT network-grpc-targets
        FILE network-grpc-config.cmake
        NAMESPACE kcenon::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network-grpc
    )
endif()

##################################################
# Summary
##################################################

message(STATUS "network-grpc: gRPC protocol library configured")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Include path: ${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NETWORK_GRPC_USE_OFFICIAL AND gRPC_FOUND)
    message(STATUS "  Using official gRPC library: YES")
else()
    message(STATUS "  Using official gRPC library: NO")
endif()
