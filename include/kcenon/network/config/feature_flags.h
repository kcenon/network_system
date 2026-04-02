// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file feature_flags.h
 * @brief Public header for compile-time feature flags
 *
 * This header provides KCENON_WITH_* macros for conditional compilation
 * based on available features and integrations.
 *
 * @note This is the public API path for feature flags. Use this header
 *       instead of including detail/config/feature_flags.h directly.
 *
 * Available flags:
 * - KCENON_WITH_COMMON_SYSTEM: common_system integration
 * - KCENON_WITH_THREAD_SYSTEM: thread_system integration
 * - KCENON_WITH_LOGGER_SYSTEM: logger_system integration
 * - KCENON_WITH_CONTAINER_SYSTEM: container_system integration
 */

#pragma once

// Include the implementation from detail
#include "kcenon/network/detail/config/feature_flags.h"
