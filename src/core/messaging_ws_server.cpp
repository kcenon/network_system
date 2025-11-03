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

#include "network_system/core/messaging_ws_server.h"

#include "network_system/core/ws_session_manager.h"
#include "network_system/internal/tcp_socket.h"
#include "network_system/internal/websocket_socket.h"
#include "network_system/integration/logger_integration.h"
#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/io_context_executor.h"

#include <asio.hpp>

#include <atomic>
#include <future>
#include <map>
#include <mutex>

namespace network_system::core
{
	using tcp = asio::ip::tcp;

	// ========================================================================
	// ws_connection::impl
	// ========================================================================

	class ws_connection::impl
	{
	public:
		impl(const std::string& conn_id,
			 std::shared_ptr<internal::websocket_socket> ws_socket,
			 const std::string& remote_addr)
			: connection_id_(conn_id)
			, ws_socket_(ws_socket)
			, remote_address_(remote_addr)
		{
		}

		auto send_text(std::string&& message,
					   std::function<void(std::error_code, std::size_t)> handler)
			-> VoidResult
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			if (!ws_socket_)
			{
				return error_void(error_codes::network_system::connection_closed,
								  "Connection closed");
			}
			return ws_socket_->async_send_text(std::move(message),
											   std::move(handler));
		}

		auto send_binary(std::vector<uint8_t>&& data,
						 std::function<void(std::error_code, std::size_t)> handler)
			-> VoidResult
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			if (!ws_socket_)
			{
				return error_void(error_codes::network_system::connection_closed,
								  "Connection closed");
			}
			return ws_socket_->async_send_binary(std::move(data), std::move(handler));
		}

		auto close(internal::ws_close_code code, const std::string& reason)
			-> VoidResult
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			if (!ws_socket_)
			{
				return error_void(error_codes::network_system::connection_closed,
								  "Connection closed");
			}
			ws_socket_->async_close(code, reason, [](std::error_code) {});
			return ok();
		}

		auto connection_id() const -> const std::string& { return connection_id_; }

		auto remote_endpoint() const -> std::string { return remote_address_; }

		auto get_socket() -> std::shared_ptr<internal::websocket_socket>
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			return ws_socket_;
		}

		auto invalidate() -> void
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			ws_socket_.reset();
		}

	private:
		std::string connection_id_;
		std::shared_ptr<internal::websocket_socket> ws_socket_;
		std::string remote_address_;
		std::mutex socket_mutex_;
	};

	// ========================================================================
	// ws_connection
	// ========================================================================

	ws_connection::ws_connection(std::shared_ptr<impl> impl) : pimpl_(impl) {}

	auto ws_connection::send_text(
		std::string&& message,
		std::function<void(std::error_code, std::size_t)> handler) -> VoidResult
	{
		return pimpl_->send_text(std::move(message), std::move(handler));
	}

	auto ws_connection::send_binary(
		std::vector<uint8_t>&& data,
		std::function<void(std::error_code, std::size_t)> handler) -> VoidResult
	{
		return pimpl_->send_binary(std::move(data), std::move(handler));
	}

	auto ws_connection::close(internal::ws_close_code code,
							  const std::string& reason) -> VoidResult
	{
		return pimpl_->close(code, reason);
	}

	auto ws_connection::connection_id() const -> const std::string&
	{
		return pimpl_->connection_id();
	}

	auto ws_connection::remote_endpoint() const -> std::string
	{
		return pimpl_->remote_endpoint();
	}

	// ========================================================================
	// messaging_ws_server::impl
	// ========================================================================

	class messaging_ws_server::impl
	{
	public:
		impl(const std::string& server_id) : server_id_(server_id) {}

		~impl()
		{
			try
			{
				if (is_running_.load())
				{
					stop();
				}
			}
			catch (...)
			{
				// Swallow exceptions in destructor
			}
		}

		auto start(const ws_server_config& config) -> VoidResult
		{
			if (is_running_.load())
			{
				return error_void(error_codes::common::already_exists,
								  "Server is already running");
			}

			config_ = config;

			try
			{
				// Get thread pool from manager
				auto& mgr = integration::thread_pool_manager::instance();
				if (!mgr.is_initialized())
				{
					return error_void(error_codes::common::internal_error,
									  "thread_pool_manager not initialized");
				}

				auto pool = mgr.create_io_pool("messaging_ws_server");
				if (!pool)
				{
					return error_void(error_codes::common::internal_error,
									  "Failed to create I/O pool");
				}

				// Create session manager
				session_config sess_config;
				sess_config.max_sessions = config_.max_connections;
				session_mgr_ = std::make_shared<ws_session_manager>(sess_config);

				// Create io_context and work guard
				io_context_ = std::make_unique<asio::io_context>();
				work_guard_ = std::make_unique<
					asio::executor_work_guard<asio::io_context::executor_type>>(
					asio::make_work_guard(*io_context_));

				// Create acceptor
				acceptor_ = std::make_unique<tcp::acceptor>(
					*io_context_, tcp::endpoint(tcp::v4(), config_.port));

				// Setup stop promise/future
				stop_promise_.emplace();
				stop_future_ = stop_promise_->get_future();

				// Start accepting connections
				do_accept();

				// Create io_context executor
				io_executor_ = std::make_unique<integration::io_context_executor>(
					pool,
					*io_context_,
					"messaging_ws_server"
				);

				// Start io_context execution in thread pool
				io_executor_->start();

				is_running_.store(true);

				NETWORK_LOG_INFO("[messaging_ws_server] Server started on port " +
								 std::to_string(config_.port) + " (ID: " +
								 server_id_ + ")");

				return ok();
			}
			catch (const std::system_error& e)
			{
				is_running_.store(false);
				io_executor_.reset();

				// Check for specific error codes
				if (e.code() == asio::error::address_in_use ||
					e.code() == std::errc::address_in_use)
				{
					return error_void(error_codes::network_system::bind_failed,
									  "Failed to bind to port: address already in use");
				}
				else if (e.code() == asio::error::access_denied ||
						 e.code() == std::errc::permission_denied)
				{
					return error_void(error_codes::network_system::bind_failed,
									  "Failed to bind to port: permission denied");
				}

				return error_void(error_codes::common::internal_error,
								  std::string("Failed to start server: ") + e.what());
			}
			catch (const std::exception& e)
			{
				is_running_.store(false);
				io_executor_.reset();
				return error_void(error_codes::network_system::bind_failed,
								  std::string("Failed to start server: ") + e.what());
			}
		}

		auto stop() -> VoidResult
		{
			if (!is_running_.load())
			{
				return error_void(error_codes::common::not_initialized,
								  "Server is not running");
			}

			try
			{
				is_running_.store(false);

				// Close all connections
				if (session_mgr_)
				{
					auto conns = session_mgr_->get_all_connections();
					for (auto& conn : conns)
					{
						conn->close();
					}
					session_mgr_->clear_all_connections();
				}

				// Stop acceptor
				if (acceptor_)
				{
					asio::error_code ec;
					acceptor_->cancel(ec);
					if (acceptor_->is_open())
					{
						acceptor_->close(ec);
					}
				}

				// Release work guard to allow io_context to finish
				if (work_guard_)
				{
					work_guard_.reset();
				}

				// Stop io_context
				if (io_context_)
				{
					io_context_->stop();
				}

				// Release io_context executor (automatically stops and joins)
				io_executor_.reset();

				// Release resources explicitly
				acceptor_.reset();

				// Notify stop_future
				if (stop_promise_)
				{
					stop_promise_->set_value();
				}

				NETWORK_LOG_INFO("[messaging_ws_server] Server stopped (ID: " +
								 server_id_ + ")");

				return ok();
			}
			catch (const std::exception& e)
			{
				return error_void(error_codes::common::internal_error,
								  std::string("Failed to stop server: ") + e.what());
			}
		}

		auto wait_for_stop() -> void
		{
			if (stop_future_.valid())
			{
				stop_future_.wait();
			}
		}

		auto is_running() const -> bool { return is_running_.load(); }

		auto broadcast_text(const std::string& message) -> void
		{
			if (!session_mgr_)
			{
				return;
			}

			auto conns = session_mgr_->get_all_connections();
			for (auto& conn : conns)
			{
				conn->send_text(std::string(message), nullptr);
			}
		}

		auto broadcast_binary(const std::vector<uint8_t>& data) -> void
		{
			if (!session_mgr_)
			{
				return;
			}

			auto conns = session_mgr_->get_all_connections();
			for (auto& conn : conns)
			{
				conn->send_binary(std::vector<uint8_t>(data), nullptr);
			}
		}

		auto get_connection(const std::string& connection_id)
			-> std::shared_ptr<ws_connection>
		{
			if (!session_mgr_)
			{
				return nullptr;
			}
			return session_mgr_->get_connection(connection_id);
		}

		auto get_all_connections() -> std::vector<std::string>
		{
			if (!session_mgr_)
			{
				return {};
			}
			return session_mgr_->get_all_connection_ids();
		}

		auto connection_count() const -> size_t
		{
			if (!session_mgr_)
			{
				return 0;
			}
			return session_mgr_->get_connection_count();
		}

		auto set_connection_callback(
			std::function<void(std::shared_ptr<ws_connection>)> callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			connection_callback_ = std::move(callback);
		}

		auto set_disconnection_callback(
			std::function<void(const std::string&, internal::ws_close_code,
							   const std::string&)>
				callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			disconnection_callback_ = std::move(callback);
		}

		auto set_message_callback(
			std::function<void(std::shared_ptr<ws_connection>,
							   const internal::ws_message&)>
				callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			message_callback_ = std::move(callback);
		}

		auto set_text_message_callback(
			std::function<void(std::shared_ptr<ws_connection>, const std::string&)>
				callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			text_message_callback_ = std::move(callback);
		}

		auto set_binary_message_callback(
			std::function<void(std::shared_ptr<ws_connection>,
							   const std::vector<uint8_t>&)>
				callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			binary_message_callback_ = std::move(callback);
		}

		auto set_error_callback(
			std::function<void(const std::string&, std::error_code)> callback)
			-> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			error_callback_ = std::move(callback);
		}

		auto server_id() const -> const std::string& { return server_id_; }

	private:
		auto do_accept() -> void
		{
			auto socket = std::make_shared<tcp::socket>(*io_context_);

			acceptor_->async_accept(*socket,
									[this, socket](std::error_code ec)
									{
										if (!ec)
										{
											handle_new_connection(socket);
										}
										else
										{
											NETWORK_LOG_ERROR(
												"[messaging_ws_server] Accept error: " +
												ec.message());
										}

										// Continue accepting
										if (is_running_.load())
										{
											do_accept();
										}
									});
		}

		auto handle_new_connection(std::shared_ptr<tcp::socket> socket) -> void
		{
			// Check connection limit using session manager
			if (!session_mgr_ || !session_mgr_->can_accept_connection())
			{
				NETWORK_LOG_WARN("[messaging_ws_server] Connection limit reached");
				return;
			}

			// Generate connection ID
			auto remote_ep = socket->remote_endpoint();
			std::string conn_id = remote_ep.address().to_string() + ":" +
								  std::to_string(remote_ep.port());
			std::string remote_addr = conn_id;

			// Wrap in tcp_socket
			auto tcp_sock = std::make_shared<internal::tcp_socket>(std::move(*socket));

			// Wrap in websocket_socket
			auto ws_socket =
				std::make_shared<internal::websocket_socket>(tcp_sock, false);

			// Create connection wrapper
			auto conn_impl = std::make_shared<ws_connection::impl>(conn_id, ws_socket,
																	remote_addr);
			auto conn = std::make_shared<ws_connection>(conn_impl);

			// Set WebSocket callbacks
			ws_socket->set_message_callback(
				[this, conn](const internal::ws_message& msg)
				{ on_message(conn, msg); });

			ws_socket->set_ping_callback(
				[this, ws_socket](const std::vector<uint8_t>& payload)
				{
					if (config_.auto_pong)
					{
						ws_socket->async_send_ping(std::vector<uint8_t>(payload),
												   [](std::error_code) {});
					}
				});

			ws_socket->set_close_callback(
				[this, conn_id](internal::ws_close_code code,
								const std::string& reason)
				{ on_close(conn_id, code, reason); });

			ws_socket->set_error_callback(
				[this, conn_id](std::error_code ec) { on_error(conn_id, ec); });

			// Accept WebSocket handshake
			ws_socket->async_accept(
				[this, conn_id, conn](std::error_code ec)
				{
					if (ec)
					{
						NETWORK_LOG_ERROR("[messaging_ws_server] Handshake failed: " +
										  ec.message());
						return;
					}

					// Add to session manager
					auto added_id = session_mgr_->add_connection(conn, conn_id);
					if (added_id.empty())
					{
						NETWORK_LOG_WARN(
							"[messaging_ws_server] Failed to add connection");
						return;
					}

					NETWORK_LOG_INFO("[messaging_ws_server] Client connected: " +
									 conn_id);

					// Start reading frames
					auto ws_sock = conn->pimpl_->get_socket();
					if (ws_sock)
					{
						ws_sock->start_read();
					}

					// Invoke connection callback
					invoke_connection_callback(conn);
				});
		}

		auto on_message(std::shared_ptr<ws_connection> conn,
						const internal::ws_message& msg) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);

			if (message_callback_)
			{
				message_callback_(conn, msg);
			}

			if (msg.type == internal::ws_message_type::text &&
				text_message_callback_)
			{
				text_message_callback_(conn, msg.as_text());
			}
			else if (msg.type == internal::ws_message_type::binary &&
					 binary_message_callback_)
			{
				binary_message_callback_(conn, msg.as_binary());
			}
		}

		auto on_close(const std::string& conn_id, internal::ws_close_code code,
					  const std::string& reason) -> void
		{
			// Remove from session manager
			if (session_mgr_)
			{
				auto conn = session_mgr_->get_connection(conn_id);
				if (conn)
				{
					conn->pimpl_->invalidate();
				}
				session_mgr_->remove_connection(conn_id);
			}

			NETWORK_LOG_INFO("[messaging_ws_server] Client disconnected: " +
							 conn_id);

			invoke_disconnection_callback(conn_id, code, reason);
		}

		auto on_error(const std::string& conn_id, std::error_code ec) -> void
		{
			NETWORK_LOG_ERROR("[messaging_ws_server] Connection error (" + conn_id +
							  "): " + ec.message());
			invoke_error_callback(conn_id, ec);
		}

		auto invoke_connection_callback(std::shared_ptr<ws_connection> conn) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (connection_callback_)
			{
				connection_callback_(conn);
			}
		}

		auto invoke_disconnection_callback(const std::string& conn_id,
										   internal::ws_close_code code,
										   const std::string& reason) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (disconnection_callback_)
			{
				disconnection_callback_(conn_id, code, reason);
			}
		}

		auto invoke_error_callback(const std::string& conn_id, std::error_code ec)
			-> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (error_callback_)
			{
				error_callback_(conn_id, ec);
			}
		}

	private:
		std::string server_id_;
		ws_server_config config_;

		std::unique_ptr<asio::io_context> io_context_;
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
			work_guard_;
		std::unique_ptr<tcp::acceptor> acceptor_;
		std::unique_ptr<integration::io_context_executor> io_executor_;

		std::shared_ptr<ws_session_manager> session_mgr_;

		std::atomic<bool> is_running_{false};

		std::optional<std::promise<void>> stop_promise_;
		std::future<void> stop_future_;

		std::mutex callback_mutex_;
		std::function<void(std::shared_ptr<ws_connection>)> connection_callback_;
		std::function<void(const std::string&, internal::ws_close_code,
						   const std::string&)>
			disconnection_callback_;
		std::function<void(std::shared_ptr<ws_connection>,
						   const internal::ws_message&)>
			message_callback_;
		std::function<void(std::shared_ptr<ws_connection>, const std::string&)>
			text_message_callback_;
		std::function<void(std::shared_ptr<ws_connection>,
						   const std::vector<uint8_t>&)>
			binary_message_callback_;
		std::function<void(const std::string&, std::error_code)> error_callback_;
	};

	// ========================================================================
	// messaging_ws_server
	// ========================================================================

	messaging_ws_server::messaging_ws_server(const std::string& server_id)
		: pimpl_(std::make_unique<impl>(server_id))
	{
	}

	messaging_ws_server::~messaging_ws_server() = default;

	auto messaging_ws_server::start_server(const ws_server_config& config)
		-> VoidResult
	{
		return pimpl_->start(config);
	}

	auto messaging_ws_server::start_server(uint16_t port, std::string_view path)
		-> VoidResult
	{
		ws_server_config config;
		config.port = port;
		config.path = std::string(path);
		return pimpl_->start(config);
	}

	auto messaging_ws_server::stop_server() -> VoidResult { return pimpl_->stop(); }

	auto messaging_ws_server::wait_for_stop() -> void { pimpl_->wait_for_stop(); }

	auto messaging_ws_server::is_running() const -> bool
	{
		return pimpl_->is_running();
	}

	auto messaging_ws_server::broadcast_text(const std::string& message) -> void
	{
		pimpl_->broadcast_text(message);
	}

	auto messaging_ws_server::broadcast_binary(const std::vector<uint8_t>& data)
		-> void
	{
		pimpl_->broadcast_binary(data);
	}

	auto messaging_ws_server::get_connection(const std::string& connection_id)
		-> std::shared_ptr<ws_connection>
	{
		return pimpl_->get_connection(connection_id);
	}

	auto messaging_ws_server::get_all_connections() -> std::vector<std::string>
	{
		return pimpl_->get_all_connections();
	}

	auto messaging_ws_server::connection_count() const -> size_t
	{
		return pimpl_->connection_count();
	}

	auto messaging_ws_server::set_connection_callback(
		std::function<void(std::shared_ptr<ws_connection>)> callback) -> void
	{
		pimpl_->set_connection_callback(std::move(callback));
	}

	auto messaging_ws_server::set_disconnection_callback(
		std::function<void(const std::string&, internal::ws_close_code,
						   const std::string&)>
			callback) -> void
	{
		pimpl_->set_disconnection_callback(std::move(callback));
	}

	auto messaging_ws_server::set_message_callback(
		std::function<void(std::shared_ptr<ws_connection>,
						   const internal::ws_message&)>
			callback) -> void
	{
		pimpl_->set_message_callback(std::move(callback));
	}

	auto messaging_ws_server::set_text_message_callback(
		std::function<void(std::shared_ptr<ws_connection>, const std::string&)>
			callback) -> void
	{
		pimpl_->set_text_message_callback(std::move(callback));
	}

	auto messaging_ws_server::set_binary_message_callback(
		std::function<void(std::shared_ptr<ws_connection>,
						   const std::vector<uint8_t>&)>
			callback) -> void
	{
		pimpl_->set_binary_message_callback(std::move(callback));
	}

	auto messaging_ws_server::set_error_callback(
		std::function<void(const std::string&, std::error_code)> callback) -> void
	{
		pimpl_->set_error_callback(std::move(callback));
	}

	auto messaging_ws_server::server_id() const -> const std::string&
	{
		return pimpl_->server_id();
	}

} // namespace network_system::core
