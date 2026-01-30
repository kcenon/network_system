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
#include <span>
#include <system_error>

namespace kcenon::network_core::interfaces
{

	/*!
	 * \interface socket_observer
	 * \brief Observer interface for low-level socket events.
	 *
	 * This interface provides a unified way to handle socket-level events
	 * through the Observer pattern, replacing individual callback setters
	 * (set_receive_callback, set_error_callback, set_backpressure_callback).
	 *
	 * ### Design Goals
	 * - **Consolidation**: Single observer replaces multiple callback setters
	 * - **Type safety**: Compile-time verification of observer interface
	 * - **Extensibility**: New events can be added with minimal impact
	 * - **Zero-copy**: Receive callbacks use span for efficiency
	 *
	 * ### Thread Safety
	 * - Observer methods may be invoked from I/O threads
	 * - Implementations must be thread-safe if shared across sockets
	 *
	 * ### Usage Example
	 * \code
	 * class my_socket_observer : public socket_observer {
	 * public:
	 *     void on_receive(std::span<const uint8_t> data) override {
	 *         // Handle received data (zero-copy)
	 *     }
	 *     void on_error(std::error_code ec) override {
	 *         // Handle socket error
	 *     }
	 *     void on_backpressure(bool apply) override {
	 *         // Handle backpressure signal
	 *     }
	 * };
	 *
	 * auto observer = std::make_shared<my_socket_observer>();
	 * socket->attach_observer(observer);
	 * \endcode
	 *
	 * \see null_socket_observer
	 * \see socket_callback_adapter
	 */
	class socket_observer
	{
	public:
		virtual ~socket_observer() = default;

		/*!
		 * \brief Called when data is received on the socket.
		 * \param data The received data as a non-owning view.
		 *
		 * ### Zero-Copy Semantics
		 * - The span is valid **only** until this method returns.
		 * - Do not store, capture, or use the span after returning.
		 * - If data must be retained, copy it into your own container.
		 *
		 * ### Thread Safety
		 * May be called from I/O threads. Implementation must be thread-safe.
		 */
		virtual auto on_receive(std::span<const uint8_t> data) -> void = 0;

		/*!
		 * \brief Called when a socket error occurs.
		 * \param ec The error code describing the error.
		 *
		 * ### Error Recovery
		 * - Errors typically indicate socket closure or I/O failure.
		 * - Further reads/writes may not succeed after an error.
		 */
		virtual auto on_error(std::error_code ec) -> void = 0;

		/*!
		 * \brief Called when backpressure state changes.
		 * \param apply_backpressure True when backpressure should be applied
		 *        (high water mark reached), false when it should be released
		 *        (low water mark reached).
		 *
		 * ### Backpressure Semantics
		 * - true: Pending send buffer exceeded high_water_mark.
		 *         Caller should stop or slow down sending data.
		 * - false: Pending send buffer dropped below low_water_mark.
		 *          Caller may resume normal send operations.
		 */
		virtual auto on_backpressure(bool apply_backpressure) -> void = 0;
	};

	/*!
	 * \class null_socket_observer
	 * \brief No-op implementation of socket_observer.
	 *
	 * This class provides a default implementation that does nothing,
	 * useful as a base class when only some events need to be handled.
	 *
	 * ### Usage
	 * \code
	 * // Derive when you only need specific events
	 * class my_partial_observer : public null_socket_observer {
	 * public:
	 *     void on_receive(std::span<const uint8_t> data) override {
	 *         // Only handle receive
	 *     }
	 *     // on_error and on_backpressure use no-op implementations
	 * };
	 * \endcode
	 */
	class null_socket_observer : public socket_observer
	{
	public:
		auto on_receive(std::span<const uint8_t> /*data*/) -> void override {}
		auto on_error(std::error_code /*ec*/) -> void override {}
		auto on_backpressure(bool /*apply_backpressure*/) -> void override {}
	};

	/*!
	 * \class socket_callback_adapter
	 * \brief Adapter to use function callbacks with socket_observer.
	 *
	 * This class enables gradual migration from callback-based API
	 * to observer pattern by wrapping std::function callbacks.
	 *
	 * ### Usage
	 * \code
	 * auto adapter = std::make_shared<socket_callback_adapter>();
	 * adapter->on_receive([](std::span<const uint8_t> data) {
	 *     // Handle data
	 * }).on_error([](std::error_code ec) {
	 *     // Handle error
	 * }).on_backpressure([](bool apply) {
	 *     // Handle backpressure
	 * });
	 *
	 * socket->attach_observer(adapter);
	 * \endcode
	 *
	 * ### Thread Safety
	 * Thread-safe for callback invocation. Setting callbacks should be
	 * done before attaching to socket.
	 */
	class socket_callback_adapter : public socket_observer
	{
	public:
		//! \brief Callback type for received data (zero-copy span)
		using receive_fn = std::function<void(std::span<const uint8_t>)>;
		//! \brief Callback type for errors
		using error_fn = std::function<void(std::error_code)>;
		//! \brief Callback type for backpressure
		using backpressure_fn = std::function<void(bool)>;

		/*!
		 * \brief Sets the callback for received data.
		 * \param fn The callback function.
		 * \return Reference to this adapter for chaining.
		 */
		auto on_receive(receive_fn fn) -> socket_callback_adapter&
		{
			receive_callback_ = std::move(fn);
			return *this;
		}

		/*!
		 * \brief Sets the callback for errors.
		 * \param fn The callback function.
		 * \return Reference to this adapter for chaining.
		 */
		auto on_error(error_fn fn) -> socket_callback_adapter&
		{
			error_callback_ = std::move(fn);
			return *this;
		}

		/*!
		 * \brief Sets the callback for backpressure.
		 * \param fn The callback function.
		 * \return Reference to this adapter for chaining.
		 */
		auto on_backpressure(backpressure_fn fn) -> socket_callback_adapter&
		{
			backpressure_callback_ = std::move(fn);
			return *this;
		}

		// socket_observer implementation
		auto on_receive(std::span<const uint8_t> data) -> void override
		{
			if (receive_callback_)
			{
				receive_callback_(data);
			}
		}

		auto on_error(std::error_code ec) -> void override
		{
			if (error_callback_)
			{
				error_callback_(ec);
			}
		}

		auto on_backpressure(bool apply_backpressure) -> void override
		{
			if (backpressure_callback_)
			{
				backpressure_callback_(apply_backpressure);
			}
		}

	private:
		receive_fn receive_callback_;
		error_fn error_callback_;
		backpressure_fn backpressure_callback_;
	};

} // namespace kcenon::network_core::interfaces
