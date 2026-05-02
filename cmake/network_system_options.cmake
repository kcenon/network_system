##################################################
# network_system_options.cmake
#
# Build option declarations for network_system.
# Centralizes all option(...) calls so they can be reviewed
# and modified independently of the orchestration in the
# top-level CMakeLists.txt.
#
# Note: a small number of options remain inline in
# CMakeLists.txt where they are tightly coupled with their
# consumer logic (e.g. SKIP_ASIO_RECYCLING_WORKAROUND used
# by sanitizer wiring, BUILD_TLS_SUPPORT/BUILD_LZ4_COMPRESSION/
# BUILD_ZLIB_COMPRESSION/BUILD_VERIFY_BUILD used by the main
# library target rules). All such options are still declared
# via standard option() and therefore remain user-overridable
# on the command line in the same way.
##################################################

##################################################
# General build toggles
##################################################
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build usage examples" ON)
option(BUILD_WEBSOCKET_SUPPORT "Build WebSocket protocol support (RFC 6455)" ON)
option(BUILD_MESSAGING_BRIDGE "Build messaging_system bridge" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(NETWORK_BUILD_BENCHMARKS "Build network system benchmarks" OFF)
option(NETWORK_BUILD_INTEGRATION_TESTS "Build network system integration tests" ON)

##################################################
# Sanitizer toggles (mutually exclusive - enforced in
# network_system_sanitizers.cmake)
##################################################
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)

##################################################
# C++20 Module Support (Experimental)
#
# Enable C++20 modules with CMake 3.28+ when building with
# module-capable compilers. This creates a separate module
# library target (network_system_modules) that can be used
# instead of the header-based library.
#
# Usage:
#   cmake -DNETWORK_BUILD_MODULES=ON ..
#
# Requirements:
#   - CMake 3.28+
#   - Clang 16+ or GCC 14+ or MSVC 17.4+
##################################################
option(NETWORK_BUILD_MODULES "Build C++20 module version of network_system" OFF)

##################################################
# Dependency Configuration (Option A Structure)
#
# Tier 4: network_system
#   Required:
#     - common_system (Tier 0)
#     - thread_system (Tier 1)
#     - logger_system (Tier 2)
#   Optional:
#     - container_system (Tier 1) - serialization
#     - monitoring_system (Tier 3) - metrics collection
#
# Note: monitoring_system is OPTIONAL to prevent circular
#       dependency. network_system provides basic_monitoring
#       for standalone operation.
##################################################

# Required dependencies (Tier 0-1)
option(BUILD_WITH_COMMON_SYSTEM "Build with common_system integration (REQUIRED)" ON)
option(BUILD_WITH_THREAD_SYSTEM "Build with thread_system integration (REQUIRED)" ON)

# Optional dependencies - logger_system is now OPTIONAL (runtime binding via GlobalLoggerRegistry)
# Issue #285: Migrated to common_system's ILogger interface
option(BUILD_WITH_LOGGER_SYSTEM "Build with logger_system integration (OPTIONAL - runtime binding)" OFF)

# Optional dependencies
option(BUILD_WITH_CONTAINER_SYSTEM "Build with container_system integration" ON)

# Official gRPC library integration (issue #360)
# When enabled, wraps the official grpc++ library for production use.
# When disabled, uses the existing prototype implementation.
option(NETWORK_ENABLE_GRPC_OFFICIAL "Use official gRPC library (grpc++) for production" OFF)

##################################################
# Dependency Validation (Advisory)
#
# Note: These dependencies are RECOMMENDED for full
#       functionality. Disabling them may result in reduced
#       features but allows standalone builds for testing
#       and CI purposes.
##################################################

if(NOT BUILD_WITH_COMMON_SYSTEM)
    message(WARNING "common_system (Tier 0) is disabled - some features may be unavailable")
endif()

if(NOT BUILD_WITH_THREAD_SYSTEM)
    message(WARNING "thread_system (Tier 1) is disabled - some features may be unavailable")
endif()

if(BUILD_WITH_LOGGER_SYSTEM)
    message(STATUS "logger_system integration is ENABLED (optional - will use logger_system_adapter)")
else()
    message(STATUS "logger_system integration is DISABLED (default - uses common_system's ILogger)")
endif()

message(STATUS "Monitoring: EventBus-based metric publishing (no compile-time dependency)")

# Respect global BUILD_INTEGRATION_TESTS flag if set
if(DEFINED BUILD_INTEGRATION_TESTS)
    if(BUILD_INTEGRATION_TESTS)
        set(_NETWORK_BUILD_IT_VALUE ON)
    else()
        set(_NETWORK_BUILD_IT_VALUE OFF)
    endif()
    set(NETWORK_BUILD_INTEGRATION_TESTS ${_NETWORK_BUILD_IT_VALUE} CACHE BOOL "Build network system integration tests" FORCE)
endif()
