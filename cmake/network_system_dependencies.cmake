##################################################
# NetworkSystemDependencies.cmake
#
# Dependency finding module for network_system
# Handles ASIO and system integrations
##################################################

include(FetchContent)

##################################################
# Pinned ASIO version
#
# SOUP traceability (IEC 62304): ASIO is a critical networking dependency.
# This version must be kept in sync with:
#   - vcpkg.json "version>=" field
#   - FetchContent GIT_TAG below
#
# Version history:
#   1.30.2 (2024-04) - Pinned to match vcpkg baseline c4af3593
##################################################
set(NETWORK_SYSTEM_ASIO_PINNED_VERSION "1.30.2")
set(NETWORK_SYSTEM_ASIO_PINNED_TAG "asio-1-30-2")
# ASIO_VERSION macro encodes as: MAJOR*100000 + MINOR*100 + PATCH
set(NETWORK_SYSTEM_ASIO_MINIMUM_VERSION_INT 103002)

##################################################
# Detect ASIO version from headers
#
# Reads ASIO_VERSION from asio/version.hpp and converts
# the encoded integer (MAJOR*100000 + MINOR*100 + PATCH)
# to a human-readable "MAJOR.MINOR.PATCH" string.
##################################################
function(_detect_asio_version _include_dir _out_version _out_version_int)
    set(_version_file "${_include_dir}/asio/version.hpp")
    if(NOT EXISTS "${_version_file}")
        set(${_out_version} "unknown" PARENT_SCOPE)
        set(${_out_version_int} 0 PARENT_SCOPE)
        return()
    endif()

    file(STRINGS "${_version_file}" _version_line
        REGEX "^#define ASIO_VERSION [0-9]+")

    if(_version_line)
        string(REGEX MATCH "[0-9]+" _version_int "${_version_line}")
        math(EXPR _major "${_version_int} / 100000")
        math(EXPR _minor "(${_version_int} / 100) % 1000")
        math(EXPR _patch "${_version_int} % 100")
        set(${_out_version} "${_major}.${_minor}.${_patch}" PARENT_SCOPE)
        set(${_out_version_int} ${_version_int} PARENT_SCOPE)
    else()
        set(${_out_version} "unknown" PARENT_SCOPE)
        set(${_out_version_int} 0 PARENT_SCOPE)
    endif()
endfunction()

##################################################
# Validate ASIO version against pinned minimum
##################################################
function(_validate_asio_version _version_str _version_int _source)
    if(_version_int EQUAL 0)
        message(STATUS "  ASIO version: unknown (${_source})")
        return()
    endif()

    message(STATUS "  ASIO version: ${_version_str} (${_source})")

    if(_version_int LESS ${NETWORK_SYSTEM_ASIO_MINIMUM_VERSION_INT})
        message(WARNING
            "ASIO version ${_version_str} is below pinned minimum "
            "${NETWORK_SYSTEM_ASIO_PINNED_VERSION}. "
            "Update to ASIO >= ${NETWORK_SYSTEM_ASIO_PINNED_VERSION} for "
            "consistent behavior across build paths.")
    endif()
endfunction()

##################################################
# Find ASIO (standalone or Boost.ASIO)
##################################################
function(find_asio_library)
    message(STATUS "Looking for ASIO library (pinned: ${NETWORK_SYSTEM_ASIO_PINNED_VERSION})...")

    # Try CMake config package first (vcpkg provides asioConfig.cmake)
    find_package(asio QUIET CONFIG)
    if(TARGET asio::asio)
        message(STATUS "Found ASIO via CMake package (target: asio::asio)")
        # Detect version from vcpkg-provided headers
        get_target_property(_asio_inc asio::asio INTERFACE_INCLUDE_DIRECTORIES)
        if(_asio_inc)
            list(GET _asio_inc 0 _asio_inc_first)
            _detect_asio_version("${_asio_inc_first}" _ver _ver_int)
            _validate_asio_version("${_ver}" "${_ver_int}" "vcpkg")
        endif()
        set(ASIO_FOUND TRUE PARENT_SCOPE)
        set(ASIO_TARGET asio::asio PARENT_SCOPE)
        return()
    endif()

    # Standard header search (respects CMAKE_PREFIX_PATH set by vcpkg)
    find_path(ASIO_INCLUDE_DIR
        NAMES asio.hpp
        DOC "Path to standalone ASIO headers"
    )

    # Fallback: explicit common locations
    if(NOT ASIO_INCLUDE_DIR)
        find_path(ASIO_INCLUDE_DIR
            NAMES asio.hpp
            PATHS
                /opt/homebrew/include
                /usr/local/include
                /usr/include
            NO_DEFAULT_PATH
            DOC "Path to standalone ASIO headers in common locations"
        )
    endif()

    if(ASIO_INCLUDE_DIR)
        message(STATUS "Found standalone ASIO at: ${ASIO_INCLUDE_DIR}")
        _detect_asio_version("${ASIO_INCLUDE_DIR}" _ver _ver_int)
        _validate_asio_version("${_ver}" "${_ver_int}" "system")
        set(ASIO_INCLUDE_DIR ${ASIO_INCLUDE_DIR} PARENT_SCOPE)
        set(ASIO_FOUND TRUE PARENT_SCOPE)
        return()
    endif()

    # Note: Boost.ASIO fallback is NOT compatible with our source code
    # Source files use #include <asio.hpp> (standalone path)
    # Boost provides #include <boost/asio.hpp> (different path)
    # Therefore, we skip Boost.ASIO and fetch standalone ASIO directly

    # Fetch standalone ASIO as a last resort
    message(STATUS "Standalone ASIO not found locally - fetching ${NETWORK_SYSTEM_ASIO_PINNED_TAG} from upstream...")

    FetchContent_Declare(network_system_asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG ${NETWORK_SYSTEM_ASIO_PINNED_TAG}
    )

    FetchContent_GetProperties(network_system_asio)
    if(NOT network_system_asio_POPULATED)
        FetchContent_Populate(network_system_asio)
    endif()

    set(_asio_include_dir "${network_system_asio_SOURCE_DIR}/asio/include")

    if(EXISTS "${_asio_include_dir}/asio.hpp")
        message(STATUS "Fetched standalone ASIO headers at: ${_asio_include_dir}")
        _detect_asio_version("${_asio_include_dir}" _ver _ver_int)
        _validate_asio_version("${_ver}" "${_ver_int}" "FetchContent")
        set(ASIO_INCLUDE_DIR ${_asio_include_dir} PARENT_SCOPE)
        set(ASIO_FOUND TRUE PARENT_SCOPE)
        set(ASIO_FETCHED TRUE PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR "Failed to fetch standalone ASIO - cannot build without ASIO")
endfunction()

##################################################
# Find container_system
##################################################
function(find_container_system)
    if(NOT BUILD_WITH_CONTAINER_SYSTEM)
        return()
    endif()

    message(STATUS "Looking for container_system...")

    # Try CMake target first
    foreach(_candidate ContainerSystem::container container_system)
        if(TARGET ${_candidate})
            message(STATUS "Found container_system CMake target: ${_candidate}")
            set(CONTAINER_SYSTEM_FOUND TRUE PARENT_SCOPE)
            set(CONTAINER_SYSTEM_TARGET ${_candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Path-based detection - prioritize environment variable, then standard paths
    set(_container_search_paths)
    set(_container_lib_paths)

    # 1. Check environment variable
    if(DEFINED ENV{CONTAINER_SYSTEM_ROOT})
        list(APPEND _container_search_paths "$ENV{CONTAINER_SYSTEM_ROOT}")
        list(APPEND _container_lib_paths "$ENV{CONTAINER_SYSTEM_ROOT}/build/lib")
    endif()

    # 2. Check CMake variable
    if(DEFINED CONTAINER_SYSTEM_ROOT)
        list(APPEND _container_search_paths "${CONTAINER_SYSTEM_ROOT}")
        list(APPEND _container_lib_paths "${CONTAINER_SYSTEM_ROOT}/build/lib")
    endif()

    # 3. CI workspace path (actions/checkout@v4 with path: container_system)
    list(APPEND _container_search_paths
        "${CMAKE_SOURCE_DIR}/container_system"
    )

    # 4. Local development paths
    list(APPEND _container_search_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../container_system"
        "${CMAKE_SOURCE_DIR}/../container_system"
        "../container_system"
    )

    list(APPEND _container_lib_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../container_system/build/lib"
        "${CMAKE_SOURCE_DIR}/container_system/build/lib"
        "${CMAKE_SOURCE_DIR}/../container_system/build/lib"
        "../container_system/build/lib"
    )

    find_path(CONTAINER_SYSTEM_INCLUDE_DIR
        NAMES container.h
        PATHS ${_container_search_paths}
        NO_DEFAULT_PATH
    )

    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found container_system at: ${CONTAINER_SYSTEM_INCLUDE_DIR}")

        # After the header migration (container_system PR #440), forwarding headers
        # at core/container.h do #include <kcenon/container/container.h>.
        # The canonical headers live under <root>/include/kcenon/container/.
        # Detect and export this additional include directory so the
        # kcenon/container/ includes resolve correctly.
        # After the header migration (container_system PR #440), forwarding headers
        # at core/container.h do #include <kcenon/container/container.h>.
        # Search multiple possible locations for the include/ directory.
        set(_container_new_include_dir)
        foreach(_candidate
            "${CONTAINER_SYSTEM_INCLUDE_DIR}/../include"
            "${CONTAINER_SYSTEM_INCLUDE_DIR}/include"
            "${CMAKE_SOURCE_DIR}/container_system/include"
        )
            if(EXISTS "${_candidate}/kcenon/container/container.h")
                get_filename_component(_container_new_include_dir "${_candidate}" ABSOLUTE)
                message(STATUS "  container_system new-style headers at: ${_container_new_include_dir}")
                break()
            endif()
        endforeach()

        find_library(CONTAINER_SYSTEM_LIBRARY
            NAMES container_system ContainerSystem
            PATHS ${_container_lib_paths}
            PATH_SUFFIXES Debug Release
            NO_DEFAULT_PATH
        )

        if(CONTAINER_SYSTEM_LIBRARY)
            message(STATUS "Found container_system library: ${CONTAINER_SYSTEM_LIBRARY}")
        endif()

        set(CONTAINER_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(CONTAINER_SYSTEM_INCLUDE_DIR ${CONTAINER_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
        set(CONTAINER_SYSTEM_NEW_INCLUDE_DIR "${_container_new_include_dir}" PARENT_SCOPE)
        set(CONTAINER_SYSTEM_LIBRARY ${CONTAINER_SYSTEM_LIBRARY} PARENT_SCOPE)
    else()
        message(WARNING "container_system not found, integration disabled")
        set(BUILD_WITH_CONTAINER_SYSTEM OFF PARENT_SCOPE)
        set(CONTAINER_SYSTEM_FOUND FALSE PARENT_SCOPE)
    endif()
endfunction()

##################################################
# Find thread_system
##################################################
function(find_thread_system)
    if(NOT BUILD_WITH_THREAD_SYSTEM)
        return()
    endif()

    message(STATUS "Looking for thread_system...")

    # Check if THREAD_SYSTEM_INCLUDE_DIR is already set (e.g., by parent project via FetchContent)
    if(THREAD_SYSTEM_INCLUDE_DIR AND EXISTS "${THREAD_SYSTEM_INCLUDE_DIR}/kcenon/thread/core/thread_pool.h")
        message(STATUS "Found thread_system via pre-set THREAD_SYSTEM_INCLUDE_DIR: ${THREAD_SYSTEM_INCLUDE_DIR}")
        set(THREAD_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(THREAD_SYSTEM_INCLUDE_DIR ${THREAD_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
        return()
    endif()

    foreach(_candidate ThreadSystem utilities thread_system::thread_system)
        if(TARGET ${_candidate})
            message(STATUS "Found thread_system CMake target: ${_candidate}")
            set(THREAD_SYSTEM_FOUND TRUE PARENT_SCOPE)
            set(THREAD_SYSTEM_TARGET ${_candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Prioritize environment variable, then standard paths
    set(_thread_search_paths)
    set(_thread_lib_paths)

    # 1. Check environment variable
    if(DEFINED ENV{THREAD_SYSTEM_ROOT})
        list(APPEND _thread_search_paths "$ENV{THREAD_SYSTEM_ROOT}/include")
        list(APPEND _thread_lib_paths "$ENV{THREAD_SYSTEM_ROOT}/build/lib")
    endif()

    # 2. Check CMake variable
    if(DEFINED THREAD_SYSTEM_ROOT)
        list(APPEND _thread_search_paths "${THREAD_SYSTEM_ROOT}/include")
        list(APPEND _thread_lib_paths "${THREAD_SYSTEM_ROOT}/build/lib")
    endif()

    # 3. CI workspace path (actions/checkout@v4 with path: thread_system)
    list(APPEND _thread_search_paths
        "${CMAKE_SOURCE_DIR}/thread_system/include"
    )

    # 4. Local development paths
    list(APPEND _thread_search_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system/include"
        "${CMAKE_SOURCE_DIR}/../thread_system/include"
        "../thread_system/include"
    )

    list(APPEND _thread_lib_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system/build/lib"
        "${CMAKE_SOURCE_DIR}/thread_system/build/lib"
        "${CMAKE_SOURCE_DIR}/../thread_system/build/lib"
        "../thread_system/build/lib"
    )

    find_path(THREAD_SYSTEM_INCLUDE_DIR
        NAMES kcenon/thread/core/thread_pool.h
        PATHS ${_thread_search_paths}
        NO_DEFAULT_PATH
    )

    if(THREAD_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found thread_system at: ${THREAD_SYSTEM_INCLUDE_DIR}")

        find_library(THREAD_SYSTEM_LIBRARY
            NAMES ThreadSystem
            PATHS ${_thread_lib_paths}
            NO_DEFAULT_PATH
        )

        if(THREAD_SYSTEM_LIBRARY)
            message(STATUS "Found thread_system library: ${THREAD_SYSTEM_LIBRARY}")

            # ThreadSystem may depend on simdutf (fetched via FetchContent and installed
            # to CMAKE_PREFIX_PATH, or present in the thread_system build tree).
            # First try find_package so the cmake config from deps/install is used.
            find_package(simdutf CONFIG QUIET)
            if(simdutf_FOUND AND TARGET simdutf::simdutf)
                message(STATUS "Found simdutf via cmake config (alongside ThreadSystem)")
                set(THREAD_SYSTEM_SIMDUTF_LIBRARY simdutf::simdutf CACHE STRING "" FORCE)
            else()
                # Fall back to searching in the thread_system FetchContent build tree
                get_filename_component(_thread_lib_dir "${THREAD_SYSTEM_LIBRARY}" DIRECTORY)
                find_library(THREAD_SYSTEM_SIMDUTF_LIBRARY
                    NAMES simdutf
                    PATHS
                        "${_thread_lib_dir}"
                        "${_thread_lib_dir}/../_deps/simdutf-build"
                        "${_thread_lib_dir}/../_deps/simdutf-build/lib"
                        "${CMAKE_SOURCE_DIR}/thread_system/build/_deps/simdutf-build"
                        "${CMAKE_SOURCE_DIR}/thread_system/build/_deps/simdutf-build/lib"
                    NO_DEFAULT_PATH
                )
                if(THREAD_SYSTEM_SIMDUTF_LIBRARY)
                    message(STATUS "Found simdutf in thread_system build tree: ${THREAD_SYSTEM_SIMDUTF_LIBRARY}")
                endif()
            endif()
        else()
            message(WARNING "thread_system library not found")
        endif()

        set(THREAD_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(THREAD_SYSTEM_INCLUDE_DIR ${THREAD_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
        set(THREAD_SYSTEM_LIBRARY ${THREAD_SYSTEM_LIBRARY} PARENT_SCOPE)
        set(THREAD_SYSTEM_SIMDUTF_LIBRARY ${THREAD_SYSTEM_SIMDUTF_LIBRARY} PARENT_SCOPE)
    else()
        message(WARNING "thread_system not found, integration disabled")
        set(BUILD_WITH_THREAD_SYSTEM OFF PARENT_SCOPE)
        set(THREAD_SYSTEM_FOUND FALSE PARENT_SCOPE)
    endif()
endfunction()

##################################################
# Find logger_system
##################################################
function(find_logger_system)
    if(NOT BUILD_WITH_LOGGER_SYSTEM)
        return()
    endif()

    message(STATUS "Looking for logger_system...")

    # Check if LOGGER_SYSTEM_INCLUDE_DIR is already set (e.g., by parent project via FetchContent)
    if(LOGGER_SYSTEM_INCLUDE_DIR AND EXISTS "${LOGGER_SYSTEM_INCLUDE_DIR}/kcenon/logger/core/logger.h")
        message(STATUS "Found logger_system via pre-set LOGGER_SYSTEM_INCLUDE_DIR: ${LOGGER_SYSTEM_INCLUDE_DIR}")
        set(LOGGER_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(LOGGER_SYSTEM_INCLUDE_DIR ${LOGGER_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
        return()
    endif()

    foreach(_candidate LoggerSystem logger logger_system::logger_system)
        if(TARGET ${_candidate})
            message(STATUS "Found logger_system CMake target: ${_candidate}")
            set(LOGGER_SYSTEM_FOUND TRUE PARENT_SCOPE)
            set(LOGGER_SYSTEM_TARGET ${_candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Prioritize environment variable, then standard paths
    set(_logger_search_paths)
    set(_logger_lib_paths)

    # 1. Check environment variable
    if(DEFINED ENV{LOGGER_SYSTEM_ROOT})
        list(APPEND _logger_search_paths "$ENV{LOGGER_SYSTEM_ROOT}/include")
        list(APPEND _logger_lib_paths "$ENV{LOGGER_SYSTEM_ROOT}/build/lib")
    endif()

    # 2. Check CMake variable
    if(DEFINED LOGGER_SYSTEM_ROOT)
        list(APPEND _logger_search_paths "${LOGGER_SYSTEM_ROOT}/include")
        list(APPEND _logger_lib_paths "${LOGGER_SYSTEM_ROOT}/build/lib")
    endif()

    # 3. CI workspace path (actions/checkout@v4 with path: logger_system)
    list(APPEND _logger_search_paths
        "${CMAKE_SOURCE_DIR}/logger_system/include"
    )

    # 4. Local development paths
    list(APPEND _logger_search_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../logger_system/include"
        "${CMAKE_SOURCE_DIR}/../logger_system/include"
        "../logger_system/include"
    )

    list(APPEND _logger_lib_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../logger_system/build/lib"
        "${CMAKE_SOURCE_DIR}/logger_system/build/lib"
        "${CMAKE_SOURCE_DIR}/../logger_system/build/lib"
        "../logger_system/build/lib"
    )

    find_path(LOGGER_SYSTEM_INCLUDE_DIR
        NAMES kcenon/logger/core/logger.h
        PATHS ${_logger_search_paths}
        NO_DEFAULT_PATH
    )

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found logger_system at: ${LOGGER_SYSTEM_INCLUDE_DIR}")

        find_library(LOGGER_SYSTEM_LIBRARY
            NAMES LoggerSystem logger_system logger
            PATHS ${_logger_lib_paths}
            NO_DEFAULT_PATH
        )

        if(LOGGER_SYSTEM_LIBRARY)
            message(STATUS "Found logger_system library: ${LOGGER_SYSTEM_LIBRARY}")
        else()
            message(WARNING "logger_system library not found")
        endif()

        set(LOGGER_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(LOGGER_SYSTEM_INCLUDE_DIR ${LOGGER_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
        set(LOGGER_SYSTEM_LIBRARY ${LOGGER_SYSTEM_LIBRARY} PARENT_SCOPE)
    else()
        message(WARNING "logger_system not found, integration disabled")
        set(BUILD_WITH_LOGGER_SYSTEM OFF PARENT_SCOPE)
        set(LOGGER_SYSTEM_FOUND FALSE PARENT_SCOPE)
    endif()
endfunction()

##################################################
# Find common_system
##################################################
function(find_common_system)
    if(NOT BUILD_WITH_COMMON_SYSTEM)
        return()
    endif()

    message(STATUS "Looking for common_system...")

    # Check if COMMON_SYSTEM_INCLUDE_DIR is already set (e.g., by parent project via FetchContent)
    if(COMMON_SYSTEM_INCLUDE_DIR AND EXISTS "${COMMON_SYSTEM_INCLUDE_DIR}/kcenon/common/patterns/result.h")
        message(STATUS "Found common_system via pre-set COMMON_SYSTEM_INCLUDE_DIR: ${COMMON_SYSTEM_INCLUDE_DIR}")
        set(COMMON_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(COMMON_SYSTEM_INCLUDE_DIR ${COMMON_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
        return()
    endif()

    foreach(_candidate common_system kcenon::common kcenon::common_system)
        if(TARGET ${_candidate})
            message(STATUS "Found common_system CMake target: ${_candidate}")
            set(COMMON_SYSTEM_FOUND TRUE PARENT_SCOPE)
            set(COMMON_SYSTEM_TARGET ${_candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Prioritize environment variable, then CMAKE_PREFIX_PATH, then standard paths
    set(_common_search_paths)

    # 1. Check environment variable
    if(DEFINED ENV{COMMON_SYSTEM_ROOT})
        list(APPEND _common_search_paths "$ENV{COMMON_SYSTEM_ROOT}/include")
    endif()

    # 2. Check CMake variable
    if(DEFINED COMMON_SYSTEM_ROOT)
        list(APPEND _common_search_paths "${COMMON_SYSTEM_ROOT}/include")
    endif()

    # 3. CMAKE_PREFIX_PATH (set by CI via -DCMAKE_PREFIX_PATH)
    # This is critical for CI builds where dependencies are installed to a custom location
    if(CMAKE_PREFIX_PATH)
        foreach(_prefix ${CMAKE_PREFIX_PATH})
            list(APPEND _common_search_paths "${_prefix}/include")
        endforeach()
    endif()

    # 4. CI workspace path (actions/checkout@v4 with path: common_system)
    list(APPEND _common_search_paths
        "${CMAKE_SOURCE_DIR}/common_system/include"
    )

    # 5. Local development paths
    list(APPEND _common_search_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../common_system/include"
        "${CMAKE_SOURCE_DIR}/../common_system/include"
        "../common_system/include"
    )

    # Try explicit paths first
    find_path(COMMON_SYSTEM_INCLUDE_DIR
        NAMES kcenon/common/patterns/result.h
        PATHS ${_common_search_paths}
        NO_DEFAULT_PATH
    )

    # If not found in explicit paths, let CMake search standard locations
    # This allows find_path to use CMAKE_PREFIX_PATH automatically
    if(NOT COMMON_SYSTEM_INCLUDE_DIR)
        find_path(COMMON_SYSTEM_INCLUDE_DIR
            NAMES kcenon/common/patterns/result.h
        )
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found common_system at: ${COMMON_SYSTEM_INCLUDE_DIR}")
        set(COMMON_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(COMMON_SYSTEM_INCLUDE_DIR ${COMMON_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
    else()
        message(WARNING "common_system not found, integration disabled")
        set(BUILD_WITH_COMMON_SYSTEM OFF PARENT_SCOPE)
        set(COMMON_SYSTEM_FOUND FALSE PARENT_SCOPE)
    endif()
endfunction()

##################################################
# Find gRPC library (optional - for NETWORK_ENABLE_GRPC_OFFICIAL)
#
# gRPC is a complex library with many dependencies (protobuf, abseil, etc.)
# FetchContent is not recommended due to long build times (~30+ minutes).
# Instead, we recommend using a package manager:
#   - macOS:  brew install grpc
#   - Ubuntu: apt-get install libgrpc++-dev protobuf-compiler-grpc
#   - vcpkg:  vcpkg install grpc
#
# Required versions:
#   - grpc++ >= 1.51.1
#   - protobuf >= 3.21.12
#   - abseil (bundled with grpc, but may need explicit linking)
##################################################
function(find_grpc_library)
    if(NOT NETWORK_ENABLE_GRPC_OFFICIAL)
        set(GRPC_FOUND FALSE PARENT_SCOPE)
        return()
    endif()

    message(STATUS "Looking for gRPC library (official)...")

    # Try CMake config package first (preferred - works with vcpkg, brew, etc.)
    find_package(gRPC CONFIG QUIET)
    if(gRPC_FOUND)
        message(STATUS "Found gRPC via CMake config")

        # Get version if available
        if(DEFINED gRPC_VERSION)
            message(STATUS "  gRPC version: ${gRPC_VERSION}")
            if(gRPC_VERSION VERSION_LESS "1.51.1")
                message(WARNING "gRPC version ${gRPC_VERSION} is below recommended 1.51.1")
            endif()
        endif()

        # Find Protobuf (required by gRPC)
        find_package(Protobuf CONFIG QUIET)
        if(NOT Protobuf_FOUND)
            find_package(Protobuf REQUIRED)
        endif()
        message(STATUS "  Protobuf version: ${Protobuf_VERSION}")

        if(Protobuf_VERSION VERSION_LESS "3.21.12")
            message(WARNING "Protobuf version ${Protobuf_VERSION} is below recommended 3.21.12")
        endif()

        # Find absl (required by gRPC >= 1.50)
        # gRPC 1.50+ requires abseil, which is usually bundled or installed alongside
        find_package(absl CONFIG QUIET)
        if(absl_FOUND)
            message(STATUS "  Abseil: Found")
            set(ABSEIL_FOUND TRUE PARENT_SCOPE)
        else()
            # abseil might be bundled with gRPC installation
            message(STATUS "  Abseil: Not found separately (may be bundled with gRPC)")
            set(ABSEIL_FOUND FALSE PARENT_SCOPE)
        endif()

        set(GRPC_FOUND TRUE PARENT_SCOPE)
        set(GRPC_CMAKE_CONFIG TRUE PARENT_SCOPE)
        return()
    endif()

    # Try pkg-config as fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GRPCPP QUIET grpc++)
        pkg_check_modules(PROTOBUF QUIET protobuf)
        pkg_check_modules(ABSL_BASE QUIET absl_base)

        if(GRPCPP_FOUND AND PROTOBUF_FOUND)
            message(STATUS "Found gRPC via pkg-config")
            message(STATUS "  gRPC++ version: ${GRPCPP_VERSION}")
            message(STATUS "  Protobuf version: ${PROTOBUF_VERSION}")

            if(ABSL_BASE_FOUND)
                message(STATUS "  Abseil: Found")
                set(ABSEIL_FOUND TRUE PARENT_SCOPE)
            else()
                message(STATUS "  Abseil: Not found separately")
                set(ABSEIL_FOUND FALSE PARENT_SCOPE)
            endif()

            set(GRPC_FOUND TRUE PARENT_SCOPE)
            set(GRPC_PKGCONFIG TRUE PARENT_SCOPE)
            set(GRPCPP_INCLUDE_DIRS ${GRPCPP_INCLUDE_DIRS} PARENT_SCOPE)
            set(GRPCPP_LIBRARIES ${GRPCPP_LIBRARIES} PARENT_SCOPE)
            set(PROTOBUF_INCLUDE_DIRS ${PROTOBUF_INCLUDE_DIRS} PARENT_SCOPE)
            set(PROTOBUF_LIBRARIES ${PROTOBUF_LIBRARIES} PARENT_SCOPE)
            return()
        endif()
    endif()

    # gRPC not found
    message(WARNING "")
    message(WARNING "========================================")
    message(WARNING "gRPC not found")
    message(WARNING "========================================")
    message(WARNING "")
    message(WARNING "NETWORK_ENABLE_GRPC_OFFICIAL is ON but gRPC was not found.")
    message(WARNING "The prototype implementation will be used instead.")
    message(WARNING "")
    message(WARNING "To use the official gRPC library, install it using:")
    message(WARNING "  macOS:   brew install grpc")
    message(WARNING "  Ubuntu:  apt-get install libgrpc++-dev protobuf-compiler-grpc")
    message(WARNING "  vcpkg:   vcpkg install grpc")
    message(WARNING "  Windows: vcpkg install grpc:x64-windows")
    message(WARNING "")
    message(WARNING "Required versions:")
    message(WARNING "  grpc++ >= 1.51.1")
    message(WARNING "  protobuf >= 3.21.12")
    message(WARNING "========================================")

    set(GRPC_FOUND FALSE PARENT_SCOPE)
    set(NETWORK_ENABLE_GRPC_OFFICIAL OFF PARENT_SCOPE)
endfunction()

##################################################
# Main dependency finding function
##################################################
function(find_network_system_dependencies)
    message(STATUS "Finding network_system dependencies...")

    find_asio_library()
    find_container_system()
    find_thread_system()
    find_logger_system()
    find_common_system()
    find_grpc_library()

    # Export all variables to parent scope
    # ASIO variables (standalone only - Boost.ASIO not supported)
    set(ASIO_FOUND ${ASIO_FOUND} PARENT_SCOPE)
    set(ASIO_INCLUDE_DIR ${ASIO_INCLUDE_DIR} PARENT_SCOPE)
    set(ASIO_TARGET ${ASIO_TARGET} PARENT_SCOPE)
    set(ASIO_FETCHED ${ASIO_FETCHED} PARENT_SCOPE)

    # System integration variables
    set(CONTAINER_SYSTEM_FOUND ${CONTAINER_SYSTEM_FOUND} PARENT_SCOPE)
    set(CONTAINER_SYSTEM_INCLUDE_DIR ${CONTAINER_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
    set(CONTAINER_SYSTEM_NEW_INCLUDE_DIR ${CONTAINER_SYSTEM_NEW_INCLUDE_DIR} PARENT_SCOPE)
    set(CONTAINER_SYSTEM_LIBRARY ${CONTAINER_SYSTEM_LIBRARY} PARENT_SCOPE)
    set(CONTAINER_SYSTEM_TARGET ${CONTAINER_SYSTEM_TARGET} PARENT_SCOPE)

    set(THREAD_SYSTEM_FOUND ${THREAD_SYSTEM_FOUND} PARENT_SCOPE)
    set(THREAD_SYSTEM_INCLUDE_DIR ${THREAD_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
    set(THREAD_SYSTEM_LIBRARY ${THREAD_SYSTEM_LIBRARY} PARENT_SCOPE)

    set(LOGGER_SYSTEM_FOUND ${LOGGER_SYSTEM_FOUND} PARENT_SCOPE)
    set(LOGGER_SYSTEM_INCLUDE_DIR ${LOGGER_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
    set(LOGGER_SYSTEM_LIBRARY ${LOGGER_SYSTEM_LIBRARY} PARENT_SCOPE)

    set(COMMON_SYSTEM_FOUND ${COMMON_SYSTEM_FOUND} PARENT_SCOPE)
    set(COMMON_SYSTEM_INCLUDE_DIR ${COMMON_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)

    # gRPC variables (optional - for NETWORK_ENABLE_GRPC_OFFICIAL)
    set(GRPC_FOUND ${GRPC_FOUND} PARENT_SCOPE)
    set(GRPC_CMAKE_CONFIG ${GRPC_CMAKE_CONFIG} PARENT_SCOPE)
    set(GRPC_PKGCONFIG ${GRPC_PKGCONFIG} PARENT_SCOPE)
    set(GRPCPP_INCLUDE_DIRS ${GRPCPP_INCLUDE_DIRS} PARENT_SCOPE)
    set(GRPCPP_LIBRARIES ${GRPCPP_LIBRARIES} PARENT_SCOPE)
    set(PROTOBUF_INCLUDE_DIRS ${PROTOBUF_INCLUDE_DIRS} PARENT_SCOPE)
    set(PROTOBUF_LIBRARIES ${PROTOBUF_LIBRARIES} PARENT_SCOPE)
    set(ABSEIL_FOUND ${ABSEIL_FOUND} PARENT_SCOPE)

    # Update BUILD_WITH_* flags if systems not found
    set(BUILD_WITH_CONTAINER_SYSTEM ${BUILD_WITH_CONTAINER_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_THREAD_SYSTEM ${BUILD_WITH_THREAD_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_LOGGER_SYSTEM ${BUILD_WITH_LOGGER_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_COMMON_SYSTEM ${BUILD_WITH_COMMON_SYSTEM} PARENT_SCOPE)
    set(NETWORK_ENABLE_GRPC_OFFICIAL ${NETWORK_ENABLE_GRPC_OFFICIAL} PARENT_SCOPE)

    message(STATUS "Dependency detection complete")
endfunction()
