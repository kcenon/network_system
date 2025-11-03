/**
 * @file thread_integration.cpp
 * @brief Simplified thread system integration
 *
 * This file provides minimal backward compatibility for legacy code.
 * New code should use thread_pool_manager directly.
 *
 * @author kcenon
 * @date 2025-09-20
 */

#include "network_system/integration/thread_integration.h"
#include "network_system/integration/logger_integration.h"
#include <thread>
#include <stdexcept>

namespace network_system::integration {

std::shared_ptr<kcenon::thread::thread_pool> create_thread_pool(
    const std::string& name,
    size_t thread_count) {

    NETWORK_LOG_WARN("[thread_integration] create_thread_pool() is deprecated. "
                    "Use thread_pool_manager instead.");

    // Determine thread count
    if (thread_count == 0) {
        thread_count = std::thread::hardware_concurrency();
        if (thread_count == 0) {
            thread_count = 2; // Fallback
        }
    }

    // Create thread pool with default context
    auto pool = std::make_shared<kcenon::thread::thread_pool>(name);

    // Create and enqueue workers
    for (size_t i = 0; i < thread_count; ++i) {
        auto worker = std::make_unique<kcenon::thread::thread_worker>();
        pool->enqueue(std::move(worker));
    }

    // Start the pool
    auto result = pool->start();
    if (result.has_error()) {
        NETWORK_LOG_ERROR("[thread_integration] Failed to start pool: " +
                        result.get_error().message());
        throw std::runtime_error("Failed to start thread pool: " +
                                result.get_error().message());
    }

    NETWORK_LOG_INFO("[thread_integration] Created thread pool '" + name +
                    "' with " + std::to_string(thread_count) + " threads");

    return pool;
}

} // namespace network_system::integration
