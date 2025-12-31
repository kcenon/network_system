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

#include <asio.hpp>

#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/integration/io_context_thread_manager.h"

namespace kcenon::network::core
{

	/*!
	 * \class messaging_client_base
	 * \brief CRTP base class for messaging clients that provides common lifecycle
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
	 * - `auto do_start(std::string_view host, unsigned short port) -> VoidResult`
	 * - `auto do_stop() -> VoidResult`
	 * - `auto do_send(std::vector<uint8_t>&& data) -> VoidResult`
	 *
	 * ### Usage Example
	 * \code
	 * class my_client : public messaging_client_base<my_client> {
	 * protected:
	 *     friend class messaging_client_base<my_client>;
	 *     auto do_start(std::string_view host, unsigned short port) -> VoidResult;
	 *     auto do_stop() -> VoidResult;
	 *     auto do_send(std::vector<uint8_t>&& data) -> VoidResult;
	 * };
	 * \endcode
	 */
	template<typename Derived>
	class messaging_client_base
		: public std::enable_shared_from_this<Derived>
	{
	public:
		//! \brief Callback type for received data
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;
		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;
		//! \brief Callback type for disconnection
		using disconnected_callback_t = std::function<void()>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a client with a given identifier.
		 * \param client_id A string identifier for this client instance.
		 */
		explicit messaging_client_base(std::string_view client_id)
			: client_id_(client_id)
		{
		}

		/*!
		 * \brief Virtual destructor; calls stop_client() if still running.
		 */
		virtual ~messaging_client_base() noexcept
		{
			if (is_running_.load())
			{
				stop_client();
			}
		}

		// Non-copyable, non-movable
		messaging_client_base(const messaging_client_base&) = delete;
		messaging_client_base& operator=(const messaging_client_base&) = delete;
		messaging_client_base(messaging_client_base&&) = delete;
		messaging_client_base& operator=(messaging_client_base&&) = delete;

		/*!
		 * \brief Starts the client by connecting to the specified host and port.
		 * \param host The remote hostname or IP address.
		 * \param port The remote port number.
		 * \return Result<void> - Success if client started, or error with code:
		 *         - error_codes::common_errors::already_exists if already running
		 *         - error_codes::common_errors::invalid_argument if empty host
		 *         - error_codes::common_errors::internal_error for other failures
		 */
		auto start_client(std::string_view host, unsigned short port) -> VoidResult
		{
			if (is_running_.load())
			{
				return error_void(
					error_codes::common_errors::already_exists,
					"Client is already running",
					"messaging_client_base");
			}

			if (host.empty())
			{
				return error_void(
					error_codes::common_errors::invalid_argument,
					"Host cannot be empty",
					"messaging_client_base");
			}

			is_running_.store(true);
			is_connected_.store(false);
			stop_initiated_.store(false);

			// Initialize stop promise/future for wait_for_stop()
			stop_promise_.emplace();
			stop_future_ = stop_promise_->get_future();

			// Call derived class implementation
			auto result = derived().do_start(host, port);
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
		 * \return Result<void> - Success if client stopped, or error with code:
		 *         - error_codes::common_errors::internal_error for failures
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

			// Invoke disconnected callback
			invoke_disconnected_callback();

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
		 * \brief Sends data to the remote endpoint.
		 * \param data The buffer to send (moved for efficiency).
		 * \return Result<void> - Success if data queued for send, or error with code:
		 *         - error_codes::network_system::connection_closed if not connected
		 *         - error_codes::common_errors::invalid_argument if empty data
		 */
		auto send_packet(std::vector<uint8_t>&& data) -> VoidResult
		{
			if (!is_connected_.load())
			{
				return error_void(
					error_codes::network_system::connection_closed,
					"Not connected",
					"messaging_client_base");
			}

			if (data.empty())
			{
				return error_void(
					error_codes::common_errors::invalid_argument,
					"Data cannot be empty",
					"messaging_client_base");
			}

			return derived().do_send(std::move(data));
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
		 * \brief Sets the callback for received data.
		 * \param callback Function called when data is received.
		 */
		auto set_receive_callback(receive_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			receive_callback_ = std::move(callback);
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
		 * \param callback Function called when disconnected.
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
		 * \brief Invokes the receive callback with the given data.
		 * \param data The received data.
		 *
		 * Thread-safe. Should be called by derived class when data arrives.
		 */
		auto invoke_receive_callback(const std::vector<uint8_t>& data) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (receive_callback_)
			{
				receive_callback_(data);
			}
		}

		/*!
		 * \brief Invokes the connected callback.
		 *
		 * Thread-safe. Should be called by derived class when connection is established.
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
		 *
		 * Thread-safe. Called automatically by stop_client().
		 */
		auto invoke_disconnected_callback() -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (disconnected_callback_)
			{
				disconnected_callback_();
			}
		}

		/*!
		 * \brief Invokes the error callback with the given error code.
		 * \param ec The error code.
		 *
		 * Thread-safe. Should be called by derived class when an error occurs.
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
		 *
		 * Should be called by derived class when connection state changes.
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
		receive_callback_t receive_callback_;           /*!< Receive callback. */
		connected_callback_t connected_callback_;       /*!< Connected callback. */
		disconnected_callback_t disconnected_callback_; /*!< Disconnected callback. */
		error_callback_t error_callback_;               /*!< Error callback. */

		mutable std::mutex callback_mutex_;             /*!< Protects callback access. */
	};

} // namespace kcenon::network::core
