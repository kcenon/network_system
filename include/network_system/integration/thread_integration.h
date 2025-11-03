#pragma once

/**
 * @file thread_integration.h
 * @brief Direct integration with thread_system
 *
 * This module provides direct access to thread_system's thread_pool.
 * All network components should use thread_pool_manager instead of
 * calling these functions directly.
 *
 * @author kcenon
 * @date 2025-09-20
 * @note DEPRECATED: Use thread_pool_manager for centralized pool management
 */

#include <kcenon/thread/core/thread_pool.h>
#include <memory>
#include <string>

namespace network_system::integration {

/**
 * @brief Type alias for thread pool
 *
 * Direct alias to thread_system's thread_pool.
 */
using thread_pool = kcenon::thread::thread_pool;

/**
 * @brief Create a new thread pool
 *
 * @param name Pool name for debugging
 * @param thread_count Number of worker threads (default: hardware concurrency)
 * @return Shared pointer to thread pool
 *
 * @deprecated Use thread_pool_manager::create_io_pool() or get_utility_pool()
 * @note This function exists for backward compatibility only
 */
[[deprecated("Use thread_pool_manager instead")]]
std::shared_ptr<kcenon::thread::thread_pool> create_thread_pool(
    const std::string& name = "network_pool",
    size_t thread_count = 0);

} // namespace network_system::integration