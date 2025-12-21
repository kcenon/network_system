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

#include "kcenon/network/core/messaging_ws_client.h"

#include "kcenon/network/core/network_context.h"
#include "kcenon/network/internal/tcp_socket.h"
#include "kcenon/network/internal/websocket_socket.h"
#include "kcenon/network/integration/logger_integration.h"

#include <asio.hpp>

#include <atomic>
#include <future>
#include <mutex>
#include <thread>

namespace kcenon::network::core
{
	using tcp = asio::ip::tcp;

	// ========================================================================
	// messaging_ws_client::impl
	// ========================================================================

	class messaging_ws_client::impl
	{
	public:
		impl(const std::string& client_id)
			: client_id_(client_id)
		{
		}

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

		auto start(const ws_client_config& config) -> VoidResult
		{
			if (is_running_.load())
			{
				return error_void(error_codes::common_errors::already_exists,
								  "Client is already running");
			}

			config_ = config;

			try
			{
				is_running_.store(true);
				is_connected_.store(false);

				// Create io_context and work guard
				io_context_ = std::make_unique<asio::io_context>();
				work_guard_ = std::make_unique<
					asio::executor_work_guard<asio::io_context::executor_type>>(
					asio::make_work_guard(*io_context_));

				// Setup stop promise/future
				stop_promise_.emplace();
				stop_future_ = stop_promise_->get_future();

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
				is_running_.store(false);
				return error_void(error_codes::network_system::connection_failed,
								  std::string("Failed to start client: ") + e.what());
			}
		}

		auto stop() -> VoidResult
		{
			if (!is_running_.load())
			{
				return error_void(error_codes::common_errors::not_initialized,
								  "Client is not running");
			}

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

				is_running_.store(false);
				is_connected_.store(false);

				// Notify stop_future
				if (stop_promise_)
				{
					stop_promise_->set_value();
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

		auto wait_for_stop() -> void
		{
			if (stop_future_.valid())
			{
				stop_future_.wait();
			}
		}

		auto is_running() const -> bool { return is_running_.load(); }

		auto is_connected() const -> bool { return is_connected_.load(); }

		auto send_text(std::string&& message,
					   std::function<void(std::error_code, std::size_t)> handler)
			-> VoidResult
		{
			std::lock_guard<std::mutex> lock(ws_socket_mutex_);
			if (!ws_socket_)
			{
				return error_void(error_codes::network_system::connection_closed,
								  "WebSocket not connected");
			}

			return ws_socket_->async_send_text(std::move(message),
											   std::move(handler));
		}

		auto send_binary(std::vector<uint8_t>&& data,
						 std::function<void(std::error_code, std::size_t)> handler)
			-> VoidResult
		{
			std::lock_guard<std::mutex> lock(ws_socket_mutex_);
			if (!ws_socket_)
			{
				return error_void(error_codes::network_system::connection_closed,
								  "WebSocket not connected");
			}

			return ws_socket_->async_send_binary(std::move(data), std::move(handler));
		}

		auto send_ping(std::vector<uint8_t>&& payload) -> VoidResult
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

		auto close(internal::ws_close_code code, const std::string& reason)
			-> VoidResult
		{
			std::lock_guard<std::mutex> lock(ws_socket_mutex_);
			if (!ws_socket_)
			{
				return error_void(error_codes::network_system::connection_closed,
								  "WebSocket not connected");
			}

			ws_socket_->async_close(code, reason, [](std::error_code) {});
			return ok();
		}

		auto set_message_callback(
			std::function<void(const internal::ws_message&)> callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			message_callback_ = std::move(callback);
		}

		auto set_text_message_callback(
			std::function<void(const std::string&)> callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			text_message_callback_ = std::move(callback);
		}

		auto set_binary_message_callback(
			std::function<void(const std::vector<uint8_t>&)> callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			binary_message_callback_ = std::move(callback);
		}

		auto set_connected_callback(std::function<void()> callback) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			connected_callback_ = std::move(callback);
		}

		auto set_disconnected_callback(
			std::function<void(internal::ws_close_code, const std::string&)> callback)
			-> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			disconnected_callback_ = std::move(callback);
		}

		auto set_error_callback(std::function<void(std::error_code)> callback)
			-> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			error_callback_ = std::move(callback);
		}

		auto client_id() const -> const std::string& { return client_id_; }

	private:
		auto do_connect() -> void
		{
			tcp::resolver resolver(*io_context_);
			auto endpoints =
				resolver.resolve(config_.host, std::to_string(config_.port));

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
					auto tcp_sock =
						std::make_shared<internal::tcp_socket>(std::move(*socket));

					// Wrap in websocket_socket
					{
						std::lock_guard<std::mutex> lock(ws_socket_mutex_);
						ws_socket_ = std::make_shared<internal::websocket_socket>(
							tcp_sock, true);

						// Set WebSocket callbacks
						ws_socket_->set_message_callback(
							[this](const internal::ws_message& msg)
							{ on_message(msg); });

						ws_socket_->set_ping_callback(
							[this](const std::vector<uint8_t>& payload)
							{ on_ping(payload); });

						ws_socket_->set_close_callback(
							[this](internal::ws_close_code code,
								   const std::string& reason)
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
										"[messaging_ws_client] Handshake failed: " +
										ec.message());
									invoke_error_callback(ec);
									return;
								}

								is_connected_.store(true);
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

		auto on_message(const internal::ws_message& msg) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);

			if (message_callback_)
			{
				message_callback_(msg);
			}

			if (msg.type == internal::ws_message_type::text &&
				text_message_callback_)
			{
				text_message_callback_(msg.as_text());
			}
			else if (msg.type == internal::ws_message_type::binary &&
					 binary_message_callback_)
			{
				binary_message_callback_(msg.as_binary());
			}
		}

		auto on_ping(const std::vector<uint8_t>& payload) -> void
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

		auto on_close(internal::ws_close_code code, const std::string& reason)
			-> void
		{
			is_connected_.store(false);
			NETWORK_LOG_INFO("[messaging_ws_client] Connection closed (ID: " +
							 client_id_ + ")");

			invoke_disconnected_callback(code, reason);
		}

		auto on_error(std::error_code ec) -> void
		{
			NETWORK_LOG_ERROR("[messaging_ws_client] Error: " + ec.message());
			invoke_error_callback(ec);
		}

		auto invoke_connected_callback() -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (connected_callback_)
			{
				connected_callback_();
			}
		}

		auto invoke_disconnected_callback(internal::ws_close_code code,
										  const std::string& reason) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (disconnected_callback_)
			{
				disconnected_callback_(code, reason);
			}
		}

		auto invoke_error_callback(std::error_code ec) -> void
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (error_callback_)
			{
				error_callback_(ec);
			}
		}

	private:
		std::string client_id_;
		ws_client_config config_;

		std::unique_ptr<asio::io_context> io_context_;
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
			work_guard_;

		std::shared_ptr<integration::thread_pool_interface> thread_pool_;
		std::future<void> io_context_future_;

		std::shared_ptr<internal::websocket_socket> ws_socket_;
		std::mutex ws_socket_mutex_;

		std::atomic<bool> is_running_{false};
		std::atomic<bool> is_connected_{false};

		std::optional<std::promise<void>> stop_promise_;
		std::future<void> stop_future_;

		std::mutex callback_mutex_;
		std::function<void(const internal::ws_message&)> message_callback_;
		std::function<void(const std::string&)> text_message_callback_;
		std::function<void(const std::vector<uint8_t>&)> binary_message_callback_;
		std::function<void()> connected_callback_;
		std::function<void(internal::ws_close_code, const std::string&)>
			disconnected_callback_;
		std::function<void(std::error_code)> error_callback_;
	};

	// ========================================================================
	// messaging_ws_client
	// ========================================================================

	messaging_ws_client::messaging_ws_client(const std::string& client_id)
		: pimpl_(std::make_unique<impl>(client_id))
	{
	}

	messaging_ws_client::~messaging_ws_client() = default;

	auto messaging_ws_client::start_client(const ws_client_config& config)
		-> VoidResult
	{
		return pimpl_->start(config);
	}

	auto messaging_ws_client::start_client(std::string_view host, uint16_t port,
										   std::string_view path) -> VoidResult
	{
		ws_client_config config;
		config.host = std::string(host);
		config.port = port;
		config.path = std::string(path);
		return pimpl_->start(config);
	}

	auto messaging_ws_client::stop_client() -> VoidResult
	{
		return pimpl_->stop();
	}

	auto messaging_ws_client::wait_for_stop() -> void { pimpl_->wait_for_stop(); }

	auto messaging_ws_client::is_running() const -> bool
	{
		return pimpl_->is_running();
	}

	auto messaging_ws_client::is_connected() const -> bool
	{
		return pimpl_->is_connected();
	}

	auto messaging_ws_client::send_text(
		std::string&& message,
		std::function<void(std::error_code, std::size_t)> handler) -> VoidResult
	{
		return pimpl_->send_text(std::move(message), std::move(handler));
	}

	auto messaging_ws_client::send_binary(
		std::vector<uint8_t>&& data,
		std::function<void(std::error_code, std::size_t)> handler) -> VoidResult
	{
		return pimpl_->send_binary(std::move(data), std::move(handler));
	}

	auto messaging_ws_client::send_ping(std::vector<uint8_t>&& payload)
		-> VoidResult
	{
		return pimpl_->send_ping(std::move(payload));
	}

	auto messaging_ws_client::close(internal::ws_close_code code,
									const std::string& reason) -> VoidResult
	{
		return pimpl_->close(code, reason);
	}

	auto messaging_ws_client::set_message_callback(
		std::function<void(const internal::ws_message&)> callback) -> void
	{
		pimpl_->set_message_callback(std::move(callback));
	}

	auto messaging_ws_client::set_text_message_callback(
		std::function<void(const std::string&)> callback) -> void
	{
		pimpl_->set_text_message_callback(std::move(callback));
	}

	auto messaging_ws_client::set_binary_message_callback(
		std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		pimpl_->set_binary_message_callback(std::move(callback));
	}

	auto messaging_ws_client::set_connected_callback(std::function<void()> callback)
		-> void
	{
		pimpl_->set_connected_callback(std::move(callback));
	}

	auto messaging_ws_client::set_disconnected_callback(
		std::function<void(internal::ws_close_code, const std::string&)> callback)
		-> void
	{
		pimpl_->set_disconnected_callback(std::move(callback));
	}

	auto messaging_ws_client::set_error_callback(
		std::function<void(std::error_code)> callback) -> void
	{
		pimpl_->set_error_callback(std::move(callback));
	}

	auto messaging_ws_client::client_id() const -> const std::string&
	{
		return pimpl_->client_id();
	}

} // namespace kcenon::network::core
