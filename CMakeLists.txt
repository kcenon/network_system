cmake_minimum_required(VERSION 3.20)

# Set policy for FindBoost module
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

##################################################
# Network System CMakeLists.txt
#
# Independent high-performance network system.
#
# This file is intentionally a thin orchestrator: every
# substantive directive lives in a cmake/network_system_*.cmake
# module. The order below mirrors the conceptual phases of
# the build:
#
#   1. project() declaration
#   2. compiler defaults  (network_system_compiler.cmake)
#   3. user-facing options (network_system_options.cmake)
#   4. sanitizer / coverage flag wiring
#                         (network_system_sanitizers.cmake)
#   5. dependency / feature detection
#                         (network_system_dependencies.cmake,
#                          network_system_features.cmake,
#                          network_system_integration.cmake)
#   6. libs/network-core (header-only interfaces)
#   7. shared ODR OBJECT library
#                         (network_system_integration_objects.cmake)
#   8. modular protocol libraries
#                         (network_system_protocol_libs.cmake)
#   9. main library targets
#                         (network_system_targets.cmake)
#  10. tests / examples / benchmarks / fuzz subdirectories
#                         (network_system_subdirs.cmake)
#  11. installation       (network_system_install.cmake)
#  12. CPack              (network_system_cpack.cmake)
#  13. C++20 module target
#                         (network_system_modules.cmake,
#                          gated on NETWORK_BUILD_MODULES)
#  14. summary printout   (network_system_summary.cmake)
#
# Adding a new option, sanitizer, or install rule should be
# a single-file diff against the matching module — if you
# find yourself editing this file for anything other than
# orchestration, the change probably belongs in a module.
##################################################

project(network_system
    VERSION 0.1.1
    DESCRIPTION "Independent High-Performance Network System"
    LANGUAGES CXX
)

# Module path — must precede include() of any project module.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Compiler defaults, options, sanitizer / coverage wiring.
include(cmake/network_system_compiler.cmake)
include(cmake/network_system_options.cmake)
include(cmake/network_system_sanitizers.cmake)

# Helper modules (function definitions only — no targets,
# no detection runs at include() time).
include(network_system_features)
include(network_system_dependencies)
include(network_system_integration)
include(network_system_install)

# Dependency + feature detection.
find_package(PkgConfig QUIET)
find_network_system_dependencies()
check_network_system_features()

# network-core (header-only, must come before
# network-integration-objects so the integration objects
# can link against it).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/network-core/CMakeLists.txt)
    add_subdirectory(libs/network-core)
    message(STATUS "network-core library enabled")
endif()

# Shared OBJECT library for ODR-fix sources (Issue #555).
# Defines network-integration-objects with the PUBLIC
# network-core/include path that downstream libs (network-tcp,
# network-udp) inherit, which in turn lets the main
# network_system library resolve `kcenon/network-core/...`
# headers via target_link_libraries(network_system PUBLIC
# network-tcp / network-udp). Must come BEFORE the protocol
# subdirectories.
include(cmake/network_system_integration_objects.cmake)

# Modular protocol libraries (libs/network-tcp etc.). These
# come BEFORE network_system_targets.cmake because
# target_link_libraries(network_system PUBLIC network-tcp)
# in that module is gated on `if(TARGET network-tcp)` and
# silently no-ops when the target does not yet exist.
include(cmake/network_system_protocol_libs.cmake)

# Main network_system library + verify_build executable.
include(cmake/network_system_targets.cmake)

# Tests, examples, benchmarks, fuzzing.
include(cmake/network_system_subdirs.cmake)

# Skip installation in unified build to avoid export conflicts.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    setup_network_system_install()
    include(cmake/network_system_cpack.cmake)
else()
    message(STATUS "Installation skipped in unified build mode")
endif()

# Optional C++20 module target (no-op when NETWORK_BUILD_MODULES is OFF).
include(cmake/network_system_modules.cmake)

# Final configuration printout.
include(cmake/network_system_summary.cmake)
