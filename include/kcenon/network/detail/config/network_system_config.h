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

/**
 * @file network_system_config.h
 * @brief Configuration for network_system with external dependencies
 *
 * This header provides configuration structures for initializing the network
 * system with externally provided components. Use this configuration when
 * integrating the network system into an existing application infrastructure
 * where you want to share thread pools, loggers, and monitoring systems
 * across multiple components.
 *
 * For standalone usage where the network system should manage its own
 * resources internally, see network_config.h instead.
 *
 * @see network_config.h For standalone configuration with internal resources
 * @see network_system.h For initialization functions
 *
 * @author kcenon
 * @date 2025-01-13
 */

#pragma once

#include <memory>

#include "kcenon/network/detail/config/network_config.h"

namespace kcenon::common::interfaces {
class IExecutor;
class ILogger;
class IMonitor;
} // namespace kcenon::common::interfaces

namespace kcenon::network::config {

/**
 * @struct network_system_config
 * @brief Configuration for network_system with external dependencies
 *
 * Use this configuration when integrating the network system with existing
 * application infrastructure. This allows sharing thread pools, loggers,
 * and monitoring components with other parts of your application.
 *
 * Example usage:
 * @code
 * // Create shared infrastructure
 * auto app_executor = std::make_shared<MyExecutor>();
 * auto app_logger = std::make_shared<MyLogger>();
 *
 * // Configure network system to use shared resources
 * network_system_config cfg;
 * cfg.executor = app_executor;
 * cfg.logger = app_logger;
 * cfg.runtime = network_config::production();
 *
 * auto result = kcenon::network::initialize(cfg);
 * @endcode
 *
 * @note Components not provided (nullptr) will be created internally
 *       based on the runtime configuration settings.
 *
 * @see network_config For standalone usage with internal resources
 * @see initialize(const network_system_config&) For initialization
 */
struct network_system_config {
    /// Runtime configuration settings (thread pool, logger, monitoring config)
    network_config runtime{network_config::production()};

    /// External executor/thread pool (nullptr = create internal)
    std::shared_ptr<kcenon::common::interfaces::IExecutor> executor;

    /// External logger instance (nullptr = create internal)
    std::shared_ptr<kcenon::common::interfaces::ILogger> logger;

    /// External monitoring instance (nullptr = create internal if enabled)
    std::shared_ptr<kcenon::common::interfaces::IMonitor> monitor;
};

} // namespace kcenon::network::config
