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
#warning "messaging_client.h is deprecated and will move to internal in v2.0. Use tcp_facade.h instead. See docs/refactoring/MIGRATION_GUIDE_V2.md"
#elif defined(_MSC_VER)
#pragma message("Warning: messaging_client.h is deprecated and will move to internal in v2.0. Use tcp_facade.h instead. See docs/refactoring/MIGRATION_GUIDE_V2.md")
#endif
#endif

/**
 * @file messaging_client.h
 * @brief TCP client implementation (DEPRECATED - Will be moved to internal in v2.0)
 *
 * @deprecated This header will be moved to src/internal/ in network_system v2.0.
 *             Use kcenon/network/facade/tcp_facade.h instead for a simpler, stable API.
 *
 * @warning This header is scheduled for removal from public API in v3.0.
 *          See docs/refactoring/MIGRATION_GUIDE_V2.md for migration instructions.
 *
 * Migration guide:
 * @code
 * // Old code (v1.x):
 * #include "internal/core/messaging_client.h"
 * auto client = std::make_shared<messaging_client>("client-id", "127.0.0.1", 8080);
 *
 * // New code (v2.0+):
 * #include "kcenon/network/facade/tcp_facade.h"
 * tcp_facade facade;
 * auto client = facade.create_client({
 *     .host = "127.0.0.1",
 *     .port = 8080,
 *     .client_id = "client-id"
 * });
 * @endcode
 *
 * @see unified_messaging_client.h for the new template-based API
 */

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include <asio.hpp>

#include "internal/core/callback_indices.h"
#include "internal/tcp/tcp_socket.h"
#include "internal/integration/io_context_thread_manager.h"
#include "kcenon/network/detail/utils/startable_base.h"
#include "kcenon/network/detail/utils/callback_manager.h"
#include "kcenon/network/detail/utils/result_types.h"
#include "kcenon/network/interfaces/i_protocol_client.h"
#include "kcenon/network/interfaces/connection_observer.h"

// Use nested namespace definition in C++17
namespace kcenon::network::core
{

	/*!
	 * \class messaging_client
	 * \brief A basic TCP client that connects to a remote host, sends/receives
	 *
	 * @deprecated Use unified_messaging_client<tcp_protocol> or tcp_client instead.
	 * data using asynchronous operations, and can apply a pipeline for
	 * transformations.
	 *
	 * This class uses composition pattern with lifecycle_manager and
	 * callback_manager for common lifecycle management and callback handling.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Socket access is protected by socket_mutex_.
	 * - Atomic flags (is_running_, is_connected_, stop_initiated_) prevent race conditions.
	 * - send_packet() can be called from any thread safely.
	 * - Connection state changes are serialized through ASIO's io_context.
	 *
	 * ### Key Features
	 * - Uses \c asio::io_context in a dedicated thread to handle I/O events.
	 * - Connects via \c async_connect, then wraps the socket in a \c tcp_socket
	 * for asynchronous reads and writes.
	 * - Optionally compresses/encrypts data before sending, and can similarly
	 *   decompress/decrypt incoming data if extended.
	 * - Provides \c start_client(), \c stop_client(), and \c wait_for_stop() to
	 * control lifecycle.
	 */
	class messaging_client
		: public std::enable_shared_from_this<messaging_client>
		, public utils::startable_base<messaging_client>
		, public interfaces::i_protocol_client
	{
		friend class utils::startable_base<messaging_client>;

	public:
		// =====================================================================
		// INetworkComponent Interface Implementation (via IProtocolClient)
		// =====================================================================

		/*!
		 * \brief Checks if the client is currently running.
		 * \return true if running, false otherwise.
		 */
		[[nodiscard]] auto is_running() const -> bool override
		{
			return utils::startable_base<messaging_client>::is_running();
		}

		/*!
		 * \brief Waits for the client to stop.
		 */
		auto wait_for_stop() -> void override
		{
			return utils::startable_base<messaging_client>::wait_for_stop();
		}
		//! \brief Callback type for received data
		using receive_callback_t = std::function<void(const std::vector<uint8_t>&)>;
		//! \brief Callback type for connection established
		using connected_callback_t = std::function<void()>;
		//! \brief Callback type for disconnection
		using disconnected_callback_t = std::function<void()>;
		//! \brief Callback type for errors
		using error_callback_t = std::function<void(std::error_code)>;

		/*!
		 * \brief Constructs a client with a given \p client_id used for logging
		 * or identification.
		 * \param client_id A string identifier for this client instance.
		 */
		explicit messaging_client(std::string_view client_id);

		/*!
		 * \brief Destructor; automatically calls \c stop_client() if the client
		 * is still running.
		 */
		~messaging_client() noexcept;

		// Non-copyable, non-movable
		messaging_client(const messaging_client&) = delete;
		messaging_client& operator=(const messaging_client&) = delete;
		messaging_client(messaging_client&&) = delete;
		messaging_client& operator=(messaging_client&&) = delete;

		// =====================================================================
		// IProtocolClient Interface Implementation
		// =====================================================================

		/*!
		 * \brief Starts the client and connects to the specified server (IProtocolClient interface).
		 * \param host The server hostname or IP address.
		 * \param port The server port number.
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] auto start(std::string_view host, uint16_t port) -> VoidResult override;

		/*!
		 * \brief Stops the client and closes the connection (IProtocolClient interface).
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] auto stop() -> VoidResult override;

		/*!
		 * \brief Sends data to the connected server (IProtocolClient interface).
		 * \param data The data to send.
		 * \return VoidResult indicating success or failure.
		 */
		[[nodiscard]] auto send(std::vector<uint8_t>&& data) -> VoidResult override;

		/*!
		 * \brief Checks if the client is connected to the server (IProtocolClient interface).
		 * \return true if connected, false otherwise.
		 */
		[[nodiscard]] auto is_connected() const -> bool override;

		/*!
		 * \brief Sets the connection observer for unified event handling.
		 * \param observer The observer instance (shared ownership).
		 */
		auto set_observer(std::shared_ptr<interfaces::connection_observer> observer) -> void override;

		// =====================================================================
		// Lifecycle Management (Legacy API)
		// =====================================================================

		/*!
		 * \brief Starts the client and connects to the specified host and port.
		 * \param host The remote hostname or IP address.
		 * \param port The remote port number to connect.
		 * \return Result<void> - Success if client started, or error with code:
		 *         - error_codes::network_system::client_already_running if already running
		 *         - error_codes::common_errors::internal_error for other failures
		 */
		[[nodiscard]] auto start_client(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief Stops the client and disconnects from the server.
		 * \return Result<void> - Success if client stopped, or error with code:
		 *         - error_codes::network_system::client_not_started if not running
		 *         - error_codes::common_errors::internal_error for failures
		 */
		[[nodiscard]] auto stop_client() -> VoidResult;

		// Note: wait_for_stop() and is_running() are inherited from startable_base
		// Note: is_connected() is declared in IProtocolClient interface section

		/*!
		 * \brief Returns the client identifier.
		 * \return The client_id string.
		 */
		[[nodiscard]] auto client_id() const -> const std::string&;

		// =====================================================================
		// Data Transfer
		// =====================================================================

		/*!
		 * \brief Sends data to the connected server.
		 * \param data The buffer to send (moved for efficiency).
		 * \return Result<void> - Success if data queued for send, or error with code:
		 *         - error_codes::network_system::connection_closed if not connected
		 *         - error_codes::network_system::send_failed for other failures
		 */
		[[nodiscard]] auto send_packet(std::vector<uint8_t>&& data) -> VoidResult;

		// =====================================================================
		// Callback Setters (Deprecated - IProtocolClient interface)
		// =====================================================================

		/*!
		 * \brief Sets the callback for received data.
		 * \param callback Function called when data is received.
		 * \deprecated Use set_observer() with connection_observer instead.
		 */
		auto set_receive_callback(receive_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for connection established.
		 * \param callback Function called when connection is established.
		 * \deprecated Use set_observer() with connection_observer instead.
		 */
		auto set_connected_callback(connected_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for disconnection.
		 * \param callback Function called when disconnected.
		 * \deprecated Use set_observer() with connection_observer instead.
		 */
		auto set_disconnected_callback(disconnected_callback_t callback) -> void override;

		/*!
		 * \brief Sets the callback for errors.
		 * \param callback Function called when an error occurs.
		 * \deprecated Use set_observer() with connection_observer instead.
		 */
		auto set_error_callback(error_callback_t callback) -> void override;

	private:
		// =====================================================================
		// startable_base CRTP interface
		// =====================================================================

		/*!
		 * \brief Returns the component name for error messages.
		 * \return The component name string view.
		 */
		[[nodiscard]] static constexpr auto component_name() noexcept -> std::string_view
		{
			return "Client";
		}

		/*!
		 * \brief Called after stop operation completes.
		 * Invokes the disconnected callback.
		 */
		auto on_stopped() -> void;

		// =====================================================================
		// Internal Implementation Methods
		// =====================================================================

		/*!
		 * \brief TCP-specific implementation of client start.
		 * \param host The remote hostname or IP address.
		 * \param port The remote port number to connect.
		 * \return Result<void> - Success if client started, or error.
		 */
		auto do_start_impl(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief TCP-specific implementation of client stop.
		 * \return Result<void> - Success if client stopped, or error.
		 */
		auto do_stop_impl() -> VoidResult;

		/*!
		 * \brief TCP-specific implementation of data send.
		 * \param data The buffer to send (moved for efficiency).
		 * \return Result<void> - Success if data queued for send, or error.
		 */
		auto do_send_impl(std::vector<uint8_t>&& data) -> VoidResult;

		// =====================================================================
		// Internal Callback Helpers
		// =====================================================================

		/*!
		 * \brief Sets the connected state.
		 * \param connected The new connection state.
		 */
		auto set_connected(bool connected) -> void;

		/*!
		 * \brief Invokes the receive callback.
		 */
		auto invoke_receive_callback(const std::vector<uint8_t>& data) -> void;

		/*!
		 * \brief Invokes the connected callback.
		 */
		auto invoke_connected_callback() -> void;

		/*!
		 * \brief Invokes the disconnected callback.
		 */
		auto invoke_disconnected_callback() -> void;

		/*!
		 * \brief Invokes the error callback.
		 */
		auto invoke_error_callback(std::error_code ec) -> void;

		// =====================================================================
		// Internal Connection Handlers
		// =====================================================================

		/*!
		 * \brief Internally attempts to resolve and connect to the remote \p
		 * host:\p port.
		 */
		auto do_connect(std::string_view host, unsigned short port) -> void;

		/*!
		 * \brief Callback invoked upon completion of an async connect.
		 * \param ec The \c std::error_code indicating success/failure.
		 */
		auto on_connect(std::error_code ec) -> void;

		/*!
		 * \brief Callback for receiving data from the \c tcp_socket.
		 * \param data A span view of bytes that has arrived.
		 *
		 * ### Zero-Copy Performance
		 * The span provides a non-owning view directly into the socket's
		 * internal read buffer, avoiding per-read vector allocations.
		 *
		 * ### Lifetime Contract
		 * - The span is valid **only** until this callback returns.
		 * - Data is copied into a vector only when invoking external
		 *   receive_callback_ to maintain API compatibility.
		 *
		 * By default, logs the size of received data. To fully handle incoming
		 * messages, one could parse, decompress, decrypt, etc.
		 */
		auto on_receive(std::span<const uint8_t> data) -> void;

		/*!
		 * \brief Callback for handling socket errors from \c tcp_socket.
		 * \param ec The \c std::error_code describing the error.
		 *
		 * By default, logs the error message and \c stop_client().
		 */
		auto on_error(std::error_code ec) -> void;

		/*!
		 * \brief Handles connection failure during async resolve or connect.
		 * \param ec The error code from the failed operation.
		 *
		 * Cleans up resources and signals stop to prevent hangs in destructor.
		 */
		auto on_connection_failed(std::error_code ec) -> void;

	private:
		//! \brief Callback index type alias for clarity
		using callback_index = tcp_client_callback;

		//! \brief Callback manager type for this client
		using callbacks_t = utils::callback_manager<
			receive_callback_t,
			connected_callback_t,
			disconnected_callback_t,
			error_callback_t
		>;

		// =====================================================================
		// Member Variables
		// =====================================================================

		std::string client_id_;              /*!< Client identifier. */
		// Note: lifecycle_ and stop_initiated_ are managed by startable_base
		callbacks_t callbacks_;              /*!< Callback manager. */
		std::atomic<bool> is_connected_{false}; /*!< Connection state. */

		//! \brief Connection observer for unified event handling (IProtocolClient interface)
		std::shared_ptr<interfaces::connection_observer> observer_;

		std::shared_ptr<asio::io_context>
			io_context_; /*!< I/O context for async operations. */
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
			work_guard_; /*!< Keeps io_context running. */
		std::future<void>
			io_context_future_; /*!< Future for the io_context run task. */

		auto get_socket() const -> std::shared_ptr<internal::tcp_socket>;

		mutable std::mutex socket_mutex_; /*!< Protects socket_ from data races. */
		std::shared_ptr<internal::tcp_socket>
			socket_;   /*!< The \c tcp_socket wrapper once connected. */

		/*!
		 * \brief Pending connection resources that need explicit cleanup.
		 * These are stored as members to allow cancellation during stop_client(),
		 * preventing heap corruption when io_context is destroyed with pending
		 * async operations.
		 */
		mutable std::mutex pending_mutex_; /*!< Protects pending connection state. */
		std::shared_ptr<asio::ip::tcp::resolver> pending_resolver_;
		std::shared_ptr<asio::ip::tcp::socket> pending_socket_;
	};

} // namespace kcenon::network::core
