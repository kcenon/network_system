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
    if(TARGET ContainerSystem::container)
        message(STATUS "Found container_system CMake target")
        set(CONTAINER_SYSTEM_FOUND TRUE PARENT_SCOPE)
        set(CONTAINER_SYSTEM_TARGET ContainerSystem::container PARENT_SCOPE)
        return()
    endif()

    # Path-based detection - prioritize Sources/ directory
    if(APPLE)
        set(_container_search_paths
            /Users/${USER}/Sources/container_system
            ../container_system
            ${CMAKE_CURRENT_SOURCE_DIR}/../container_system
        )
        set(_container_lib_paths
            /Users/${USER}/Sources/container_system/build/lib
            ../container_system/build/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/../container_system/build/lib
        )
    else()
        set(_container_search_paths
            /home/${USER}/Sources/container_system
            ../container_system
            ${CMAKE_CURRENT_SOURCE_DIR}/../container_system
        )
        set(_container_lib_paths
            /home/${USER}/Sources/container_system/build/lib
            ../container_system/build/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/../container_system/build/lib
        )
    endif()

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

    # Strategy 1: Check for thread_system source directory first (development environment)
    # Priority order:
    # 1. User's Sources directory (HIGHEST PRIORITY)
    # 2. Sibling directory (workspace layout)
    # 3. CONFIG mode (installed package)

    set(_thread_source_paths
        "/Users/$ENV{USER}/Sources/thread_system"     # macOS development
        "/home/$ENV{USER}/Sources/thread_system"      # Linux development
        "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system" # Sibling directory
    )

    set(_FOUND_THREAD_SOURCE FALSE)
    set(_THREAD_BUILD_DIR "")
    foreach(_path ${_thread_source_paths})
        if(EXISTS "${_path}/CMakeLists.txt")
            message(STATUS "Found thread_system source at: ${_path}")
            set(_THREAD_BUILD_DIR "${_path}/build")
            set(_FOUND_THREAD_SOURCE TRUE)
            break()
        endif()
    endforeach()

    # Strategy 2: Try CONFIG mode
    # Only use thread_system_DIR if thread_system-targets.cmake exists (complete installation)
    set(_try_config_mode FALSE)
    if(_FOUND_THREAD_SOURCE AND EXISTS "${_THREAD_BUILD_DIR}/thread_system-targets.cmake")
        message(STATUS "Found complete thread_system build at: ${_THREAD_BUILD_DIR}")
        set(thread_system_DIR "${_THREAD_BUILD_DIR}" CACHE PATH "thread_system build directory")
        set(_try_config_mode TRUE)
    elseif(NOT _FOUND_THREAD_SOURCE)
        # Try system-installed package (no thread_system_DIR set)
        set(_try_config_mode TRUE)
    else()
        message(STATUS "thread_system build incomplete (missing targets file), skipping CONFIG mode")
    endif()

    if(_try_config_mode)
        find_package(thread_system CONFIG QUIET)
    endif()

    if(thread_system_FOUND)
        message(STATUS "Found thread_system via CONFIG mode")

        # Check if targets are actually available
        set(_has_valid_target FALSE)
        if(TARGET thread_system::thread_base)
            get_target_property(_thread_includes thread_system::thread_base INTERFACE_INCLUDE_DIRECTORIES)
            if(_thread_includes)
                set(THREAD_SYSTEM_INCLUDE_DIR "${_thread_includes}" PARENT_SCOPE)
                set(THREAD_SYSTEM_FOUND TRUE PARENT_SCOPE)
                set(_has_valid_target TRUE)
                message(STATUS "thread_system include from target: ${_thread_includes}")
            endif()
        endif()

        # If CONFIG found but targets are incomplete, fall back to manual search
        if(_has_valid_target)
            return()
        else()
            message(WARNING "thread_system CONFIG found but targets incomplete, falling back to manual search")
        endif()
    endif()

    # Strategy 3: Manual search in prioritized paths as fallback

    if(APPLE)
        set(_thread_search_paths
            "/Users/$ENV{USER}/Sources/thread_system/include"     # macOS development
            "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system/include" # Sibling directory
            "../thread_system/include"                             # Relative fallback
            "/usr/local/include"                                   # Homebrew default
            "/opt/homebrew/include"                                # Apple Silicon Homebrew
        )
        set(_thread_lib_paths
            "/Users/$ENV{USER}/Sources/thread_system/build/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system/build/lib"
            "../thread_system/build/lib"
            "/usr/local/lib"
            "/opt/homebrew/lib"
        )
    else()
        set(_thread_search_paths
            "/home/$ENV{USER}/Sources/thread_system/include"      # Linux development
            "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system/include" # Sibling directory
            "../thread_system/include"                             # Relative fallback
            "/usr/local/include"                                   # Standard Unix path
            "/usr/include"                                         # System path
        )
        set(_thread_lib_paths
            "/home/$ENV{USER}/Sources/thread_system/build/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/../thread_system/build/lib"
            "../thread_system/build/lib"
            "/usr/local/lib"
            "/usr/lib"
            "/usr/lib/x86_64-linux-gnu"  # Ubuntu/Debian x86_64
            "/usr/lib/aarch64-linux-gnu" # Ubuntu/Debian ARM64
        )
    endif()

    find_path(THREAD_SYSTEM_INCLUDE_DIR
        NAMES kcenon/thread/core/thread_pool.h
        PATHS ${_thread_search_paths}
        DOC "thread_system include directory"
    )

    if(THREAD_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found thread_system headers at: ${THREAD_SYSTEM_INCLUDE_DIR}")

        # Search for thread_system library with multiple possible names
        find_library(THREAD_SYSTEM_LIBRARY
            NAMES
                ThreadSystem      # Standard name (CMake target export)
                thread_system     # Alternative name (lowercase)
                thread_base       # Legacy name
            PATHS ${_thread_lib_paths}
            DOC "thread_system library"
        )

        if(THREAD_SYSTEM_LIBRARY)
            message(STATUS "Found thread_system library: ${THREAD_SYSTEM_LIBRARY}")
            set(THREAD_SYSTEM_FOUND TRUE PARENT_SCOPE)
            set(THREAD_SYSTEM_INCLUDE_DIR ${THREAD_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)
            set(THREAD_SYSTEM_LIBRARY ${THREAD_SYSTEM_LIBRARY} PARENT_SCOPE)
        else()
            message(WARNING "thread_system headers found but library not found")
            message(WARNING "Searched in: ${_thread_lib_paths}")
            set(THREAD_SYSTEM_FOUND FALSE PARENT_SCOPE)
            set(BUILD_WITH_THREAD_SYSTEM OFF PARENT_SCOPE)
        endif()
    else()
        message(WARNING "thread_system headers not found")
        message(WARNING "Searched in: ${_thread_search_paths}")
        message(WARNING "thread_system integration will be disabled")
        message(WARNING "To fix: Install thread_system or set THREAD_SYSTEM_INCLUDE_DIR manually")
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

    # Prioritize Sources/ directory
    if(APPLE)
        set(_logger_search_paths
            /Users/${USER}/Sources/logger_system/include
            ../logger_system/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../logger_system/include
        )
        set(_logger_lib_paths
            /Users/${USER}/Sources/logger_system/build/lib
            ../logger_system/build/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/../logger_system/build/lib
        )
    else()
        set(_logger_search_paths
            /home/${USER}/Sources/logger_system/include
            ../logger_system/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../logger_system/include
        )
        set(_logger_lib_paths
            /home/${USER}/Sources/logger_system/build/lib
            ../logger_system/build/lib
            ${CMAKE_CURRENT_SOURCE_DIR}/../logger_system/build/lib
        )
    endif()

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

    # Prioritize Sources/ directory
    if(APPLE)
        set(_common_search_paths
            /Users/${USER}/Sources/common_system/include
            ../common_system/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../common_system/include
        )
    else()
        set(_common_search_paths
            /home/${USER}/Sources/common_system/include
            ../common_system/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../common_system/include
        )
    endif()

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
# Main dependency finding function
##################################################
function(find_network_system_dependencies)
    message(STATUS "Finding NetworkSystem dependencies...")

    find_asio_library()
    find_fmt_library()
    find_container_system()
    find_thread_system()
    find_logger_system()
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

    set(COMMON_SYSTEM_FOUND ${COMMON_SYSTEM_FOUND} PARENT_SCOPE)
    set(COMMON_SYSTEM_INCLUDE_DIR ${COMMON_SYSTEM_INCLUDE_DIR} PARENT_SCOPE)

    # Update BUILD_WITH_* flags if systems not found
    set(BUILD_WITH_CONTAINER_SYSTEM ${BUILD_WITH_CONTAINER_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_THREAD_SYSTEM ${BUILD_WITH_THREAD_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_LOGGER_SYSTEM ${BUILD_WITH_LOGGER_SYSTEM} PARENT_SCOPE)
    set(BUILD_WITH_COMMON_SYSTEM ${BUILD_WITH_COMMON_SYSTEM} PARENT_SCOPE)

    message(STATUS "Dependency detection complete")
endfunction()
