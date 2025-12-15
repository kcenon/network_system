// BSD 3-Clause License
//
// Copyright (c) 2021-2025, kcenon
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

/**
 * @file io_context_thread_manager.h
 * @brief Unified io_context thread management for network components
 *
 * Provides centralized management of asio::io_context execution on thread pools,
 * ensuring consistent thread lifecycle management across all network components.
 *
 * @author kcenon
 * @date 2025-01-06
 */

#include <asio.hpp>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "kcenon/network/integration/thread_integration.h"

namespace kcenon::network::integration {

/**
 * @class io_context_thread_manager
 * @brief Manages io_context execution on shared thread pools
 *
 * This class provides unified thread management for all asio::io_context
 * instances in the network system. Instead of each component managing its own
 * threads, this manager provides a centralized approach.
 *
 * ### Benefits
 * - Unified thread resource management
 * - Consistent shutdown behavior across all components
 * - Reduced total thread count
 * - Simplified component implementation
 *
 * ### Thread Safety
 * All public methods are thread-safe.
 *
 * ### Usage Example
 * @code
 * auto& manager = io_context_thread_manager::instance();
 *
 * // Run io_context on the shared thread pool
 * auto io_ctx = std::make_shared<asio::io_context>();
 * auto future = manager.run_io_context(io_ctx, "my_component");
 *
 * // ... use io_context for async operations ...
 *
 * // Stop when done
 * manager.stop_io_context(io_ctx);
 * future.wait();
 * @endcode
 */
class io_context_thread_manager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static io_context_thread_manager& instance();

    /**
     * @brief Run an io_context on the shared thread pool
     *
     * Submits the io_context::run() call as a task to the thread pool.
     * The io_context will run until stopped or an error occurs.
     *
     * @param io_context The io_context to run
     * @param component_name Optional name for logging/identification
     * @return Future that completes when io_context::run() returns
     *
     * @note The io_context must have work posted to it or use a work_guard,
     *       otherwise io_context::run() will return immediately.
     */
    std::future<void> run_io_context(
        std::shared_ptr<asio::io_context> io_context,
        const std::string& component_name = ""
    );

    /**
     * @brief Stop an io_context managed by this manager
     *
     * Calls io_context::stop() to terminate the run loop.
     * The future returned by run_io_context() will complete after this.
     *
     * @param io_context The io_context to stop
     */
    void stop_io_context(std::shared_ptr<asio::io_context> io_context);

    /**
     * @brief Stop all managed io_contexts
     *
     * Stops all io_contexts that were started via run_io_context().
     * Useful for application shutdown.
     */
    void stop_all();

    /**
     * @brief Perform graceful shutdown of the manager
     *
     * Convenience method that stops all io_contexts and waits for completion.
     * Equivalent to calling stop_all() followed by wait_all().
     *
     * @note Since this class uses Intentional Leak pattern, this method provides
     *       explicit cleanup for users who want to ensure graceful shutdown
     *       before process termination.
     */
    void shutdown();

    /**
     * @brief Wait for all managed io_contexts to complete
     *
     * Blocks until all io_context::run() calls have returned.
     * Should be called after stop_all() for clean shutdown.
     */
    void wait_all();

    /**
     * @brief Get the number of active io_contexts
     * @return Number of io_contexts currently running
     */
    size_t active_count() const;

    /**
     * @brief Check if an io_context is managed and running
     * @param io_context The io_context to check
     * @return true if the io_context is active, false otherwise
     */
    bool is_active(std::shared_ptr<asio::io_context> io_context) const;

    /**
     * @brief Set a custom thread pool
     *
     * By default, uses the thread pool from network_context.
     * This allows using a different thread pool if needed.
     *
     * @param pool The thread pool to use
     */
    void set_thread_pool(std::shared_ptr<thread_pool_interface> pool);

    /**
     * @brief Get current metrics
     */
    struct metrics {
        size_t active_contexts = 0;     ///< Number of running io_contexts
        size_t total_started = 0;       ///< Total io_contexts started
        size_t total_completed = 0;     ///< Total io_contexts completed
    };

    /**
     * @brief Get current metrics
     * @return Current metrics
     */
    metrics get_metrics() const;

private:
    io_context_thread_manager();
    ~io_context_thread_manager();

    // Non-copyable, non-movable
    io_context_thread_manager(const io_context_thread_manager&) = delete;
    io_context_thread_manager& operator=(const io_context_thread_manager&) = delete;
    io_context_thread_manager(io_context_thread_manager&&) = delete;
    io_context_thread_manager& operator=(io_context_thread_manager&&) = delete;

    class impl;
    std::unique_ptr<impl> pimpl_;
};

} // namespace kcenon::network::integration

// Backward compatibility - use the existing namespace alias from thread_integration.h
// The alias `namespace network_system::integration = kcenon::network::integration;` is
// already defined in thread_integration.h, which is included before this header.
