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

#include "kcenon/network/interfaces/i_network_component.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::interfaces
{

	/*!
	 * \interface i_quic_client
	 * \brief Interface for QUIC client components.
	 *
	 * This interface extends i_network_component with QUIC-specific
	 * operations such as multi-stream support and 0-RTT session resumption.
	 *
	 * ### Key Features
	 * - Multiple concurrent bidirectional streams
	 * - Unidirectional stream support
	 * - 0-RTT early data for reduced latency
	 * - Built-in TLS 1.3 integration
	 *
	 * ### Thread Safety
	 * - All public methods must be thread-safe
	 * - Callbacks may be invoked from I/O threads
	 *
	 * \see i_client
	 */
	class i_quic_client : public i_network_component
	{
	public:
		//! \brief Callback type for received data on default stream
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;

		//! \brief Callback type for stream data (stream_id, data, is_fin)
		using stream_callback_t = std::function<void(uint64_t, const std::vector<uint8_t>&, bool)>;

		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;

		//! \brief Callback type for disconnection
		using disconnected_callback_t = std::function<void()>;

		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		//! \brief Callback type for session ticket received (for 0-RTT resumption)
		using session_ticket_callback_t = std::function<void(
			std::vector<uint8_t> ticket_data,
			uint32_t lifetime_hint,
			uint32_t max_early_data)>;

		//! \brief Callback type for early data production
		using early_data_callback_t = std::function<std::vector<uint8_t>()>;

		//! \brief Callback type for early data acceptance notification
		using early_data_accepted_callback_t = std::function<void(bool accepted)>;

		/*!
		 * \brief Starts the QUIC client connecting to the specified server.
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Resolves the server address
		 * - Initiates QUIC handshake (includes TLS 1.3)
		 * - Creates default stream (stream 0)
		 *
		 * ### Thread Safety
		 * Thread-safe. Only one start operation can succeed at a time.
		 */
		[[nodiscard]] virtual auto start(std::string_view host, uint16_t port) -> VoidResult = 0;

		/*!
		 * \brief Stops the QUIC client.
		 * \return VoidResult indicating success or failure.
		 *
		 * ### Behavior
		 * - Closes all streams gracefully
		 * - Sends connection close frame
		 *
		 * ### Thread Safety
		 * Thread-safe.
		 */
		[[nodiscard]] virtual auto stop() -> VoidResult = 0;

		/*!
		 * \brief Checks if the client is connected.
		 * \return true if connected, false otherwise.
		 */
		[[nodiscard]] virtual auto is_connected() const -> bool = 0;

		/*!
		 * \brief Checks if TLS handshake is complete.
		 * \return true if handshake is done, false otherwise.
		 */
		[[nodiscard]] virtual auto is_handshake_complete() const -> bool = 0;

		// =====================================================================
		// Default Stream Operations
		// =====================================================================

		/*!
		 * \brief Sends data on the default stream (stream 0).
		 * \param data The data to send.
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] virtual auto send(std::vector<uint8_t>&& data) -> VoidResult = 0;

		// =====================================================================
		// Multi-Stream Operations (QUIC Specific)
		// =====================================================================

		/*!
		 * \brief Creates a new bidirectional stream.
		 * \return Stream ID or error.
		 */
		[[nodiscard]] virtual auto create_stream() -> Result<uint64_t> = 0;

		/*!
		 * \brief Creates a new unidirectional stream.
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

		// =====================================================================
		// ALPN Configuration
		// =====================================================================

		/*!
		 * \brief Sets the ALPN protocols for negotiation.
		 * \param protocols List of protocol identifiers (e.g., {"h3", "hq-29"}).
		 */
		virtual auto set_alpn_protocols(const std::vector<std::string>& protocols) -> void = 0;

		/*!
		 * \brief Gets the negotiated ALPN protocol.
		 * \return Protocol string if negotiated, empty optional otherwise.
		 */
		[[nodiscard]] virtual auto alpn_protocol() const -> std::optional<std::string> = 0;

		// =====================================================================
		// 0-RTT Support
		// =====================================================================

		/*!
		 * \brief Checks if early data was accepted by the server.
		 * \return true if accepted, false otherwise.
		 */
		[[nodiscard]] virtual auto is_early_data_accepted() const -> bool = 0;

		// =====================================================================
		// Callbacks
		// =====================================================================

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
		 * \brief Sets the callback for connection established.
		 * \param callback The callback function.
		 */
		virtual auto set_connected_callback(connected_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for disconnection.
		 * \param callback The callback function.
		 */
		virtual auto set_disconnected_callback(disconnected_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 */
		virtual auto set_error_callback(error_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for session tickets (for 0-RTT).
		 * \param callback The callback function.
		 */
		virtual auto set_session_ticket_callback(session_ticket_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for early data production.
		 * \param callback The callback function.
		 */
		virtual auto set_early_data_callback(early_data_callback_t callback) -> void = 0;

		/*!
		 * \brief Sets the callback for early data acceptance notification.
		 * \param callback The callback function.
		 */
		virtual auto set_early_data_accepted_callback(early_data_accepted_callback_t callback) -> void = 0;
	};

} // namespace kcenon::network::interfaces
