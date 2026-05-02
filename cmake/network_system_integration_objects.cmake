##################################################
# network_system_integration_objects.cmake
#
# Defines the network-integration-objects OBJECT library.
#
# These sources are shared between network_system and
# network-tcp / network-udp. Using an OBJECT library
# ensures symbols are defined exactly once, preventing
# ODR (One Definition Rule) violations that cause SEGV
# in sanitizer tests (Issue #555).
#
# ORDERING IS LOAD-BEARING:
#   This module MUST be included AFTER libs/network-core
#   (so the include path inheritance works) and BEFORE
#   the libs/network-tcp / libs/network-udp subdirectories
#   are added — those subdirectories link against
#   network-integration-objects via $<TARGET_OBJECTS:...>.
#
# It must also come BEFORE network_system_targets.cmake
# because the main network_system library is created in
# that module and needs the integration objects to be
# available for linkage.
##################################################

##################################################
# Internal symlink for namespaced headers
#
# Public headers use #include "kcenon/network/internal/..."
# which must resolve from the PRIVATE src/ include directory
# at build time.
# COPY_ON_ERROR: falls back to a directory copy on platforms
# where symlinks require elevated privileges (e.g. Windows
# without Developer Mode).
##################################################
set(_INTERNAL_LINK_DIR "${CMAKE_CURRENT_BINARY_DIR}/internal_include/kcenon/network")
file(MAKE_DIRECTORY "${_INTERNAL_LINK_DIR}")
file(CREATE_LINK
    "${CMAKE_CURRENT_SOURCE_DIR}/src/internal"
    "${_INTERNAL_LINK_DIR}/internal"
    COPY_ON_ERROR
    SYMBOLIC
)

##################################################
# Shared Integration Objects (ODR Fix - Issue #555)
##################################################

add_library(network-integration-objects OBJECT
    src/integration/logger_integration.cpp
    src/integration/thread_integration.cpp
    src/integration/io_context_thread_manager.cpp
    src/integration/monitoring_integration.cpp
    src/integration/thread_system_adapter.cpp
    src/integration/thread_pool_bridge.cpp
    src/integration/observability_bridge.cpp
    src/integration/network_system_bridge.cpp
    src/core/network_context.cpp
    src/session/messaging_session.cpp
    src/metrics/network_metrics.cpp
    src/metrics/sliding_histogram.cpp
    src/metrics/histogram.cpp
)

# Configure integration objects with necessary includes and dependencies.
# The PUBLIC root-level network-core/include path is the historical link
# that lets transitive consumers (network_system, network-tcp, network-udp)
# resolve `kcenon/network-core/...` headers without their own
# target_include_directories() repeating it. Do not narrow this to PRIVATE.
target_include_directories(network-integration-objects
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/network-core/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/internal_include
)

target_compile_features(network-integration-objects PUBLIC cxx_std_20)

# ASIO setup for integration objects
find_package(asio QUIET CONFIG)
if(TARGET asio::asio)
    target_link_libraries(network-integration-objects PUBLIC asio::asio)
else()
    find_path(ASIO_INCLUDE_DIR
        NAMES asio.hpp
        PATHS
            /opt/homebrew/include
            /usr/local/include
            /usr/include
        DOC "Path to standalone ASIO headers"
    )
    if(ASIO_INCLUDE_DIR)
        target_include_directories(network-integration-objects SYSTEM PUBLIC
            $<BUILD_INTERFACE:${ASIO_INCLUDE_DIR}>
        )
        target_compile_definitions(network-integration-objects PUBLIC ASIO_STANDALONE)
    endif()
endif()

# common_system integration for network-integration-objects
setup_common_system_integration(network-integration-objects)

# Export the OBJECT library for network-tcp/network-udp to use
set(NETWORK_INTEGRATION_OBJECTS_TARGET network-integration-objects CACHE INTERNAL "Integration objects target")
