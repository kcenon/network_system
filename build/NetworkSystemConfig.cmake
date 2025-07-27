
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was NetworkSystemConfig.cmake.in                            ########

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
# NetworkSystem Package Configuration
##################################################

include(CMakeFindDependencyMacro)

# Find required dependencies
find_dependency(Threads REQUIRED)
find_dependency(asio CONFIG REQUIRED)
find_dependency(fmt CONFIG REQUIRED)

# Find container system dependency
find_dependency(ContainerSystem QUIET)
if(NOT ContainerSystem_FOUND)
    # Container system is required
    message(FATAL_ERROR "NetworkSystem requires ContainerSystem")
endif()

# Include targets
include("${CMAKE_CURRENT_LIST_DIR}/NetworkSystemTargets.cmake")

# Verify targets exist
check_required_components(NetworkSystem)

# Set variables for compatibility
set(NetworkSystem_FOUND TRUE)
set(NETWORKSYSTEM_FOUND TRUE)

# Provide information about the package
set(NetworkSystem_VERSION 1.0.0)
set(NetworkSystem_INCLUDE_DIRS "/network_system")
set(NetworkSystem_LIBRARIES NetworkSystem::network)

# Legacy variables for backward compatibility
set(NETWORKSYSTEM_VERSION ${NetworkSystem_VERSION})
set(NETWORKSYSTEM_INCLUDE_DIRS ${NetworkSystem_INCLUDE_DIRS})
set(NETWORKSYSTEM_LIBRARIES ${NetworkSystem_LIBRARIES})

message(STATUS "Found NetworkSystem: ${NetworkSystem_VERSION}")
