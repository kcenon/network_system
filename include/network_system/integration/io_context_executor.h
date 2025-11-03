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
#include <asio.hpp>
#include <memory>
#include <string>

namespace network_system::integration
{

	/**
	 * @brief RAII wrapper for executing io_context::run() in thread pool
	 *
	 * Automatically manages the lifecycle of io_context execution:
	 * - Submits io_context::run() to thread pool on start()
	 * - Stops io_context and waits for completion on stop()
	 * - Ensures clean shutdown in destructor
	 *
	 * Thread Safety:
	 * - All operations are thread-safe
	 * - Multiple start/stop calls are safe (subsequent calls are no-op)
	 *
	 * Usage Example:
	 * @code
	 * asio::io_context io_ctx;
	 * auto& mgr = thread_pool_manager::instance();
	 * auto pool = mgr.create_io_pool("my_server");
	 *
	 * io_context_executor executor(pool, io_ctx, "my_server");
	 * executor.start();
	 *
	 * // io_context now runs in thread pool
	 * // ...
	 *
	 * executor.stop();  // or automatic in destructor
	 * @endcode
	 */
	class io_context_executor
	{
	public:
		/**
		 * @brief Constructor
		 *
		 * @param pool Thread pool (should have size=1 for I/O)
		 * @param io_context ASIO io_context to execute
		 * @param component_name Name for logging and identification
		 */
		io_context_executor(std::shared_ptr<kcenon::thread::thread_pool> pool,
							asio::io_context& io_context,
							const std::string& component_name);

		/**
		 * @brief Destructor
		 *
		 * Automatically calls stop() if still running
		 */
		~io_context_executor();

		// Delete copy, allow move
		io_context_executor(const io_context_executor&) = delete;
		auto operator=(const io_context_executor&) -> io_context_executor& = delete;
		io_context_executor(io_context_executor&&) noexcept = default;
		auto operator=(io_context_executor&&) noexcept -> io_context_executor& = default;

		/**
		 * @brief Start executing io_context::run() in thread pool
		 *
		 * Submits the io_context execution task to the thread pool.
		 * Safe to call multiple times (subsequent calls are no-op).
		 *
		 * @throws std::runtime_error if pool is not running
		 */
		void start();

		/**
		 * @brief Stop io_context execution and wait for completion
		 *
		 * Stops the io_context and waits for the execution thread to complete.
		 * Safe to call multiple times (subsequent calls are no-op).
		 */
		void stop();

		/**
		 * @brief Check if currently running
		 * @return true if io_context is being executed
		 */
		[[nodiscard]] auto is_running() const -> bool;

		/**
		 * @brief Get the underlying thread pool
		 * @return Shared pointer to thread pool
		 */
		[[nodiscard]] auto get_pool() const -> std::shared_ptr<kcenon::thread::thread_pool>;

		/**
		 * @brief Get the component name
		 * @return Component name string
		 */
		[[nodiscard]] auto get_component_name() const -> std::string;

	private:
		class impl;
		std::unique_ptr<impl> pimpl_;
	};

} // namespace network_system::integration
