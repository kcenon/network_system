# FindProtobuf.cmake
#
# Find Protocol Buffers and gRPC libraries for gRPC integration
#
# This module finds if Protocol Buffers and gRPC are installed and
# provides helper functions for generating protobuf/grpc code.
#
# Usage:
#   find_package(Protobuf REQUIRED)
#   find_package(gRPC CONFIG)
#   add_proto_library(my_proto proto/service.proto)
#
# Variables:
#   PROTOBUF_FOUND - True if Protocol Buffers was found
#   PROTOBUF_INCLUDE_DIRS - Include directories for protobuf
#   PROTOBUF_LIBRARIES - Libraries to link against
#   GRPC_FOUND - True if gRPC was found
#

include(CMakeFindDependencyMacro)

# Find Protobuf package
function(find_protobuf_library)
    find_package(Protobuf CONFIG QUIET)
    if(Protobuf_FOUND)
        set(PROTOBUF_FOUND TRUE PARENT_SCOPE)
        message(STATUS "Found Protobuf (CONFIG): ${Protobuf_VERSION}")
        return()
    endif()

    find_package(Protobuf QUIET)
    if(Protobuf_FOUND)
        set(PROTOBUF_FOUND TRUE PARENT_SCOPE)
        message(STATUS "Found Protobuf (Module): ${Protobuf_VERSION}")
        return()
    endif()

    # Try pkg-config as fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PROTOBUF QUIET protobuf)
        if(PROTOBUF_FOUND)
            message(STATUS "Found Protobuf (pkg-config): ${PROTOBUF_VERSION}")
            set(PROTOBUF_FOUND TRUE PARENT_SCOPE)
            return()
        endif()
    endif()

    message(STATUS "Protobuf not found - gRPC support will be limited")
    set(PROTOBUF_FOUND FALSE PARENT_SCOPE)
endfunction()

# Find gRPC package
function(find_grpc_library)
    find_package(gRPC CONFIG QUIET)
    if(gRPC_FOUND)
        set(GRPC_FOUND TRUE PARENT_SCOPE)
        message(STATUS "Found gRPC (CONFIG)")
        return()
    endif()

    # Try pkg-config as fallback
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GRPC QUIET grpc++)
        if(GRPC_FOUND)
            message(STATUS "Found gRPC (pkg-config): ${GRPC_VERSION}")
            set(GRPC_FOUND TRUE PARENT_SCOPE)
            return()
        endif()
    endif()

    message(STATUS "gRPC not found - using built-in implementation")
    set(GRPC_FOUND FALSE PARENT_SCOPE)
endfunction()

# Function to add a protobuf library from .proto files
# Usage: add_proto_library(TARGET proto1.proto proto2.proto)
function(add_proto_library TARGET)
    if(NOT PROTOBUF_FOUND)
        message(WARNING "Protobuf not found - cannot generate proto files for ${TARGET}")
        return()
    endif()

    set(PROTO_FILES ${ARGN})
    set(ALL_SRCS "")
    set(ALL_HDRS "")

    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)

        if(NOT IS_ABSOLUTE ${PROTO_FILE})
            set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}")
            set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_DIR}")
        endif()

        set(PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
        set(PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
        set(GRPC_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
        set(GRPC_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h")

        # Generate protobuf files
        if(TARGET protobuf::protoc)
            set(PROTOC_CMD protobuf::protoc)
        else()
            find_program(PROTOC_CMD protoc)
        endif()

        if(PROTOC_CMD)
            add_custom_command(
                OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
                COMMAND ${PROTOC_CMD}
                ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                     -I ${PROTO_DIR}
                     ${PROTO_FILE}
                DEPENDS ${PROTO_FILE}
                COMMENT "Generating protobuf files for ${PROTO_NAME}"
            )
            list(APPEND ALL_SRCS ${PROTO_SRCS})
            list(APPEND ALL_HDRS ${PROTO_HDRS})

            # Generate gRPC files if gRPC is found
            if(GRPC_FOUND AND TARGET gRPC::grpc_cpp_plugin)
                add_custom_command(
                    OUTPUT ${GRPC_SRCS} ${GRPC_HDRS}
                    COMMAND ${PROTOC_CMD}
                    ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
                         --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
                         -I ${PROTO_DIR}
                         ${PROTO_FILE}
                    DEPENDS ${PROTO_FILE}
                    COMMENT "Generating gRPC files for ${PROTO_NAME}"
                )
                list(APPEND ALL_SRCS ${GRPC_SRCS})
                list(APPEND ALL_HDRS ${GRPC_HDRS})
            endif()
        endif()
    endforeach()

    if(ALL_SRCS)
        add_library(${TARGET} ${ALL_SRCS})
        target_include_directories(${TARGET} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

        if(TARGET protobuf::libprotobuf)
            target_link_libraries(${TARGET} PUBLIC protobuf::libprotobuf)
        elseif(PROTOBUF_LIBRARIES)
            target_link_libraries(${TARGET} PUBLIC ${PROTOBUF_LIBRARIES})
        endif()

        if(GRPC_FOUND)
            if(TARGET gRPC::grpc++)
                target_link_libraries(${TARGET} PUBLIC gRPC::grpc++)
            elseif(GRPC_LIBRARIES)
                target_link_libraries(${TARGET} PUBLIC ${GRPC_LIBRARIES})
            endif()
        endif()
    else()
        message(WARNING "No proto files generated for ${TARGET}")
    endif()
endfunction()
