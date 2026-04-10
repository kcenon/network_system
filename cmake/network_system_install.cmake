##################################################
# NetworkSystemInstall.cmake
#
# Installation configuration for network_system
##################################################

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

##################################################
# Install library
##################################################
function(install_network_system_library)
    # Build list of targets to install
    set(_INSTALL_TARGETS network_system)

    # Include network-core if it exists (Issue #538, #539)
    if(TARGET network-core)
        list(APPEND _INSTALL_TARGETS network-core)
    endif()

    install(TARGETS ${_INSTALL_TARGETS}
        EXPORT network_system-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    message(STATUS "Configured library installation (targets: ${_INSTALL_TARGETS})")
endfunction()

##################################################
# Install headers
##################################################
function(install_network_system_headers)
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    # Install internal headers under namespaced path (Issue #944, vcpkg-registry#79)
    # Public headers use #include "kcenon/network/internal/..." which resolves
    # via the PRIVATE src/ include directory at build time. At install time
    # these must be available under the namespaced include directory.
    install(DIRECTORY src/internal/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/kcenon/network/internal
        FILES_MATCHING PATTERN "*.h"
    )

    # Backward-compatible wrapper headers at the old install path (Issue vcpkg-registry#79)
    # These forward to the new namespaced location with a deprecation warning.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/cmake/compat/internal/)
        install(DIRECTORY cmake/compat/internal/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/internal
            FILES_MATCHING PATTERN "*.h"
        )
    endif()

    # Install network-core headers (Issue #944)
    # network-core is an INTERFACE library whose headers are needed by
    # internal headers (e.g. tcp_socket.h includes network-core interfaces).
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/network-core/include/)
        install(DIRECTORY network-core/include/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h"
        )
    endif()

    message(STATUS "Configured header installation")
endfunction()

##################################################
# Install CMake config files
##################################################
function(install_cmake_config_files)
    # Generate config file
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/network_system-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/network_system-config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network_system
    )

    # Generate version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/network_system-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY AnyNewerVersion
    )

    # Install config files
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/network_system-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/network_system-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network_system
    )

    # Install export targets
    install(EXPORT network_system-targets
        FILE network_system-targets.cmake
        NAMESPACE network_system::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/network_system
    )

    message(STATUS "Configured CMake config file installation")
endfunction()

##################################################
# Main installation setup
##################################################
function(setup_network_system_install)
    install_network_system_library()
    install_network_system_headers()
    install_cmake_config_files()

    message(STATUS "Installation configuration complete")
endfunction()
