
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was ContainerSystemConfig.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################

##################################################
# ContainerSystem Package Configuration
##################################################

include(CMakeFindDependencyMacro)

# Find required dependencies
find_dependency(Threads REQUIRED)
find_dependency(fmt CONFIG REQUIRED)

# Include targets
include("${CMAKE_CURRENT_LIST_DIR}/ContainerSystemTargets.cmake")

# Verify targets exist
check_required_components(ContainerSystem)

# Set variables for compatibility
set(ContainerSystem_FOUND TRUE)
set(CONTAINERSYSTEM_FOUND TRUE)

# Provide information about the package
set(ContainerSystem_VERSION 2.0.0)
set(ContainerSystem_INCLUDE_DIRS "/container_system")
set(ContainerSystem_LIBRARIES ContainerSystem::container)

# Legacy variables for backward compatibility
set(CONTAINERSYSTEM_VERSION ${ContainerSystem_VERSION})
set(CONTAINERSYSTEM_INCLUDE_DIRS ${ContainerSystem_INCLUDE_DIRS})
set(CONTAINERSYSTEM_LIBRARIES ${ContainerSystem_LIBRARIES})

message(STATUS "Found ContainerSystem: ${ContainerSystem_VERSION}")
