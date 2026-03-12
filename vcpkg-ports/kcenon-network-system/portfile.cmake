# kcenon-network-system portfile
# High-performance C++20 asynchronous network system with multi-protocol support

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/network_system
    REF e52be358d3fcddca86522a2326e748f127caa559
    SHA512 0  # TODO: Update with actual SHA512 hash after release
    HEAD_REF main
)

# Feature-based protocol and option selection
set(NET_BUILD_TLS OFF)
if("tls" IN_LIST FEATURES)
    set(NET_BUILD_TLS ON)
endif()

set(NET_BUILD_WEBSOCKET OFF)
if("websocket" IN_LIST FEATURES)
    set(NET_BUILD_WEBSOCKET ON)
endif()

set(NET_BUILD_LZ4 OFF)
if("lz4-compression" IN_LIST FEATURES)
    set(NET_BUILD_LZ4 ON)
endif()

set(NET_BUILD_ZLIB OFF)
if("zlib-compression" IN_LIST FEATURES)
    set(NET_BUILD_ZLIB ON)
endif()

set(NET_ENABLE_GRPC OFF)
if("grpc" IN_LIST FEATURES)
    set(NET_ENABLE_GRPC ON)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TLS_SUPPORT=${NET_BUILD_TLS}
        -DBUILD_WEBSOCKET_SUPPORT=${NET_BUILD_WEBSOCKET}
        -DBUILD_LZ4_COMPRESSION=${NET_BUILD_LZ4}
        -DBUILD_ZLIB_COMPRESSION=${NET_BUILD_ZLIB}
        -DNETWORK_ENABLE_GRPC_OFFICIAL=${NET_ENABLE_GRPC}
        -DBUILD_TESTS=OFF
        -DBUILD_SAMPLES=OFF
        -DNETWORK_BUILD_BENCHMARKS=OFF
        -DNETWORK_BUILD_INTEGRATION_TESTS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_MESSAGING_BRIDGE=OFF
        -DBUILD_WITH_LOGGER_SYSTEM=OFF
        -DBUILD_WITH_CONTAINER_SYSTEM=OFF
        -DBUILD_WITH_MONITORING_SYSTEM=OFF
        -DBUILD_VERIFY_BUILD=OFF
        -DNETWORK_BUILD_MODULES=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME NetworkSystem
    CONFIG_PATH lib/cmake/NetworkSystem
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
