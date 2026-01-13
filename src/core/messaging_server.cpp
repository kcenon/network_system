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

#include <kcenon/network/config/feature_flags.h>

#include "kcenon/network/core/messaging_server.h"
#include "kcenon/network/session/messaging_session.h"
#include "kcenon/network/integration/logger_integration.h"
#include "kcenon/network/integration/io_context_thread_manager.h"
#include "kcenon/network/metrics/network_metrics.h"

namespace kcenon::network::core
{

	using tcp = asio::ip::tcp;

	messaging_server::messaging_server(const std::string& server_id)
		: messaging_server_base<messaging_server>(server_id)
	{
	}

	messaging_server::~messaging_server() noexcept
	{
		// Base class destructor will call stop_server() if still running.
		// Additional cleanup is handled here for resources not managed by base.
		try
		{
			// Ensure all resources are cleaned up even if stop_server() returned early
			// This handles the case where start_server() failed and is_running() is false
			// but resources were partially created
			// Lock to prevent race with do_accept() accessing the acceptor
			{
				std::lock_guard<std::mutex> lock(acceptor_mutex_);
				if (acceptor_)
				{
					asio::error_code ec;
					acceptor_->cancel(ec);
					if (acceptor_->is_open())
					{
						acceptor_->close(ec);
					}
					acceptor_.reset();
				}
			}

			if (cleanup_timer_)
			{
				cleanup_timer_->cancel();
				cleanup_timer_.reset();
			}

			if (work_guard_)
			{
				work_guard_.reset();
			}

			if (io_context_)
			{
				io_context_->stop();
				io_context_.reset();
			}
		}
		catch (...)
		{
			// Destructor must not throw - swallow all exceptions
		}
	}

	auto messaging_server::do_start(unsigned short port) -> VoidResult
	{
		// Base class has already checked is_running() and set state.
		// This method handles TCP-specific initialization.
		try
		{
			// Create io_context and acceptor
			io_context_ = std::make_shared<asio::io_context>();
			// Create work guard to keep io_context running
			work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
				asio::make_work_guard(*io_context_)
			);
			acceptor_ = std::make_unique<tcp::acceptor>(
				*io_context_, tcp::endpoint(tcp::v4(), port));

			// Create cleanup timer
			cleanup_timer_ = std::make_unique<asio::steady_timer>(*io_context_);

			// Begin accepting connections
			do_accept();

			// Start periodic cleanup timer
			start_cleanup_timer();

			// Use io_context_thread_manager for unified thread management
			io_context_future_ = integration::io_context_thread_manager::instance()
				.run_io_context(io_context_, "messaging_server:" + server_id());

			NETWORK_LOG_INFO("[messaging_server] Started listening on port " + std::to_string(port));
			return ok();
		}
		catch (const std::system_error& e)
		{
			// Cleanup partially created resources to prevent memory corruption
			// Resources may have been created before the exception was thrown
			acceptor_.reset();
			cleanup_timer_.reset();
			work_guard_.reset();
			io_context_.reset();

			// Check for specific error codes (ASIO uses asio::error category)
			if (e.code() == asio::error::address_in_use ||
			    e.code() == std::errc::address_in_use)
			{
				return error_void(
					error_codes::network_system::bind_failed,
					"Failed to bind to port: address already in use",
					"messaging_server::do_start",
					"Port: " + std::to_string(port)
				);
			}
			else if (e.code() == asio::error::access_denied ||
			         e.code() == std::errc::permission_denied)
			{
				return error_void(
					error_codes::network_system::bind_failed,
					"Failed to bind to port: permission denied",
					"messaging_server::do_start",
					"Port: " + std::to_string(port)
				);
			}

			return error_void(
				error_codes::common_errors::internal_error,
				"Failed to start server: " + std::string(e.what()),
				"messaging_server::do_start",
				"Port: " + std::to_string(port)
			);
		}
		catch (const std::exception& e)
		{
			// Cleanup partially created resources to prevent memory corruption
			acceptor_.reset();
			cleanup_timer_.reset();
			work_guard_.reset();
			io_context_.reset();

			return error_void(
				error_codes::common_errors::internal_error,
				"Failed to start server: " + std::string(e.what()),
				"messaging_server::do_start",
				"Port: " + std::to_string(port)
			);
		}
	}

	auto messaging_server::do_stop() -> VoidResult
	{
		// Base class has already checked is_running() and set state.
		// This method handles TCP-specific cleanup.
		try
		{
			// Step 1: Cancel and close the acceptor to stop accepting new connections
			// Lock to prevent race with do_accept() accessing the acceptor
			{
				std::lock_guard<std::mutex> lock(acceptor_mutex_);
				if (acceptor_)
				{
					asio::error_code ec;
					// Cancel pending async_accept operations to prevent memory leaks
					acceptor_->cancel(ec);
					if (acceptor_->is_open())
					{
						acceptor_->close(ec);
					}
				}
			}

			// Cancel cleanup timer
			if (cleanup_timer_)
			{
				cleanup_timer_->cancel();
			}

			// Step 2: Stop all active sessions (close sockets, stop reading)
			// NOTE: Do NOT clear sessions here - they may still be referenced by
			// pending async callbacks. We must wait for io_context to finish first.
			{
				std::lock_guard<std::mutex> lock(sessions_mutex_);
				for (auto& sess : sessions_)
				{
					if (sess)
					{
						sess->stop_session();
					}
				}
			}

			// Step 3: Release work guard to allow io_context to finish
			if (work_guard_)
			{
				work_guard_.reset();
			}

			// Step 4: Stop io_context via unified manager
			if (io_context_)
			{
				integration::io_context_thread_manager::instance().stop_io_context(io_context_);
			}

			// Step 5: Wait for io_context task to complete
			// This ensures all pending async callbacks have finished executing
			if (io_context_future_.valid())
			{
				io_context_future_.wait();
			}

			// Step 5.5: Run remaining handlers to clean up pending async operations.
			// When io_context::stop() is called, pending handlers may not have
			// executed yet. Running poll() ensures all handlers are invoked and
			// their captured resources (resolver/socket) are properly destroyed
			// before io_context is destroyed, preventing heap corruption.
			if (io_context_)
			{
				try
				{
					io_context_->restart();
					io_context_->poll();
				}
				catch (...)
				{
					// Ignore exceptions during cleanup
				}
			}

			// Step 6: NOW it's safe to clear sessions - all async operations are done
			{
				std::lock_guard<std::mutex> lock(sessions_mutex_);
				sessions_.clear();
			}

			// Step 7: Release resources explicitly to ensure cleanup
			acceptor_.reset();
			cleanup_timer_.reset();
			io_context_.reset();

			NETWORK_LOG_INFO("[messaging_server] Stopped.");
			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common_errors::internal_error,
				"Failed to stop server: " + std::string(e.what()),
				"messaging_server::do_stop",
				"Server ID: " + server_id()
			);
		}
	}

	auto messaging_server::do_accept() -> void
	{
		// Lock to prevent race with do_stop() closing the acceptor
		std::lock_guard<std::mutex> lock(acceptor_mutex_);

		// Early return if server is stopping or acceptor is invalid
		if (!is_running() || !acceptor_ || !acceptor_->is_open())
		{
			return;
		}

		auto self = shared_from_this();
		acceptor_->async_accept(
			[this, self](std::error_code ec, tcp::socket sock)
			{ on_accept(ec, std::move(sock)); });
	}

	auto messaging_server::on_accept(std::error_code ec, tcp::socket socket)
		-> void
	{
		if (!is_running())
		{
			return;
		}
		if (ec)
		{
			NETWORK_LOG_ERROR("[messaging_server] Accept error: " + ec.message());
#if KCENON_WITH_COMMON_SYSTEM
			// Record connection error
			connection_errors_.fetch_add(1, std::memory_order_relaxed);
			if (monitor_) {
				monitor_->record_metric("connection_errors", static_cast<double>(connection_errors_.load()));
			}
#endif
			// Report metrics
			metrics::metric_reporter::report_connection_failed(ec.message());
			return;
		}

		// Report successful connection
		metrics::metric_reporter::report_connection_accepted();

		// Cleanup dead sessions before adding new one
		cleanup_dead_sessions();

		// Create a new messaging_session
		auto new_session = std::make_shared<kcenon::network::session::messaging_session>(
			std::move(socket), server_id());

		// Set up session callbacks to forward to server callbacks
		// Use base class getters to avoid direct member access
		auto self = shared_from_this();

		// Get copies of callbacks via thread-safe getters from base class
		auto local_receive_callback = get_receive_callback();
		auto local_disconnection_callback = get_disconnection_callback();
		auto local_error_callback = get_error_callback();

		// Set up callbacks without holding any locks to avoid lock-order-inversion
		// Lambdas use captured local callbacks directly to avoid locking callback_mutex_
		if (local_receive_callback)
		{
			new_session->set_receive_callback(
				[self, new_session, local_receive_callback](const std::vector<uint8_t>& data)
				{
					// Call the captured callback directly without locking to prevent deadlock
					local_receive_callback(new_session, data);
				});
		}

		if (local_disconnection_callback)
		{
			new_session->set_disconnection_callback(
				[self, local_disconnection_callback](const std::string& session_id)
				{
					// Call the captured callback directly without locking to prevent deadlock
					local_disconnection_callback(session_id);
				});
		}

		if (local_error_callback)
		{
			new_session->set_error_callback(
				[self, new_session, local_error_callback](std::error_code ec)
				{
					// Call the captured callback directly without locking to prevent deadlock
					local_error_callback(new_session, ec);
				});
		}

		// Track it in our sessions_ vector (protected by mutex)
		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			sessions_.push_back(new_session);
		}

		// Start the session
		new_session->start_session();

		// Invoke connection callback via base class helper
		invoke_connection_callback(new_session);

#if KCENON_WITH_COMMON_SYSTEM
		// Record active connections metric
		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			if (monitor_) {
				monitor_->record_metric("active_connections", static_cast<double>(sessions_.size()));
			}
		}
#endif

		// Report active connections to metrics system
		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			metrics::metric_reporter::report_active_connections(sessions_.size());
		}

		// Accept next connection
		do_accept();
	}

	auto messaging_server::cleanup_dead_sessions() -> void
	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);

		// Remove sessions that have been stopped
		sessions_.erase(
			std::remove_if(sessions_.begin(), sessions_.end(),
				[](const auto& session) {
					return session && session->is_stopped();
				}),
			sessions_.end()
		);

		NETWORK_LOG_DEBUG("[messaging_server] Cleaned up dead sessions. Active: " +
			std::to_string(sessions_.size()));

#if KCENON_WITH_COMMON_SYSTEM
		// Update active connections metric after cleanup
		if (monitor_) {
			monitor_->record_metric("active_connections", static_cast<double>(sessions_.size()));
		}
#endif
	}

	auto messaging_server::start_cleanup_timer() -> void
	{
		if (!cleanup_timer_ || !is_running())
		{
			return;
		}

		// Schedule cleanup every 30 seconds
		cleanup_timer_->expires_after(std::chrono::seconds(30));

		auto self = shared_from_this();
		cleanup_timer_->async_wait(
			[this, self](const std::error_code& ec)
			{
				if (!ec && is_running())
				{
					cleanup_dead_sessions();
					start_cleanup_timer(); // Reschedule
				}
			}
		);
	}

#if KCENON_WITH_COMMON_SYSTEM
	auto messaging_server::set_monitor(kcenon::common::interfaces::IMonitor* monitor) -> void
	{
		monitor_ = monitor;
	}

	auto messaging_server::get_monitor() const -> kcenon::common::interfaces::IMonitor*
	{
		return monitor_;
	}
#endif

	// Callback setters are inherited from messaging_server_base

} // namespace kcenon::network::core