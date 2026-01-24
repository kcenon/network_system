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

#include <cstdint>
#include <string_view>
#include <vector>

#include "kcenon/network-core/utils/result_types.h"

namespace kcenon::network_core::interfaces
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

} // namespace kcenon::network_core::interfaces
