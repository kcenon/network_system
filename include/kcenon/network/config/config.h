// BSD 3-Clause License
// Copyright (c) 2024-2025, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/**
 * @file config.h
 * @brief Unified configuration header for network_system
 *
 * This is the main entry point for network_system configuration.
 * It includes all configuration-related headers:
 * - Feature flags for compile-time feature detection
 * - Standalone configuration for internal resource management
 * - Integration configuration for external dependency injection
 *
 * Usage:
 * @code
 * #include <kcenon/network/config/config.h>
 *
 * using namespace kcenon::network::config;
 *
 * // Standalone initialization
 * auto result = kcenon::network::initialize(network_config::production());
 *
 * // Integration with existing infrastructure
 * network_system_config cfg;
 * cfg.executor = my_shared_executor;
 * cfg.logger = my_shared_logger;
 * auto result = kcenon::network::initialize(cfg);
 * @endcode
 *
 * @note Individual headers in detail/config/ are implementation details.
 * Please use this unified header for configuration needs.
 *
 * @see detail/config/feature_flags.h For compile-time feature detection
 * @see detail/config/network_config.h For standalone configuration
 * @see detail/config/network_system_config.h For integration configuration
 */

// Feature flags must come first (defines KCENON_WITH_* macros)
#include "kcenon/network/config/feature_flags.h"

// Standalone configuration (creates internal resources)
#include "kcenon/network/detail/config/network_config.h"

// Integration configuration (accepts external dependencies)
#include "kcenon/network/detail/config/network_system_config.h"
