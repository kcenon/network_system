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
 * @file network_context.h
 * @brief Global context for shared network system resources
 *
 * @author kcenon
 * @date 2025-01-13
 */

#pragma once

#include <memory>
#include "kcenon/network/integration/thread_integration.h"
#include "internal/integration/logger_integration.h"
#include "internal/integration/monitoring_integration.h"

namespace kcenon::network::core {

/**
 * @class network_context
 * @brief Global context for shared network system resources
 *
 * This class manages shared resources like thread pools, loggers, and monitoring
 * across all network system components. It follows the singleton pattern.
 */
class network_context {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static network_context& instance();

    /**
     * @brief Set custom thread pool
     * @param pool Thread pool implementation
     */
    void set_thread_pool(std::shared_ptr<kcenon::network::integration::thread_pool_interface> pool);

    /**
     * @brief Get current thread pool
     * @return Shared pointer to thread pool interface
     */
    std::shared_ptr<kcenon::network::integration::thread_pool_interface> get_thread_pool();

    /**
     * @brief Set custom logger
     * @param logger Logger implementation
     */
    void set_logger(std::shared_ptr<kcenon::network::integration::logger_interface> logger);

    /**
     * @brief Get current logger
     * @return Shared pointer to logger interface
     */
    std::shared_ptr<kcenon::network::integration::logger_interface> get_logger();

    /**
     * @brief Set custom monitoring system
     * @param monitoring Monitoring implementation
     *
     * Note: Since issue #342, metrics are also published via EventBus.
     * External consumers can subscribe to network_metric_event instead
     * of using this interface directly.
     */
    void set_monitoring(std::shared_ptr<kcenon::network::integration::monitoring_interface> monitoring);

    /**
     * @brief Get current monitoring system
     * @return Shared pointer to monitoring interface
     *
     * Note: Since issue #342, metrics are also published via EventBus.
     * External consumers can subscribe to network_metric_event instead
     * of using this interface directly.
     */
    std::shared_ptr<kcenon::network::integration::monitoring_interface> get_monitoring();

    /**
     * @brief Initialize all systems
     * @param thread_count Number of worker threads (0 = auto-detect)
     */
    void initialize(size_t thread_count = 0);

    /**
     * @brief Shutdown all systems
     */
    void shutdown();

    /**
     * @brief Check if context is initialized
     * @return true if initialized, false otherwise
     */
    bool is_initialized() const;

private:
    network_context();
    ~network_context();

    // Non-copyable, non-movable
    network_context(const network_context&) = delete;
    network_context& operator=(const network_context&) = delete;
    network_context(network_context&&) = delete;
    network_context& operator=(network_context&&) = delete;

    class impl;

    /**
     * @brief PIMPL pointer with intentional leak pattern
     *
     * Uses shared_ptr with no-op deleter to prevent heap corruption during
     * static destruction. This is intentional - see docs/DESIGN_DECISIONS.md
     * for detailed rationale.
     *
     * @note Memory is intentionally not freed. This is safe because:
     *       - Only ~100-200 bytes per instance
     *       - Process terminates immediately after static destruction
     *       - OS reclaims all process memory on exit
     *
     * @warning Do not "fix" this by changing to unique_ptr or adding a real
     *          deleter. Doing so will cause heap corruption on shutdown.
     *
     * @see docs/DESIGN_DECISIONS.md#intentional-leak-pattern
     */
    std::shared_ptr<impl> pimpl_;
};

} // namespace kcenon::network::core
