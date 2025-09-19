cmake_minimum_required(VERSION 3.16)

# Set policy for FindBoost module (suppress warning)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

##################################################
# Network System CMakeLists.txt
#
# Independent high-performance network system
# Separated from messaging_system for modularity
##################################################

# Project definition
project(NetworkSystem
    VERSION 2.0.0
    DESCRIPTION "Independent High-Performance Network System"
    LANGUAGES CXX
)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_SAMPLES "Build samples" ON)
option(BUILD_WITH_CONTAINER_SYSTEM "Build with container_system integration" ON)
option(BUILD_WITH_THREAD_SYSTEM "Build with thread_system integration" ON)
option(BUILD_MESSAGING_BRIDGE "Build messaging_system bridge" ON)

# Global compile features
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find packages with flexible detection
# Try modern CMake targets first, then fallback to manual detection
find_package(PkgConfig QUIET)

# ASIO Detection (standalone, not Boost.ASIO)
find_path(ASIO_INCLUDE_DIR
    NAMES asio.hpp
    PATHS
        # vcpkg paths
        ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include
        # macOS Homebrew
        /opt/homebrew/include
        /usr/local/include
        # Ubuntu/Debian system paths
        /usr/include
        /usr/include/asio
        # MSYS2/MinGW paths
        /mingw64/include
        /mingw32/include
        # Custom installation paths
        ${ASIO_ROOT}/include
        $ENV{ASIO_ROOT}/include
)

# FMT Detection with pkgconfig support
if(PkgConfig_FOUND)
    pkg_check_modules(FMT QUIET IMPORTED_TARGET fmt)
    if(FMT_FOUND)
        message(STATUS "Found fmt via pkgconfig: ${FMT_VERSION}")
    endif()
endif()

if(NOT FMT_FOUND)
    find_path(FMT_INCLUDE_DIR
        NAMES fmt/format.h
        PATHS
            # vcpkg paths
            ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include
            # macOS Homebrew
            /opt/homebrew/include
            /usr/local/include
            # Ubuntu/Debian system paths
            /usr/include
            # MSYS2/MinGW paths
            /mingw64/include
            /mingw32/include
            # Custom installation paths
            ${FMT_ROOT}/include
            $ENV{FMT_ROOT}/include
    )

    if(FMT_INCLUDE_DIR)
        find_library(FMT_LIBRARY
            NAMES fmt
            PATHS
                # vcpkg paths
                ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/lib
                # macOS Homebrew
                /opt/homebrew/lib
                /usr/local/lib
                # Ubuntu/Debian system paths
                /usr/lib
                /usr/lib/x86_64-linux-gnu
                # MSYS2/MinGW paths
                /mingw64/lib
                /mingw32/lib
        )
    endif()
endif()

# Check for ASIO availability
if(NOT ASIO_INCLUDE_DIR)
    message(STATUS "ASIO not found in standard locations. Checking alternative approaches...")

    # Try to find Boost.ASIO as fallback
    # First try modern CMake config mode
    find_package(Boost QUIET CONFIG COMPONENTS system)
    if(NOT Boost_FOUND)
        # Fallback to module mode for older installations
        find_package(Boost QUIET MODULE COMPONENTS system)
    endif()

    if(Boost_FOUND)
        message(STATUS "Using Boost.ASIO as fallback (version: ${Boost_VERSION})")
        set(USE_BOOST_ASIO ON)
    else()
        # Try to find boost headers manually for header-only usage
        find_path(BOOST_INCLUDE_DIR
            NAMES boost/asio.hpp
            PATHS
                /usr/include
                /usr/local/include
                /opt/homebrew/include
                /mingw64/include
                /mingw32/include
        )

        if(BOOST_INCLUDE_DIR)
            message(STATUS "Using Boost.ASIO headers-only fallback at: ${BOOST_INCLUDE_DIR}")
            set(USE_BOOST_ASIO ON)
            set(Boost_INCLUDE_DIRS ${BOOST_INCLUDE_DIR})
        else()
            message(FATAL_ERROR "ASIO not found. Please install one of:")
            message(FATAL_ERROR "  - libasio-dev (Ubuntu/Debian): sudo apt install libasio-dev")
            message(FATAL_ERROR "  - Boost libraries: sudo apt install libboost-all-dev")
            message(FATAL_ERROR "  - vcpkg: vcpkg install asio")
            message(FATAL_ERROR "  - macOS Homebrew: brew install asio boost")
        endif()
    endif()
else()
    message(STATUS "Found ASIO at: ${ASIO_INCLUDE_DIR}")
    set(USE_BOOST_ASIO OFF)
endif()

# FMT library check
if(FMT_FOUND)
    message(STATUS "Found fmt via pkgconfig")
elseif(FMT_INCLUDE_DIR)
    message(STATUS "Found fmt at: ${FMT_INCLUDE_DIR}")
    if(FMT_LIBRARY)
        message(STATUS "Found fmt library at: ${FMT_LIBRARY}")
    endif()
else()
    message(STATUS "FMT not found, will use std::format (C++20) if available")
    set(USE_STD_FORMAT ON)
endif()

# Conditional dependencies
if(BUILD_WITH_CONTAINER_SYSTEM)
    find_path(CONTAINER_SYSTEM_INCLUDE_DIR
        NAMES container_system/container.h
        PATHS ../container_system/include
        NO_DEFAULT_PATH
    )
    if(CONTAINER_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found container_system at: ${CONTAINER_SYSTEM_INCLUDE_DIR}")
        add_compile_definitions(BUILD_WITH_CONTAINER_SYSTEM)
    else()
        message(WARNING "container_system not found, integration disabled")
        set(BUILD_WITH_CONTAINER_SYSTEM OFF)
    endif()
endif()

if(BUILD_WITH_THREAD_SYSTEM)
    find_path(THREAD_SYSTEM_INCLUDE_DIR
        NAMES thread_system/thread_pool.h
        PATHS ../thread_system/include
        NO_DEFAULT_PATH
    )
    if(THREAD_SYSTEM_INCLUDE_DIR)
        message(STATUS "Found thread_system at: ${THREAD_SYSTEM_INCLUDE_DIR}")
        add_compile_definitions(BUILD_WITH_THREAD_SYSTEM)
    else()
        message(WARNING "thread_system not found, integration disabled")
        set(BUILD_WITH_THREAD_SYSTEM OFF)
    endif()
endif()

# Create main library
add_library(NetworkSystem
    # Core implementation
    src/core/messaging_client.cpp
    src/core/messaging_server.cpp

    # Session management
    src/session/messaging_session.cpp

    # Internal implementation
    src/internal/tcp_socket.cpp
    src/internal/send_coroutine.cpp
    src/internal/pipeline.cpp
)

# Set target properties
set_target_properties(NetworkSystem PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Include directories
target_include_directories(NetworkSystem
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# External system include directories
if(BUILD_WITH_CONTAINER_SYSTEM AND CONTAINER_SYSTEM_INCLUDE_DIR)
    target_include_directories(NetworkSystem PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
endif()

if(BUILD_WITH_THREAD_SYSTEM AND THREAD_SYSTEM_INCLUDE_DIR)
    target_include_directories(NetworkSystem PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
endif()

# Configure ASIO
if(USE_BOOST_ASIO)
    target_include_directories(NetworkSystem PRIVATE ${Boost_INCLUDE_DIRS})
    if(Boost_LIBRARIES)
        target_link_libraries(NetworkSystem PRIVATE ${Boost_LIBRARIES})
        message(STATUS "Configured to use Boost.ASIO with libraries")
    else()
        message(STATUS "Configured to use Boost.ASIO header-only")
    endif()
    target_compile_definitions(NetworkSystem PRIVATE USE_BOOST_ASIO)
    target_compile_definitions(NetworkSystem PRIVATE BOOST_ASIO_STANDALONE)
elseif(ASIO_INCLUDE_DIR)
    target_include_directories(NetworkSystem PRIVATE ${ASIO_INCLUDE_DIR})
    target_compile_definitions(NetworkSystem PRIVATE ASIO_STANDALONE)
    target_compile_definitions(NetworkSystem PRIVATE ASIO_NO_DEPRECATED)
    message(STATUS "Configured to use standalone ASIO")
endif()

# Configure FMT
if(FMT_FOUND AND TARGET PkgConfig::FMT)
    target_link_libraries(NetworkSystem PRIVATE PkgConfig::FMT)
    target_compile_definitions(NetworkSystem PRIVATE USE_FMT_LIBRARY)
    message(STATUS "Configured to use fmt library via pkgconfig")
elseif(FMT_INCLUDE_DIR)
    target_include_directories(NetworkSystem PRIVATE ${FMT_INCLUDE_DIR})
    if(FMT_LIBRARY)
        target_link_libraries(NetworkSystem PRIVATE ${FMT_LIBRARY})
        target_compile_definitions(NetworkSystem PRIVATE USE_FMT_LIBRARY)
        message(STATUS "Configured to use fmt library")
    else()
        target_compile_definitions(NetworkSystem PRIVATE FMT_HEADER_ONLY)
        message(STATUS "Configured to use fmt header-only")
    endif()
elseif(USE_STD_FORMAT)
    target_compile_definitions(NetworkSystem PRIVATE USE_STD_FORMAT)
    message(STATUS "Configured to use std::format")
endif()

# Link system libraries
target_link_libraries(NetworkSystem
    PUBLIC
        pthread
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(NetworkSystem PRIVATE ws2_32 mswsock)
endif()

# Integration layer (conditional compilation)
if(BUILD_MESSAGING_BRIDGE)
    target_sources(NetworkSystem PRIVATE
        src/integration/messaging_bridge.cpp
    )
    add_compile_definitions(BUILD_MESSAGING_BRIDGE)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Samples
if(BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

# Installation rules
include(GNUInstallDirs)

# Install library
install(TARGETS NetworkSystem
    EXPORT NetworkSystemTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

# Config file generation
include(CMakePackageConfigHelpers)

# Generate config file
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NetworkSystemConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)

# Generate version file
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

# Install config files
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)

# Install export targets
install(EXPORT NetworkSystemTargets
    FILE NetworkSystemTargets.cmake
    NAMESPACE NetworkSystem::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
)

# Print configuration summary
message(STATUS "NetworkSystem Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build shared libs: ${BUILD_SHARED_LIBS}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  Build samples: ${BUILD_SAMPLES}")
message(STATUS "  Container system integration: ${BUILD_WITH_CONTAINER_SYSTEM}")
message(STATUS "  Thread system integration: ${BUILD_WITH_THREAD_SYSTEM}")
message(STATUS "  Messaging bridge: ${BUILD_MESSAGING_BRIDGE}")

# Cleanup old files
set(OLD_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/session
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
    ${CMAKE_CURRENT_SOURCE_DIR}/network
    ${CMAKE_CURRENT_SOURCE_DIR}/network.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_old.txt
)

# Note: We'll clean these manually after successful build