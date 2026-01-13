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

#include "i_network_component.h"
#include "i_session.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <system_error>
#include <vector>

#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::interfaces
{

	/*!
	 * \interface i_quic_session
	 * \brief Interface for a QUIC session on the server side.
	 *
	 * This interface extends i_session with QUIC-specific operations
	 * such as multi-stream support.
	 */
	class i_quic_session : public i_session
	{
	public:
		/*!
		 * \brief Creates a new server-initiated bidirectional stream.
		 * \return Stream ID or error.
		 */
		[[nodiscard]] virtual auto create_stream() -> Result<uint64_t> = 0;

		/*!
		 * \brief Creates a new server-initiated unidirectional stream.
		 * \return Stream ID or error.
		 */
		[[nodiscard]] virtual auto create_unidirectional_stream() -> Result<uint64_t> = 0;

		/*!
		 * \brief Sends data on a specific stream.
		 * \param stream_id The target stream ID.
		 * \param data The data to send.
		 * \param fin True if this is the final data on the stream.
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] virtual auto send_on_stream(
			uint64_t stream_id,
			std::vector<uint8_t>&& data,
			bool fin = false) -> VoidResult = 0;

		/*!
		 * \brief Closes a stream.
		 * \param stream_id The stream to close.
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] virtual auto close_stream(uint64_t stream_id) -> VoidResult = 0;
	};

	/*!
	 * \interface i_quic_server
	 * \brief Interface for QUIC server components.
	 *
	 * This interface extends i_network_component with QUIC server-specific
	 * operations such as multi-stream support and session management.
	 *
	 * ### Key Features
	 * - Multiple concurrent streams per connection
	 * - Built-in TLS 1.3 integration
	 * - 0-RTT early data support
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks may be invoked from I/O threads
	 *
	 * \see i_server
	 */
	class i_quic_server : public i_network_component
	{
	public:
		//! \brief Callback type for new connections
		using connection_callback_t = std::function<void(std::shared_ptr<i_quic_session>)>;

		//! \brief Callback type for disconnections (session_id)
		using disconnection_callback_t = std::function<void(std::string_view)>;

		//! \brief Callback type for default stream data (session_id, data)
		using receive_callback_t = std::function<void(std::string_view, const std::vector<uint8_t>&)>;

		//! \brief Callback type for stream data (session_id, stream_id, data, is_fin)
		using stream_callback_t = std::function<void(
			std::string_view,
			uint64_t,
			const std::vector<uint8_t>&,
			bool)>;

		//! \brief Callback type for errors (session_id, error)
		using error_callback_t = std::function<void(std::string_view, std::error_code)>;

		/*!
		 * \brief Starts the QUIC server on the specified port.
		 * \param port The port number to listen on.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Binds to the specified port (UDP)
		 * - Begins accepting QUIC connections
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 */
		[[nodiscard]] virtual auto start(uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Stops the QUIC server.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Stops accepting new connections
		 * - Closes all active sessions
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Gets the number of active QUIC connections.
		 * \return The count of currently connected clients.
		 */
		[[nodiscard]] virtual auto connection_count() const -> size_t = 0;

		/*!
		 * \brief Sets the callback for new connections.
		 * \param callback The callback function.
		 */
		virtual auto set_connection_callback(connection_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for disconnections.
		 * \param callback The callback function.
		 */
		virtual auto set_disconnection_callback(disconnection_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for received data on default stream.
		 * \param callback The callback function.
		 */
		virtual auto set_receive_callback(receive_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for stream data.
		 * \param callback The callback function.
		 */
		virtual auto set_stream_callback(stream_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 */
		virtual auto set_error_callback(error_callback_t callback) -> void = 0;
	};

} // namespace kcenon::network::interfaces
