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
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "kcenon/network/detail/utils/lifecycle_manager.h"
#include "kcenon/network/detail/utils/result_types.h"

namespace kcenon::network::utils
{

	/*!
	 * \class startable_base
	 * \brief CRTP base class providing unified start/stop lifecycle management.
	 *
	 * This template extracts the common start/stop lifecycle pattern from
	 * client and server implementations. It handles:
	 * - Running state checks and transitions
	 * - Atomic flag management for stop prevention
	 * - Connection state reset
	 * - Error handling with state rollback
	 *
	 * \tparam Derived The derived class using CRTP pattern.
	 *
	 * ### Required Derived Class Interface
	 * The derived class must implement:
	 * - `component_name()` - Returns component identifier for error messages
	 * - `do_start_impl(args...)` - Protocol-specific start implementation
	 * - `do_stop_impl()` - Protocol-specific stop implementation
	 * - `on_stopped()` - Called after successful stop (optional callbacks)
	 *
	 * ### Thread Safety
	 * All lifecycle methods are thread-safe using atomic operations.
	 *
	 * ### Usage Example
	 * \code
	 * class my_client : public startable_base<my_client> {
	 * public:
	 *     static constexpr std::string_view component_name() { return "MyClient"; }
	 *
	 *     auto start_client(std::string_view host, unsigned short port) -> VoidResult {
	 *         return do_start(host, port);
	 *     }
	 *
	 *     auto stop_client() -> VoidResult {
	 *         return do_stop();
	 *     }
	 *
	 * protected:
	 *     friend class startable_base<my_client>;
	 *
	 *     auto do_start_impl(std::string_view host, unsigned short port) -> VoidResult {
	 *         // TCP-specific implementation
	 *     }
	 *
	 *     auto do_stop_impl() -> VoidResult {
	 *         // TCP-specific cleanup
	 *     }
	 *
	 *     auto on_stopped() -> void {
	 *         // Invoke disconnected callback
	 *     }
	 * };
	 * \endcode
	 */
	template<typename Derived>
	class startable_base
	{
	public:
		/*!
		 * \brief Checks if the component is currently running.
		 * \return true if running, false otherwise.
		 */
		[[nodiscard]] auto is_running() const noexcept -> bool
		{
			return lifecycle_.is_running();
		}

		/*!
		 * \brief Blocks until stop is called.
		 */
		auto wait_for_stop() -> void
		{
			lifecycle_.wait_for_stop();
		}

	protected:
		/*!
		 * \brief Default constructor.
		 */
		startable_base() = default;

		/*!
		 * \brief Destructor.
		 */
		~startable_base() = default;

		// Non-copyable
		startable_base(const startable_base&) = delete;
		startable_base& operator=(const startable_base&) = delete;

		// Movable
		startable_base(startable_base&&) noexcept = default;
		startable_base& operator=(startable_base&&) noexcept = default;

		/*!
		 * \brief Unified start operation with lifecycle management.
		 * \tparam Args The argument types for the start operation.
		 * \param args The arguments to pass to do_start_impl.
		 * \return Result<void> - Success if started, or error with code:
		 *         - error_codes::common_errors::already_exists if already running
		 *         - Propagated error from do_start_impl on failure
		 *
		 * ### State Transitions
		 * 1. Check if already running -> return error
		 * 2. Set running state
		 * 3. Reset connection state flags
		 * 4. Call derived's do_start_impl
		 * 5. On failure: rollback to stopped state
		 */
		template<typename... Args>
		[[nodiscard]] auto do_start(Args&&... args) -> VoidResult
		{
			if (lifecycle_.is_running())
			{
				return error_void(
					error_codes::common_errors::already_exists,
					std::string(derived().component_name()) + " is already running",
					"startable_base::do_start",
					std::string(derived().component_name()));
			}

			lifecycle_.set_running();
			reset_connection_state();

			auto result = derived().do_start_impl(std::forward<Args>(args)...);

			if (result.is_err())
			{
				lifecycle_.mark_stopped();
			}

			return result;
		}

		/*!
		 * \brief Unified stop operation with lifecycle management.
		 * \return Result<void> - Success if stopped (or already stopped).
		 *
		 * ### State Transitions
		 * 1. If not running -> return ok (idempotent)
		 * 2. Prevent multiple concurrent stops via atomic flag
		 * 3. Call derived's do_stop_impl
		 * 4. Mark as stopped
		 * 5. Call derived's on_stopped for cleanup callbacks
		 */
		[[nodiscard]] auto do_stop() -> VoidResult
		{
			if (!lifecycle_.is_running())
			{
				return ok();
			}

			// Prevent multiple stop calls
			bool expected = false;
			if (!stop_initiated_.compare_exchange_strong(expected, true,
			                                              std::memory_order_acq_rel))
			{
				return ok();
			}

			auto result = derived().do_stop_impl();
			lifecycle_.mark_stopped();

			// Call derived class's on_stopped hook for callbacks
			derived().on_stopped();

			return result;
		}

		/*!
		 * \brief Checks if stop has been initiated.
		 * \return true if stop is in progress, false otherwise.
		 */
		[[nodiscard]] auto is_stop_initiated() const noexcept -> bool
		{
			return stop_initiated_.load(std::memory_order_acquire);
		}

		/*!
		 * \brief Gets the lifecycle manager for derived class access.
		 * \return Reference to the lifecycle manager.
		 */
		[[nodiscard]] auto get_lifecycle() -> lifecycle_manager&
		{
			return lifecycle_;
		}

		/*!
		 * \brief Gets the lifecycle manager (const version).
		 * \return Const reference to the lifecycle manager.
		 */
		[[nodiscard]] auto get_lifecycle() const -> const lifecycle_manager&
		{
			return lifecycle_;
		}

	private:
		/*!
		 * \brief Resets connection state flags for a fresh start.
		 */
		auto reset_connection_state() -> void
		{
			stop_initiated_.store(false, std::memory_order_release);
		}

		/*!
		 * \brief CRTP accessor for derived class.
		 * \return Reference to the derived class instance.
		 */
		[[nodiscard]] auto derived() -> Derived&
		{
			return static_cast<Derived&>(*this);
		}

		/*!
		 * \brief CRTP accessor for derived class (const version).
		 * \return Const reference to the derived class instance.
		 */
		[[nodiscard]] auto derived() const -> const Derived&
		{
			return static_cast<const Derived&>(*this);
		}

		lifecycle_manager lifecycle_;                   /*!< Lifecycle state manager. */
		std::atomic<bool> stop_initiated_{false};       /*!< Stop in progress flag. */
	};

} // namespace kcenon::network::utils
