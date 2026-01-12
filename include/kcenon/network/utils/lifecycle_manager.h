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

#include <atomic>
#include <future>
#include <optional>

namespace kcenon::network::utils
{

	/*!
	 * \class lifecycle_manager
	 * \brief Thread-safe lifecycle state management for network components.
	 *
	 * This utility class encapsulates the common lifecycle management logic
	 * that was previously duplicated across all CRTP base classes. It handles:
	 * - Running state tracking with atomic operations
	 * - Stop synchronization via promise/future
	 * - Thread-safe state transitions
	 *
	 * ### Thread Safety
	 * All methods are thread-safe and use atomic operations for state changes.
	 *
	 * ### Usage Example
	 * \code
	 * class my_client {
	 *     lifecycle_manager lifecycle_;
	 *
	 *     auto start() -> VoidResult {
	 *         if (!lifecycle_.try_start()) {
	 *             return make_error("Already running");
	 *         }
	 *         // ... start operations ...
	 *         return success();
	 *     }
	 *
	 *     auto stop() -> VoidResult {
	 *         if (!lifecycle_.prepare_stop()) {
	 *             return make_error("Not running");
	 *         }
	 *         // ... stop operations ...
	 *         lifecycle_.mark_stopped();
	 *         return success();
	 *     }
	 * };
	 * \endcode
	 */
	class lifecycle_manager
	{
	public:
		/*!
		 * \brief Default constructor.
		 */
		lifecycle_manager() = default;

		/*!
		 * \brief Destructor.
		 */
		~lifecycle_manager() = default;

		// Non-copyable
		lifecycle_manager(const lifecycle_manager&) = delete;
		lifecycle_manager& operator=(const lifecycle_manager&) = delete;

		// Movable
		lifecycle_manager(lifecycle_manager&& other) noexcept
			: is_running_(other.is_running_.load(std::memory_order_acquire))
			, stop_initiated_(other.stop_initiated_.load(std::memory_order_acquire))
			, stop_promise_(std::move(other.stop_promise_))
			, stop_future_(std::move(other.stop_future_))
		{
			other.is_running_.store(false, std::memory_order_release);
			other.stop_initiated_.store(false, std::memory_order_release);
		}

		lifecycle_manager& operator=(lifecycle_manager&& other) noexcept
		{
			if (this != &other)
			{
				is_running_.store(other.is_running_.load(std::memory_order_acquire),
				                  std::memory_order_release);
				stop_initiated_.store(other.stop_initiated_.load(std::memory_order_acquire),
				                      std::memory_order_release);
				stop_promise_ = std::move(other.stop_promise_);
				stop_future_ = std::move(other.stop_future_);

				other.is_running_.store(false, std::memory_order_release);
				other.stop_initiated_.store(false, std::memory_order_release);
			}
			return *this;
		}

		/*!
		 * \brief Checks if the component is currently running.
		 * \return true if running, false otherwise.
		 */
		[[nodiscard]] auto is_running() const -> bool
		{
			return is_running_.load(std::memory_order_acquire);
		}

		/*!
		 * \brief Attempts to transition from stopped to running state.
		 * \return true if transition succeeded (was not running), false if already running.
		 *
		 * This method uses compare-and-exchange to ensure only one caller
		 * can successfully start the component.
		 */
		[[nodiscard]] auto try_start() -> bool
		{
			bool expected = false;
			return is_running_.compare_exchange_strong(
			    expected, true, std::memory_order_acq_rel);
		}

		/*!
		 * \brief Marks the component as running.
		 *
		 * Use this when you need to set running state without the
		 * atomic check (e.g., after successful initialization).
		 */
		auto set_running() -> void
		{
			is_running_.store(true, std::memory_order_release);
		}

		/*!
		 * \brief Marks the component as stopped and signals waiters.
		 *
		 * This method:
		 * 1. Sets is_running_ to false
		 * 2. Sets the promise value (if exists) to unblock wait_for_stop()
		 * 3. Resets the stop_initiated flag
		 */
		auto mark_stopped() -> void
		{
			is_running_.store(false, std::memory_order_release);
			if (stop_promise_)
			{
				stop_promise_->set_value();
				stop_promise_.reset();
			}
			stop_initiated_.store(false, std::memory_order_release);
		}

		/*!
		 * \brief Blocks until the component has stopped.
		 *
		 * If prepare_stop() was called, this will block until mark_stopped()
		 * is called. Returns immediately if no stop is in progress.
		 */
		auto wait_for_stop() -> void
		{
			if (stop_future_.valid())
			{
				stop_future_.wait();
			}
		}

		/*!
		 * \brief Prepares for stop operation.
		 * \return true if stop can proceed (was running), false if not running or already stopping.
		 *
		 * This method:
		 * 1. Checks if a stop is already initiated
		 * 2. Creates the promise/future pair for synchronization
		 * 3. Returns whether the caller should proceed with stop logic
		 */
		[[nodiscard]] auto prepare_stop() -> bool
		{
			// Check if already stopping
			bool expected = false;
			if (!stop_initiated_.compare_exchange_strong(expected, true,
			                                             std::memory_order_acq_rel))
			{
				return false; // Already stopping
			}

			// Check if running
			if (!is_running_.load(std::memory_order_acquire))
			{
				stop_initiated_.store(false, std::memory_order_release);
				return false; // Not running
			}

			// Create synchronization primitives
			stop_promise_.emplace();
			stop_future_ = stop_promise_->get_future();
			return true;
		}

		/*!
		 * \brief Resets the lifecycle manager to initial state.
		 *
		 * Use this to prepare for reuse after a stop operation.
		 */
		auto reset() -> void
		{
			is_running_.store(false, std::memory_order_release);
			stop_initiated_.store(false, std::memory_order_release);
			stop_promise_.reset();
			stop_future_ = std::future<void>{};
		}

	private:
		std::atomic<bool> is_running_{false};
		std::atomic<bool> stop_initiated_{false};
		std::optional<std::promise<void>> stop_promise_;
		std::future<void> stop_future_;
	};

} // namespace kcenon::network::utils
