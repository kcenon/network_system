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
#include <cstdint>

namespace kcenon::network::utils
{

	/*!
	 * \enum connection_status
	 * \brief Represents the connection state of a network component.
	 */
	enum class connection_status : uint8_t
	{
		disconnected = 0,  //!< Not connected
		connecting = 1,    //!< Connection in progress
		connected = 2,     //!< Successfully connected
		disconnecting = 3  //!< Disconnection in progress
	};

	/*!
	 * \class connection_state
	 * \brief Thread-safe connection state management.
	 *
	 * This utility class encapsulates connection state tracking with
	 * proper atomic operations for thread safety. It provides state
	 * machine semantics for connection lifecycle.
	 *
	 * ### State Transitions
	 * \code
	 *     ┌──────────────┐
	 *     │ disconnected │◄────────────────────┐
	 *     └──────┬───────┘                     │
	 *            │ set_connecting()            │
	 *            ▼                             │
	 *     ┌──────────────┐                     │
	 *     │  connecting  │─────────────────────┤
	 *     └──────┬───────┘   (on failure)      │
	 *            │ set_connected()             │
	 *            ▼                             │
	 *     ┌──────────────┐                     │
	 *     │  connected   │                     │
	 *     └──────┬───────┘                     │
	 *            │ set_disconnecting()         │
	 *            ▼                             │
	 *     ┌──────────────┐                     │
	 *     │disconnecting │─────────────────────┘
	 *     └──────────────┘   set_disconnected()
	 * \endcode
	 *
	 * ### Thread Safety
	 * All methods use atomic operations and are safe for concurrent access.
	 *
	 * ### Usage Example
	 * \code
	 * connection_state state;
	 *
	 * if (state.set_connecting()) {
	 *     // Only one thread reaches here
	 *     if (connect_succeeded) {
	 *         state.set_connected();
	 *     } else {
	 *         state.set_disconnected();
	 *     }
	 * }
	 * \endcode
	 */
	class connection_state
	{
	public:
		/*!
		 * \brief Default constructor. Initializes to disconnected state.
		 */
		connection_state() = default;

		/*!
		 * \brief Destructor.
		 */
		~connection_state() = default;

		// Non-copyable
		connection_state(const connection_state&) = delete;
		connection_state& operator=(const connection_state&) = delete;

		// Movable
		connection_state(connection_state&& other) noexcept
			: status_(other.status_.load(std::memory_order_acquire))
		{
			other.status_.store(connection_status::disconnected,
			                    std::memory_order_release);
		}

		connection_state& operator=(connection_state&& other) noexcept
		{
			if (this != &other)
			{
				status_.store(other.status_.load(std::memory_order_acquire),
				              std::memory_order_release);
				other.status_.store(connection_status::disconnected,
				                    std::memory_order_release);
			}
			return *this;
		}

		/*!
		 * \brief Gets the current connection status.
		 * \return The current status.
		 */
		[[nodiscard]] auto status() const -> connection_status
		{
			return status_.load(std::memory_order_acquire);
		}

		/*!
		 * \brief Checks if currently connected.
		 * \return true if status is connected.
		 */
		[[nodiscard]] auto is_connected() const -> bool
		{
			return status_.load(std::memory_order_acquire) == connection_status::connected;
		}

		/*!
		 * \brief Checks if currently disconnected.
		 * \return true if status is disconnected.
		 */
		[[nodiscard]] auto is_disconnected() const -> bool
		{
			return status_.load(std::memory_order_acquire) == connection_status::disconnected;
		}

		/*!
		 * \brief Checks if connection is in progress.
		 * \return true if status is connecting.
		 */
		[[nodiscard]] auto is_connecting() const -> bool
		{
			return status_.load(std::memory_order_acquire) == connection_status::connecting;
		}

		/*!
		 * \brief Checks if disconnection is in progress.
		 * \return true if status is disconnecting.
		 */
		[[nodiscard]] auto is_disconnecting() const -> bool
		{
			return status_.load(std::memory_order_acquire) == connection_status::disconnecting;
		}

		/*!
		 * \brief Attempts to transition from disconnected to connecting.
		 * \return true if transition succeeded, false if not in disconnected state.
		 */
		[[nodiscard]] auto set_connecting() -> bool
		{
			auto expected = connection_status::disconnected;
			return status_.compare_exchange_strong(
			    expected, connection_status::connecting, std::memory_order_acq_rel);
		}

		/*!
		 * \brief Transitions to connected state.
		 *
		 * Should only be called after successful connection.
		 */
		auto set_connected() -> void
		{
			status_.store(connection_status::connected, std::memory_order_release);
		}

		/*!
		 * \brief Attempts to transition from connected to disconnecting.
		 * \return true if transition succeeded, false if not in connected state.
		 */
		[[nodiscard]] auto set_disconnecting() -> bool
		{
			auto expected = connection_status::connected;
			return status_.compare_exchange_strong(
			    expected, connection_status::disconnecting, std::memory_order_acq_rel);
		}

		/*!
		 * \brief Transitions to disconnected state.
		 *
		 * Can be called from any state to force disconnection.
		 */
		auto set_disconnected() -> void
		{
			status_.store(connection_status::disconnected, std::memory_order_release);
		}

		/*!
		 * \brief Resets to disconnected state.
		 *
		 * Alias for set_disconnected() for clarity.
		 */
		auto reset() -> void { set_disconnected(); }

	private:
		std::atomic<connection_status> status_{connection_status::disconnected};
	};

} // namespace kcenon::network::utils
