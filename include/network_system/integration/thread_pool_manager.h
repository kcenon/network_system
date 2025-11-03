/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
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

#include <kcenon/thread/core/thread_pool.h>
#include <kcenon/thread/core/typed_thread_pool.h>
#include <memory>
#include <string>
#include <thread>

namespace network_system::integration
{

	/**
	 * @brief Priority levels for network data pipeline
	 *
	 * Used by typed_thread_pool to prioritize network data processing tasks.
	 * Lower numeric values indicate higher priority.
	 */
	enum class pipeline_priority : int
	{
		REALTIME = 0,   ///< Real-time encryption, urgent transmission
		HIGH = 1,       ///< Important data processing
		NORMAL = 2,     ///< General compression, serialization
		LOW = 3,        ///< Background validation
		BACKGROUND = 4  ///< Logging, statistics
	};

	/**
	 * @brief Centralized thread pool manager for network_system
	 *
	 * Provides singleton access to thread pools used throughout the system:
	 * - I/O thread pools (size=1) for io_context execution
	 * - Data pipeline pool (typed) for priority-based processing
	 * - Utility pool for general-purpose async tasks
	 *
	 * Thread Safety:
	 * - All public methods are thread-safe
	 * - Singleton instance is thread-safe
	 *
	 * Usage Example:
	 * @code
	 * auto& mgr = thread_pool_manager::instance();
	 * mgr.initialize(10, 4, 2);
	 *
	 * // Create I/O pool for server component
	 * auto io_pool = mgr.create_io_pool("messaging_server:srv1");
	 *
	 * // Get shared pools
	 * auto pipeline = mgr.get_pipeline_pool();
	 * auto utility = mgr.get_utility_pool();
	 * @endcode
	 */
	class thread_pool_manager
	{
	public:
		/**
		 * @brief Get singleton instance
		 * @return Reference to the singleton instance
		 */
		static auto instance() -> thread_pool_manager&;

		/**
		 * @brief Initialize all thread pools
		 *
		 * Must be called before using any pools. Safe to call multiple times
		 * (subsequent calls are no-op).
		 *
		 * @param io_pool_count Reserved capacity for I/O pools (default: 10)
		 * @param pipeline_workers Number of pipeline workers (default: hardware cores)
		 * @param utility_workers Number of utility workers (default: half cores)
		 * @return true if initialized successfully, false if already initialized
		 * @throws std::runtime_error if thread pool creation fails
		 */
		auto initialize(
			size_t io_pool_count = 10,
			size_t pipeline_workers = std::thread::hardware_concurrency(),
			size_t utility_workers = std::thread::hardware_concurrency() / 2) -> bool;

		/**
		 * @brief Check if manager is initialized
		 * @return true if initialized
		 */
		[[nodiscard]] auto is_initialized() const -> bool;

		/**
		 * @brief Shutdown all thread pools
		 *
		 * Stops all pools and waits for completion. After shutdown, pools
		 * cannot be used unless initialize() is called again.
		 */
		void shutdown();

		/**
		 * @brief Create a dedicated I/O thread pool for a component
		 *
		 * Creates a new thread pool with exactly 1 worker for running
		 * io_context::run(). Each component should get its own pool.
		 *
		 * @param component_name Unique name for logging (e.g., "messaging_server:srv1")
		 * @return Shared pointer to thread pool (size=1)
		 * @throws std::runtime_error if not initialized or creation fails
		 */
		auto create_io_pool(const std::string& component_name)
			-> std::shared_ptr<kcenon::thread::thread_pool>;

		/**
		 * @brief Get the shared data pipeline thread pool
		 *
		 * Returns the thread pool for data processing.
		 * This pool is shared across all components for CPU-intensive tasks.
		 *
		 * NOTE: Phase 3 fallback - returns regular thread_pool instead of typed_thread_pool.
		 * Priority information is tracked via logging. Clean API for future upgrade.
		 *
		 * @return Shared pointer to thread pool
		 * @throws std::runtime_error if not initialized
		 */
		auto get_pipeline_pool() -> std::shared_ptr<kcenon::thread::thread_pool>;

		/**
		 * @brief Get the general-purpose utility thread pool
		 *
		 * Returns the shared utility pool for blocking I/O and background tasks.
		 *
		 * @return Shared pointer to thread pool
		 * @throws std::runtime_error if not initialized
		 */
		auto get_utility_pool() -> std::shared_ptr<kcenon::thread::thread_pool>;

		/**
		 * @brief Statistics for monitoring
		 */
		struct statistics
		{
			size_t total_io_pools = 0;      ///< Number of I/O pools created
			size_t active_io_tasks = 0;     ///< Active tasks in I/O pools
			size_t pipeline_queue_size = 0; ///< Pending pipeline jobs
			size_t pipeline_workers = 0;    ///< Pipeline worker count
			size_t utility_queue_size = 0;  ///< Pending utility jobs
			size_t utility_workers = 0;     ///< Utility worker count
			bool is_initialized = false;    ///< Initialization status
		};

		/**
		 * @brief Get current statistics
		 * @return Current pool statistics
		 */
		[[nodiscard]] auto get_statistics() const -> statistics;

		// Delete copy and move
		thread_pool_manager(const thread_pool_manager&) = delete;
		auto operator=(const thread_pool_manager&) -> thread_pool_manager& = delete;
		thread_pool_manager(thread_pool_manager&&) = delete;
		auto operator=(thread_pool_manager&&) -> thread_pool_manager& = delete;

	private:
		thread_pool_manager() = default;
		~thread_pool_manager();

		class impl;
		std::unique_ptr<impl> pimpl_;
	};

} // namespace network_system::integration
