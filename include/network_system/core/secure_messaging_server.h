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
#include <thread>
#include <vector>

#include <asio.hpp>
#include <asio/ssl.hpp>

#include "network_system/utils/result_types.h"

// Optional monitoring support via common_system
#ifdef BUILD_WITH_COMMON_SYSTEM
	#include <kcenon/common/interfaces/monitoring_interface.h>
#endif

namespace network_system::session {
	class secure_session;
}

namespace network_system::core
{

	/*!
	 * \class secure_messaging_server
	 * \brief A secure server class that manages incoming TLS/SSL encrypted TCP connections,
	 *        creating \c secure_session instances for each accepted socket.
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe.
	 * - Internal state (is_running_, sessions_) is protected by atomics and mutex.
	 * - Background thread runs io_context.run() independently.
	 * - Multiple sessions can be active concurrently without blocking each other.
	 * - Sessions vector is protected by sessions_mutex_ for thread-safe cleanup.
	 *
	 * ### Key Responsibilities
	 * - Maintains an \c asio::io_context and \c tcp::acceptor to listen on a
	 * specified port.
	 * - Maintains an \c asio::ssl::context for TLS/SSL encryption.
	 * - For each incoming connection, performs SSL handshake and instantiates a
	 * \c secure_session to handle encrypted communication.
	 * - Allows external control via \c start_server(), \c stop_server(), and \c
	 * wait_for_stop().
	 *
	 * ### Thread Model
	 * - A single background thread calls \c io_context.run() to process I/O
	 * events.
	 * - Each accepted connection runs asynchronously; thus multiple sessions
	 * can be active concurrently without blocking each other.
	 *
	 * ### Usage Example
	 * \code
	 * auto server = std::make_shared<secure_messaging_server>(
	 *     "SecureServerID", "server.crt", "server.key");
	 *
	 * server->start_server(5555);
	 *
	 * // ... do work ...
	 *
	 * server->stop_server();
	 * \endcode
	 */
	class secure_messaging_server
		: public std::enable_shared_from_this<secure_messaging_server>
	{
	public:
		/*!
		 * \brief Constructs a \c secure_messaging_server with SSL/TLS support.
		 * \param server_id A descriptive identifier for this server instance
		 * \param cert_file Path to the SSL certificate file (.crt or .pem)
		 * \param key_file Path to the SSL private key file (.key or .pem)
		 */
		secure_messaging_server(const std::string& server_id,
								const std::string& cert_file,
								const std::string& key_file);

		/*!
		 * \brief Destructor. If the server is still running, \c stop_server()
		 * is invoked.
		 */
		~secure_messaging_server() noexcept;

		/*!
		 * \brief Begins listening on the specified TCP \p port, creates a
		 * background thread to run I/O operations, and starts accepting
		 * connections.
		 *
		 * \param port The TCP port to bind and listen on (e.g., 5555).
		 * \return Result<void> - Success if server started, or error with code:
		 *         - error_codes::network_system::server_already_running if already running
		 *         - error_codes::network_system::bind_failed if port binding failed
		 *         - error_codes::common_errors::internal_error for other failures
		 */
		auto start_server(unsigned short port) -> VoidResult;

		/*!
		 * \brief Stops the server, closing the acceptor and all active
		 * sessions, then stops the \c io_context and joins the internal thread.
		 *
		 * \return Result<void> - Success if server stopped, or error with code:
		 *         - error_codes::network_system::server_not_started if not running
		 *         - error_codes::common_errors::internal_error for other failures
		 */
		auto stop_server() -> VoidResult;

		/*!
		 * \brief Blocks until \c stop_server() is called, allowing the caller
		 *        to wait for a graceful shutdown in another context.
		 */
		auto wait_for_stop() -> void;

#ifdef BUILD_WITH_COMMON_SYSTEM
		/*!
		 * \brief Set a monitoring interface for metrics collection
		 * \param monitor Pointer to IMonitor implementation (not owned)
		 */
		auto set_monitor(kcenon::common::interfaces::IMonitor* monitor) -> void;

		/*!
		 * \brief Get the current monitor
		 * \return Pointer to monitor or nullptr if not set
		 */
		auto get_monitor() const -> kcenon::common::interfaces::IMonitor*;
#endif

		/*!
		 * \brief Sets the callback for new client connections.
		 * \param callback Function called when a client connects.
		 */
		auto set_connection_callback(
			std::function<void(std::shared_ptr<network_system::session::secure_session>)> callback)
			-> void;

		/*!
		 * \brief Sets the callback for client disconnections.
		 * \param callback Function called when a client disconnects.
		 */
		auto set_disconnection_callback(
			std::function<void(const std::string&)> callback) -> void;

		/*!
		 * \brief Sets the callback for received messages.
		 * \param callback Function called when data is received from a client.
		 */
		auto set_receive_callback(
			std::function<void(std::shared_ptr<network_system::session::secure_session>,
			                   const std::vector<uint8_t>&)> callback) -> void;

		/*!
		 * \brief Sets the callback for session errors.
		 * \param callback Function called when an error occurs on a session.
		 */
		auto set_error_callback(
			std::function<void(std::shared_ptr<network_system::session::secure_session>,
			                   std::error_code)> callback) -> void;

	private:
		/*!
		 * \brief Initiates an asynchronous accept operation (\c async_accept).
		 *
		 * On success, \c on_accept() is invoked with a newly accepted \c
		 * tcp::socket.
		 */
		auto do_accept() -> void;

		/*!
		 * \brief Handler called when an asynchronous accept finishes.
		 *
		 * \param ec     The \c std::error_code indicating success or error.
		 * \param socket The newly accepted \c tcp::socket.
		 *
		 * If \c ec is successful and \c is_running_ is still \c true,
		 * a \c secure_session is created and stored, then \c do_accept() is
		 * invoked again to accept the next connection.
		 */
		auto on_accept(std::error_code ec, asio::ip::tcp::socket socket)
			-> void;

		/*!
		 * \brief Removes stopped sessions from the sessions vector.
		 *
		 * Thread-safe: Protected by sessions_mutex_.
		 */
		auto cleanup_dead_sessions() -> void;

		/*!
		 * \brief Starts a periodic timer that triggers session cleanup.
		 *
		 * The cleanup timer runs every 30 seconds and calls cleanup_dead_sessions()
		 * to remove stopped sessions from the vector.
		 */
		auto start_cleanup_timer() -> void;

	private:
		std::string
			server_id_;		/*!< Name or identifier for this server instance. */

		std::unique_ptr<asio::io_context>
			io_context_;	/*!< The I/O context for async ops. */
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
			work_guard_;	/*!< Keeps io_context running. */
		std::unique_ptr<asio::ip::tcp::acceptor>
			acceptor_;		/*!< Acceptor to listen for new connections. */
		std::unique_ptr<std::thread>
			server_thread_; /*!< Thread that runs \c io_context_->run(). */

		std::unique_ptr<asio::ssl::context>
			ssl_context_;	/*!< SSL context for encryption. */

		std::optional<std::promise<void>>
			stop_promise_;	/*!< Used to signal \c wait_for_stop(). */
		std::future<void>
			stop_future_;	/*!< Future that \c wait_for_stop() waits on. */

		std::atomic<bool> is_running_{
			false
		}; /*!< Indicates whether the server is active. */

		/*!
		 * \brief Holds all active secure sessions.
		 */
		std::vector<std::shared_ptr<network_system::session::secure_session>> sessions_;

		/*!
		 * \brief Mutex protecting access to sessions_ vector.
		 */
		std::mutex sessions_mutex_;

		/*!
		 * \brief Timer for periodic cleanup of stopped sessions.
		 */
		std::unique_ptr<asio::steady_timer> cleanup_timer_;

#ifdef BUILD_WITH_COMMON_SYSTEM
		/*!
		 * \brief Optional monitoring interface for metrics collection
		 */
		kcenon::common::interfaces::IMonitor* monitor_ = nullptr;

		/*!
		 * \brief Atomic counters for metrics
		 */
		std::atomic<uint64_t> messages_received_{0};
		std::atomic<uint64_t> messages_sent_{0};
		std::atomic<uint64_t> connection_errors_{0};
#endif

		/*!
		 * \brief Callbacks for server events
		 */
		std::function<void(std::shared_ptr<network_system::session::secure_session>)>
			connection_callback_;
		std::function<void(const std::string&)>
			disconnection_callback_;
		std::function<void(std::shared_ptr<network_system::session::secure_session>,
		                   const std::vector<uint8_t>&)>
			receive_callback_;
		std::function<void(std::shared_ptr<network_system::session::secure_session>,
		                   std::error_code)>
			error_callback_;

		/*!
		 * \brief Mutex protecting callback access
		 */
		mutable std::mutex callback_mutex_;
	};

} // namespace network_system::core
