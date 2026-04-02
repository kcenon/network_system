// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

namespace kcenon::network::interfaces
{

	/*!
	 * \interface i_network_component
	 * \brief Base interface for all network components.
	 *
	 * This interface defines the common contract for both client and server
	 * network components, providing basic lifecycle query methods.
	 *
	 * ### Thread Safety
	 * All methods must be implemented as thread-safe.
	 *
	 * ### Design Note
	 * This interface is part of the composition-based architecture that
	 * replaces the CRTP base classes. It enables:
	 * - Easy mocking for unit tests
	 * - Dependency injection
	 * - Runtime polymorphism where needed
	 *
	 * \see i_client
	 * \see i_server
	 */
	class i_network_component
	{
	public:
		/*!
		 * \brief Virtual destructor for proper cleanup of derived classes.
		 */
		virtual ~i_network_component() = default;

		// Non-copyable, non-movable (interface class)
		i_network_component(const i_network_component&) = delete;
		i_network_component& operator=(const i_network_component&) = delete;
		i_network_component(i_network_component&&) = delete;
		i_network_component& operator=(i_network_component&&) = delete;

	protected:
		//! \brief Default constructor (only for derived classes)
		i_network_component() = default;

		/*!
		 * \brief Checks if the component is currently running.
		 * \return true if the component is active, false otherwise.
		 *
		 * This method must be thread-safe and use atomic operations
		 * for the running state.
		 */
		[[nodiscard]] virtual auto is_running() const -> bool = 0;

		/*!
		 * \brief Blocks until the component has stopped.
		 *
		 * This method should be used to synchronize shutdown operations.
		 * It returns immediately if the component is not running.
		 *
		 * ### Thread Safety
		 * Safe to call from any thread. Uses internal synchronization.
		 */
		virtual auto wait_for_stop() -> void = 0;
	};

} // namespace kcenon::network::interfaces
