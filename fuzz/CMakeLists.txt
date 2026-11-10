# Fuzz the production protocol parsers with Clang/libFuzzer.
option(BUILD_FUZZING "Build fuzz targets" OFF)
if(NOT BUILD_FUZZING)
    return()
endif()
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "BUILD_FUZZING requires Clang with libFuzzer support")
endif()

# Instrument the parser implementations, not just the thin fuzz harnesses.
target_compile_options(network_system PRIVATE
    -fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
target_link_options(network_system PUBLIC -fsanitize=address,undefined)

set(FUZZ_PARSERS http2_frame quic_varint quic_frame grpc_frame hpack_decode http_parser)
if(BUILD_WEBSOCKET_SUPPORT)
    list(APPEND FUZZ_PARSERS websocket_frame websocket_handshake)
endif()

foreach(parser IN LISTS FUZZ_PARSERS)
    add_executable(fuzz_${parser} fuzz_${parser}.cpp)
    set_target_properties(fuzz_${parser} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    target_link_libraries(fuzz_${parser} PRIVATE network_system)
    target_include_directories(fuzz_${parser} PRIVATE
        ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
    target_compile_options(fuzz_${parser} PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(fuzz_${parser} PRIVATE -fsanitize=fuzzer,address,undefined)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/corpus/${parser})
endforeach()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/corpus)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/corpus DESTINATION ${CMAKE_BINARY_DIR})
endif()
