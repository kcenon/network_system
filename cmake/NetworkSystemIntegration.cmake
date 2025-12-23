##################################################
# NetworkSystemIntegration.cmake
#
# Integration configuration for NetworkSystem
# Handles linking with external systems
##################################################

##################################################
# Configure ASIO integration
##################################################
function(setup_asio_integration target)
    # Note: This project only supports standalone ASIO (not Boost.ASIO)
    # Source files use #include <asio.hpp> which is standalone ASIO path
    # Boost.ASIO would require #include <boost/asio.hpp>

    # First try using ASIO CMake target (vcpkg provides this)
    if(ASIO_TARGET AND TARGET ${ASIO_TARGET})
        target_link_libraries(${target} PRIVATE ${ASIO_TARGET})
        target_compile_definitions(${target} PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
        message(STATUS "Configured ${target} with ASIO target: ${ASIO_TARGET}")
    elseif(ASIO_INCLUDE_DIR)
        # Standalone ASIO via include directory
        # Use PUBLIC because:
        # 1. NetworkSystem's source files (network_system.cpp) include headers that use <asio.hpp>
        # 2. NetworkSystem's public headers include <asio.hpp>
        # 3. Dependent systems (like database_system) need access to ASIO headers
        target_include_directories(${target}
            PUBLIC
                $<BUILD_INTERFACE:${ASIO_INCLUDE_DIR}>
        )
        target_compile_definitions(${target} PUBLIC ASIO_STANDALONE ASIO_NO_DEPRECATED)
        message(STATUS "Configured ${target} with standalone ASIO at: ${ASIO_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "${target}: Standalone ASIO not found - cannot compile without ASIO")
    endif()

    # Windows-specific definitions
    if(WIN32)
        target_compile_definitions(${target} PRIVATE _WIN32_WINNT=0x0601)
    endif()
endfunction()


##################################################
# Configure container_system integration
##################################################
function(setup_container_system_integration target)
    if(NOT BUILD_WITH_CONTAINER_SYSTEM)
        return()
    endif()

    if(CONTAINER_SYSTEM_TARGET)
        target_link_libraries(${target} PUBLIC ${CONTAINER_SYSTEM_TARGET})
        target_compile_definitions(${target} PRIVATE WITH_CONTAINER_SYSTEM)
        message(STATUS "Linked ${target} with container_system target: ${CONTAINER_SYSTEM_TARGET}")
    elseif(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(${target} PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        if(CONTAINER_SYSTEM_LIBRARY)
            target_link_libraries(${target} PRIVATE ${CONTAINER_SYSTEM_LIBRARY})
        endif()
        target_compile_definitions(${target} PRIVATE WITH_CONTAINER_SYSTEM)
        message(STATUS "Configured ${target} with container_system paths")
    endif()
endfunction()

##################################################
# Configure thread_system integration
##################################################
function(setup_thread_system_integration target)
    if(NOT BUILD_WITH_THREAD_SYSTEM)
        return()
    endif()

    set(_thread_integration FALSE)

    # First check if ThreadSystem CMake target exists (from FetchContent or add_subdirectory)
    # This is the preferred method when building as part of a larger project
    if(TARGET ThreadSystem)
        target_link_libraries(${target} PUBLIC ThreadSystem)
        target_compile_definitions(${target} PRIVATE WITH_THREAD_SYSTEM)
        # thread_system builds with KCENON_HAS_COMMON_EXECUTOR when common_system is available
        # We must match this definition to ensure consistent class layout across compilation units
        if(BUILD_WITH_COMMON_SYSTEM)
            target_compile_definitions(${target} PRIVATE KCENON_HAS_COMMON_EXECUTOR=1)
        endif()
        set(_thread_integration TRUE)
        message(STATUS "Configured ${target} with ThreadSystem CMake target")
    elseif(THREAD_SYSTEM_TARGET)
        target_link_libraries(${target} PUBLIC ${THREAD_SYSTEM_TARGET})
        target_compile_definitions(${target} PRIVATE WITH_THREAD_SYSTEM)
        if(BUILD_WITH_COMMON_SYSTEM)
            target_compile_definitions(${target} PRIVATE KCENON_HAS_COMMON_EXECUTOR=1)
        endif()
        set(_thread_integration TRUE)
        message(STATUS "Configured ${target} with thread_system target: ${THREAD_SYSTEM_TARGET}")
    elseif(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(${target} PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        if(THREAD_SYSTEM_LIBRARY)
            target_link_libraries(${target} PUBLIC ${THREAD_SYSTEM_LIBRARY})
        endif()
        target_compile_definitions(${target} PRIVATE WITH_THREAD_SYSTEM)
        if(BUILD_WITH_COMMON_SYSTEM)
            target_compile_definitions(${target} PRIVATE KCENON_HAS_COMMON_EXECUTOR=1)
        endif()
        set(_thread_integration TRUE)
        message(STATUS "Configured ${target} with thread_system integration")
    endif()

    if(_thread_integration)
        target_sources(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/integration/thread_system_adapter.cpp
        )
    endif()
endfunction()

##################################################
# Configure logger_system integration
#
# Issue #285: logger_system is now OPTIONAL.
# Logging uses common_system's ILogger and GlobalLoggerRegistry by default.
# If BUILD_WITH_LOGGER_SYSTEM is ON, logger_system_adapter is available
# for runtime binding.
##################################################
function(setup_logger_system_integration target)
    # Always add logger_integration.cpp - it now uses common_system's logging
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/integration/logger_integration.cpp
    )

    # logger_system is optional - if enabled, add the adapter for runtime binding
    if(NOT BUILD_WITH_LOGGER_SYSTEM)
        message(STATUS "${target}: Using common_system's ILogger (no logger_system dependency)")
        return()
    endif()

    # First check if LoggerSystem CMake target exists (from FetchContent or add_subdirectory)
    if(TARGET LoggerSystem)
        target_link_libraries(${target} PUBLIC LoggerSystem)
        target_compile_definitions(${target} PRIVATE WITH_LOGGER_SYSTEM)
        message(STATUS "Configured ${target} with LoggerSystem CMake target")
    elseif(LOGGER_SYSTEM_TARGET)
        target_link_libraries(${target} PUBLIC ${LOGGER_SYSTEM_TARGET})
        target_compile_definitions(${target} PRIVATE WITH_LOGGER_SYSTEM)
        message(STATUS "Configured ${target} with logger_system target: ${LOGGER_SYSTEM_TARGET}")
    elseif(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(${target} PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        if(LOGGER_SYSTEM_LIBRARY)
            target_link_libraries(${target} PUBLIC ${LOGGER_SYSTEM_LIBRARY})
        endif()
        target_compile_definitions(${target} PRIVATE WITH_LOGGER_SYSTEM)
        message(STATUS "Configured ${target} with logger_system integration")
    else()
        message(STATUS "${target}: logger_system not found, using common_system's ILogger")
    endif()
endfunction()

##################################################
# Configure monitoring_system integration
##################################################
function(setup_monitoring_system_integration target)
    if(NOT BUILD_WITH_MONITORING_SYSTEM)
        return()
    endif()

    # First check for CMake target
    if(MONITORING_SYSTEM_TARGET)
        target_link_libraries(${target} PUBLIC ${MONITORING_SYSTEM_TARGET})
        target_compile_definitions(${target} PUBLIC WITH_MONITORING_SYSTEM)
        message(STATUS "Linked ${target} with monitoring_system target: ${MONITORING_SYSTEM_TARGET}")
    elseif(MONITORING_SYSTEM_INCLUDE_DIR)
        target_include_directories(${target} PUBLIC ${MONITORING_SYSTEM_INCLUDE_DIR})
        if(MONITORING_SYSTEM_LIBRARY)
            target_link_libraries(${target} PUBLIC ${MONITORING_SYSTEM_LIBRARY})
        endif()
        target_compile_definitions(${target} PUBLIC WITH_MONITORING_SYSTEM)
        message(STATUS "Configured ${target} with monitoring_system paths")
    else()
        message(WARNING "BUILD_WITH_MONITORING_SYSTEM is ON but monitoring_system not found")
        set(BUILD_WITH_MONITORING_SYSTEM OFF PARENT_SCOPE)
    endif()
endfunction()

##################################################
# Configure common_system integration
##################################################
function(setup_common_system_integration target)
    if(NOT BUILD_WITH_COMMON_SYSTEM)
        return()
    endif()

    if(COMMON_SYSTEM_TARGET)
        target_link_libraries(${target} PUBLIC ${COMMON_SYSTEM_TARGET})
        target_compile_definitions(${target} PUBLIC WITH_COMMON_SYSTEM)
        message(STATUS "Configured ${target} with common_system target: ${COMMON_SYSTEM_TARGET}")
    elseif(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(${target} PUBLIC
            $<BUILD_INTERFACE:${COMMON_SYSTEM_INCLUDE_DIR}>
        )
        target_compile_definitions(${target} PUBLIC WITH_COMMON_SYSTEM)
        message(STATUS "Configured ${target} with common_system integration")
    endif()
endfunction()

##################################################
# Setup all integrations for a target
##################################################
function(setup_network_system_integrations target)
    message(STATUS "Setting up integrations for ${target}...")

    setup_asio_integration(${target})
    setup_container_system_integration(${target})
    setup_thread_system_integration(${target})
    setup_logger_system_integration(${target})
    setup_monitoring_system_integration(${target})
    setup_common_system_integration(${target})

    # Platform-specific libraries
    if(WIN32)
        target_link_libraries(${target} PRIVATE ws2_32 mswsock)
    endif()

    if(NOT WIN32)
        target_link_libraries(${target} PUBLIC pthread)
    endif()

    message(STATUS "Integration setup complete for ${target}")
endfunction()
