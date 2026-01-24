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
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>

namespace kcenon::network_core::interfaces
{

	/*!
	 * \interface connection_observer
	 * \brief Observer interface for client connection events.
	 *
	 * This interface provides a unified way to handle all connection-related
	 * events through the Observer pattern, replacing the individual callback
	 * setters (set_receive_callback, set_connected_callback, etc.).
	 *
	 * ### Design Goals
	 * - **Single responsibility**: One observer handles all connection events
	 * - **Extensibility**: New events can be added without breaking changes
	 * - **Testability**: Easy to mock for unit testing
	 *
	 * ### Thread Safety
	 * - Observer methods may be invoked from I/O threads
	 * - Implementations must be thread-safe if shared across connections
	 *
	 * ### Usage Example
	 * \code
	 * class my_observer : public connection_observer {
	 * public:
	 *     void on_receive(std::span<const uint8_t> data) override {
	 *         // Handle received data
	 *     }
	 *     void on_connected() override {
	 *         // Handle connection established
	 *     }
	 *     void on_disconnected(std::optional<std::string_view> reason) override {
	 *         // Handle disconnection
	 *     }
	 *     void on_error(std::error_code ec) override {
	 *         // Handle error
	 *     }
	 * };
	 *
	 * auto observer = std::make_shared<my_observer>();
	 * client->set_observer(observer);
	 * \endcode
	 *
	 * \see i_client
	 * \see callback_adapter
	 * \see null_connection_observer
	 */
	class connection_observer
	{
	public:
		virtual ~connection_observer() = default;

		/*!
		 * \brief Called when data is received from the server.
		 * \param data The received data as a span of bytes.
		 *
		 * ### Thread Safety
		 * May be called from I/O threads. Implementation must be thread-safe.
		 */
		virtual auto on_receive(std::span<const uint8_t> data) -> void = 0;

		/*!
		 * \brief Called when the connection is established.
		 *
		 * This is called after a successful connection to the server.
		 */
		virtual auto on_connected() -> void = 0;

		/*!
		 * \brief Called when the connection is closed.
		 * \param reason Optional reason for the disconnection.
		 *
		 * ### Note
		 * The reason may be empty for normal disconnections.
		 */
		virtual auto on_disconnected(std::optional<std::string_view> reason = std::nullopt) -> void
			= 0;

		/*!
		 * \brief Called when an error occurs.
		 * \param ec The error code describing the error.
		 */
		virtual auto on_error(std::error_code ec) -> void = 0;
	};

	/*!
	 * \class null_connection_observer
	 * \brief No-op implementation of connection_observer.
	 *
	 * This class provides a default implementation that does nothing,
	 * useful as a placeholder or for connections that don't need all events.
	 *
	 * ### Usage
	 * \code
	 * // Use when you only need some events
	 * class my_partial_observer : public null_connection_observer {
	 * public:
	 *     void on_receive(std::span<const uint8_t> data) override {
	 *         // Only handle receive
	 *     }
	 * };
	 * \endcode
	 */
	class null_connection_observer : public connection_observer
	{
	public:
		auto on_receive(std::span<const uint8_t> /*data*/) -> void override {}
		auto on_connected() -> void override {}
		auto on_disconnected(std::optional<std::string_view> /*reason*/) -> void override {}
		auto on_error(std::error_code /*ec*/) -> void override {}
	};

	/*!
	 * \class callback_adapter
	 * \brief Adapter to use function callbacks with the observer pattern.
	 *
	 * This class enables gradual migration from the callback-based API
	 * to the observer pattern by wrapping std::function callbacks.
	 *
	 * ### Usage
	 * \code
	 * auto adapter = std::make_shared<callback_adapter>();
	 * adapter->on_receive([](std::span<const uint8_t> data) {
	 *     // Handle data
	 * }).on_connected([]() {
	 *     // Handle connection
	 * }).on_error([](std::error_code ec) {
	 *     // Handle error
	 * });
	 *
	 * client->set_observer(adapter);
	 * \endcode
	 *
	 * ### Thread Safety
	 * Thread-safe for callback invocation. Setting callbacks should be
	 * done before starting the client.
	 */
	class callback_adapter : public connection_observer
	{
	public:
		//! \brief Callback type for received data (using span)
		using receive_fn = std::function<void(std::span<const uint8_t>)>;
		//! \brief Callback type for connection established
		using connected_fn = std::function<void()>;
		//! \brief Callback type for disconnection
		using disconnected_fn = std::function<void(std::optional<std::string_view>)>;
		//! \brief Callback type for errors
		using error_fn = std::function<void(std::error_code)>;

		/*!
		 * \brief Sets the callback for received data.
		 * \param fn The callback function.
		 * \return Reference to this adapter for chaining.
		 */
		auto on_receive(receive_fn fn) -> callback_adapter&
		{
			receive_callback_ = std::move(fn);
			return *this;
		}

		/*!
		 * \brief Sets the callback for connection established.
		 * \param fn The callback function.
		 * \return Reference to this adapter for chaining.
		 */
		auto on_connected(connected_fn fn) -> callback_adapter&
		{
			connected_callback_ = std::move(fn);
			return *this;
		}

		/*!
		 * \brief Sets the callback for disconnection.
		 * \param fn The callback function.
		 * \return Reference to this adapter for chaining.
		 */
		auto on_disconnected(disconnected_fn fn) -> callback_adapter&
		{
			disconnected_callback_ = std::move(fn);
			return *this;
		}

		/*!
		 * \brief Sets the callback for errors.
		 * \param fn The callback function.
		 * \return Reference to this adapter for chaining.
		 */
		auto on_error(error_fn fn) -> callback_adapter&
		{
			error_callback_ = std::move(fn);
			return *this;
		}

		// connection_observer implementation
		auto on_receive(std::span<const uint8_t> data) -> void override
		{
			if (receive_callback_)
			{
				receive_callback_(data);
			}
		}

		auto on_connected() -> void override
		{
			if (connected_callback_)
			{
				connected_callback_();
			}
		}

		auto on_disconnected(std::optional<std::string_view> reason) -> void override
		{
			if (disconnected_callback_)
			{
				disconnected_callback_(reason);
			}
		}

		auto on_error(std::error_code ec) -> void override
		{
			if (error_callback_)
			{
				error_callback_(ec);
			}
		}

	private:
		receive_fn receive_callback_;
		connected_fn connected_callback_;
		disconnected_fn disconnected_callback_;
		error_fn error_callback_;
	};

} // namespace kcenon::network_core::interfaces
