# Fuzzing targets for Network System protocol parsers
# Requires Clang with -fsanitize=fuzzer support

# Check if we're using Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    option(BUILD_FUZZING "Build fuzz targets" OFF)

    if(BUILD_FUZZING)
        message(STATUS "Building fuzz targets with libFuzzer")

        # Add fuzzer and sanitizer flags
        set(FUZZ_COMPILE_FLAGS "-fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer")
        set(FUZZ_LINK_FLAGS "-fsanitize=fuzzer,address,undefined")

        # Common include directories for all fuzz targets
        set(FUZZ_INCLUDE_DIRS
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
        )

        # --- HTTP/2 frame parser fuzz target ---
        add_executable(fuzz_http2_frame fuzz_http2_frame.cpp)
        target_link_libraries(fuzz_http2_frame PRIVATE network_system)
        target_include_directories(fuzz_http2_frame PRIVATE ${FUZZ_INCLUDE_DIRS})
        set_target_properties(fuzz_http2_frame PROPERTIES
            COMPILE_FLAGS "${FUZZ_COMPILE_FLAGS}"
            LINK_FLAGS "${FUZZ_LINK_FLAGS}"
        )

        # --- QUIC varint decoder fuzz target ---
        add_executable(fuzz_quic_varint fuzz_quic_varint.cpp)
        target_link_libraries(fuzz_quic_varint PRIVATE network_system)
        target_include_directories(fuzz_quic_varint PRIVATE ${FUZZ_INCLUDE_DIRS})
        set_target_properties(fuzz_quic_varint PROPERTIES
            COMPILE_FLAGS "${FUZZ_COMPILE_FLAGS}"
            LINK_FLAGS "${FUZZ_LINK_FLAGS}"
        )

        # --- QUIC frame parser fuzz target ---
        add_executable(fuzz_quic_frame fuzz_quic_frame.cpp)
        target_link_libraries(fuzz_quic_frame PRIVATE network_system)
        target_include_directories(fuzz_quic_frame PRIVATE ${FUZZ_INCLUDE_DIRS})
        set_target_properties(fuzz_quic_frame PROPERTIES
            COMPILE_FLAGS "${FUZZ_COMPILE_FLAGS}"
            LINK_FLAGS "${FUZZ_LINK_FLAGS}"
        )

        # --- gRPC message parser fuzz target ---
        add_executable(fuzz_grpc_frame fuzz_grpc_frame.cpp)
        target_link_libraries(fuzz_grpc_frame PRIVATE network_system)
        target_include_directories(fuzz_grpc_frame PRIVATE ${FUZZ_INCLUDE_DIRS})
        set_target_properties(fuzz_grpc_frame PROPERTIES
            COMPILE_FLAGS "${FUZZ_COMPILE_FLAGS}"
            LINK_FLAGS "${FUZZ_LINK_FLAGS}"
        )

        # --- HPACK header decoder fuzz target ---
        add_executable(fuzz_hpack_decode fuzz_hpack_decode.cpp)
        target_link_libraries(fuzz_hpack_decode PRIVATE network_system)
        target_include_directories(fuzz_hpack_decode PRIVATE ${FUZZ_INCLUDE_DIRS})
        set_target_properties(fuzz_hpack_decode PROPERTIES
            COMPILE_FLAGS "${FUZZ_COMPILE_FLAGS}"
            LINK_FLAGS "${FUZZ_LINK_FLAGS}"
        )

        # Create corpus directories
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/http2_frame)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/quic_varint)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/quic_frame)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/grpc_frame)
        file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/hpack_decode)

        # Copy seed corpus if it exists
        if(EXISTS ${CMAKE_SOURCE_DIR}/fuzz/corpus)
            file(COPY ${CMAKE_SOURCE_DIR}/fuzz/corpus DESTINATION ${CMAKE_BINARY_DIR})
        endif()
    endif()
else()
    message(STATUS "Fuzzing targets require Clang compiler - skipping")
endif()
