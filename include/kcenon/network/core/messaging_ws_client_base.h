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

	/*!
	 * \class messaging_ws_client_base
	 * \brief CRTP base class for WebSocket clients that provides common lifecycle
	 *        management and callback handling.
	 *
	 * \tparam Derived The derived class type (CRTP pattern)
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Atomic flags (is_running_, is_connected_) prevent race conditions.
	 * - Callback access is protected by callback_mutex_.
	 *
	 * ### CRTP Pattern
	 * Derived classes must implement the following protected methods:
	 * - `auto do_start(const ws_client_config& config) -> VoidResult`
	 * - `auto do_stop() -> VoidResult`
	 *
	 * ### Usage Example
	 * \code
	 * class my_ws_client : public messaging_ws_client_base<my_ws_client> {
	 * protected:
	 *     friend class messaging_ws_client_base<my_ws_client>;
	 *     auto do_start(const ws_client_config& config) -> VoidResult;
	 *     auto do_stop() -> VoidResult;
	 * };
	 * \endcode
	 */
	template<typename Derived>
	class messaging_ws_client_base
		: public std::enable_shared_from_this<Derived>
	{
	public:
		//! \brief Callback type for WebSocket messages
		using message_callback_t = std::function<void(const internal::ws_message&)>;
		//! \brief Callback type for text messages
		using text_message_callback_t = std::function<void(const std::string&)>;
		//! \brief Callback type for binary messages
		using binary_message_callback_t = std::function<void(const std::vector<uint8_t>&)>;
		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;
		//! \brief Callback type for disconnection with close code
		using disconnected_callback_t = std::function<void(internal::ws_close_code, const std::string&)>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a WebSocket client with a given identifier.
		 * \param client_id A string identifier for this client instance.
		 */
		explicit messaging_ws_client_base(std::string_view client_id)
			: client_id_(client_id)
		{
		}

		/*!
		 * \brief Virtual destructor; calls stop_client() if still running.
		 */
		virtual ~messaging_ws_client_base() noexcept
		{
			if (is_running_.load())
			{
				stop_client();
			}
		}

		// Non-copyable, non-movable
		messaging_ws_client_base(const messaging_ws_client_base&) = delete;
		messaging_ws_client_base& operator=(const messaging_ws_client_base&) = delete;
		messaging_ws_client_base(messaging_ws_client_base&&) = delete;
		messaging_ws_client_base& operator=(messaging_ws_client_base&&) = delete;

		/*!
		 * \brief Starts the client by connecting to the WebSocket server.
		 * \param host The remote hostname or IP address.
		 * \param port The remote port number.
		 * \param path The WebSocket path (default: "/").
		 * \return Result<void> - Success if client started, or error with code.
		 */
		auto start_client(std::string_view host, uint16_t port, std::string_view path = "/") -> VoidResult
		{
			if (is_running_.load())
			{
				return error_void(
					error_codes::common_errors::already_exists,
					"WebSocket client is already running",
					"messaging_ws_client_base");
			}

			if (host.empty())
			{
				return error_void(
					error_codes::common_errors::invalid_argument,
					"Host cannot be empty",
					"messaging_ws_client_base");
			}

			is_running_.store(true);
			is_connected_.store(false);
			stop_initiated_.store(false);

			// Initialize stop promise/future for wait_for_stop()
			stop_promise_.emplace();
			stop_future_ = stop_promise_->get_future();

			// Call derived class implementation
			auto result = derived().do_start(host, port, path);
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
		 * \brief Stops the client and releases all resources.
		 * \return Result<void> - Success if client stopped, or error with code.
		 */
		auto stop_client() -> VoidResult
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
			is_connected_.store(false);

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
		 * \brief Blocks until stop_client() is called.
		 */
		auto wait_for_stop() -> void
		{
			if (stop_future_.valid())
			{
				stop_future_.wait();
			}
		}

		/*!
		 * \brief Check if the client is currently running.
		 * \return true if running, false otherwise.
		 */
		[[nodiscard]] auto is_running() const noexcept -> bool
		{
			return is_running_.load(std::memory_order_relaxed);
		}

		/*!
		 * \brief Check if the client is connected to the server.
		 * \return true if connected, false otherwise.
		 */
		[[nodiscard]] auto is_connected() const noexcept -> bool
		{
			return is_connected_.load(std::memory_order_relaxed);
		}

		/*!
		 * \brief Returns the client identifier.
		 * \return The client_id string.
		 */
		[[nodiscard]] auto client_id() const -> const std::string&
		{
			return client_id_;
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
		 * \brief Sets the callback for connection established.
		 * \param callback Function called when connection succeeds.
		 */
		auto set_connected_callback(connected_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			connected_callback_ = std::move(callback);
		}

		/*!
		 * \brief Sets the callback for disconnection.
		 * \param callback Function called when disconnected (includes close code).
		 */
		auto set_disconnected_callback(disconnected_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			disconnected_callback_ = std::move(callback);
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
		 * \brief Invokes the message callback.
		 * \param msg The received message.
		 */
		auto invoke_message_callback(const internal::ws_message& msg) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (message_callback_)
			{
				message_callback_(msg);
			}

			if (msg.type == internal::ws_message_type::text && text_message_callback_)
			{
				text_message_callback_(msg.as_text());
			}
			else if (msg.type == internal::ws_message_type::binary && binary_message_callback_)
			{
				binary_message_callback_(msg.as_binary());
			}
		}

		/*!
		 * \brief Invokes the connected callback.
		 */
		auto invoke_connected_callback() -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (connected_callback_)
			{
				connected_callback_();
			}
		}

		/*!
		 * \brief Invokes the disconnected callback.
		 * \param code The close code.
		 * \param reason The close reason.
		 */
		auto invoke_disconnected_callback(internal::ws_close_code code,
		                                  const std::string& reason) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (disconnected_callback_)
			{
				disconnected_callback_(code, reason);
			}
		}

		/*!
		 * \brief Invokes the error callback.
		 * \param ec The error code.
		 */
		auto invoke_error_callback(std::error_code ec) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (error_callback_)
			{
				error_callback_(ec);
			}
		}

		/*!
		 * \brief Sets the connected state.
		 * \param connected The new connection state.
		 */
		auto set_connected(bool connected) -> void
		{
			is_connected_.store(connected, std::memory_order_release);
		}

	protected:
		std::string client_id_;                         /*!< Client identifier. */

		std::atomic<bool> is_running_{false};           /*!< True if client is active. */
		std::atomic<bool> is_connected_{false};         /*!< True if connected to remote. */
		std::atomic<bool> stop_initiated_{false};       /*!< True if stop has been called. */

		std::optional<std::promise<void>> stop_promise_;/*!< Signals wait_for_stop(). */
		std::future<void> stop_future_;                 /*!< Used by wait_for_stop(). */

	private:
		message_callback_t message_callback_;           /*!< Message callback. */
		text_message_callback_t text_message_callback_; /*!< Text message callback. */
		binary_message_callback_t binary_message_callback_; /*!< Binary message callback. */
		connected_callback_t connected_callback_;       /*!< Connected callback. */
		disconnected_callback_t disconnected_callback_; /*!< Disconnected callback. */
		error_callback_t error_callback_;               /*!< Error callback. */

		mutable std::mutex callback_mutex_;             /*!< Protects callback access. */
	};

} // namespace kcenon::network::core
