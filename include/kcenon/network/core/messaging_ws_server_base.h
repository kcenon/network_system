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
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/internal/websocket_protocol.h"

namespace kcenon::network::core
{
	// Forward declaration
	class ws_connection;

	/*!
	 * \class messaging_ws_server_base
	 * \brief CRTP base class for WebSocket servers that provides common lifecycle
	 *        management and callback handling.
	 *
	 * \tparam Derived The derived class type (CRTP pattern)
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Atomic flag (is_running_) prevents race conditions.
	 * - Callback access is protected by callback_mutex_.
	 *
	 * ### CRTP Pattern
	 * Derived classes must implement the following protected methods:
	 * - `auto do_start(uint16_t port, std::string_view path) -> VoidResult`
	 * - `auto do_stop() -> VoidResult`
	 *
	 * ### Usage Example
	 * \code
	 * class my_ws_server : public messaging_ws_server_base<my_ws_server> {
	 * protected:
	 *     friend class messaging_ws_server_base<my_ws_server>;
	 *     auto do_start(uint16_t port, std::string_view path) -> VoidResult;
	 *     auto do_stop() -> VoidResult;
	 * };
	 * \endcode
	 */
	template<typename Derived>
	class messaging_ws_server_base
		: public std::enable_shared_from_this<Derived>
	{
	public:
		//! \brief Callback type for new connections
		using connection_callback_t = std::function<void(std::shared_ptr<ws_connection>)>;
		//! \brief Callback type for disconnections
		using disconnection_callback_t = std::function<void(const std::string&, internal::ws_close_code, const std::string&)>;
		//! \brief Callback type for WebSocket messages
		using message_callback_t = std::function<void(std::shared_ptr<ws_connection>, const internal::ws_message&)>;
		//! \brief Callback type for text messages
		using text_message_callback_t = std::function<void(std::shared_ptr<ws_connection>, const std::string&)>;
		//! \brief Callback type for binary messages
		using binary_message_callback_t = std::function<void(std::shared_ptr<ws_connection>, const std::vector<uint8_t>&)>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(const std::string&, std::error_code)>;

		/*!
		 * \brief Constructs a WebSocket server with a given identifier.
		 * \param server_id A string identifier for this server instance.
		 */
		explicit messaging_ws_server_base(std::string_view server_id)
			: server_id_(server_id)
		{
		}

		/*!
		 * \brief Virtual destructor; calls stop_server() if still running.
		 */
		virtual ~messaging_ws_server_base() noexcept
		{
			if (is_running_.load())
			{
				stop_server();
			}
		}

		// Non-copyable, non-movable
		messaging_ws_server_base(const messaging_ws_server_base&) = delete;
		messaging_ws_server_base& operator=(const messaging_ws_server_base&) = delete;
		messaging_ws_server_base(messaging_ws_server_base&&) = delete;
		messaging_ws_server_base& operator=(messaging_ws_server_base&&) = delete;

		/*!
		 * \brief Starts the server on the specified port.
		 * \param port The port number to listen on.
		 * \param path The WebSocket path (default: "/").
		 * \return Result<void> - Success if server started, or error with code.
		 */
		auto start_server(uint16_t port, std::string_view path = "/") -> VoidResult
		{
			if (is_running_.load())
			{
				return error_void(
					error_codes::common_errors::already_exists,
					"WebSocket server is already running",
					"messaging_ws_server_base");
			}

			is_running_.store(true);
			stop_initiated_.store(false);

			// Initialize stop promise/future for wait_for_stop()
			stop_promise_.emplace();
			stop_future_ = stop_promise_->get_future();

			// Call derived class implementation
			auto result = derived().do_start(port, path);
			if (result.is_err())
			{
				is_running_.store(false);
				if (stop_promise_.has_value())
				{
					stop_promise_->set_value();
					stop_promise_.reset();
				}
			}

			return result;
		}

		/*!
		 * \brief Stops the server and releases all resources.
		 * \return Result<void> - Success if server stopped, or error with code.
		 */
		auto stop_server() -> VoidResult
		{
			if (!is_running_.load())
			{
				return ok();
			}

			// Prevent multiple stop calls
			bool expected = false;
			if (!stop_initiated_.compare_exchange_strong(expected, true))
			{
				return ok();
			}

			is_running_.store(false);

			// Call derived class implementation
			auto result = derived().do_stop();

			// Signal stop completion
			if (stop_promise_.has_value())
			{
				stop_promise_->set_value();
				stop_promise_.reset();
			}

			return result;
		}

		/*!
		 * \brief Blocks until stop_server() is called.
		 */
		auto wait_for_stop() -> void
		{
			if (stop_future_.valid())
			{
				stop_future_.wait();
			}
		}

		/*!
		 * \brief Check if the server is currently running.
		 * \return true if running, false otherwise.
		 */
		[[nodiscard]] auto is_running() const noexcept -> bool
		{
			return is_running_.load(std::memory_order_relaxed);
		}

		/*!
		 * \brief Returns the server identifier.
		 * \return The server_id string.
		 */
		[[nodiscard]] auto server_id() const -> const std::string&
		{
			return server_id_;
		}

		/*!
		 * \brief Sets the callback for new connections.
		 * \param callback Function called when a client connects.
		 */
		auto set_connection_callback(connection_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			connection_callback_ = std::move(callback);
		}

		/*!
		 * \brief Sets the callback for disconnections.
		 * \param callback Function called when a client disconnects.
		 */
		auto set_disconnection_callback(disconnection_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			disconnection_callback_ = std::move(callback);
		}

		/*!
		 * \brief Sets the callback for all message types.
		 * \param callback Function called when a message is received.
		 */
		auto set_message_callback(message_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			message_callback_ = std::move(callback);
		}

		/*!
		 * \brief Sets the callback for text messages only.
		 * \param callback Function called when a text message is received.
		 */
		auto set_text_message_callback(text_message_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			text_message_callback_ = std::move(callback);
		}

		/*!
		 * \brief Sets the callback for binary messages only.
		 * \param callback Function called when a binary message is received.
		 */
		auto set_binary_message_callback(binary_message_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			binary_message_callback_ = std::move(callback);
		}

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback Function called when an error occurs.
		 */
		auto set_error_callback(error_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			error_callback_ = std::move(callback);
		}

	protected:
		/*!
		 * \brief Returns a reference to the derived class instance.
		 */
		Derived& derived() noexcept
		{
			return static_cast<Derived&>(*this);
		}

		/*!
		 * \brief Returns a const reference to the derived class instance.
		 */
		const Derived& derived() const noexcept
		{
			return static_cast<const Derived&>(*this);
		}

		/*!
		 * \brief Invokes the connection callback.
		 * \param conn The new connection.
		 */
		auto invoke_connection_callback(std::shared_ptr<ws_connection> conn) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (connection_callback_)
			{
				connection_callback_(conn);
			}
		}

		/*!
		 * \brief Invokes the disconnection callback.
		 * \param conn_id The connection ID.
		 * \param code The close code.
		 * \param reason The close reason.
		 */
		auto invoke_disconnection_callback(const std::string& conn_id,
		                                   internal::ws_close_code code,
		                                   const std::string& reason) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (disconnection_callback_)
			{
				disconnection_callback_(conn_id, code, reason);
			}
		}

		/*!
		 * \brief Invokes the message callback.
		 * \param conn The connection that received the message.
		 * \param msg The received message.
		 */
		auto invoke_message_callback(std::shared_ptr<ws_connection> conn,
		                             const internal::ws_message& msg) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (message_callback_)
			{
				message_callback_(conn, msg);
			}

			if (msg.type == internal::ws_message_type::text && text_message_callback_)
			{
				text_message_callback_(conn, msg.as_text());
			}
			else if (msg.type == internal::ws_message_type::binary && binary_message_callback_)
			{
				binary_message_callback_(conn, msg.as_binary());
			}
		}

		/*!
		 * \brief Invokes the error callback.
		 * \param conn_id The connection ID.
		 * \param ec The error code.
		 */
		auto invoke_error_callback(const std::string& conn_id, std::error_code ec) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (error_callback_)
			{
				error_callback_(conn_id, ec);
			}
		}

	protected:
		std::string server_id_;                         /*!< Server identifier. */

		std::atomic<bool> is_running_{false};           /*!< True if server is active. */
		std::atomic<bool> stop_initiated_{false};       /*!< True if stop has been called. */

		std::optional<std::promise<void>> stop_promise_;/*!< Signals wait_for_stop(). */
		std::future<void> stop_future_;                 /*!< Used by wait_for_stop(). */

	private:
		connection_callback_t connection_callback_;     /*!< Connection callback. */
		disconnection_callback_t disconnection_callback_; /*!< Disconnection callback. */
		message_callback_t message_callback_;           /*!< Message callback. */
		text_message_callback_t text_message_callback_; /*!< Text message callback. */
		binary_message_callback_t binary_message_callback_; /*!< Binary message callback. */
		error_callback_t error_callback_;               /*!< Error callback. */

		mutable std::mutex callback_mutex_;             /*!< Protects callback access. */
	};

} // namespace kcenon::network::core
