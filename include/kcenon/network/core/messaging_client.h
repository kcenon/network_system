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

#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

#include <asio.hpp>

#include "kcenon/network/core/messaging_client_base.h"
#include "kcenon/network/internal/tcp_socket.h"
#include "kcenon/network/internal/pipeline.h"
#include "kcenon/network/integration/io_context_thread_manager.h"

// Use nested namespace definition in C++17
namespace kcenon::network::core
{

	/*!
	 * \class messaging_client
	 * \brief A basic TCP client that connects to a remote host, sends/receives
	 * data using asynchronous operations, and can apply a pipeline for
	 * transformations.
	 *
	 * This class inherits from messaging_client_base using the CRTP pattern,
	 * which provides common lifecycle management and callback handling.
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
	 * control lifecycle (inherited from messaging_client_base).
	 */
	class messaging_client
		: public messaging_client_base<messaging_client>
	{
	public:
		//! \brief Allow base class to access protected methods
		friend class messaging_client_base<messaging_client>;
		/*!
		 * \brief Constructs a client with a given \p client_id used for logging
		 * or identification.
		 * \param client_id A string identifier for this client instance.
		 */
		explicit messaging_client(std::string_view client_id);

		/*!
		 * \brief Destructor; automatically calls \c stop_client() if the client
		 * is still running (handled by base class).
		 */
		~messaging_client() noexcept override = default;

	protected:
		/*!
		 * \brief TCP-specific implementation of client start.
		 * \param host The remote hostname or IP address.
		 * \param port The remote port number to connect.
		 * \return Result<void> - Success if client started, or error with code:
		 *         - error_codes::common_errors::internal_error for other failures
		 *
		 * Called by base class start_client() after common validation.
		 * Creates io_context, starts worker thread, and initiates async connect.
		 */
		auto do_start(std::string_view host, unsigned short port) -> VoidResult;

		/*!
		 * \brief TCP-specific implementation of client stop.
		 * \return Result<void> - Success if client stopped, or error with code:
		 *         - error_codes::common_errors::internal_error for failures
		 *
		 * Called by base class stop_client() after common cleanup.
		 * Closes socket, stops io_context, and releases resources.
		 */
		auto do_stop() -> VoidResult;

		/*!
		 * \brief TCP-specific implementation of data send.
		 * \param data The buffer to send (moved for efficiency).
		 * \return Result<void> - Success if data queued for send, or error with code:
		 *         - error_codes::network_system::send_failed for other failures
		 *
		 * Called by base class send_packet() after common validation.
		 * Optionally compresses/encrypts data before sending via pipeline.
		 */
		auto do_send(std::vector<uint8_t>&& data) -> VoidResult;

	private:
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
		// TCP protocol-specific members (base class provides client_id_, is_running_,
		// is_connected_, stop_initiated_, stop_promise_, stop_future_, and callbacks)

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

		internal::pipeline
			pipeline_; /*!< Pipeline for optional compression/encryption. */
		bool compress_mode_{
			false
		}; /*!< If true, compress data before sending. */
		bool encrypt_mode_{
			false
		}; /*!< If true, encrypt data before sending. */
	};

} // namespace kcenon::network::core
