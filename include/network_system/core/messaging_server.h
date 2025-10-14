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

#include <memory>
#include <vector>
#include <optional>
#include <future>
#include <thread>
#include <atomic>
#include <string>

#include <asio.hpp>

#include "network_system/utils/result_types.h"

// Optional monitoring support via common_system
#ifdef BUILD_WITH_COMMON_SYSTEM
	#include <kcenon/common/interfaces/monitoring_interface.h>
#endif

namespace network_system::core
{
} // namespace network_system::core

namespace network_system::session {
	class messaging_session;
}

namespace network_system::core {

	/*!
	 * \class messaging_server
	 * \brief A server class that manages incoming TCP connections, creating
	 *        \c messaging_session instances for each accepted socket.
	 *
	 * ### Key Responsibilities
	 * - Maintains an \c asio::io_context and \c tcp::acceptor to listen on a
	 * specified port.
	 * - For each incoming connection, instantiates a \c messaging_session to
	 * handle the communication logic (compression, encryption, message parsing,
	 * etc.).
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
	 * // 1. Create a messaging_server:
	 * auto server = std::make_shared<network::messaging_server>("ServerID");
	 *
	 * // 2. Start listening on a port:
	 * server->start_server(5555);
	 *
	 * // 3. Wait for stop or run other tasks:
	 * //    ...
	 *
	 * // 4. Eventually stop the server:
	 * server->stop_server();
	 *
	 * // 5. (optional) If a separate thread is calling wait_for_stop():
	 * server->wait_for_stop();
	 * \endcode
	 */
	class messaging_server
		: public std::enable_shared_from_this<messaging_server>
	{
	public:
		/*!
		 * \brief Constructs a \c messaging_server with an optional string \p
		 * server_id.
		 * \param server_id A descriptive identifier for this server instance
		 * (e.g., "main_server").
		 */
		messaging_server(const std::string& server_id);

		/*!
		 * \brief Destructor. If the server is still running, \c stop_server()
		 * is invoked.
		 */
		~messaging_server();

		/*!
		 * \brief Begins listening on the specified TCP \p port, creates a
		 * background thread to run I/O operations, and starts accepting
		 * connections.
		 *
		 * \param port The TCP port to bind and listen on (e.g., 5555).
		 * \return Result<void> - Success if server started, or error with code:
		 *         - error_codes::network_system::server_already_running if already running
		 *         - error_codes::network_system::bind_failed if port binding failed
		 *         - error_codes::common::internal_error for other failures
		 *
		 * #### Behavior
		 * - If the server is already running (\c is_running_ is \c true), returns error.
		 * - Otherwise, an \c io_context and \c acceptor are created, \c
		 * do_accept() is invoked, and a new thread is spawned to run \c
		 * io_context->run().
		 *
		 * #### Example
		 * \code
		 * auto result = server->start_server(5555);
		 * if (!result) {
		 *     std::cerr << "Server start failed: " << result.error().message << "\n";
		 *     return -1;
		 * }
		 * \endcode
		 */
		auto start_server(unsigned short port) -> VoidResult;

		/*!
		 * \brief Stops the server, closing the acceptor and all active
		 * sessions, then stops the \c io_context and joins the internal thread.
		 *
		 * \return Result<void> - Success if server stopped, or error with code:
		 *         - error_codes::network_system::server_not_started if not running
		 *         - error_codes::common::internal_error for other failures
		 *
		 * #### Steps:
		 * 1. Set \c is_running_ to \c false.
		 * 2. Close the \c acceptor if open.
		 * 3. Iterate through all active sessions, calling \c stop_session().
		 * 4. \c io_context->stop() to halt asynchronous operations.
		 * 5. Join the background thread if it's joinable.
		 * 6. Fulfill the \c stop_promise_ so that \c wait_for_stop() can
		 * return.
		 *
		 * #### Example
		 * \code
		 * auto result = server->stop_server();
		 * if (!result) {
		 *     std::cerr << "Server stop failed: " << result.error().message << "\n";
		 * }
		 * \endcode
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
		 *
		 * When set, the server will record metrics such as:
		 * - messages_received: Total count of received messages
		 * - messages_sent: Total count of sent messages
		 * - active_connections: Current number of active sessions
		 * - connection_errors: Count of connection failures
		 */
		auto set_monitor(common::interfaces::IMonitor* monitor) -> void;

		/*!
		 * \brief Get the current monitor
		 * \return Pointer to monitor or nullptr if not set
		 */
		auto get_monitor() const -> common::interfaces::IMonitor*;
#endif

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
		 * a \c messaging_session is created and stored, then \c do_accept() is
		 * invoked again to accept the next connection.
		 */
		auto on_accept(std::error_code ec, asio::ip::tcp::socket socket)
			-> void;

	private:
		std::string
			server_id_;		/*!< Name or identifier for this server instance. */

		std::unique_ptr<asio::io_context>
			io_context_;	/*!< The I/O context for async ops. */
	std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>> work_guard_; /*!< Keeps io_context running. */
		std::unique_ptr<asio::ip::tcp::acceptor>
			acceptor_;		/*!< Acceptor to listen for new connections. */
		std::unique_ptr<std::thread>
			server_thread_; /*!< Thread that runs \c io_context_->run(). */

		std::optional<std::promise<void>>
			stop_promise_;	/*!< Used to signal \c wait_for_stop(). */
		std::future<void>
			stop_future_;	/*!< Future that \c wait_for_stop() waits on. */

		std::atomic<bool> is_running_{
			false
		}; /*!< Indicates whether the server is active. */

		/*!
		 * \brief Holds all active sessions. When \c stop_server() is invoked,
		 *        each session's \c stop_session() is called and they are
		 * cleared.
		 */
		std::vector<std::shared_ptr<network_system::session::messaging_session>> sessions_;

#ifdef BUILD_WITH_COMMON_SYSTEM
		/*!
		 * \brief Optional monitoring interface for metrics collection
		 */
		common::interfaces::IMonitor* monitor_ = nullptr;

		/*!
		 * \brief Atomic counters for metrics
		 */
		std::atomic<uint64_t> messages_received_{0};
		std::atomic<uint64_t> messages_sent_{0};
		std::atomic<uint64_t> connection_errors_{0};
#endif
	};

} // namespace network_system::core