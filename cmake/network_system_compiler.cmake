##################################################
# network_system_compiler.cmake
#
# C++ standard, compile-feature defaults, and global
# build flags shared across all targets.
#
# This module is intentionally minimal — per-target compile
# features are applied where the target is created (e.g.
# target_compile_features in network_system_targets.cmake).
##################################################

##################################################
# C++ Standard
##################################################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

##################################################
# Compile-time helpers shared by all targets
##################################################
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# All shared libraries must be position-independent. Static
# libraries that are linked into shared libraries also need
# this; setting it globally is the simplest correct default.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
