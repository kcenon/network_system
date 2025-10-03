##################################################
# NetworkSystemInstall.cmake
#
# Installation configuration for NetworkSystem
##################################################

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

##################################################
# Install library
##################################################
function(install_network_system_library)
    install(TARGETS NetworkSystem
        EXPORT NetworkSystemTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    message(STATUS "Configured library installation")
endfunction()

##################################################
# Install headers
##################################################
function(install_network_system_headers)
    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )
    message(STATUS "Configured header installation")
endfunction()

##################################################
# Install CMake config files
##################################################
function(install_cmake_config_files)
    # Generate config file
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/NetworkSystemConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/NetworkSystem
    )

    # Generate version file
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/NetworkSystemConfigVersion.cmake"
        VERSION 0.0.0
        COMPATIBILITY AnyNewerVersion
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
