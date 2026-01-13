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

#include <kcenon/network/config/feature_flags.h>

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <asio.hpp>

#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/core/network_context.h"
#include "kcenon/network/core/messaging_server_base.h"
#include "kcenon/network/integration/thread_integration.h"
#include "kcenon/network/integration/io_context_thread_manager.h"

// Optional monitoring support via common_system
#if KCENON_WITH_COMMON_SYSTEM
	#include <kcenon/common/interfaces/monitoring_interface.h>
#endif // KCENON_WITH_COMMON_SYSTEM

namespace kcenon::network::core
{
} // namespace kcenon::network::core

namespace kcenon::network::session {
	class messaging_session;
}

namespace kcenon::network::core {

	/*!
	 * \class messaging_server
	 * \brief A server class that manages incoming TCP connections, creating
	 *        \c messaging_session instances for each accepted socket.
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
		: public messaging_server_base<messaging_server>
	{
	public:
		//! \brief Allow base class to access protected methods
		friend class messaging_server_base<messaging_server>;
		/*!
		 * \brief Constructs a \c messaging_server with an optional string \p
		 * server_id.
		 * \param server_id A descriptive identifier for this server instance
		 * (e.g., "main_server").
		 */
		explicit messaging_server(const std::string& server_id);

		/*!
		 * \brief Destructor. If the server is still running, \c stop_server()
		 * is invoked (handled by base class).
		 */
		~messaging_server() noexcept override;

#if KCENON_WITH_COMMON_SYSTEM
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
		auto set_monitor(kcenon::common::interfaces::IMonitor* monitor) -> void;

		/*!
		 * \brief Get the current monitor
		 * \return Pointer to monitor or nullptr if not set
		 */
		auto get_monitor() const -> kcenon::common::interfaces::IMonitor*;
#endif // KCENON_WITH_COMMON_SYSTEM

	protected:
		/*!
		 * \brief TCP-specific implementation of server start.
		 * \param port The TCP port to bind and listen on.
		 * \return Result<void> - Success if server started, or error with code:
		 *         - error_codes::network_system::bind_failed if port binding failed
		 *         - error_codes::common_errors::internal_error for other failures
		 *
		 * Called by base class start_server() after common validation.
		 * Creates io_context, acceptor, and starts accepting connections.
		 */
		auto do_start(unsigned short port) -> VoidResult;

		/*!
		 * \brief TCP-specific implementation of server stop.
		 * \return Result<void> - Success if server stopped, or error with code:
		 *         - error_codes::common_errors::internal_error for failures
		 *
		 * Called by base class stop_server() after common cleanup.
		 * Closes acceptor, stops sessions, and releases resources.
		 */
		auto do_stop() -> VoidResult;

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

		/*!
		 * \brief Removes stopped sessions from the sessions vector.
		 *
		 * This method iterates through all sessions and removes those that have
		 * been stopped. Should be called periodically to prevent memory leaks
		 * from accumulating dead sessions.
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
		// TCP protocol-specific members (base class provides server_id_, is_running_,
		// stop_initiated_, stop_promise_, stop_future_, and callbacks)

		std::shared_ptr<asio::io_context>
			io_context_;	/*!< The I/O context for async ops. */
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
			work_guard_; /*!< Keeps io_context running. */
		std::unique_ptr<asio::ip::tcp::acceptor>
			acceptor_;		/*!< Acceptor to listen for new connections. */
		std::future<void>
			io_context_future_; /*!< Future for the io_context run task. */

		/*!
		 * \brief Holds all active sessions. When \c stop_server() is invoked,
		 *        each session's \c stop_session() is called and they are
		 * cleared.
		 */
		std::vector<std::shared_ptr<kcenon::network::session::messaging_session>> sessions_;

		/*!
		 * \brief Mutex protecting access to sessions_ vector.
		 */
		std::mutex sessions_mutex_;

		/*!
		 * \brief Mutex protecting access to acceptor_ for thread-safe operations.
		 */
		mutable std::mutex acceptor_mutex_;

		/*!
		 * \brief Timer for periodic cleanup of stopped sessions.
		 */
		std::unique_ptr<asio::steady_timer> cleanup_timer_;

#if KCENON_WITH_COMMON_SYSTEM
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
#endif // KCENON_WITH_COMMON_SYSTEM
	};

} // namespace kcenon::network::core