# kcenon-thread-system portfile
# High-performance C++20 multithreading framework
# Backport the public ABI export from thread_system#749 to the pinned 0.3.2
# archive. Keep its published tag and archive hash unchanged.

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO kcenon/thread_system
    REF "v${VERSION}"
    SHA512 075e63bbb968209296f459ae26c230226a124a1b789f914dd92b86dd6b0d6c26336bae97c2e2fc4e751f2cad4b604bd433b2461a60eb7b9f6b16b0db929986b6
    HEAD_REF main
    PATCHES export-public-abi.patch
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_WITH_COMMON_SYSTEM=ON
        -DTHREAD_BUILD_INTEGRATION_TESTS=OFF
        -DBUILD_DOCUMENTATION=OFF
        -DTHREAD_ENABLE_LOCKFREE_QUEUE=ON
        -DTHREAD_ENABLE_WORK_STEALING=OFF
        -DFETCHCONTENT_FULLY_DISCONNECTED=ON
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME thread_system
    CONFIG_PATH lib/cmake/thread_system
)

# In legacy/component build mode the config does not create a
# thread_system::thread_system umbrella target.  Downstream projects
# (monitoring_system) link against that canonical name, so inject the
# alias when it is missing after the config has been installed.
vcpkg_replace_string(
    "${CURRENT_PACKAGES_DIR}/share/thread_system/thread_system-config.cmake"
    "check_required_components(thread_system)"
    "# Canonical umbrella alias (added by vcpkg portfile)\nif(TARGET thread_system::thread_base AND NOT TARGET thread_system::thread_system)\n    add_library(thread_system::thread_system ALIAS thread_system::thread_base)\nendif()\n\ncheck_required_components(thread_system)"
)

# Remove empty directories that cause vcpkg post-build validation errors
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/include/thread_system/interfaces")

# Fix absolute paths in pkgconfig files
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")

configure_file("${CMAKE_CURRENT_LIST_DIR}/usage" "${CURRENT_PACKAGES_DIR}/share/${PORT}/usage" COPYONLY)
