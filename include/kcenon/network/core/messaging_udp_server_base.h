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

namespace kcenon::network::core
{

	/*!
	 * \class messaging_udp_server_base
	 * \brief CRTP base class for UDP servers that provides common lifecycle
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
	 * - `auto do_start(uint16_t port) -> VoidResult`
	 * - `auto do_stop() -> VoidResult`
	 *
	 * ### Usage Example
	 * \code
	 * class my_udp_server : public messaging_udp_server_base<my_udp_server> {
	 * protected:
	 *     friend class messaging_udp_server_base<my_udp_server>;
	 *     auto do_start(uint16_t port) -> VoidResult;
	 *     auto do_stop() -> VoidResult;
	 * };
	 * \endcode
	 */
	template<typename Derived>
	class messaging_udp_server_base
		: public std::enable_shared_from_this<Derived>
	{
	public:
		//! \brief Callback type for received datagrams with sender endpoint
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&,
		                                              const asio::ip::udp::endpoint&)>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a UDP server with a given identifier.
		 * \param server_id A string identifier for this server instance.
		 */
		explicit messaging_udp_server_base(std::string_view server_id)
			: server_id_(server_id)
		{
		}

		/*!
		 * \brief Virtual destructor; calls stop_server() if still running.
		 */
		virtual ~messaging_udp_server_base() noexcept
		{
			if (is_running_.load())
			{
				stop_server();
			}
		}

		// Non-copyable, non-movable
		messaging_udp_server_base(const messaging_udp_server_base&) = delete;
		messaging_udp_server_base& operator=(const messaging_udp_server_base&) = delete;
		messaging_udp_server_base(messaging_udp_server_base&&) = delete;
		messaging_udp_server_base& operator=(messaging_udp_server_base&&) = delete;

		/*!
		 * \brief Starts the server on the specified port.
		 * \param port The UDP port to bind and listen on.
		 * \return Result<void> - Success if server started, or error with code:
		 *         - error_codes::network_system::server_already_running if already running
		 *         - error_codes::network_system::bind_failed if port binding failed
		 *         - error_codes::common_errors::internal_error for other failures
		 */
		auto start_server(uint16_t port) -> VoidResult
		{
			if (is_running_.load())
			{
				return error_void(
					error_codes::network_system::server_already_running,
					"UDP server is already running",
					"messaging_udp_server_base");
			}

			is_running_.store(true);
			stop_initiated_.store(false);

			// Initialize stop promise/future for wait_for_stop()
			stop_promise_.emplace();
			stop_future_ = stop_promise_->get_future();

			// Call derived class implementation
			auto result = derived().do_start(port);
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
		 * \return Result<void> - Success if server stopped, or error with code:
		 *         - error_codes::common_errors::internal_error for failures
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
		 * \brief Sets the callback for received datagrams.
		 * \param callback Function called when a datagram is received.
		 *        Receives the data and sender endpoint.
		 */
		auto set_receive_callback(receive_callback_t callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			receive_callback_ = std::move(callback);
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
		 * \brief Invokes the receive callback with the given data and endpoint.
		 * \param data The received data.
		 * \param endpoint The sender's endpoint.
		 *
		 * Thread-safe. Should be called by derived class when a datagram arrives.
		 */
		auto invoke_receive_callback(const std::vector<uint8_t>& data,
		                             const asio::ip::udp::endpoint& endpoint) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (receive_callback_)
			{
				receive_callback_(data, endpoint);
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
		 * \brief Gets a copy of the receive callback.
		 * \return Copy of the receive callback (may be empty).
		 *
		 * Thread-safe. Used by derived classes to get callback for socket setup.
		 */
		[[nodiscard]] auto get_receive_callback() const -> receive_callback_t
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			return receive_callback_;
		}

		/*!
		 * \brief Gets a copy of the error callback.
		 * \return Copy of the error callback (may be empty).
		 *
		 * Thread-safe. Used by derived classes to get callback for socket setup.
		 */
		[[nodiscard]] auto get_error_callback() const -> error_callback_t
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			return error_callback_;
		}

	protected:
		std::string server_id_;                         /*!< Server identifier. */

		std::atomic<bool> is_running_{false};           /*!< True if server is active. */
		std::atomic<bool> stop_initiated_{false};       /*!< True if stop has been called. */

		std::optional<std::promise<void>> stop_promise_;/*!< Signals wait_for_stop(). */
		std::future<void> stop_future_;                 /*!< Used by wait_for_stop(). */

	private:
		receive_callback_t receive_callback_;           /*!< Receive callback. */
		error_callback_t error_callback_;               /*!< Error callback. */

		mutable std::mutex callback_mutex_;             /*!< Protects callback access. */
	};

} // namespace kcenon::network::core
