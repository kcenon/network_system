#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "ContainerSystem::container" for configuration "Release"
set_property(TARGET ContainerSystem::container APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(ContainerSystem::container PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libcontainer.a"
  )

list(APPEND _cmake_import_check_targets ContainerSystem::container )
list(APPEND _cmake_import_check_files_for_ContainerSystem::container "${_IMPORT_PREFIX}/lib/libcontainer.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
