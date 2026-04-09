# kcenon-network-system portfile
# Modern C++20 async network library with TCP/UDP, HTTP/1.1, WebSocket, and TLS 1.3
#
# This overlay portfile uses the local project source for CI validation.
# The registry portfile (in kcenon/vcpkg-registry) downloads from GitHub tags.

# Use local project source when running as overlay port from the project tree
get_filename_component(_port_parent "${CURRENT_PORT_DIR}" DIRECTORY)
get_filename_component(_project_root "${_port_parent}" DIRECTORY)

if(EXISTS "${_project_root}/CMakeLists.txt"
   AND EXISTS "${_project_root}/scripts/validate_vcpkg_chain.sh")
    set(SOURCE_PATH "${_project_root}")
else()
    vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO kcenon/network_system
        REF "v${VERSION}"
        SHA512 2d147a3eac787919842c0d74c80eaf560e761b90dd435ba2f8d7d9459bf2a79d3dd637d20abe7204ffc72b98e82e4c0a6384b9a20ef8282777da05fca994304d
        HEAD_REF main
    )
endif()

# Static-only: the modular library targets (network-tcp, network-http2, etc.)
# do not export DLL symbols, causing LNK1104 on Windows shared builds.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        logging BUILD_WITH_LOGGER_SYSTEM
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=OFF
        -DBUILD_SAMPLES=OFF
        -DBUILD_WEBSOCKET_SUPPORT=ON
        -DBUILD_MESSAGING_BRIDGE=OFF
        -DNETWORK_BUILD_BENCHMARKS=OFF
        -DNETWORK_BUILD_INTEGRATION_TESTS=OFF
        -DNETWORK_BUILD_MODULES=OFF
        -DBUILD_WITH_COMMON_SYSTEM=ON
        -DBUILD_WITH_THREAD_SYSTEM=ON
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON
        "-DTHREAD_SYSTEM_INCLUDE_DIR=${CURRENT_INSTALLED_DIR}/include"
        "-DCOMMON_SYSTEM_INCLUDE_DIR=${CURRENT_INSTALLED_DIR}/include"
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()

# Install internal headers required by public headers.
# Several installed headers (messaging_session.h, secure_session.h, network_system.h,
# quic_session.h, network_system_bridge.h, network_config.h) reference headers from
# src/internal/ which are not installed by CMake. Copy them to the install tree so
# downstream consumers can compile against the package.
file(COPY "${SOURCE_PATH}/src/internal/"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include/internal/"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl"
)

# Install network-core modular library headers required by internal headers.
# src/internal/tcp/tcp_socket.h includes kcenon/network-core/interfaces/socket_observer.h
file(COPY "${SOURCE_PATH}/network-core/include/"
    DESTINATION "${CURRENT_PACKAGES_DIR}/include/"
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Ensure debug cmake config directory exists for vcpkg_cmake_config_fixup.
# The upstream install may only produce cmake config files in the release tree;
# copy them from release so the fixup can merge both configurations.
set(_rel_config_dir "${CURRENT_PACKAGES_DIR}/lib/cmake/network_system")
set(_dbg_config_dir "${CURRENT_PACKAGES_DIR}/debug/lib/cmake/network_system")
if(EXISTS "${_rel_config_dir}" AND NOT EXISTS "${_dbg_config_dir}")
    file(MAKE_DIRECTORY "${_dbg_config_dir}")
    file(GLOB _rel_configs "${_rel_config_dir}/*")
    foreach(_f ${_rel_configs})
        file(COPY "${_f}" DESTINATION "${_dbg_config_dir}")
    endforeach()
endif()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME network_system
    CONFIG_PATH lib/cmake/network_system
)

# Remove empty directories that cause vcpkg post-build validation warnings
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/include/kcenon/network/core"
    "${CURRENT_PACKAGES_DIR}/include/kcenon/network/experimental"
    "${CURRENT_PACKAGES_DIR}/include/kcenon/network/http"
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
