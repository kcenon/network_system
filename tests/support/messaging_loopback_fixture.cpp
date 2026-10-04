// BSD 3-Clause License
// Copyright (c) 2026, kcenon
// See the LICENSE file in the project root for full license information.

/**
 * @file messaging_loopback_fixture.cpp
 * @brief Implementation of plain-TCP messaging fixture helpers (Issue #1088).
 */

#include "messaging_loopback_fixture.h"

#include <cstdlib>
#include <string_view>

namespace kcenon::network::tests::support
{

bool is_sanitizer_run() noexcept
{
#if defined(NETWORK_SYSTEM_SANITIZER)
    return true;
#else
    const auto flag_set = [](const char* value) {
        return value != nullptr && *value != '\0' && std::string_view(value) != "0";
    };
    return flag_set(std::getenv("TSAN_OPTIONS"))
        || flag_set(std::getenv("ASAN_OPTIONS"))
        || flag_set(std::getenv("UBSAN_OPTIONS"))
        || flag_set(std::getenv("SANITIZER"))
        || flag_set(std::getenv("NETWORK_SYSTEM_SANITIZER"));
#endif
}

} // namespace kcenon::network::tests::support
