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

#include "network_system/core/messaging_server.h"
#include "network_system/session/messaging_session.h"
#include "network_system/integration/logger_integration.h"

namespace network_system::core
{

	using tcp = asio::ip::tcp;

	messaging_server::messaging_server(const std::string& server_id)
		: server_id_(server_id)
	{
	}

	messaging_server::~messaging_server() noexcept
	{
		try
		{
			// Ignore the return value in destructor to avoid throwing
			(void)stop_server();
		}
		catch (...)
		{
			// Destructor must not throw - swallow all exceptions
		}
	}

	auto messaging_server::start_server(unsigned short port) -> VoidResult
	{
		// If already running, return error
		if (is_running_.load())
		{
			return error_void(
				error_codes::network_system::server_already_running,
				"Server is already running",
				"messaging_server::start_server",
				"Server ID: " + server_id_
			);
		}

		try
		{
			// Create io_context and acceptor
			io_context_ = std::make_unique<asio::io_context>();
			// Create work guard to keep io_context running
			work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
				asio::make_work_guard(*io_context_)
			);
			acceptor_ = std::make_unique<tcp::acceptor>(
				*io_context_, tcp::endpoint(tcp::v4(), port));

			// Create cleanup timer
			cleanup_timer_ = std::make_unique<asio::steady_timer>(*io_context_);

			is_running_.store(true);

			// Prepare promise/future for wait_for_stop()
			stop_promise_.emplace();
			stop_future_ = stop_promise_->get_future();

			// Begin accepting connections
			do_accept();

			// Start periodic cleanup timer
			start_cleanup_timer();

			// Start thread to run the io_context
			server_thread_ = std::make_unique<std::thread>(
				[this]()
				{
					try
					{
						io_context_->run();
					}
					catch (...)
					{
						// Optionally handle any uncaught exceptions
					}
				});

			NETWORK_LOG_INFO("[messaging_server] Started listening on port " + std::to_string(port));
			return ok();
		}
		catch (const std::system_error& e)
		{
			is_running_.store(false);

			// Check for specific error codes (ASIO uses asio::error category)
			if (e.code() == asio::error::address_in_use ||
			    e.code() == std::errc::address_in_use)
			{
				return error_void(
					error_codes::network_system::bind_failed,
					"Failed to bind to port: address already in use",
					"messaging_server::start_server",
					"Port: " + std::to_string(port)
				);
			}
			else if (e.code() == asio::error::access_denied ||
			         e.code() == std::errc::permission_denied)
			{
				return error_void(
					error_codes::network_system::bind_failed,
					"Failed to bind to port: permission denied",
					"messaging_server::start_server",
					"Port: " + std::to_string(port)
				);
			}

			return error_void(
				error_codes::common::internal_error,
				"Failed to start server: " + std::string(e.what()),
				"messaging_server::start_server",
				"Port: " + std::to_string(port)
			);
		}
		catch (const std::exception& e)
		{
			is_running_.store(false);
			return error_void(
				error_codes::common::internal_error,
				"Failed to start server: " + std::string(e.what()),
				"messaging_server::start_server",
				"Port: " + std::to_string(port)
			);
		}
	}

	auto messaging_server::stop_server() -> VoidResult
	{
		if (!is_running_.load())
		{
			return error_void(
				error_codes::network_system::server_not_started,
				"Server is not running",
				"messaging_server::stop_server",
				"Server ID: " + server_id_
			);
		}

		try
		{
			is_running_.store(false);

			// Step 1: Cancel and close the acceptor to stop accepting new connections
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

			// Cancel cleanup timer
			if (cleanup_timer_)
			{
				cleanup_timer_->cancel();
			}

			// Step 2: Stop all active sessions
			{
				std::lock_guard<std::mutex> lock(sessions_mutex_);
				for (auto& sess : sessions_)
				{
					if (sess)
					{
						sess->stop_session();
					}
				}
				sessions_.clear();
			}

			// Step 3: Release work guard to allow io_context to finish
			if (work_guard_)
			{
				work_guard_.reset();
			}

			// Step 4: Stop io_context (this cancels all remaining pending operations)
			if (io_context_)
			{
				io_context_->stop();
			}

			// Step 4: Join the io_context thread
			if (server_thread_ && server_thread_->joinable())
			{
				server_thread_->join();
			}

			// Step 5: Release resources explicitly to ensure cleanup
			acceptor_.reset();
			cleanup_timer_.reset();
			server_thread_.reset();
			io_context_.reset();

			// Step 6: Signal the promise for wait_for_stop()
			if (stop_promise_.has_value())
			{
				try
				{
					stop_promise_->set_value();
				}
				catch (const std::future_error&)
				{
					// Promise already satisfied - this is OK during shutdown
				}
				stop_promise_.reset();
			}

			NETWORK_LOG_INFO("[messaging_server] Stopped.");
			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common::internal_error,
				"Failed to stop server: " + std::string(e.what()),
				"messaging_server::stop_server",
				"Server ID: " + server_id_
			);
		}
	}

	auto messaging_server::wait_for_stop() -> void
	{
		if (stop_future_.valid())
		{
			stop_future_.wait();
		}
	}

	auto messaging_server::do_accept() -> void
	{
		auto self = shared_from_this();
		acceptor_->async_accept(
			[this, self](std::error_code ec, tcp::socket sock)
			{ on_accept(ec, std::move(sock)); });
	}

	auto messaging_server::on_accept(std::error_code ec, tcp::socket socket)
		-> void
	{
		if (!is_running_.load())
		{
			return;
		}
		if (ec)
		{
			NETWORK_LOG_ERROR("[messaging_server] Accept error: " + ec.message());
#ifdef BUILD_WITH_COMMON_SYSTEM
			// Record connection error
			connection_errors_.fetch_add(1, std::memory_order_relaxed);
			if (monitor_) {
				monitor_->record_metric("connection_errors", static_cast<double>(connection_errors_.load()));
			}
#endif
			return;
		}

		// Cleanup dead sessions before adding new one
		cleanup_dead_sessions();

		// Create a new messaging_session
		auto new_session = std::make_shared<network_system::session::messaging_session>(
			std::move(socket), server_id_);

		// Set up session callbacks to forward to server callbacks
		auto self = shared_from_this();
		// Use weak_ptr to avoid circular reference
		std::weak_ptr<network_system::session::messaging_session> weak_session = new_session;
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (receive_callback_)
			{
				new_session->set_receive_callback(
					[this, self, weak_session](const std::vector<uint8_t>& data)
					{
						if (auto session = weak_session.lock())
						{
							std::lock_guard<std::mutex> cb_lock(callback_mutex_);
							if (receive_callback_)
							{
								receive_callback_(session, data);
							}
						}
					});
			}

			if (disconnection_callback_)
			{
				new_session->set_disconnection_callback(
					[this, self](const std::string& session_id)
					{
						std::lock_guard<std::mutex> cb_lock(callback_mutex_);
						if (disconnection_callback_)
						{
							disconnection_callback_(session_id);
						}
					});
			}

			if (error_callback_)
			{
				new_session->set_error_callback(
					[this, self, weak_session](std::error_code ec)
					{
						if (auto session = weak_session.lock())
						{
							std::lock_guard<std::mutex> cb_lock(callback_mutex_);
							if (error_callback_)
							{
								error_callback_(session, ec);
							}
						}
					});
			}
		}

		// Track it in our sessions_ vector (protected by mutex)
		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			sessions_.push_back(new_session);
		}

		// Start the session
		new_session->start_session();

		// Invoke connection callback
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (connection_callback_)
			{
				try
				{
					connection_callback_(new_session);
				}
				catch (const std::exception& e)
				{
					NETWORK_LOG_ERROR("[messaging_server] Exception in connection callback: "
					                  + std::string(e.what()));
				}
			}
		}

#ifdef BUILD_WITH_COMMON_SYSTEM
		// Record active connections metric
		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			if (monitor_) {
				monitor_->record_metric("active_connections", static_cast<double>(sessions_.size()));
			}
		}
#endif

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

#ifdef BUILD_WITH_COMMON_SYSTEM
		// Update active connections metric after cleanup
		if (monitor_) {
			monitor_->record_metric("active_connections", static_cast<double>(sessions_.size()));
		}
#endif
	}

	auto messaging_server::start_cleanup_timer() -> void
	{
		if (!cleanup_timer_ || !is_running_.load())
		{
			return;
		}

		// Schedule cleanup every 30 seconds
		cleanup_timer_->expires_after(std::chrono::seconds(30));

		auto self = shared_from_this();
		cleanup_timer_->async_wait(
			[this, self](const std::error_code& ec)
			{
				if (!ec && is_running_.load())
				{
					cleanup_dead_sessions();
					start_cleanup_timer(); // Reschedule
				}
			}
		);
	}

#ifdef BUILD_WITH_COMMON_SYSTEM
	auto messaging_server::set_monitor(common::interfaces::IMonitor* monitor) -> void
	{
		monitor_ = monitor;
	}

	auto messaging_server::get_monitor() const -> common::interfaces::IMonitor*
	{
		return monitor_;
	}
#endif

	auto messaging_server::set_connection_callback(
		std::function<void(std::shared_ptr<network_system::session::messaging_session>)> callback)
		-> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		connection_callback_ = std::move(callback);
	}

	auto messaging_server::set_disconnection_callback(
		std::function<void(const std::string&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		disconnection_callback_ = std::move(callback);
	}

	auto messaging_server::set_receive_callback(
		std::function<void(std::shared_ptr<network_system::session::messaging_session>,
		                   const std::vector<uint8_t>&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		receive_callback_ = std::move(callback);
	}

	auto messaging_server::set_error_callback(
		std::function<void(std::shared_ptr<network_system::session::messaging_session>,
		                   std::error_code)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		error_callback_ = std::move(callback);
	}

} // namespace network_system::core