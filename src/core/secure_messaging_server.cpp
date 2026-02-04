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

#include <kcenon/network/detail/config/feature_flags.h>

#include "internal/core/secure_messaging_server.h"

#include "internal/core/network_context.h"
#include "internal/integration/logger_integration.h"
#include "kcenon/network/detail/session/secure_session.h"

#include <algorithm>
#include <chrono>
#include <string_view>

namespace kcenon::network::core
{

	using tcp = asio::ip::tcp;

	secure_messaging_server::secure_messaging_server(std::string_view server_id,
													 const std::string& cert_file,
													 const std::string& key_file)
		: server_id_(server_id)
	{
		// Initialize SSL context with TLS 1.3 support (TICKET-009: TLS 1.3 only by default)
		ssl_context_ = std::make_unique<asio::ssl::context>(
			asio::ssl::context::tls_server);

		// Set SSL context options - disable all legacy protocols
		ssl_context_->set_options(
			asio::ssl::context::default_workarounds |
			asio::ssl::context::no_sslv2 |
			asio::ssl::context::no_sslv3 |
			asio::ssl::context::no_tlsv1 |
			asio::ssl::context::no_tlsv1_1 |
			asio::ssl::context::single_dh_use);

		// Enforce TLS 1.3 minimum version using native OpenSSL handle
		// This prevents protocol downgrade attacks (CVSS 7.5)
		SSL_CTX* native_ctx = ssl_context_->native_handle();
		if (native_ctx)
		{
			// Set minimum protocol version to TLS 1.3 (security hardening)
			SSL_CTX_set_min_proto_version(native_ctx, TLS1_3_VERSION);
			SSL_CTX_set_max_proto_version(native_ctx, TLS1_3_VERSION);

			// Set strong cipher suites for TLS 1.3
			SSL_CTX_set_ciphersuites(native_ctx,
				"TLS_AES_256_GCM_SHA384:"
				"TLS_CHACHA20_POLY1305_SHA256:"
				"TLS_AES_128_GCM_SHA256");
		}

		// Load certificate and private key
		try
		{
			ssl_context_->use_certificate_chain_file(cert_file);
			ssl_context_->use_private_key_file(key_file, asio::ssl::context::pem);

			NETWORK_LOG_INFO("[secure_messaging_server] SSL context initialized with TLS 1.3 only, cert: " +
				cert_file + ", key: " + key_file);
		}
		catch (const std::exception& e)
		{
			NETWORK_LOG_ERROR("[secure_messaging_server] Failed to load SSL certificates: " +
				std::string(e.what()));
			throw;
		}
	}

	secure_messaging_server::~secure_messaging_server() noexcept
	{
		if (lifecycle_.is_running())
		{
			(void)stop_server();
		}
	}

	// =====================================================================
	// Lifecycle Management
	// =====================================================================

	auto secure_messaging_server::start_server(unsigned short port) -> VoidResult
	{
		if (lifecycle_.is_running())
		{
			return error_void(
				error_codes::network_system::server_already_running,
				"Server is already running",
				"secure_messaging_server::start_server");
		}

		if (!lifecycle_.try_start())
		{
			return error_void(
				error_codes::network_system::server_already_running,
				"Server is already starting",
				"secure_messaging_server::start_server");
		}

		// Call implementation
		auto result = do_start_impl(port);
		if (result.is_err())
		{
			lifecycle_.mark_stopped();
		}

		return result;
	}

	auto secure_messaging_server::stop_server() -> VoidResult
	{
		if (!lifecycle_.is_running())
		{
			return error_void(
				error_codes::network_system::server_not_started,
				"Server is not running",
				"secure_messaging_server::stop_server");
		}

		if (!lifecycle_.prepare_stop())
		{
			return ok(); // Already stopping
		}

		// Call implementation
		auto result = do_stop_impl();

		// Signal stop completion
		lifecycle_.mark_stopped();

		return result;
	}

	auto secure_messaging_server::wait_for_stop() -> void
	{
		lifecycle_.wait_for_stop();
	}

	auto secure_messaging_server::is_running() const noexcept -> bool
	{
		return lifecycle_.is_running();
	}

	auto secure_messaging_server::server_id() const -> const std::string&
	{
		return server_id_;
	}

	// =====================================================================
	// Callback Setters
	// =====================================================================

	auto secure_messaging_server::set_connection_callback(connection_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::connection)>(std::move(callback));
	}

	auto secure_messaging_server::set_disconnection_callback(disconnection_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::disconnection)>(std::move(callback));
	}

	auto secure_messaging_server::set_receive_callback(receive_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::receive)>(std::move(callback));
	}

	auto secure_messaging_server::set_error_callback(error_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::error)>(std::move(callback));
	}

	// =====================================================================
	// Internal Callback Helpers
	// =====================================================================

	auto secure_messaging_server::get_connection_callback() const -> connection_callback_t
	{
		return callbacks_.get<to_index(callback_index::connection)>();
	}

	auto secure_messaging_server::get_disconnection_callback() const -> disconnection_callback_t
	{
		return callbacks_.get<to_index(callback_index::disconnection)>();
	}

	auto secure_messaging_server::get_receive_callback() const -> receive_callback_t
	{
		return callbacks_.get<to_index(callback_index::receive)>();
	}

	auto secure_messaging_server::get_error_callback() const -> error_callback_t
	{
		return callbacks_.get<to_index(callback_index::error)>();
	}

	auto secure_messaging_server::invoke_connection_callback(
		std::shared_ptr<session::secure_session> session) -> void
	{
		callbacks_.invoke<to_index(callback_index::connection)>(session);
	}

	// =====================================================================
	// Internal Implementation Methods
	// =====================================================================

	auto secure_messaging_server::do_start_impl(unsigned short port) -> VoidResult
	{
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

			// Begin accepting connections
			do_accept();

			// Start periodic cleanup timer
			start_cleanup_timer();

			// Get thread pool from network context
			thread_pool_ = network_context::instance().get_thread_pool();
			if (!thread_pool_) {
				// Fallback: create a temporary thread pool if network_context is not initialized
				NETWORK_LOG_WARN("[secure_messaging_server] network_context not initialized, creating temporary thread pool");
				thread_pool_ = std::make_shared<integration::basic_thread_pool>(std::thread::hardware_concurrency());
			}

			// Start io_context on thread pool
			io_context_future_ = thread_pool_->submit(
				[this]()
				{
					try
					{
						NETWORK_LOG_DEBUG("[secure_messaging_server] io_context started");
						io_context_->run();
						NETWORK_LOG_DEBUG("[secure_messaging_server] io_context stopped");
					}
					catch (const std::exception& e)
					{
						NETWORK_LOG_ERROR("[secure_messaging_server] Exception in io_context: " +
										  std::string(e.what()));
					}
				});

			NETWORK_LOG_INFO("[secure_messaging_server] Started listening on port " +
				std::to_string(port) + " (TLS/SSL enabled)");
			return ok();
		}
		catch (const std::system_error& e)
		{
			// Check for specific error codes
			if (e.code() == asio::error::address_in_use ||
			    e.code() == std::errc::address_in_use)
			{
				return error_void(
					error_codes::network_system::bind_failed,
					"Failed to bind to port: address already in use",
					"secure_messaging_server::do_start_impl",
					"Port: " + std::to_string(port)
				);
			}
			else if (e.code() == asio::error::access_denied ||
			         e.code() == std::errc::permission_denied)
			{
				return error_void(
					error_codes::network_system::bind_failed,
					"Failed to bind to port: permission denied",
					"secure_messaging_server::do_start_impl",
					"Port: " + std::to_string(port)
				);
			}

			return error_void(
				error_codes::common_errors::internal_error,
				"Failed to start secure server: " + std::string(e.what()),
				"secure_messaging_server::do_start_impl",
				"Port: " + std::to_string(port)
			);
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common_errors::internal_error,
				"Failed to start secure server: " + std::string(e.what()),
				"secure_messaging_server::do_start_impl",
				"Port: " + std::to_string(port)
			);
		}
	}

	auto secure_messaging_server::do_stop_impl() -> VoidResult
	{
		try
		{
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

			// Step 4: Stop io_context (this cancels all remaining pending operations)
			if (io_context_)
			{
				io_context_->stop();
			}

			// Step 5: Wait for io_context task to complete
			// This ensures all pending async callbacks have finished executing
			if (io_context_future_.valid())
			{
				try {
					io_context_future_.wait();
				} catch (const std::exception& e) {
					NETWORK_LOG_ERROR("[secure_messaging_server] Exception while waiting for io_context: " +
									  std::string(e.what()));
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
			thread_pool_.reset();
			io_context_.reset();

			NETWORK_LOG_INFO("[secure_messaging_server] Stopped.");
			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(
				error_codes::common_errors::internal_error,
				"Failed to stop secure server: " + std::string(e.what()),
				"secure_messaging_server::do_stop_impl",
				"Server ID: " + server_id_
			);
		}
	}

	// =====================================================================
	// Internal Connection Handlers
	// =====================================================================

	auto secure_messaging_server::do_accept() -> void
	{
		auto self = shared_from_this();
		acceptor_->async_accept(
			[this, self](std::error_code ec, tcp::socket sock)
			{ on_accept(ec, std::move(sock)); });
	}

	auto secure_messaging_server::on_accept(std::error_code ec, tcp::socket socket)
		-> void
	{
		if (!is_running())
		{
			return;
		}
		if (ec)
		{
			NETWORK_LOG_ERROR("[secure_messaging_server] Accept error: " + ec.message());
#if KCENON_WITH_COMMON_SYSTEM
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

		// Create a new secure_session
		auto new_session = std::make_shared<kcenon::network::session::secure_session>(
			std::move(socket), *ssl_context_, server_id_);

		// Set up session callbacks
		auto self = shared_from_this();
		auto receive_cb = get_receive_callback();
		auto disconnection_cb = get_disconnection_callback();
		auto error_cb = get_error_callback();

		if (receive_cb)
		{
			new_session->set_receive_callback(
				[this, self, new_session, receive_cb](const std::vector<uint8_t>& data)
				{
					receive_cb(new_session, data);
				});
		}

		if (disconnection_cb)
		{
			new_session->set_disconnection_callback(
				[this, self, new_session, disconnection_cb](const std::string& session_id)
				{
					disconnection_cb(session_id);
				});
		}

		if (error_cb)
		{
			new_session->set_error_callback(
				[this, self, new_session, error_cb](std::error_code err)
				{
					error_cb(new_session, err);
				});
		}

		// Track it in our sessions_ vector (protected by mutex)
		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			sessions_.push_back(new_session);
		}

		// Start the session (this will trigger SSL handshake)
		new_session->start_session();

		// Invoke connection callback
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

		// Accept next connection
		do_accept();
	}

	auto secure_messaging_server::cleanup_dead_sessions() -> void
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

		NETWORK_LOG_DEBUG("[secure_messaging_server] Cleaned up dead sessions. Active: " +
			std::to_string(sessions_.size()));

#if KCENON_WITH_COMMON_SYSTEM
		// Update active connections metric after cleanup
		if (monitor_) {
			monitor_->record_metric("active_connections", static_cast<double>(sessions_.size()));
		}
#endif
	}

	auto secure_messaging_server::start_cleanup_timer() -> void
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
	auto secure_messaging_server::set_monitor(kcenon::common::interfaces::IMonitor* monitor) -> void
	{
		monitor_ = monitor;
	}

	auto secure_messaging_server::get_monitor() const -> kcenon::common::interfaces::IMonitor*
	{
		return monitor_;
	}
#endif

} // namespace kcenon::network::core
