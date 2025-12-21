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

#include "kcenon/network/network_system.h"
#include "kcenon/network/core/network_context.h"
#include "kcenon/network/integration/common_system_adapter.h"
#include "kcenon/network/integration/logger_integration.h"
#include "kcenon/network/integration/thread_integration.h"

#ifdef BUILD_WITH_MONITORING_SYSTEM
#include "kcenon/network/integration/monitoring_integration.h"
#endif

#include <atomic>

namespace kcenon::network {

using error_codes::common_errors::already_exists;
using error_codes::common_errors::internal_error;
using error_codes::common_errors::not_initialized;

static std::atomic<bool> g_initialized{false};

VoidResult initialize() { return initialize(config::network_config::production()); }

VoidResult initialize(const config::network_config &config) {
  if (g_initialized.load()) {
    NETWORK_LOG_WARN("[network_system] Already initialized");
    return error_void(already_exists, "Network system already initialized", "network_system");
  }

  try {
    auto &ctx = core::network_context::instance();

    // Initialize thread pool
    if (config.thread_pool.worker_count >= 0) {
      auto thread_pool = std::make_shared<integration::basic_thread_pool>(
          config.thread_pool.worker_count == 0
              ? std::thread::hardware_concurrency()
              : config.thread_pool.worker_count);
      ctx.set_thread_pool(thread_pool);
      ctx.initialize(config.thread_pool.worker_count);
    }

    // Initialize logger
    // Issue #285: Uses common_system_logger_adapter when available, basic_logger otherwise
#ifdef BUILD_WITH_COMMON_SYSTEM
    auto logger = std::make_shared<integration::common_system_logger_adapter>();
#else
    auto logger = std::make_shared<integration::basic_logger>(config.logger.min_level);
#endif
    ctx.set_logger(logger);
    integration::logger_integration_manager::instance().set_logger(logger);

    // Initialize monitoring
#ifdef BUILD_WITH_MONITORING_SYSTEM
    if (config.monitoring.enabled) {
      auto monitoring =
          std::make_shared<integration::monitoring_system_adapter>(
              config.monitoring.service_name);
      // Note: monitoring_system_adapter needs start() method
      // monitoring->start();
      ctx.set_monitoring(monitoring);
    }
#else
    (void)config.monitoring; // Suppress unused parameter warning
#endif

    g_initialized.store(true);
    NETWORK_LOG_INFO("[network_system] Initialized successfully");

    return ok();
  } catch (const std::exception &e) {
    NETWORK_LOG_ERROR("[network_system] Initialization failed: " +
                      std::string(e.what()));
    return error_void(internal_error, std::string("Initialization failed: ") + e.what(), "network_system");
  }
}

VoidResult initialize(const config::network_system_config &config_with_dependencies) {
#ifdef BUILD_WITH_COMMON_SYSTEM
  auto &ctx = core::network_context::instance();

  if (config_with_dependencies.executor) {
    auto pool_adapter =
        std::make_shared<integration::common_thread_pool_adapter>(
            config_with_dependencies.executor);
    ctx.set_thread_pool(pool_adapter);
  }

  if (config_with_dependencies.logger) {
    auto logger_adapter = std::make_shared<integration::common_logger_adapter>(
        config_with_dependencies.logger);
    ctx.set_logger(logger_adapter);
  }

#ifdef BUILD_WITH_MONITORING_SYSTEM
  if (config_with_dependencies.monitor) {
    auto monitoring_adapter =
        std::make_shared<integration::common_monitoring_adapter>(
            config_with_dependencies.monitor);
    ctx.set_monitoring(monitoring_adapter);
  }
#endif
#else
  (void)config_with_dependencies;
#endif

  return initialize(config_with_dependencies.runtime);
}

VoidResult shutdown() {
  if (!g_initialized.load()) {
    return error_void(not_initialized, "Network system not initialized", "network_system");
  }

  try {
    NETWORK_LOG_INFO("[network_system] Shutting down...");

    auto &ctx = core::network_context::instance();
    ctx.shutdown();

    g_initialized.store(false);
    NETWORK_LOG_INFO("[network_system] Shutdown complete");

    return ok();
  } catch (const std::exception &e) {
    NETWORK_LOG_ERROR("[network_system] Shutdown error: " +
                      std::string(e.what()));
    return error_void(internal_error, std::string("Shutdown failed: ") + e.what(), "network_system");
  }
}

bool is_initialized() { return g_initialized.load(); }

} // namespace kcenon::network
