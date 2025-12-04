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

#include "kcenon/network/core/messaging_quic_client.h"
#include "kcenon/network/integration/logger_integration.h"
#include "internal/quic_socket.h"

namespace network_system::core
{

messaging_quic_client::messaging_quic_client(std::string_view client_id)
	: client_id_(client_id)
{
}

messaging_quic_client::~messaging_quic_client() noexcept
{
	try
	{
		if (!is_running_.load(std::memory_order_acquire))
		{
			return;
		}

		auto result = stop_client();
		if (result.is_err())
		{
			NETWORK_LOG_WARN("[messaging_quic_client::~messaging_quic_client] "
			                 "Failed to stop client cleanly: " +
			                 result.error().message + " (Client ID: " + client_id_ +
			                 ")");
		}
		else
		{
			NETWORK_LOG_DEBUG("[messaging_quic_client::~messaging_quic_client] "
			                  "Client stopped successfully (Client ID: " +
			                  client_id_ + ")");
		}
	}
	catch (const std::exception& e)
	{
		NETWORK_LOG_ERROR("[messaging_quic_client::~messaging_quic_client] "
		                  "Exception during cleanup: " +
		                  std::string(e.what()) + " (Client ID: " + client_id_ +
		                  ")");
	}
	catch (...)
	{
		NETWORK_LOG_ERROR("[messaging_quic_client::~messaging_quic_client] "
		                  "Unknown exception during cleanup (Client ID: " +
		                  client_id_ + ")");
	}
}

auto messaging_quic_client::start_client(std::string_view host,
                                         unsigned short port) -> VoidResult
{
	return start_client(host, port, quic_client_config{});
}

auto messaging_quic_client::start_client(std::string_view host,
                                         unsigned short port,
                                         const quic_client_config& config) -> VoidResult
{
	if (is_running_.load())
	{
		return error_void(
			error_codes::common_errors::already_exists,
			"Client is already running",
			"messaging_quic_client::start_client",
			"Client ID: " + client_id_);
	}

	if (host.empty())
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Host cannot be empty",
			"messaging_quic_client::start_client",
			"Client ID: " + client_id_);
	}

	try
	{
		is_running_.store(true);
		is_connected_.store(false);
		stop_initiated_.store(false);
		config_ = config;

		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			socket_.reset();
		}

		io_context_ = std::make_unique<asio::io_context>();
		work_guard_ = std::make_unique<
			asio::executor_work_guard<asio::io_context::executor_type>>(
			asio::make_work_guard(*io_context_));

		stop_future_ = std::future<void>();
		stop_promise_.emplace();
		stop_future_ = stop_promise_->get_future();

		thread_pool_ = network_context::instance().get_thread_pool();
		if (!thread_pool_)
		{
			thread_pool_ = std::make_shared<integration::basic_thread_pool>(2);
		}

		io_context_future_ = thread_pool_->submit([this]() {
			try
			{
				NETWORK_LOG_INFO(
					"[messaging_quic_client] Starting io_context on thread pool");
				io_context_->run();
				NETWORK_LOG_INFO("[messaging_quic_client] io_context stopped");
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR("[messaging_quic_client] Exception in io_context: " +
				                  std::string(e.what()));
			}
		});

		do_connect(host, port);

		NETWORK_LOG_INFO("[messaging_quic_client] started. ID=" + client_id_ +
		                 " target=" + std::string(host) + ":" +
		                 std::to_string(port));

		return ok();
	}
	catch (const std::exception& e)
	{
		is_running_.store(false);
		return error_void(
			error_codes::common_errors::internal_error,
			"Failed to start client: " + std::string(e.what()),
			"messaging_quic_client::start_client",
			"Client ID: " + client_id_ + ", Host: " + std::string(host));
	}
}

auto messaging_quic_client::stop_client() -> VoidResult
{
	bool expected = false;
	if (!stop_initiated_.compare_exchange_strong(expected, true))
	{
		NETWORK_LOG_DEBUG(
			"[messaging_quic_client] stop_client already called, returning success");
		return ok();
	}

	if (!is_running_.load())
	{
		return ok();
	}

	try
	{
		is_running_.store(false);
		is_connected_.store(false);

		std::shared_ptr<internal::quic_socket> local_socket;
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			local_socket = std::move(socket_);
		}
		if (local_socket)
		{
			local_socket->stop_receive();
			auto close_result = local_socket->close();
			if (close_result.is_err())
			{
				NETWORK_LOG_WARN("[messaging_quic_client] Close error: " +
				                 close_result.error().message);
			}
		}

		if (work_guard_)
		{
			work_guard_.reset();
		}
		if (io_context_)
		{
			io_context_->stop();
		}
		if (io_context_future_.valid())
		{
			io_context_future_.wait();
		}
		thread_pool_.reset();
		io_context_.reset();

		if (stop_promise_.has_value())
		{
			try
			{
				stop_promise_->set_value();
			}
			catch (const std::future_error&)
			{
				NETWORK_LOG_DEBUG("[messaging_quic_client] Promise already satisfied");
			}
			stop_promise_.reset();
		}
		stop_future_ = std::future<void>();

		NETWORK_LOG_INFO("[messaging_quic_client] stopped.");
		return ok();
	}
	catch (const std::exception& e)
	{
		stop_initiated_.store(false);
		return error_void(
			error_codes::common_errors::internal_error,
			"Failed to stop client: " + std::string(e.what()),
			"messaging_quic_client::stop_client",
			"Client ID: " + client_id_);
	}
}

auto messaging_quic_client::wait_for_stop() -> void
{
	if (stop_future_.valid())
	{
		stop_future_.wait();
	}
}

auto messaging_quic_client::is_connected() const noexcept -> bool
{
	return is_connected_.load(std::memory_order_acquire);
}

auto messaging_quic_client::is_handshake_complete() const noexcept -> bool
{
	auto local_socket = get_socket();
	if (!local_socket)
	{
		return false;
	}
	return local_socket->is_handshake_complete();
}

auto messaging_quic_client::send_packet(std::vector<uint8_t>&& data) -> VoidResult
{
	if (!is_running_.load())
	{
		return error_void(
			error_codes::network_system::connection_closed,
			"Client is not running",
			"messaging_quic_client::send_packet",
			"Client ID: " + client_id_);
	}

	if (data.empty())
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Data cannot be empty",
			"messaging_quic_client::send_packet",
			"Client ID: " + client_id_);
	}

	auto local_socket = get_socket();
	if (!is_connected_.load() || !local_socket)
	{
		return error_void(
			error_codes::network_system::connection_closed,
			"Client is not connected",
			"messaging_quic_client::send_packet",
			"Client ID: " + client_id_);
	}

	return local_socket->send_stream_data(default_stream_id_, std::move(data));
}

auto messaging_quic_client::send_packet(std::string_view data) -> VoidResult
{
	if (data.empty())
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Data cannot be empty",
			"messaging_quic_client::send_packet",
			"Client ID: " + client_id_);
	}

	std::vector<uint8_t> byte_data(data.begin(), data.end());
	return send_packet(std::move(byte_data));
}

auto messaging_quic_client::create_stream() -> Result<uint64_t>
{
	auto local_socket = get_socket();
	if (!local_socket)
	{
		return error<uint64_t>(
			error_codes::network_system::connection_closed,
			"Client is not connected",
			"messaging_quic_client::create_stream",
			"Client ID: " + client_id_);
	}

	return local_socket->create_stream(false);
}

auto messaging_quic_client::create_unidirectional_stream() -> Result<uint64_t>
{
	auto local_socket = get_socket();
	if (!local_socket)
	{
		return error<uint64_t>(
			error_codes::network_system::connection_closed,
			"Client is not connected",
			"messaging_quic_client::create_unidirectional_stream",
			"Client ID: " + client_id_);
	}

	return local_socket->create_stream(true);
}

auto messaging_quic_client::send_on_stream(uint64_t stream_id,
                                           std::vector<uint8_t>&& data,
                                           bool fin) -> VoidResult
{
	if (data.empty())
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Data cannot be empty",
			"messaging_quic_client::send_on_stream",
			"Client ID: " + client_id_);
	}

	auto local_socket = get_socket();
	if (!local_socket)
	{
		return error_void(
			error_codes::network_system::connection_closed,
			"Client is not connected",
			"messaging_quic_client::send_on_stream",
			"Client ID: " + client_id_);
	}

	return local_socket->send_stream_data(stream_id, std::move(data), fin);
}

auto messaging_quic_client::close_stream(uint64_t stream_id) -> VoidResult
{
	auto local_socket = get_socket();
	if (!local_socket)
	{
		return error_void(
			error_codes::network_system::connection_closed,
			"Client is not connected",
			"messaging_quic_client::close_stream",
			"Client ID: " + client_id_);
	}

	return local_socket->close_stream(stream_id);
}

auto messaging_quic_client::set_receive_callback(
	std::function<void(const std::vector<uint8_t>&)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	receive_callback_ = std::move(callback);
}

auto messaging_quic_client::set_stream_receive_callback(
	std::function<void(uint64_t stream_id,
	                   const std::vector<uint8_t>& data,
	                   bool fin)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	stream_receive_callback_ = std::move(callback);
}

auto messaging_quic_client::set_connected_callback(
	std::function<void()> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	connected_callback_ = std::move(callback);
}

auto messaging_quic_client::set_disconnected_callback(
	std::function<void()> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	disconnected_callback_ = std::move(callback);
}

auto messaging_quic_client::set_error_callback(
	std::function<void(std::error_code)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	error_callback_ = std::move(callback);
}

auto messaging_quic_client::set_alpn_protocols(
	const std::vector<std::string>& protocols) -> void
{
	config_.alpn_protocols = protocols;
}

auto messaging_quic_client::alpn_protocol() const -> std::optional<std::string>
{
	// TODO: Implement ALPN negotiation result retrieval
	return std::nullopt;
}

auto messaging_quic_client::stats() const -> quic_connection_stats
{
	// TODO: Implement connection statistics retrieval
	return quic_connection_stats{};
}

auto messaging_quic_client::do_connect(std::string_view host,
                                       unsigned short port) -> void
{
	auto resolver = std::make_shared<asio::ip::udp::resolver>(*io_context_);
	auto self = shared_from_this();

	NETWORK_LOG_INFO("[messaging_quic_client] Starting async resolve for " +
	                 std::string(host) + ":" + std::to_string(port));

	resolver->async_resolve(
		std::string(host),
		std::to_string(port),
		[this, self, resolver, host_str = std::string(host)](
			std::error_code ec,
			asio::ip::udp::resolver::results_type results) {
			NETWORK_LOG_INFO("[messaging_quic_client] Resolve callback invoked");
			if (ec)
			{
				NETWORK_LOG_ERROR("[messaging_quic_client] Resolve error: " +
				                  ec.message());
				on_error(ec);
				return;
			}

			if (results.empty())
			{
				NETWORK_LOG_ERROR("[messaging_quic_client] No endpoints resolved");
				on_error(asio::error::host_not_found);
				return;
			}

			NETWORK_LOG_INFO("[messaging_quic_client] Resolve successful, creating socket");

			try
			{
				auto endpoint = *results.begin();

				// Create UDP socket
				asio::ip::udp::socket udp_socket(*io_context_, asio::ip::udp::v4());

				// Create QUIC socket as client
				auto quic_sock = std::make_shared<internal::quic_socket>(
					std::move(udp_socket),
					internal::quic_role::client);

				// Set callbacks
				quic_sock->set_stream_data_callback(
					[this, self](uint64_t stream_id,
					             std::span<const uint8_t> data,
					             bool fin) {
						on_stream_data(stream_id, data, fin);
					});

				quic_sock->set_connected_callback([this, self]() {
					on_connect();
				});

				quic_sock->set_error_callback([this, self](std::error_code err) {
					on_error(err);
				});

				quic_sock->set_close_callback(
					[this, self](uint64_t error_code, const std::string& reason) {
						on_close(error_code, reason);
					});

				// Store socket
				{
					std::lock_guard<std::mutex> lock(socket_mutex_);
					socket_ = quic_sock;
				}

				// Connect to server
				auto connect_result = quic_sock->connect(endpoint, host_str);
				if (connect_result.is_err())
				{
					NETWORK_LOG_ERROR("[messaging_quic_client] Connect failed: " +
					                  connect_result.error().message);
					on_error(std::make_error_code(std::errc::connection_refused));
					return;
				}

				// Start receive loop
				quic_sock->start_receive();

				NETWORK_LOG_INFO("[messaging_quic_client] Connection initiated");
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR("[messaging_quic_client] Exception during connect: " +
				                  std::string(e.what()));
				on_error(std::make_error_code(std::errc::connection_refused));
			}
		});
}

auto messaging_quic_client::on_connect() -> void
{
	NETWORK_LOG_INFO("[messaging_quic_client] Connected successfully.");
	is_connected_.store(true);

	// Create default stream for send_packet()
	auto local_socket = get_socket();
	if (local_socket)
	{
		auto stream_result = local_socket->create_stream(false);
		if (stream_result.is_ok())
		{
			default_stream_id_ = stream_result.value();
			NETWORK_LOG_DEBUG("[messaging_quic_client] Default stream created: " +
			                  std::to_string(default_stream_id_));
		}
	}

	// Invoke connected callback
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (connected_callback_)
		{
			try
			{
				connected_callback_();
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR(
					"[messaging_quic_client] Exception in connected callback: " +
					std::string(e.what()));
			}
		}
	}
}

auto messaging_quic_client::on_stream_data(uint64_t stream_id,
                                           std::span<const uint8_t> data,
                                           bool fin) -> void
{
	if (!is_connected_.load())
	{
		return;
	}

	NETWORK_LOG_DEBUG("[messaging_quic_client] Received " +
	                  std::to_string(data.size()) +
	                  " bytes on stream " + std::to_string(stream_id));

	std::vector<uint8_t> data_copy(data.begin(), data.end());

	// Invoke stream receive callback
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (stream_receive_callback_)
		{
			try
			{
				stream_receive_callback_(stream_id, data_copy, fin);
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR(
					"[messaging_quic_client] Exception in stream receive callback: " +
					std::string(e.what()));
			}
		}

		// Also invoke default receive callback for default stream
		if (stream_id == default_stream_id_ && receive_callback_)
		{
			try
			{
				receive_callback_(data_copy);
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR(
					"[messaging_quic_client] Exception in receive callback: " +
					std::string(e.what()));
			}
		}
	}
}

auto messaging_quic_client::on_error(std::error_code ec) -> void
{
	NETWORK_LOG_ERROR("[messaging_quic_client] Error: " + ec.message());

	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (error_callback_)
		{
			try
			{
				error_callback_(ec);
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR(
					"[messaging_quic_client] Exception in error callback: " +
					std::string(e.what()));
			}
		}
	}

	if (is_connected_.load())
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (disconnected_callback_)
		{
			try
			{
				disconnected_callback_();
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR(
					"[messaging_quic_client] Exception in disconnected callback: " +
					std::string(e.what()));
			}
		}
	}

	is_connected_.store(false);
	is_running_.store(false);
}

auto messaging_quic_client::on_close(uint64_t error_code,
                                     const std::string& reason) -> void
{
	NETWORK_LOG_INFO("[messaging_quic_client] Connection closed. Error code: " +
	                 std::to_string(error_code) + ", reason: " + reason);

	if (is_connected_.load())
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		if (disconnected_callback_)
		{
			try
			{
				disconnected_callback_();
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR(
					"[messaging_quic_client] Exception in disconnected callback: " +
					std::string(e.what()));
			}
		}
	}

	is_connected_.store(false);
	is_running_.store(false);
}

auto messaging_quic_client::get_socket() const
	-> std::shared_ptr<internal::quic_socket>
{
	std::lock_guard<std::mutex> lock(socket_mutex_);
	return socket_;
}

} // namespace network_system::core
