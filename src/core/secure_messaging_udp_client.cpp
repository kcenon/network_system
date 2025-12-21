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

#include "kcenon/network/core/secure_messaging_udp_client.h"
#include "kcenon/network/core/network_context.h"
#include "kcenon/network/internal/dtls_socket.h"

#include <chrono>
#include <condition_variable>

namespace kcenon::network::core
{

secure_messaging_udp_client::secure_messaging_udp_client(
	std::string_view client_id, bool verify_cert)
	: client_id_(client_id)
	, verify_cert_(verify_cert)
{
}

secure_messaging_udp_client::~secure_messaging_udp_client() noexcept
{
	stop_client();
}

auto secure_messaging_udp_client::init_ssl_context() -> VoidResult
{
	// Create DTLS client context
	ssl_ctx_ = SSL_CTX_new(DTLS_client_method());
	if (!ssl_ctx_)
	{
		return error_void(error_codes::common_errors::internal_error,
		                  "Failed to create DTLS context",
		                  "secure_messaging_udp_client::init_ssl_context");
	}

	// Set DTLS options
	SSL_CTX_set_options(ssl_ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

	// Set verification mode
	if (verify_cert_)
	{
		SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_PEER, nullptr);
		// Load default CA certificates
		SSL_CTX_set_default_verify_paths(ssl_ctx_);
	}
	else
	{
		SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, nullptr);
	}

	return ok();
}

auto secure_messaging_udp_client::start_client(
	std::string_view host, uint16_t port) -> VoidResult
{
	if (is_connected_.load())
	{
		return error_void(error_codes::common_errors::already_exists,
		                  "Client already running",
		                  "secure_messaging_udp_client::start_client");
	}

	if (host.empty())
	{
		return error_void(error_codes::common_errors::invalid_argument,
		                  "Host cannot be empty",
		                  "secure_messaging_udp_client::start_client");
	}

	// Initialize SSL context
	auto ssl_result = init_ssl_context();
	if (!ssl_result.is_ok())
	{
		return ssl_result;
	}

	// Create io_context
	io_context_ = std::make_unique<asio::io_context>();

	// Resolve the target endpoint
	asio::ip::udp::resolver resolver(*io_context_);
	std::error_code resolve_ec;
	auto endpoints = resolver.resolve(
		asio::ip::udp::v4(),
		std::string(host),
		std::to_string(port),
		resolve_ec);

	if (resolve_ec || endpoints.empty())
	{
		SSL_CTX_free(ssl_ctx_);
		ssl_ctx_ = nullptr;
		return error_void(error_codes::common_errors::internal_error,
		                  "Failed to resolve host: " + std::string(host),
		                  "secure_messaging_udp_client::start_client");
	}

	{
		std::lock_guard<std::mutex> lock(endpoint_mutex_);
		target_endpoint_ = *endpoints.begin();
	}

	// Create UDP socket
	asio::ip::udp::socket udp_socket(*io_context_, asio::ip::udp::v4());

	// Create DTLS socket
	socket_ = std::make_shared<internal::dtls_socket>(
		std::move(udp_socket), ssl_ctx_);

	// Set peer endpoint
	{
		std::lock_guard<std::mutex> lock(endpoint_mutex_);
		socket_->set_peer_endpoint(target_endpoint_);
	}

	// Set callbacks
	socket_->set_receive_callback(
		[this](const std::vector<uint8_t>& data, const asio::ip::udp::endpoint& sender)
		{
			std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)> callback;
			{
				std::lock_guard<std::mutex> lock(callback_mutex_);
				callback = receive_callback_;
			}
			if (callback)
			{
				callback(data, sender);
			}
		});

	socket_->set_error_callback(
		[this](std::error_code ec)
		{
			std::function<void(std::error_code)> callback;
			{
				std::lock_guard<std::mutex> lock(callback_mutex_);
				callback = error_callback_;
			}
			if (callback)
			{
				callback(ec);
			}
		});

	// Get thread pool from network context
	thread_pool_ = network_context::instance().get_thread_pool();
	if (!thread_pool_) {
		// Fallback: create a temporary thread pool if network_context is not initialized
		thread_pool_ = std::make_shared<integration::basic_thread_pool>(std::thread::hardware_concurrency());
	}

	// Start io_context in background
	io_context_future_ = thread_pool_->submit(
		[this]()
		{
			auto work_guard = asio::make_work_guard(*io_context_);
			io_context_->run();
		});

	// Perform handshake
	auto handshake_result = do_handshake();
	if (!handshake_result.is_ok())
	{
		stop_client();
		return handshake_result;
	}

	is_connected_.store(true);

	// Notify connected
	std::function<void()> connected_cb;
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		connected_cb = connected_callback_;
	}
	if (connected_cb)
	{
		connected_cb();
	}

	return ok();
}

auto secure_messaging_udp_client::do_handshake() -> VoidResult
{
	std::mutex handshake_mutex;
	std::condition_variable handshake_cv;
	bool handshake_done = false;
	std::error_code handshake_error;

	socket_->async_handshake(
		internal::dtls_socket::handshake_type::client,
		[&](std::error_code ec)
		{
			{
				std::lock_guard<std::mutex> lock(handshake_mutex);
				handshake_done = true;
				handshake_error = ec;
			}
			handshake_cv.notify_one();
		});

	// Wait for handshake with timeout
	{
		std::unique_lock<std::mutex> lock(handshake_mutex);
		if (!handshake_cv.wait_for(lock, std::chrono::seconds(30),
		                           [&] { return handshake_done; }))
		{
			return error_void(error_codes::network_system::connection_timeout,
			                  "DTLS handshake timeout",
			                  "secure_messaging_udp_client::do_handshake");
		}
	}

	if (handshake_error)
	{
		return error_void(error_codes::network_system::connection_failed,
		                  "DTLS handshake failed: " + handshake_error.message(),
		                  "secure_messaging_udp_client::do_handshake");
	}

	return ok();
}

auto secure_messaging_udp_client::stop_client() -> VoidResult
{
	if (!is_connected_.exchange(false))
	{
		// Already stopped
		if (ssl_ctx_)
		{
			SSL_CTX_free(ssl_ctx_);
			ssl_ctx_ = nullptr;
		}
		return ok();
	}

	// Stop socket
	if (socket_)
	{
		socket_->stop_receive();
		socket_.reset();
	}

	// Stop io_context
	if (io_context_)
	{
		io_context_->stop();
	}

	// Wait for io_context thread
	if (io_context_future_.valid())
	{
		io_context_future_.wait();
	}

	// Cleanup SSL context
	if (ssl_ctx_)
	{
		SSL_CTX_free(ssl_ctx_);
		ssl_ctx_ = nullptr;
	}

	// Notify disconnected
	std::function<void()> disconnected_cb;
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		disconnected_cb = disconnected_callback_;
	}
	if (disconnected_cb)
	{
		disconnected_cb();
	}

	return ok();
}

auto secure_messaging_udp_client::wait_for_stop() -> void
{
	if (io_context_future_.valid())
	{
		io_context_future_.wait();
	}
}

auto secure_messaging_udp_client::send_packet(
	std::vector<uint8_t>&& data,
	std::function<void(std::error_code, std::size_t)> handler) -> VoidResult
{
	if (!is_connected_.load())
	{
		return error_void(error_codes::common_errors::internal_error,
		                  "Client not connected",
		                  "secure_messaging_udp_client::send_packet");
	}

	if (!socket_)
	{
		return error_void(error_codes::common_errors::internal_error,
		                  "Socket not available",
		                  "secure_messaging_udp_client::send_packet");
	}

	socket_->async_send(std::move(data), std::move(handler));
	return ok();
}

auto secure_messaging_udp_client::set_receive_callback(
	std::function<void(const std::vector<uint8_t>&,
	                   const asio::ip::udp::endpoint&)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	receive_callback_ = std::move(callback);
}

auto secure_messaging_udp_client::set_error_callback(
	std::function<void(std::error_code)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	error_callback_ = std::move(callback);
}

auto secure_messaging_udp_client::set_connected_callback(
	std::function<void()> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	connected_callback_ = std::move(callback);
}

auto secure_messaging_udp_client::set_disconnected_callback(
	std::function<void()> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	disconnected_callback_ = std::move(callback);
}

} // namespace kcenon::network::core
