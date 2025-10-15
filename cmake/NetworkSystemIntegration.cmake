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
    # First try using ASIO CMake target (vcpkg provides this)
    if(ASIO_TARGET AND TARGET ${ASIO_TARGET})
        target_link_libraries(${target} PRIVATE ${ASIO_TARGET})
        target_compile_definitions(${target} PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
        message(STATUS "Configured ${target} with ASIO target: ${ASIO_TARGET}")
    elseif(USE_BOOST_ASIO)
        target_include_directories(${target}
            PRIVATE
                $<BUILD_INTERFACE:${Boost_INCLUDE_DIRS}>
        )
        if(Boost_LIBRARIES)
            target_link_libraries(${target} PRIVATE ${Boost_LIBRARIES})
            message(STATUS "Configured ${target} with Boost.ASIO libraries")
        else()
            message(STATUS "Configured ${target} with Boost.ASIO header-only")
        endif()
        target_compile_definitions(${target} PRIVATE USE_BOOST_ASIO BOOST_ASIO_STANDALONE)
    elseif(ASIO_INCLUDE_DIR)
        # Always add ASIO include directory, even if it's a system path
        # This ensures compatibility across different CMake versions and platforms
        target_include_directories(${target}
            PRIVATE
                $<BUILD_INTERFACE:${ASIO_INCLUDE_DIR}>
        )
        target_compile_definitions(${target} PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
        message(STATUS "Configured ${target} with standalone ASIO at: ${ASIO_INCLUDE_DIR}")
    else()
        message(WARNING "${target}: ASIO not found - this target may fail to compile")
    endif()

    # Windows-specific definitions
    if(WIN32)
        target_compile_definitions(${target} PRIVATE _WIN32_WINNT=0x0601)
    endif()
endfunction()

##################################################
# Configure FMT integration
##################################################
function(setup_fmt_integration target)
    if(FMT_FOUND AND TARGET PkgConfig::FMT)
        target_link_libraries(${target} PUBLIC PkgConfig::FMT)
        target_compile_definitions(${target} PUBLIC USE_FMT_LIBRARY)
        message(STATUS "Configured ${target} with fmt library (pkgconfig)")
    elseif(FMT_INCLUDE_DIR)
        target_include_directories(${target}
            PUBLIC
                $<BUILD_INTERFACE:${FMT_INCLUDE_DIR}>
                $<INSTALL_INTERFACE:include>
        )
        if(FMT_LIBRARY)
            target_link_libraries(${target} PUBLIC ${FMT_LIBRARY})
            target_compile_definitions(${target} PUBLIC USE_FMT_LIBRARY)
            message(STATUS "Configured ${target} with fmt library")
        else()
            target_compile_definitions(${target} PUBLIC FMT_HEADER_ONLY)
            message(STATUS "Configured ${target} with fmt header-only")
        endif()
    elseif(USE_STD_FORMAT)
        target_compile_definitions(${target} PUBLIC USE_STD_FORMAT)
        message(STATUS "Configured ${target} with std::format")
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
        target_link_libraries(${target} PRIVATE ${CONTAINER_SYSTEM_TARGET})
        message(STATUS "Linked ${target} with container_system target")
    elseif(CONTAINER_SYSTEM_INCLUDE_DIR)
        target_include_directories(${target} PRIVATE ${CONTAINER_SYSTEM_INCLUDE_DIR})
        if(CONTAINER_SYSTEM_LIBRARY)
            target_link_libraries(${target} PRIVATE ${CONTAINER_SYSTEM_LIBRARY})
        endif()
        target_compile_definitions(${target} PRIVATE BUILD_WITH_CONTAINER_SYSTEM)
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

    if(THREAD_SYSTEM_INCLUDE_DIR)
        target_include_directories(${target} PRIVATE ${THREAD_SYSTEM_INCLUDE_DIR})
        if(THREAD_SYSTEM_LIBRARY)
            target_link_libraries(${target} PUBLIC ${THREAD_SYSTEM_LIBRARY})
        endif()
        target_compile_definitions(${target} PRIVATE BUILD_WITH_THREAD_SYSTEM)

        # Add thread_system adapter source
        target_sources(${target} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/integration/thread_system_adapter.cpp
        )
        message(STATUS "Configured ${target} with thread_system integration")
    endif()
endfunction()

##################################################
# Configure logger_system integration
##################################################
function(setup_logger_system_integration target)
    # Always add logger_integration.cpp as it provides fallback logging
    target_sources(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/integration/logger_integration.cpp
    )

    if(NOT BUILD_WITH_LOGGER_SYSTEM)
        return()
    endif()

    if(LOGGER_SYSTEM_INCLUDE_DIR)
        target_include_directories(${target} PRIVATE ${LOGGER_SYSTEM_INCLUDE_DIR})
        if(LOGGER_SYSTEM_LIBRARY)
            target_link_libraries(${target} PUBLIC ${LOGGER_SYSTEM_LIBRARY})
        endif()
        target_compile_definitions(${target} PRIVATE BUILD_WITH_LOGGER_SYSTEM)
        message(STATUS "Configured ${target} with logger_system integration")
    endif()
endfunction()

##################################################
# Configure common_system integration
##################################################
function(setup_common_system_integration target)
    if(NOT BUILD_WITH_COMMON_SYSTEM)
        return()
    endif()

    if(COMMON_SYSTEM_INCLUDE_DIR)
        target_include_directories(${target} PUBLIC
            $<BUILD_INTERFACE:${COMMON_SYSTEM_INCLUDE_DIR}>
        )
        target_compile_definitions(${target} PUBLIC BUILD_WITH_COMMON_SYSTEM)
        message(STATUS "Configured ${target} with common_system integration")
    endif()
endfunction()

##################################################
# Setup all integrations for a target
##################################################
function(setup_network_system_integrations target)
    message(STATUS "Setting up integrations for ${target}...")

    setup_asio_integration(${target})
    setup_fmt_integration(${target})
    setup_container_system_integration(${target})
    setup_thread_system_integration(${target})
    setup_logger_system_integration(${target})
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
