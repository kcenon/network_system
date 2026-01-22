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

#include "kcenon/network/http/websocket_client.h"

#include "kcenon/network/core/network_context.h"
#include "kcenon/network/internal/tcp_socket.h"
#include "kcenon/network/internal/websocket_socket.h"
#include "kcenon/network/integration/logger_integration.h"

namespace kcenon::network::core
{
	using tcp = asio::ip::tcp;

	messaging_ws_client::messaging_ws_client(std::string_view client_id)
		: client_id_(client_id)
	{
	}

	messaging_ws_client::~messaging_ws_client() noexcept
	{
		if (lifecycle_.is_running())
		{
			stop_client();
		}
	}

	// ========================================================================
	// Lifecycle Management
	// ========================================================================

	auto messaging_ws_client::start_client(const ws_client_config& config) -> VoidResult
	{
		config_ = config;
		return start_client(config.host, config.port, config.path);
	}

	auto messaging_ws_client::start_client(std::string_view host, uint16_t port,
	                                       std::string_view path) -> VoidResult
	{
		if (lifecycle_.is_running())
		{
			return error_void(
				error_codes::common_errors::already_exists,
				"WebSocket client is already running",
				"messaging_ws_client");
		}

		if (host.empty())
		{
			return error_void(
				error_codes::common_errors::invalid_argument,
				"Host cannot be empty",
				"messaging_ws_client");
		}

		lifecycle_.set_running();
		is_connected_.store(false);

		auto result = do_start_impl(host, port, path);
		if (result.is_err())
		{
			lifecycle_.mark_stopped();
		}

		return result;
	}

	auto messaging_ws_client::stop_client() -> VoidResult
	{
		if (!lifecycle_.prepare_stop())
		{
			return ok();
		}

		is_connected_.store(false);

		auto result = do_stop_impl();

		lifecycle_.mark_stopped();

		return result;
	}

	auto messaging_ws_client::client_id() const -> const std::string&
	{
		return client_id_;
	}

	// ========================================================================
	// i_network_component interface implementation
	// ========================================================================

	auto messaging_ws_client::is_running() const -> bool
	{
		return lifecycle_.is_running();
	}

	auto messaging_ws_client::wait_for_stop() -> void
	{
		lifecycle_.wait_for_stop();
	}

	// ========================================================================
	// i_websocket_client interface implementation
	// ========================================================================

	auto messaging_ws_client::start(std::string_view host, uint16_t port,
	                                std::string_view path) -> VoidResult
	{
		return start_client(host, port, path);
	}

	auto messaging_ws_client::stop() -> VoidResult
	{
		return stop_client();
	}

	auto messaging_ws_client::is_connected() const -> bool
	{
		return is_connected_.load(std::memory_order_relaxed);
	}

	auto messaging_ws_client::send_text(
		std::string&& message,
		interfaces::i_websocket_client::send_callback_t handler) -> VoidResult
	{
		std::lock_guard<std::mutex> lock(ws_socket_mutex_);
		if (!ws_socket_)
		{
			return error_void(error_codes::network_system::connection_closed,
							  "WebSocket not connected");
		}

		return ws_socket_->async_send_text(std::move(message), std::move(handler));
	}

	auto messaging_ws_client::send_binary(
		std::vector<uint8_t>&& data,
		interfaces::i_websocket_client::send_callback_t handler) -> VoidResult
	{
		std::lock_guard<std::mutex> lock(ws_socket_mutex_);
		if (!ws_socket_)
		{
			return error_void(error_codes::network_system::connection_closed,
							  "WebSocket not connected");
		}

		return ws_socket_->async_send_binary(std::move(data), std::move(handler));
	}

	auto messaging_ws_client::send_ping(std::vector<uint8_t>&& payload) -> VoidResult
	{
		std::lock_guard<std::mutex> lock(ws_socket_mutex_);
		if (!ws_socket_)
		{
			return error_void(error_codes::network_system::connection_closed,
							  "WebSocket not connected");
		}

		ws_socket_->async_send_ping(std::move(payload), [](std::error_code) {});
		return ok();
	}

	auto messaging_ws_client::ping(std::vector<uint8_t>&& payload) -> VoidResult
	{
		return send_ping(std::move(payload));
	}

	auto messaging_ws_client::close(uint16_t code, std::string_view reason) -> VoidResult
	{
		std::lock_guard<std::mutex> lock(ws_socket_mutex_);
		if (!ws_socket_)
		{
			return error_void(error_codes::network_system::connection_closed,
							  "WebSocket not connected");
		}

		ws_socket_->async_close(static_cast<internal::ws_close_code>(code),
		                        std::string(reason), [](std::error_code) {});
		return ok();
	}

	// ========================================================================
	// Interface callback setters
	// ========================================================================

	auto messaging_ws_client::set_text_callback(
		interfaces::i_websocket_client::text_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::text_message)>(std::move(callback));
	}

	auto messaging_ws_client::set_binary_callback(
		interfaces::i_websocket_client::binary_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::binary_message)>(std::move(callback));
	}

	auto messaging_ws_client::set_connected_callback(
		interfaces::i_websocket_client::connected_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::connected)>(std::move(callback));
	}

	auto messaging_ws_client::set_disconnected_callback(
		interfaces::i_websocket_client::disconnected_callback_t callback) -> void
	{
		// Adapt interface callback to internal callback type
		if (callback) {
			callbacks_.set<to_index(callback_index::disconnected)>(
				[callback = std::move(callback)](internal::ws_close_code code, const std::string& reason) {
					callback(static_cast<uint16_t>(code), reason);
				});
		} else {
			callbacks_.set<to_index(callback_index::disconnected)>(disconnected_callback_t{});
		}
	}

	auto messaging_ws_client::set_error_callback(
		interfaces::i_websocket_client::error_callback_t callback) -> void
	{
		callbacks_.set<to_index(callback_index::error)>(std::move(callback));
	}

	// ========================================================================
	// Internal Implementation
	// ========================================================================

	auto messaging_ws_client::do_start_impl(std::string_view host, uint16_t port,
	                                        std::string_view path) -> VoidResult
	{
		try
		{
			// Store config if not already set
			if (config_.host.empty())
			{
				config_.host = std::string(host);
				config_.port = port;
				config_.path = std::string(path);
			}

			// Create io_context and work guard
			io_context_ = std::make_unique<asio::io_context>();
			work_guard_ = std::make_unique<
				asio::executor_work_guard<asio::io_context::executor_type>>(
				asio::make_work_guard(*io_context_));

			// Get thread pool from network context
			thread_pool_ = network_context::instance().get_thread_pool();
			if (!thread_pool_) {
				// Fallback: create a temporary thread pool if network_context is not initialized
				NETWORK_LOG_WARN("[messaging_ws_client] network_context not initialized, creating temporary thread pool");
				thread_pool_ = std::make_shared<integration::basic_thread_pool>(std::thread::hardware_concurrency());
			}

			// Start io_context on thread pool
			io_context_future_ = thread_pool_->submit([this]() {
				try {
					NETWORK_LOG_DEBUG("[messaging_ws_client] io_context started");
					io_context_->run();
					NETWORK_LOG_DEBUG("[messaging_ws_client] io_context stopped");
				} catch (const std::exception& e) {
					NETWORK_LOG_ERROR("[messaging_ws_client] Exception in io_context: " +
									  std::string(e.what()));
				}
			});

			// Start async connection
			do_connect();

			NETWORK_LOG_INFO("[messaging_ws_client] Client started (ID: " +
							 client_id_ + ")");

			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(error_codes::network_system::connection_failed,
							  std::string("Failed to start client: ") + e.what());
		}
	}

	auto messaging_ws_client::do_stop_impl() -> VoidResult
	{
		try
		{
			// Close WebSocket connection
			{
				std::lock_guard<std::mutex> lock(ws_socket_mutex_);
				if (ws_socket_ && ws_socket_->is_open())
				{
					ws_socket_->async_close(
						internal::ws_close_code::normal, "",
						[](std::error_code) {});
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
					NETWORK_LOG_ERROR("[messaging_ws_client] Exception while waiting for io_context: " +
									  std::string(e.what()));
				}
			}

			NETWORK_LOG_INFO("[messaging_ws_client] Client stopped (ID: " +
							 client_id_ + ")");

			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(error_codes::common_errors::internal_error,
							  std::string("Failed to stop client: ") + e.what());
		}
	}

	auto messaging_ws_client::do_connect() -> void
	{
		tcp::resolver resolver(*io_context_);
		auto endpoints = resolver.resolve(config_.host, std::to_string(config_.port));

		auto socket = std::make_shared<tcp::socket>(*io_context_);

		asio::async_connect(
			*socket, endpoints,
			[this, socket](std::error_code ec, tcp::endpoint)
			{
				if (ec)
				{
					NETWORK_LOG_ERROR("[messaging_ws_client] Connection failed: " +
									  ec.message());
					invoke_error_callback(ec);
					return;
				}

				// Wrap in tcp_socket
				auto tcp_sock = std::make_shared<internal::tcp_socket>(std::move(*socket));

				// Wrap in websocket_socket
				{
					std::lock_guard<std::mutex> lock(ws_socket_mutex_);
					ws_socket_ = std::make_shared<internal::websocket_socket>(tcp_sock, true);

					// Set WebSocket callbacks
					ws_socket_->set_message_callback(
						[this](const internal::ws_message& msg) { on_message(msg); });

					ws_socket_->set_ping_callback(
						[this](const std::vector<uint8_t>& payload) { on_ping(payload); });

					ws_socket_->set_close_callback(
						[this](internal::ws_close_code code, const std::string& reason)
						{ on_close(code, reason); });

					ws_socket_->set_error_callback(
						[this](std::error_code ec) { on_error(ec); });

					// Perform handshake
					ws_socket_->async_handshake(
						config_.host, config_.path, config_.port,
						[this](std::error_code ec)
						{
							if (ec)
							{
								NETWORK_LOG_ERROR(
									"[messaging_ws_client] Handshake failed: " + ec.message());
								invoke_error_callback(ec);
								return;
							}

							is_connected_.store(true, std::memory_order_release);
							NETWORK_LOG_INFO(
								"[messaging_ws_client] Connected (ID: " +
								client_id_ + ")");

							// Start reading frames
							ws_socket_->start_read();

							// Invoke connected callback
							invoke_connected_callback();
						});
				}
			});
	}

	auto messaging_ws_client::on_message(const internal::ws_message& msg) -> void
	{
		invoke_message_callback(msg);
	}

	auto messaging_ws_client::on_ping(const std::vector<uint8_t>& payload) -> void
	{
		if (config_.auto_pong)
		{
			std::lock_guard<std::mutex> lock(ws_socket_mutex_);
			if (ws_socket_)
			{
				ws_socket_->async_send_ping(std::vector<uint8_t>(payload),
											[](std::error_code) {});
			}
		}
	}

	auto messaging_ws_client::on_close(internal::ws_close_code code,
	                                   const std::string& reason) -> void
	{
		is_connected_.store(false, std::memory_order_release);
		NETWORK_LOG_INFO("[messaging_ws_client] Connection closed (ID: " +
						 client_id_ + ")");

		invoke_disconnected_callback(code, reason);
	}

	auto messaging_ws_client::on_error(std::error_code ec) -> void
	{
		NETWORK_LOG_ERROR("[messaging_ws_client] Error: " + ec.message());
		invoke_error_callback(ec);
	}

	// ========================================================================
	// Internal Callback Helpers
	// ========================================================================

	auto messaging_ws_client::invoke_message_callback(const internal::ws_message& msg) -> void
	{
		callbacks_.invoke<to_index(callback_index::message)>(msg);

		if (msg.type == internal::ws_message_type::text)
		{
			callbacks_.invoke<to_index(callback_index::text_message)>(msg.as_text());
		}
		else if (msg.type == internal::ws_message_type::binary)
		{
			callbacks_.invoke<to_index(callback_index::binary_message)>(msg.as_binary());
		}
	}

	auto messaging_ws_client::invoke_connected_callback() -> void
	{
		callbacks_.invoke<to_index(callback_index::connected)>();
	}

	auto messaging_ws_client::invoke_disconnected_callback(internal::ws_close_code code,
	                                                       const std::string& reason) -> void
	{
		callbacks_.invoke<to_index(callback_index::disconnected)>(code, reason);
	}

	auto messaging_ws_client::invoke_error_callback(std::error_code ec) -> void
	{
		callbacks_.invoke<to_index(callback_index::error)>(ec);
	}

} // namespace kcenon::network::core
