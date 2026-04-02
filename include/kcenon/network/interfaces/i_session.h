// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

#include "kcenon/network/types/result.h"

namespace kcenon::network::interfaces
{

	/*!
	 * \interface i_session
	 * \brief Interface for a single client session on the server side.
	 *
	 * This interface represents a connection to a single client. It is
	 * provided to server callbacks and allows sending data to and
	 * managing individual client connections.
	 *
	 * ### Lifetime Management
	 * Sessions are typically managed via shared_ptr. The session remains
	 * valid as long as the connection is active or a reference is held.
	 *
	 * ### Thread Safety
	 * All methods must be thread-safe. Send operations are typically
	 * queued and executed asynchronously.
	 *
	 * \see i_server
	 */
	class i_session
	{
	public:
		/*!
		 * \brief Virtual destructor for proper cleanup.
		 */
		virtual ~i_session() = default;

		// Non-copyable, non-movable (typically shared via shared_ptr)
		i_session(const i_session&) = delete;
		i_session& operator=(const i_session&) = delete;
		i_session(i_session&&) = delete;
		i_session& operator=(i_session&&) = delete;

		/*!
		 * \brief Gets the unique identifier for this session.
		 * \return A string view of the session ID.
		 *
		 * The session ID is unique within the server and remains
		 * constant for the lifetime of the session.
		 */
		[[nodiscard]] virtual auto id() const -> std::string_view = 0;

		/*!
		 * \brief Checks if the session is currently connected.
		 * \return true if connected, false otherwise.
		 */
		[[nodiscard]] virtual auto is_connected() const -> bool = 0;

		/*!
		 * \brief Sends data to the client.
		 * \param data The data to send.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Error Conditions
		 * - Returns error if session is closed
		 * - Returns error if send operation fails
		 *
		 * ### Thread Safety
		 * Thread-safe. Multiple sends may be queued.
		 */
		[[nodiscard]] virtual auto send(std::vector<uint8_t>&& data) -> VoidResult = 0;

		/*!
		 * \brief Closes the session.
		 *
		 * After calling this method, the session is no longer usable.
		 * The disconnection callback will be triggered.
		 */
		virtual auto close() -> void = 0;

	protected:
		//! \brief Default constructor (only for derived classes)
		i_session() = default;
	};

} // namespace kcenon::network::interfaces
