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

#include "kcenon/network/core/messaging_ws_server.h"

#include "kcenon/network/core/network_context.h"
#include "kcenon/network/core/ws_session_manager.h"
#include "kcenon/network/internal/tcp_socket.h"
#include "kcenon/network/internal/websocket_socket.h"
#include "kcenon/network/integration/logger_integration.h"

namespace kcenon::network::core
{
	using tcp = asio::ip::tcp;

	// ========================================================================
	// ws_connection_impl
	// ========================================================================

	class ws_connection_impl
	{
	public:
		ws_connection_impl(const std::string& conn_id,
						   std::shared_ptr<internal::websocket_socket> ws_socket,
						   const std::string& remote_addr,
						   const std::string& ws_path = "/")
			: connection_id_(conn_id)
			, ws_socket_(ws_socket)
			, remote_address_(remote_addr)
			, path_(ws_path)
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

		[[nodiscard]] auto connection_id() const -> const std::string& { return connection_id_; }

		[[nodiscard]] auto remote_endpoint() const -> std::string { return remote_address_; }

		[[nodiscard]] auto path() const -> std::string_view { return path_; }

		[[nodiscard]] auto is_connected() const -> bool
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			return ws_socket_ && ws_socket_->is_open();
		}

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
		std::string path_;
		mutable std::mutex socket_mutex_;
	};

	// ========================================================================
	// ws_connection
	// ========================================================================

	ws_connection::ws_connection(std::shared_ptr<ws_connection_impl> impl) : pimpl_(std::move(impl)) {}

	// ========================================================================
	// i_websocket_session interface implementation
	// ========================================================================

	auto ws_connection::id() const -> std::string_view
	{
		return pimpl_->connection_id();
	}

	auto ws_connection::is_connected() const -> bool
	{
		return pimpl_->is_connected();
	}

	auto ws_connection::send(std::vector<uint8_t>&& data) -> VoidResult
	{
		return pimpl_->send_binary(std::move(data), nullptr);
	}

	auto ws_connection::close() -> void
	{
		pimpl_->close(internal::ws_close_code::normal, "");
	}

	auto ws_connection::send_text(std::string&& message) -> VoidResult
	{
		return pimpl_->send_text(std::move(message), nullptr);
	}

	auto ws_connection::send_binary(std::vector<uint8_t>&& data) -> VoidResult
	{
		return pimpl_->send_binary(std::move(data), nullptr);
	}

	auto ws_connection::close(uint16_t code, std::string_view reason) -> void
	{
		pimpl_->close(static_cast<internal::ws_close_code>(code), std::string(reason));
	}

	auto ws_connection::path() const -> std::string_view
	{
		return pimpl_->path();
	}

	auto ws_connection::remote_endpoint() const -> std::string
	{
		return pimpl_->remote_endpoint();
	}

	// ========================================================================
	// messaging_ws_server
	// ========================================================================

	messaging_ws_server::messaging_ws_server(std::string_view server_id)
		: server_id_(server_id)
	{
	}

	messaging_ws_server::~messaging_ws_server() noexcept
	{
		if (lifecycle_.is_running())
		{
			stop_server();
		}
	}

	// ========================================================================
	// Lifecycle Management
	// ========================================================================

	auto messaging_ws_server::start_server(const ws_server_config& config) -> VoidResult
	{
		config_ = config;
		return start_server(config.port, config.path);
	}

	auto messaging_ws_server::start_server(uint16_t port, std::string_view path) -> VoidResult
	{
		if (lifecycle_.is_running())
		{
			return error_void(
				error_codes::common_errors::already_exists,
				"WebSocket server is already running",
				"messaging_ws_server");
		}

		lifecycle_.set_running();

		auto result = do_start_impl(port, path);
		if (result.is_err())
		{
			lifecycle_.mark_stopped();
		}

		return result;
	}

	auto messaging_ws_server::stop_server() -> VoidResult
	{
		if (!lifecycle_.prepare_stop())
		{
			return ok();
		}

		auto result = do_stop_impl();

		lifecycle_.mark_stopped();

		return result;
	}

	auto messaging_ws_server::server_id() const -> const std::string&
	{
		return server_id_;
	}

	// ========================================================================
	// i_network_component interface implementation
	// ========================================================================

	auto messaging_ws_server::is_running() const -> bool
	{
		return lifecycle_.is_running();
	}

	auto messaging_ws_server::wait_for_stop() -> void
	{
		lifecycle_.wait_for_stop();
	}

	// ========================================================================
	// i_websocket_server interface implementation
	// ========================================================================

	auto messaging_ws_server::start(uint16_t port) -> VoidResult
	{
		return start_server(port);
	}

	auto messaging_ws_server::stop() -> VoidResult
	{
		return stop_server();
	}

	auto messaging_ws_server::connection_count() const -> size_t
	{
		if (!session_mgr_)
		{
			return 0;
		}
		return session_mgr_->get_connection_count();
	}

	// ========================================================================
	// Interface callback setters
	// ========================================================================

	auto messaging_ws_server::set_connection_callback(
		interfaces::i_websocket_server::connection_callback_t callback) -> void
	{
		// Adapt interface callback to internal callback type
		if (callback) {
			callbacks_.set<to_index(callback_index::connection)>(
				[callback = std::move(callback)](std::shared_ptr<ws_connection> conn) {
					// ws_connection already implements i_websocket_session
					callback(conn);
				});
		} else {
			callbacks_.set<to_index(callback_index::connection)>(connection_callback_t{});
		}
	}

	auto messaging_ws_server::set_disconnection_callback(
		interfaces::i_websocket_server::disconnection_callback_t callback) -> void
	{
		// Adapt interface callback to internal callback type
		if (callback) {
			callbacks_.set<to_index(callback_index::disconnection)>(
				[callback = std::move(callback)](const std::string& session_id,
												 internal::ws_close_code code,
												 const std::string& reason) {
					callback(session_id, static_cast<uint16_t>(code), reason);
				});
		} else {
			callbacks_.set<to_index(callback_index::disconnection)>(disconnection_callback_t{});
		}
	}

	auto messaging_ws_server::set_text_callback(
		interfaces::i_websocket_server::text_callback_t callback) -> void
	{
		// Adapt interface callback to internal callback type
		if (callback) {
			callbacks_.set<to_index(callback_index::text_message)>(
				[callback = std::move(callback)](std::shared_ptr<ws_connection> conn,
												 const std::string& message) {
					callback(conn->id(), message);
				});
		} else {
			callbacks_.set<to_index(callback_index::text_message)>(text_message_callback_t{});
		}
	}

	auto messaging_ws_server::set_binary_callback(
		interfaces::i_websocket_server::binary_callback_t callback) -> void
	{
		// Adapt interface callback to internal callback type
		if (callback) {
			callbacks_.set<to_index(callback_index::binary_message)>(
				[callback = std::move(callback)](std::shared_ptr<ws_connection> conn,
												 const std::vector<uint8_t>& data) {
					callback(conn->id(), data);
				});
		} else {
			callbacks_.set<to_index(callback_index::binary_message)>(binary_message_callback_t{});
		}
	}

	auto messaging_ws_server::set_error_callback(
		interfaces::i_websocket_server::error_callback_t callback) -> void
	{
		// Interface and legacy types are compatible
		if (callback) {
			callbacks_.set<to_index(callback_index::error)>(
				[callback = std::move(callback)](const std::string& session_id, std::error_code ec) {
					callback(session_id, ec);
				});
		} else {
			callbacks_.set<to_index(callback_index::error)>(error_callback_t{});
		}
	}

	// ========================================================================
	// Internal Implementation
	// ========================================================================

	auto messaging_ws_server::do_start_impl(uint16_t port, std::string_view path) -> VoidResult
	{
		try
		{
			// Store config if not already set
			if (config_.port == 0 || config_.port != port)
			{
				config_.port = port;
				config_.path = std::string(path);
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

			// Get thread pool from network context
			thread_pool_ = network_context::instance().get_thread_pool();
			if (!thread_pool_) {
				// Fallback: create a temporary thread pool if network_context is not initialized
				NETWORK_LOG_WARN("[messaging_ws_server] network_context not initialized, creating temporary thread pool");
				thread_pool_ = std::make_shared<integration::basic_thread_pool>(std::thread::hardware_concurrency());
			}

			// Start io_context on thread pool
			io_context_future_ = thread_pool_->submit([this]() {
				try {
					NETWORK_LOG_DEBUG("[messaging_ws_server] io_context started");
					io_context_->run();
					NETWORK_LOG_DEBUG("[messaging_ws_server] io_context stopped");
				} catch (const std::exception& e) {
					NETWORK_LOG_ERROR("[messaging_ws_server] Exception in io_context: " +
									  std::string(e.what()));
				}
			});

			// Start accepting connections
			do_accept();

			NETWORK_LOG_INFO("[messaging_ws_server] Server started on port " +
							 std::to_string(config_.port) + " (ID: " +
							 server_id_ + ")");

			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(error_codes::network_system::bind_failed,
							  std::string("Failed to start server: ") + e.what());
		}
	}

	auto messaging_ws_server::do_stop_impl() -> VoidResult
	{
		try
		{
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
			// Lock to prevent race with do_accept() accessing the acceptor
			{
				std::lock_guard<std::mutex> lock(acceptor_mutex_);
				if (acceptor_)
				{
					acceptor_->close();
				}
			}

			// Stop io_context
			work_guard_.reset();
			if (io_context_)
			{
				io_context_->stop();
			}

			// Wait for io_context task to complete
			if (io_context_future_.valid())
			{
				try {
					io_context_future_.wait();
				} catch (const std::exception& e) {
					NETWORK_LOG_ERROR("[messaging_ws_server] Exception while waiting for io_context: " +
									  std::string(e.what()));
				}
			}

			NETWORK_LOG_INFO("[messaging_ws_server] Server stopped (ID: " +
							 server_id_ + ")");

			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(error_codes::common_errors::internal_error,
							  std::string("Failed to stop server: ") + e.what());
		}
	}

	auto messaging_ws_server::broadcast_text(const std::string& message) -> void
	{
		if (!session_mgr_)
		{
			return;
		}

		auto conns = session_mgr_->get_all_connections();
		for (auto& conn : conns)
		{
			(void)conn->send_text(std::string(message));
		}
	}

	auto messaging_ws_server::broadcast_binary(const std::vector<uint8_t>& data) -> void
	{
		if (!session_mgr_)
		{
			return;
		}

		auto conns = session_mgr_->get_all_connections();
		for (auto& conn : conns)
		{
			(void)conn->send_binary(std::vector<uint8_t>(data));
		}
	}

	auto messaging_ws_server::get_connection(const std::string& connection_id)
		-> std::shared_ptr<ws_connection>
	{
		if (!session_mgr_)
		{
			return nullptr;
		}
		return session_mgr_->get_connection(connection_id);
	}

	auto messaging_ws_server::get_all_connections() -> std::vector<std::string>
	{
		if (!session_mgr_)
		{
			return {};
		}
		return session_mgr_->get_all_connection_ids();
	}

	auto messaging_ws_server::do_accept() -> void
	{
		// Lock to prevent race with do_stop_impl() closing the acceptor
		std::lock_guard<std::mutex> lock(acceptor_mutex_);

		// Early return if server is stopping or acceptor is invalid
		if (!lifecycle_.is_running() || !acceptor_ || !acceptor_->is_open())
		{
			return;
		}

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
									if (lifecycle_.is_running())
									{
										do_accept();
									}
								});
	}

	auto messaging_ws_server::handle_new_connection(std::shared_ptr<tcp::socket> socket) -> void
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
		auto conn_impl = std::make_shared<ws_connection_impl>(conn_id, ws_socket, remote_addr, config_.path);
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
				auto ws_sock = conn->get_impl()->get_socket();
				if (ws_sock)
				{
					ws_sock->start_read();
				}

				// Invoke connection callback
				invoke_connection_callback(conn);
			});
	}

	auto messaging_ws_server::on_message(std::shared_ptr<ws_connection> conn,
										 const internal::ws_message& msg) -> void
	{
		invoke_message_callback(conn, msg);
	}

	auto messaging_ws_server::on_close(const std::string& conn_id,
									   internal::ws_close_code code,
									   const std::string& reason) -> void
	{
		// Remove from session manager
		if (session_mgr_)
		{
			auto conn = session_mgr_->get_connection(conn_id);
			if (conn)
			{
				conn->get_impl()->invalidate();
			}
			session_mgr_->remove_connection(conn_id);
		}

		NETWORK_LOG_INFO("[messaging_ws_server] Client disconnected: " + conn_id);

		invoke_disconnection_callback(conn_id, code, reason);
	}

	auto messaging_ws_server::on_error(const std::string& conn_id,
									   std::error_code ec) -> void
	{
		NETWORK_LOG_ERROR("[messaging_ws_server] Connection error (" + conn_id +
						  "): " + ec.message());
		invoke_error_callback(conn_id, ec);
	}

	// ========================================================================
	// Internal Callback Helpers
	// ========================================================================

	auto messaging_ws_server::invoke_connection_callback(std::shared_ptr<ws_connection> conn) -> void
	{
		callbacks_.invoke<to_index(callback_index::connection)>(conn);
	}

	auto messaging_ws_server::invoke_disconnection_callback(const std::string& conn_id,
	                                                        internal::ws_close_code code,
	                                                        const std::string& reason) -> void
	{
		callbacks_.invoke<to_index(callback_index::disconnection)>(conn_id, code, reason);
	}

	auto messaging_ws_server::invoke_message_callback(std::shared_ptr<ws_connection> conn,
	                                                  const internal::ws_message& msg) -> void
	{
		callbacks_.invoke<to_index(callback_index::message)>(conn, msg);

		if (msg.type == internal::ws_message_type::text)
		{
			callbacks_.invoke<to_index(callback_index::text_message)>(conn, msg.as_text());
		}
		else if (msg.type == internal::ws_message_type::binary)
		{
			callbacks_.invoke<to_index(callback_index::binary_message)>(conn, msg.as_binary());
		}
	}

	auto messaging_ws_server::invoke_error_callback(const std::string& conn_id,
	                                                std::error_code ec) -> void
	{
		callbacks_.invoke<to_index(callback_index::error)>(conn_id, ec);
	}

} // namespace kcenon::network::core
