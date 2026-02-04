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

// Deprecation warning for v2.0 migration
#ifndef NETWORK_SYSTEM_SUPPRESS_DEPRECATION_WARNINGS
#if defined(__GNUC__) || defined(__clang__)
#warning "messaging_udp_client.h is deprecated and will move to internal in v2.0. Use udp_facade.h instead. See docs/refactoring/MIGRATION_GUIDE_V2.md"
#elif defined(_MSC_VER)
#pragma message("Warning: messaging_udp_client.h is deprecated and will move to internal in v2.0. Use udp_facade.h instead. See docs/refactoring/MIGRATION_GUIDE_V2.md")
#endif
#endif

/**
 * @file messaging_udp_client.h
 * @brief Legacy UDP client class.
 *
 * @deprecated This header is deprecated. Use unified_udp_messaging_client.h instead.
 *
 * Migration guide:
 * @code
 * // Old code:
 * #include "internal/core/messaging_udp_client.h>
 * auto client = std::make_shared<messaging_udp_client>("client1");
 *
 * // New code:
 * #include "internal/core/unified_udp_messaging_client.h>
 * auto client = std::make_shared<udp_client>("client1");
 * // Or: auto client = std::make_shared<unified_udp_messaging_client<no_tls>>("client1");
 * @endcode
 *
 * @see unified_udp_messaging_client.h for the new template-based API
 */

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <asio.hpp>

#include "internal/core/callback_indices.h"
#include "internal/interfaces/i_udp_client.h"
#include "kcenon/network/detail/utils/result_types.h"
#include "kcenon/network/detail/utils/lifecycle_manager.h"
#include "kcenon/network/detail/utils/callback_manager.h"
#include "kcenon/network/integration/thread_integration.h"

namespace kcenon::network::internal
{
	class udp_socket;
}

namespace kcenon::network::core
{
	/*!
	 * \class messaging_udp_client
	 * \brief A UDP client that sends datagrams to a target endpoint and can receive responses.
	 *
	 * @deprecated Use unified_udp_messaging_client<no_tls> or udp_client instead.
	 *
	 * This class uses composition pattern with lifecycle_manager and
	 * callback_manager for common lifecycle management and callback handling.
	 * It also implements the i_udp_client interface for composition-based usage.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Socket access is protected by socket_mutex_.
	 * - Atomic flags prevent race conditions.
	 * - send_packet() can be called from any thread safely.
	 *
	 * ### Key Characteristics
	 * - Connectionless: No persistent connection, each send is independent.
	 * - Target endpoint: Configured at start, can be changed via set_target().
	 * - Bidirectional: Can both send and receive datagrams.
	 * - Stateless: No built-in acknowledgment or reliability.
	 *
	 * ### Interface Compliance
	 * This class implements interfaces::i_udp_client for composition-based usage.
	 *
	 * ### Usage Example
	 * \code
	 * auto client = std::make_shared<messaging_udp_client>("UDPClient");
	 *
	 * // Set callback to handle received datagrams (using interface callback)
	 * client->set_receive_callback(
	 *     [](const std::vector<uint8_t>& data,
	 *        const interfaces::i_udp_client::endpoint_info& sender) {
	 *         std::cout << "Received " << data.size() << " bytes from "
	 *                   << sender.address << ":" << sender.port << "\n";
	 *     });
	 *
	 * // Start client targeting localhost:5555
	 * auto result = client->start_client("localhost", 5555);
	 * if (!result) {
	 *     std::cerr << "Failed to start client: " << result.error().message << "\n";
	 *     return -1;
	 * }
	 *
	 * // Send a datagram using send()
	 * std::vector<uint8_t> data = {0x01, 0x02, 0x03};
	 * client->send(std::move(data),
	 *     [](std::error_code ec, std::size_t bytes) {
	 *         if (!ec) {
	 *             std::cout << "Sent " << bytes << " bytes\n";
	 *         }
	 *     });
	 *
	 * // Stop client
	 * client->stop_client();
	 * \endcode
	 */
	class messaging_udp_client
		: public std::enable_shared_from_this<messaging_udp_client>
		, public interfaces::i_udp_client
	{
	public:
		//! \brief Callback type for received datagrams with sender endpoint
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&,
		                                              const asio::ip::udp::endpoint&)>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a messaging_udp_client with an identifier.
		 * \param client_id A string identifier for this client instance.
		 */
		explicit messaging_udp_client(std::string_view client_id);

		/*!
		 * \brief Destructor. Automatically calls stop_client() if the client is still running.
		 */
		~messaging_udp_client() noexcept override;

		// Non-copyable, non-movable
		messaging_udp_client(const messaging_udp_client&) = delete;
		messaging_udp_client& operator=(const messaging_udp_client&) = delete;
		messaging_udp_client(messaging_udp_client&&) = delete;
		messaging_udp_client& operator=(messaging_udp_client&&) = delete;

		// ========================================================================
		// Lifecycle Management
		// ========================================================================

		/*!
		 * \brief Starts the client by resolving target host and port.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return Result<void> - Success if client started, or error with code:
		 *         - error_codes::common_errors::already_exists if already running
		 *         - error_codes::common_errors::invalid_argument if empty host
		 *         - error_codes::common_errors::internal_error for other failures
		 */
		[[nodiscard]] auto start_client(std::string_view host, uint16_t port) -> VoidResult;

		/*!
		 * \brief Stops the client and releases all resources.
		 * \return Result<void> - Success if client stopped, or error with code:
		 *         - error_codes::common_errors::internal_error for failures
		 */
		[[nodiscard]] auto stop_client() -> VoidResult;

		/*!
		 * \brief Returns the client identifier.
		 * \return The client_id string.
		 */
		[[nodiscard]] auto client_id() const -> const std::string&;

		// ========================================================================
		// i_network_component interface implementation
		// ========================================================================

		/*!
		 * \brief Checks if the client is currently running.
		 * \return true if running, false otherwise.
		 *
		 * Implements i_network_component::is_running().
		 */
		[[nodiscard]] auto is_running() const -> bool override;

		/*!
		 * \brief Blocks until stop_client() is called.
		 *
		 * Implements i_network_component::wait_for_stop().
		 */
		auto wait_for_stop() -> void override;

		// ========================================================================
		// i_udp_client interface implementation
		// ========================================================================

		/*!
		 * \brief Starts the UDP client targeting the specified endpoint.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_client::start(). Delegates to start_client().
		 */
		[[nodiscard]] auto start(std::string_view host, uint16_t port) -> VoidResult override;

		/*!
		 * \brief Stops the UDP client.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_client::stop(). Delegates to stop_client().
		 */
		[[nodiscard]] auto stop() -> VoidResult override;

		/*!
		 * \brief Sends a datagram to the configured target endpoint.
		 * \param data The data to send.
		 * \param handler Optional completion handler.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_client::send().
		 */
		[[nodiscard]] auto send(
			std::vector<uint8_t>&& data,
			interfaces::i_udp_client::send_callback_t handler = nullptr) -> VoidResult override;

		/*!
		 * \brief Changes the target endpoint for future sends.
		 * \param host The new target hostname or IP address.
		 * \param port The new target port number.
		 * \return VoidResult indicating success or failure.
		 *
		 * Implements i_udp_client::set_target().
		 */
		[[nodiscard]] auto set_target(std::string_view host, uint16_t port) -> VoidResult override;

		/*!
		 * \brief Sets the callback for received datagrams (interface version).
		 * \param callback The callback function with endpoint_info.
		 *
		 * Implements i_udp_client::set_receive_callback().
		 */
		auto set_receive_callback(interfaces::i_udp_client::receive_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for received datagrams (legacy version).
		 * \param callback The callback function with asio endpoint.
		 *
		 * This overload maintains backward compatibility with code using
		 * asio::ip::udp::endpoint directly.
		 */
		auto set_receive_callback(receive_callback_t callback) -> void;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback The callback function.
		 *
		 * Implements i_udp_client::set_error_callback().
		 * Also compatible with legacy error_callback_t (same signature).
		 */
		auto set_error_callback(error_callback_t callback) -> void override;

	private:
		// =====================================================================
		// Internal Implementation Methods
		// =====================================================================

		/*!
		 * \brief UDP-specific implementation of client start.
		 * \param host The target hostname or IP address.
		 * \param port The target port number.
		 * \return Result<void> - Success if client started, or error with code.
		 *
		 * Creates io_context, resolves target, creates socket, and starts worker thread.
		 */
		auto do_start_impl(std::string_view host, uint16_t port) -> VoidResult;

		/*!
		 * \brief UDP-specific implementation of client stop.
		 * \return Result<void> - Success if client stopped, or error with code.
		 *
		 * Stops receiving, closes socket, and releases resources.
		 */
		auto do_stop_impl() -> VoidResult;

		// =====================================================================
		// Internal Callback Helpers
		// =====================================================================

		/*!
		 * \brief Invokes the receive callback with the given data and endpoint.
		 * \param data The received data.
		 * \param endpoint The sender's endpoint.
		 */
		auto invoke_receive_callback(const std::vector<uint8_t>& data,
		                             const asio::ip::udp::endpoint& endpoint) -> void;

		/*!
		 * \brief Invokes the error callback with the given error code.
		 * \param ec The error code.
		 */
		auto invoke_error_callback(std::error_code ec) -> void;

		/*!
		 * \brief Gets a copy of the receive callback.
		 * \return Copy of the receive callback (may be empty).
		 */
		[[nodiscard]] auto get_receive_callback() const -> receive_callback_t;

		/*!
		 * \brief Gets a copy of the error callback.
		 * \return Copy of the error callback (may be empty).
		 */
		[[nodiscard]] auto get_error_callback() const -> error_callback_t;

		//! \brief Callback index type alias for clarity
		using callback_index = udp_client_callback;

		//! \brief Callback manager type for this client
		using callbacks_t = utils::callback_manager<
			receive_callback_t,
			error_callback_t
		>;

		// =====================================================================
		// Member Variables
		// =====================================================================

		std::string client_id_;                          /*!< Client identifier. */
		utils::lifecycle_manager lifecycle_;             /*!< Lifecycle state manager. */
		callbacks_t callbacks_;                          /*!< Callback manager. */

		std::unique_ptr<asio::io_context> io_context_;   /*!< ASIO I/O context. */
		std::shared_ptr<internal::udp_socket> socket_;   /*!< UDP socket wrapper. */

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;   /*!< Thread pool for async operations. */
		std::future<void> io_context_future_;            /*!< Future for io_context run task. */

		std::mutex endpoint_mutex_;                      /*!< Protects target_endpoint_. */
		asio::ip::udp::endpoint target_endpoint_;        /*!< Target endpoint for sends. */

		mutable std::mutex socket_mutex_;                /*!< Protects socket access. */
	};

} // namespace kcenon::network::core
