/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*****************************************************************************/

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
 * @note For backward compatibility, individual headers can still be included
 * directly, but using this unified header is recommended.
 *
 * @see feature_flags.h For compile-time feature detection
 * @see network_config.h For standalone configuration
 * @see network_system_config.h For integration configuration
 */

// Feature flags must come first (defines KCENON_WITH_* macros)
#include "feature_flags.h"

// Standalone configuration (creates internal resources)
#include "network_config.h"

// Integration configuration (accepts external dependencies)
#include "network_system_config.h"
