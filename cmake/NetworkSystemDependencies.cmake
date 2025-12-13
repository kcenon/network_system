##################################################
# NetworkSystemDependencies.cmake
#
# Dependency finding module for NetworkSystem
# Handles ASIO, FMT, and system integrations
##################################################

include(FetchContent)

##################################################
# Find ASIO (standalone or Boost.ASIO)
##################################################
function(find_asio_library)
    message(STATUS "Looking for ASIO library...")

    # Try CMake config package first (vcpkg provides asioConfig.cmake)
    find_package(asio QUIET CONFIG)
    if(TARGET asio::asio)
        message(STATUS "Found ASIO via CMake package (target: asio::asio)")
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
        set(ASIO_INCLUDE_DIR ${ASIO_INCLUDE_DIR} PARENT_SCOPE)
        set(ASIO_FOUND TRUE PARENT_SCOPE)
        return()
    endif()

    # Note: Boost.ASIO fallback is NOT compatible with our source code
    # Source files use #include <asio.hpp> (standalone path)
    # Boost provides #include <boost/asio.hpp> (different path)
    # Therefore, we skip Boost.ASIO and fetch standalone ASIO directly

    # Fetch standalone ASIO as a last resort
    message(STATUS "Standalone ASIO not found locally - fetching asio-1-36-0 from upstream...")

    FetchContent_Declare(network_system_asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG asio-1-36-0
    )

    FetchContent_GetProperties(network_system_asio)
    if(NOT network_system_asio_POPULATED)
        FetchContent_Populate(network_system_asio)
    endif()

    set(_asio_include_dir "${network_system_asio_SOURCE_DIR}/asio/include")

    if(EXISTS "${_asio_include_dir}/asio.hpp")
        message(STATUS "Fetched standalone ASIO headers at: ${_asio_include_dir}")
        set(ASIO_INCLUDE_DIR ${_asio_include_dir} PARENT_SCOPE)
        set(ASIO_FOUND TRUE PARENT_SCOPE)
        set(ASIO_FETCHED TRUE PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR "Failed to fetch standalone ASIO - cannot build without ASIO")
endfunction()

##################################################
# Find FMT library
##################################################
function(find_fmt_library)
    message(STATUS "Looking for fmt library...")

    # Try pkgconfig first
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FMT QUIET IMPORTED_TARGET fmt)
        if(FMT_FOUND)
            message(STATUS "Found fmt via pkgconfig: ${FMT_VERSION}")
            set(FMT_FOUND TRUE PARENT_SCOPE)
            set(FMT_TARGET PkgConfig::FMT PARENT_SCOPE)
            return()
        endif()
    endif()

    # Manual search
    find_path(FMT_INCLUDE_DIR
        NAMES fmt/format.h
        PATHS
            /opt/homebrew/include
            /usr/local/include
            /usr/include
    )

    find_library(FMT_LIBRARY
        NAMES fmt
        PATHS
            /opt/homebrew/lib
            /usr/local/lib
            /usr/lib
    )

    if(FMT_INCLUDE_DIR)
        message(STATUS "Found fmt include dir: ${FMT_INCLUDE_DIR}")
        if(FMT_LIBRARY)
            message(STATUS "Found fmt library: ${FMT_LIBRARY}")
            set(FMT_FOUND TRUE PARENT_SCOPE)
            set(FMT_INCLUDE_DIR ${FMT_INCLUDE_DIR} PARENT_SCOPE)
            set(FMT_LIBRARY ${FMT_LIBRARY} PARENT_SCOPE)
        else()
            message(STATUS "Using fmt header-only")
            set(FMT_FOUND TRUE PARENT_SCOPE)
            set(FMT_INCLUDE_DIR ${FMT_INCLUDE_DIR} PARENT_SCOPE)
            set(FMT_HEADER_ONLY TRUE PARENT_SCOPE)
        endif()
    else()
        message(STATUS "FMT not found, will use std::format if available")
        set(FMT_FOUND FALSE PARENT_SCOPE)
        set(USE_STD_FORMAT ON PARENT_SCOPE)
    endif()
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

    # 3. Standard search paths
    list(APPEND _container_search_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../container_system"
        "${CMAKE_SOURCE_DIR}/container_system"
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

    # 3. Standard search paths
    list(APPEND _thread_search_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system/include"
        "${CMAKE_SOURCE_DIR}/thread_system/include"
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
        else()
            message(WARNING "thread_system library not found")
        endif()

        set(THREAD_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(THREAD_SYSTEM_INCLUDE_DIR ${THREAD_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
        set(THREAD_SYSTEM_LIBRARY ${THREAD_SYSTEM_LIBRARY} PARENT_SCOPE)
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

    # 3. Standard search paths
    list(APPEND _logger_search_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../logger_system/include"
        "${CMAKE_SOURCE_DIR}/logger_system/include"
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

    foreach(_candidate common_system kcenon::common)
        if(TARGET ${_candidate})
            message(STATUS "Found common_system CMake target: ${_candidate}")
            set(COMMON_SYSTEM_FOUND TRUE PARENT_SCOPE)
            set(COMMON_SYSTEM_TARGET ${_candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Prioritize environment variable, then standard paths
    set(_common_search_paths)

    # 1. Check environment variable
    if(DEFINED ENV{COMMON_SYSTEM_ROOT})
        list(APPEND _common_search_paths "$ENV{COMMON_SYSTEM_ROOT}/include")
    endif()

    # 2. Check CMake variable
    if(DEFINED COMMON_SYSTEM_ROOT)
        list(APPEND _common_search_paths "${COMMON_SYSTEM_ROOT}/include")
    endif()

    # 3. Standard search paths
    list(APPEND _common_search_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../common_system/include"
        "${CMAKE_SOURCE_DIR}/common_system/include"
        "${CMAKE_SOURCE_DIR}/../common_system/include"
        "../common_system/include"
    )

    find_path(COMMON_SYSTEM_INCLUDE_DIR
        NAMES kcenon/common/patterns/result.h
        PATHS ${_common_search_paths}
        NO_DEFAULT_PATH
    )

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
# Find monitoring_system
##################################################
function(find_monitoring_system)
    if(NOT BUILD_WITH_MONITORING_SYSTEM)
        return()
    endif()

    message(STATUS "Looking for monitoring_system...")

    foreach(_candidate monitoring_system MonitoringSystem)
        if(TARGET ${_candidate})
            message(STATUS "Found monitoring_system CMake target: ${_candidate}")
            set(MONITORING_SYSTEM_FOUND TRUE PARENT_SCOPE)
            set(MONITORING_SYSTEM_TARGET ${_candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    # Check for existing CMake targets first
    if(TARGET monitoring_system)
        message(STATUS "Found monitoring_system as existing target")
        set(MONITORING_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(MONITORING_SYSTEM_TARGET monitoring_system PARENT_SCOPE)
        return()
    endif()

    if(TARGET MonitoringSystem)
        message(STATUS "Found MonitoringSystem as existing target")
        set(MONITORING_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(MONITORING_SYSTEM_TARGET MonitoringSystem PARENT_SCOPE)
        return()
    endif()

    # Prioritize environment variable, then standard paths
    set(_monitoring_search_paths)
    set(_monitoring_lib_paths)

    # 1. Check environment variable
    if(DEFINED ENV{MONITORING_SYSTEM_ROOT})
        list(APPEND _monitoring_search_paths "$ENV{MONITORING_SYSTEM_ROOT}/include" "$ENV{MONITORING_SYSTEM_ROOT}/sources")
        list(APPEND _monitoring_lib_paths "$ENV{MONITORING_SYSTEM_ROOT}/build/lib")
    endif()

    # 2. Check CMake variable
    if(DEFINED MONITORING_SYSTEM_ROOT)
        list(APPEND _monitoring_search_paths "${MONITORING_SYSTEM_ROOT}/include" "${MONITORING_SYSTEM_ROOT}/sources")
        list(APPEND _monitoring_lib_paths "${MONITORING_SYSTEM_ROOT}/build/lib")
    endif()

    # 3. Standard search paths
    list(APPEND _monitoring_search_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../monitoring_system/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../monitoring_system/sources"
        "${CMAKE_SOURCE_DIR}/monitoring_system/include"
        "${CMAKE_SOURCE_DIR}/monitoring_system/sources"
        "${CMAKE_SOURCE_DIR}/../monitoring_system/include"
        "${CMAKE_SOURCE_DIR}/../monitoring_system/sources"
        "../monitoring_system/include"
        "../monitoring_system/sources"
    )

    list(APPEND _monitoring_lib_paths
        "${CMAKE_CURRENT_SOURCE_DIR}/../monitoring_system/build/lib"
        "${CMAKE_SOURCE_DIR}/monitoring_system/build/lib"
        "${CMAKE_SOURCE_DIR}/../monitoring_system/build/lib"
        "../monitoring_system/build/lib"
    )

    # Look for monitoring headers
    find_path(MONITORING_SYSTEM_INCLUDE_DIR
        NAMES kcenon/monitoring/core/performance_monitor.h monitoring/core/performance_monitor.h
        PATHS ${_monitoring_search_paths}
        NO_DEFAULT_PATH
    )

    if(MONITORING_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found monitoring_system at: ${MONITORING_SYSTEM_INCLUDE_DIR}")

        find_library(MONITORING_SYSTEM_LIBRARY
            NAMES monitoring_system MonitoringSystem
            PATHS ${_monitoring_lib_paths}
            PATH_SUFFIXES Debug Release
            NO_DEFAULT_PATH
        )

        if(MONITORING_SYSTEM_LIBRARY)
            message(STATUS "Found monitoring_system library: ${MONITORING_SYSTEM_LIBRARY}")
        else()
            message(WARNING "monitoring_system headers found but library not found")
        endif()

        set(MONITORING_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(MONITORING_SYSTEM_INCLUDE_DIR ${MONITORING_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
        set(MONITORING_SYSTEM_LIBRARY ${MONITORING_SYSTEM_LIBRARY} PARENT_SCOPE)
    else()
        message(WARNING "monitoring_system not found, integration disabled")
        set(BUILD_WITH_MONITORING_SYSTEM OFF PARENT_SCOPE)
        set(MONITORING_SYSTEM_FOUND FALSE PARENT_SCOPE)
    endif()
endfunction()

##################################################
# Main dependency finding function
##################################################
function(find_network_system_dependencies)
    message(STATUS "Finding NetworkSystem dependencies...")

    find_asio_library()
    find_fmt_library()
    find_container_system()
    find_thread_system()
    find_logger_system()
    find_monitoring_system()
    find_common_system()

    # Export all variables to parent scope
    # ASIO variables (standalone only - Boost.ASIO not supported)
    set(ASIO_FOUND ${ASIO_FOUND} PARENT_SCOPE)
    set(ASIO_INCLUDE_DIR ${ASIO_INCLUDE_DIR} PARENT_SCOPE)
    set(ASIO_TARGET ${ASIO_TARGET} PARENT_SCOPE)
    set(ASIO_FETCHED ${ASIO_FETCHED} PARENT_SCOPE)

    # FMT variables
    set(FMT_FOUND ${FMT_FOUND} PARENT_SCOPE)
    set(FMT_INCLUDE_DIR ${FMT_INCLUDE_DIR} PARENT_SCOPE)
    set(FMT_LIBRARY ${FMT_LIBRARY} PARENT_SCOPE)
    set(FMT_TARGET ${FMT_TARGET} PARENT_SCOPE)
    set(FMT_HEADER_ONLY ${FMT_HEADER_ONLY} PARENT_SCOPE)
    set(USE_STD_FORMAT ${USE_STD_FORMAT} PARENT_SCOPE)

    # System integration variables
    set(CONTAINER_SYSTEM_FOUND ${CONTAINER_SYSTEM_FOUND} PARENT_SCOPE)
    set(CONTAINER_SYSTEM_INCLUDE_DIR ${CONTAINER_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
    set(CONTAINER_SYSTEM_LIBRARY ${CONTAINER_SYSTEM_LIBRARY} PARENT_SCOPE)
    set(CONTAINER_SYSTEM_TARGET ${CONTAINER_SYSTEM_TARGET} PARENT_SCOPE)

    set(THREAD_SYSTEM_FOUND ${THREAD_SYSTEM_FOUND} PARENT_SCOPE)
    set(THREAD_SYSTEM_INCLUDE_DIR ${THREAD_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
    set(THREAD_SYSTEM_LIBRARY ${THREAD_SYSTEM_LIBRARY} PARENT_SCOPE)

    set(LOGGER_SYSTEM_FOUND ${LOGGER_SYSTEM_FOUND} PARENT_SCOPE)
    set(LOGGER_SYSTEM_INCLUDE_DIR ${LOGGER_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
    set(LOGGER_SYSTEM_LIBRARY ${LOGGER_SYSTEM_LIBRARY} PARENT_SCOPE)

    set(MONITORING_SYSTEM_FOUND ${MONITORING_SYSTEM_FOUND} PARENT_SCOPE)
    set(MONITORING_SYSTEM_INCLUDE_DIR ${MONITORING_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
    set(MONITORING_SYSTEM_LIBRARY ${MONITORING_SYSTEM_LIBRARY} PARENT_SCOPE)
    set(MONITORING_SYSTEM_TARGET ${MONITORING_SYSTEM_TARGET} PARENT_SCOPE)

    set(COMMON_SYSTEM_FOUND ${COMMON_SYSTEM_FOUND} PARENT_SCOPE)
    set(COMMON_SYSTEM_INCLUDE_DIR ${COMMON_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)

    # Update BUILD_WITH_* flags if systems not found
    set(BUILD_WITH_CONTAINER_SYSTEM ${BUILD_WITH_CONTAINER_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_THREAD_SYSTEM ${BUILD_WITH_THREAD_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_LOGGER_SYSTEM ${BUILD_WITH_LOGGER_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_MONITORING_SYSTEM ${BUILD_WITH_MONITORING_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_COMMON_SYSTEM ${BUILD_WITH_COMMON_SYSTEM} PARENT_SCOPE)

    message(STATUS "Dependency detection complete")
endfunction()
