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
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "kcenon/network/core/messaging_client.h"
#include "kcenon/common/resilience/circuit_breaker.h"
#include "kcenon/common/resilience/circuit_state.h"
#include "kcenon/network/utils/result_types.h"

namespace kcenon::network::utils
{

	/*!
	 * \class resilient_client
	 * \brief Wrapper around messaging_client that adds automatic reconnection
	 *        with exponential backoff and circuit breaker pattern.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe
	 * - Reconnection logic is protected by internal state management
	 * - Callbacks are invoked on reconnection thread
	 *
	 * ### Key Features
	 * - Automatic reconnection on connection loss
	 * - Exponential backoff to prevent connection storms
	 * - Configurable retry behavior (max attempts, backoff)
	 * - Circuit breaker pattern to prevent cascade failures
	 * - Callback notifications for reconnection events
	 * - Graceful degradation on persistent failures
	 *
	 * ### Circuit Breaker Integration
	 * The circuit breaker prevents excessive retry attempts when the backend
	 * is unavailable. When the circuit opens, send_with_retry() will fail
	 * immediately without attempting network calls.
	 *
	 * ### Usage Example
	 * \code
	 * auto client = std::make_shared<resilient_client>(
	 *     "client_id", "localhost", 8080,
	 *     3,  // max retries
	 *     std::chrono::seconds(1),  // initial backoff
	 *     common::resilience::circuit_breaker_config{
	 *         .failure_threshold = 5,
	 *         .timeout = std::chrono::seconds(30)
	 *     }
	 * );
	 *
	 * client->set_reconnect_callback([](size_t attempt) {
	 *     std::cout << "Reconnecting (attempt " << attempt << ")\n";
	 * });
	 *
	 * auto result = client->connect();
	 * if (!result) {
	 *     std::cerr << "Failed to connect\n";
	 * }
	 *
	 * // Send with automatic retry and circuit breaker protection
	 * std::vector<uint8_t> data = {1, 2, 3};
	 * auto send_result = client->send_with_retry(std::move(data));
	 *
	 * // Check circuit state
	 * if (client->circuit_state() == common::resilience::circuit_state::OPEN) {
	 *     std::cerr << "Circuit is open, backend unavailable\n";
	 * }
	 * \endcode
	 */
	class resilient_client
	{
	public:
		/*!
		 * \brief Constructs a resilient client with reconnection support
		 * \param client_id Client identifier
		 * \param host Server hostname or IP address
		 * \param port Server port number
		 * \param max_retries Maximum number of reconnection attempts (default: 3)
		 * \param initial_backoff Initial backoff duration (default: 1 second)
		 * \param cb_config Circuit breaker configuration (default values if not specified)
		 */
		resilient_client(const std::string& client_id,
						 const std::string& host,
						 unsigned short port,
						 size_t max_retries = 3,
						 std::chrono::milliseconds initial_backoff = std::chrono::seconds(1),
						 common::resilience::circuit_breaker_config cb_config = {});

		/*!
		 * \brief Destructor - disconnects client if still connected
		 */
		~resilient_client() noexcept;

		/*!
		 * \brief Connects to the server with retry logic
		 * \return Result<void> - Success if connected, error otherwise
		 *
		 * Attempts to connect with exponential backoff between retries.
		 * Invokes reconnect_callback_ on each retry attempt.
		 */
		auto connect() -> VoidResult;

		/*!
		 * \brief Disconnects from the server
		 * \return Result<void> - Success if disconnected, error otherwise
		 */
		auto disconnect() -> VoidResult;

		/*!
		 * \brief Sends data with automatic reconnection on failure
		 * \param data Data to send (moved for efficiency)
		 * \return Result<void> - Success if sent, error after all retries exhausted
		 *
		 * If connection is lost, automatically attempts to reconnect before
		 * retrying the send operation.
		 */
		auto send_with_retry(std::vector<uint8_t>&& data) -> VoidResult;

		/*!
		 * \brief Checks if currently connected to server
		 * \return true if connected, false otherwise
		 */
		[[nodiscard]] auto is_connected() const noexcept -> bool;

		/*!
		 * \brief Sets callback for reconnection events
		 * \param callback Function called on each reconnection attempt
		 *
		 * The callback receives the current attempt number (1-based).
		 */
		auto set_reconnect_callback(std::function<void(size_t attempt)> callback) -> void;

		/*!
		 * \brief Sets callback for connection loss events
		 * \param callback Function called when connection is lost
		 */
		auto set_disconnect_callback(std::function<void()> callback) -> void;

		/*!
		 * \brief Gets the underlying messaging client
		 * \return Shared pointer to the messaging_client
		 */
		[[nodiscard]] auto get_client() const -> std::shared_ptr<core::messaging_client>;

		/*!
		 * \brief Gets the current circuit breaker state
		 * \return Current state (CLOSED, OPEN, or HALF_OPEN)
		 */
		[[nodiscard]] auto circuit_state() const -> common::resilience::circuit_state;

	private:
		/*!
		 * \brief Attempts to reconnect with exponential backoff
		 * \return Result<void> - Success if reconnected, error otherwise
		 */
		auto reconnect() -> VoidResult;

		/*!
		 * \brief Calculates backoff duration for given attempt
		 * \param attempt Current attempt number (1-based)
		 * \return Backoff duration in milliseconds
		 */
		auto calculate_backoff(size_t attempt) const -> std::chrono::milliseconds;

		std::shared_ptr<core::messaging_client> client_; /*!< Underlying client */

		std::string host_;        /*!< Server hostname */
		unsigned short port_;     /*!< Server port */
		size_t max_retries_;      /*!< Maximum retry attempts */
		std::chrono::milliseconds initial_backoff_; /*!< Initial backoff duration */

		std::atomic<bool> is_connected_{false}; /*!< Connection state */

		std::function<void(size_t)> reconnect_callback_; /*!< Reconnection callback */
		std::function<void()> disconnect_callback_;      /*!< Disconnection callback */

		std::unique_ptr<common::resilience::circuit_breaker> circuit_breaker_; /*!< Circuit breaker instance */
	};

} // namespace kcenon::network::utils
