#*****************************************************************************
# BSD 3-Clause License
#
# Copyright (c) 2025, kcenon
# All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.16)

project(network-core
    VERSION 0.1.0
    DESCRIPTION "Core interfaces and session management for network_system"
    LANGUAGES CXX
)

##################################################
# network-core Library
#
# This is an INTERFACE (header-only) library that provides:
# - Core network interfaces (i_client, i_server, i_session, i_network_component)
# - Session management base classes (session_manager_base, session_traits)
# - Result types for error handling
#
# Design Rationale:
# - Protocol-agnostic interfaces enable dependency inversion
# - Header-only avoids ABI compatibility issues
# - Minimal dependencies for easy integration
##################################################

add_library(network-core INTERFACE)
add_library(kcenon::network-core ALIAS network-core)

# C++20 requirement
target_compile_features(network-core INTERFACE cxx_std_20)

# Include directories
# When built standalone, use local includes
# When part of network_system, use parent includes
target_include_directories(network-core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../include>
    $<INSTALL_INTERFACE:include>
)

##################################################
# Core Interface Files
#
# These files define the fundamental network abstractions:
# - i_network_component: Base for all network components
# - i_client: Client-side operations
# - i_server: Server-side operations
# - i_session: Individual client session on server
##################################################

# Note: As an INTERFACE library, we don't add sources directly.
# The headers are exposed through the include directories.
# Consumers link to this target to get access to the interfaces.

##################################################
# Dependencies
#
# network-core has minimal dependencies:
# - Standard C++ library (C++20)
# - Result types from parent project
##################################################

# If common_system provides Result<T>, link it
# This is handled by the parent CMakeLists.txt when
# integrating network-core into network_system

##################################################
# Installation (when built standalone)
##################################################

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    # Standalone build - provide installation rules
    include(GNUInstallDirs)

    install(TARGETS network-core
        EXPORT network-core-targets
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT network-core-targets
        FILE network-core-config.cmake
        NAMESPACE kcenon::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network-core
    )

    # Install interface headers
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../include/kcenon/network/interfaces/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kcenon/network/interfaces
        FILES_MATCHING PATTERN "*.h"
    )

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/kcenon/network/core/session_traits.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/kcenon/network/core/session_info.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/kcenon/network/core/session_manager_base.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kcenon/network/core
    )

    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/kcenon/network/utils/result_types.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kcenon/network/utils
    )
endif()

##################################################
# Summary
##################################################

message(STATUS "network-core: Header-only interface library configured")
message(STATUS "  Include path: ${CMAKE_CURRENT_SOURCE_DIR}/../../include")
