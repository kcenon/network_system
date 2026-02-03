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

#include "network_udp/core/secure_messaging_udp_server.h"
#include "internal/core/network_context.h"
#include "internal/tcp/dtls_socket.h"

#include <string_view>

namespace kcenon::network::core
{

secure_messaging_udp_server::secure_messaging_udp_server(std::string_view server_id)
	: server_id_(server_id)
{
}

secure_messaging_udp_server::~secure_messaging_udp_server() noexcept
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

auto secure_messaging_udp_server::set_certificate_chain_file(
	const std::string& file_path) -> VoidResult
{
	cert_file_ = file_path;
	return ok();
}

auto secure_messaging_udp_server::set_private_key_file(
	const std::string& file_path) -> VoidResult
{
	key_file_ = file_path;
	return ok();
}

auto secure_messaging_udp_server::init_ssl_context() -> VoidResult
{
	// Create DTLS server context
	ssl_ctx_ = SSL_CTX_new(DTLS_server_method());
	if (!ssl_ctx_)
	{
		return error_void(error_codes::common_errors::internal_error,
		                  "Failed to create DTLS context",
		                  "secure_messaging_udp_server::init_ssl_context");
	}

	// Set DTLS options
	SSL_CTX_set_options(ssl_ctx_, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);

	// Load certificate
	if (!cert_file_.empty())
	{
		if (SSL_CTX_use_certificate_chain_file(ssl_ctx_, cert_file_.c_str()) != 1)
		{
			SSL_CTX_free(ssl_ctx_);
			ssl_ctx_ = nullptr;
			return error_void(error_codes::common_errors::internal_error,
			                  "Failed to load certificate: " + cert_file_,
			                  "secure_messaging_udp_server::init_ssl_context");
		}
	}

	// Load private key
	if (!key_file_.empty())
	{
		if (SSL_CTX_use_PrivateKey_file(ssl_ctx_, key_file_.c_str(), SSL_FILETYPE_PEM) != 1)
		{
			SSL_CTX_free(ssl_ctx_);
			ssl_ctx_ = nullptr;
			return error_void(error_codes::common_errors::internal_error,
			                  "Failed to load private key: " + key_file_,
			                  "secure_messaging_udp_server::init_ssl_context");
		}

		// Verify private key matches certificate
		if (SSL_CTX_check_private_key(ssl_ctx_) != 1)
		{
			SSL_CTX_free(ssl_ctx_);
			ssl_ctx_ = nullptr;
			return error_void(error_codes::common_errors::internal_error,
			                  "Private key does not match certificate",
			                  "secure_messaging_udp_server::init_ssl_context");
		}
	}

	// Set verification mode (server doesn't verify client by default)
	SSL_CTX_set_verify(ssl_ctx_, SSL_VERIFY_NONE, nullptr);

	return ok();
}

auto secure_messaging_udp_server::start_server(uint16_t port) -> VoidResult
{
	if (is_running_.load(std::memory_order_acquire))
	{
		return error_void(error_codes::network_system::server_already_running,
		                  "Server already running",
		                  "secure_messaging_udp_server::start_server");
	}

	// Check if certificates are configured
	if (cert_file_.empty() || key_file_.empty())
	{
		return error_void(error_codes::common_errors::invalid_argument,
		                  "Certificate and private key files must be set",
		                  "secure_messaging_udp_server::start_server");
	}

	// Initialize SSL context
	auto ssl_result = init_ssl_context();
	if (!ssl_result.is_ok())
	{
		return ssl_result;
	}

	// Create io_context
	io_context_ = std::make_unique<asio::io_context>();

	// Create and bind UDP socket
	try
	{
		socket_ = std::make_unique<asio::ip::udp::socket>(
			*io_context_,
			asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
	}
	catch (const std::exception& e)
	{
		SSL_CTX_free(ssl_ctx_);
		ssl_ctx_ = nullptr;
		return error_void(error_codes::network_system::bind_failed,
		                  std::string("Failed to bind port: ") + e.what(),
		                  "secure_messaging_udp_server::start_server");
	}

	// Get thread pool from network context
	thread_pool_ = network_context::instance().get_thread_pool();
	if (!thread_pool_) {
		// Fallback: create a temporary thread pool if network_context is not initialized
		thread_pool_ = std::make_shared<integration::basic_thread_pool>(std::thread::hardware_concurrency());
	}

	// Prepare promise/future for wait_for_stop()
	stop_promise_.emplace();
	stop_future_ = stop_promise_->get_future();

	// Start io_context in background
	io_context_future_ = thread_pool_->submit(
		[this]()
		{
			auto work_guard = asio::make_work_guard(*io_context_);
			io_context_->run();
		});

	is_running_.store(true, std::memory_order_release);

	// Start receiving
	do_receive();

	return ok();
}

auto secure_messaging_udp_server::stop_server() -> VoidResult
{
	if (!is_running_.exchange(false, std::memory_order_acq_rel))
	{
		// Already stopped
		if (ssl_ctx_)
		{
			SSL_CTX_free(ssl_ctx_);
			ssl_ctx_ = nullptr;
		}
		return ok();
	}

	// Clear sessions
	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		for (auto& [endpoint, session] : sessions_)
		{
			if (session && session->socket)
			{
				session->socket->stop_receive();
			}
		}
		sessions_.clear();
	}

	// Close socket
	if (socket_)
	{
		std::error_code ec;
		socket_->close(ec);
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

	// Signal the promise for wait_for_stop()
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

	return ok();
}

auto secure_messaging_udp_server::wait_for_stop() -> void
{
	if (stop_future_.valid())
	{
		stop_future_.wait();
	}
}

auto secure_messaging_udp_server::do_receive() -> void
{
	if (!is_running_.load(std::memory_order_acquire) || !socket_)
	{
		return;
	}

	auto self = shared_from_this();
	socket_->async_receive_from(
		asio::buffer(read_buffer_),
		sender_endpoint_,
		[this, self](std::error_code ec, std::size_t length)
		{
			if (!is_running_.load(std::memory_order_acquire))
			{
				return;
			}

			if (ec)
			{
				if (ec != asio::error::operation_aborted)
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
				}
				return;
			}

			if (length > 0)
			{
				std::vector<uint8_t> data(read_buffer_.begin(),
				                          read_buffer_.begin() + length);
				process_session_data(data, sender_endpoint_);
			}

			// Continue receiving
			if (is_running_.load(std::memory_order_acquire))
			{
				do_receive();
			}
		});
}

auto secure_messaging_udp_server::process_session_data(
	[[maybe_unused]] const std::vector<uint8_t>& data,  // TODO: Use when DTLS handling is implemented
	const asio::ip::udp::endpoint& sender) -> void
{
	std::shared_ptr<dtls_session> session;

	// Look up existing session
	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		auto it = sessions_.find(sender);
		if (it != sessions_.end())
		{
			session = it->second;
		}
	}

	// Create new session if needed
	if (!session)
	{
		session = create_session(sender);
		if (!session)
		{
			return;
		}
	}

	// For a proper DTLS server, we would need to:
	// 1. Parse the DTLS record to determine if it's a ClientHello
	// 2. Handle cookie exchange for DoS protection
	// 3. Create per-client SSL objects

	// For this implementation, we rely on the dtls_socket to handle
	// the DTLS protocol, but in a real server, each client needs its own
	// SSL object and BIO pair.

	// Note: A complete DTLS server implementation would require
	// more sophisticated session management. This is a simplified version.
}

auto secure_messaging_udp_server::create_session(
	const asio::ip::udp::endpoint& client_endpoint) -> std::shared_ptr<dtls_session>
{
	// Create a new UDP socket for this client session
	// Note: In a real implementation, you might want to use a single socket
	// and demultiplex based on endpoint, but that requires more complex
	// BIO handling with OpenSSL's DTLS.

	try
	{
		asio::ip::udp::socket client_socket(*io_context_, asio::ip::udp::v4());

		auto session = std::make_shared<dtls_session>();
		session->socket = std::make_shared<internal::dtls_socket>(
			std::move(client_socket), ssl_ctx_);
		session->socket->set_peer_endpoint(client_endpoint);

		// Set receive callback
		session->socket->set_receive_callback(
			[this, client_endpoint](const std::vector<uint8_t>& data,
			                        const asio::ip::udp::endpoint& /*sender*/)
			{
				std::function<void(const std::vector<uint8_t>&, const asio::ip::udp::endpoint&)> callback;
				{
					std::lock_guard<std::mutex> lock(callback_mutex_);
					callback = receive_callback_;
				}
				if (callback)
				{
					callback(data, client_endpoint);
				}
			});

		// Perform handshake
		session->socket->async_handshake(
			internal::dtls_socket::handshake_type::server,
			[this, session, client_endpoint](std::error_code ec)
			{
				if (ec)
				{
					// Handshake failed, remove session
					{
						std::lock_guard<std::mutex> lock(sessions_mutex_);
						sessions_.erase(client_endpoint);
					}
					return;
				}

				session->handshake_complete = true;

				// Notify client connected
				std::function<void(const asio::ip::udp::endpoint&)> callback;
				{
					std::lock_guard<std::mutex> lock(callback_mutex_);
					callback = client_connected_callback_;
				}
				if (callback)
				{
					callback(client_endpoint);
				}
			});

		// Store session
		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			sessions_[client_endpoint] = session;
		}

		return session;
	}
	catch (const std::exception& /*e*/)
	{
		return nullptr;
	}
}

auto secure_messaging_udp_server::async_send_to(
	std::vector<uint8_t>&& data,
	const asio::ip::udp::endpoint& endpoint,
	std::function<void(std::error_code, std::size_t)> handler) -> void
{
	std::shared_ptr<dtls_session> session;

	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		auto it = sessions_.find(endpoint);
		if (it != sessions_.end())
		{
			session = it->second;
		}
	}

	if (!session || !session->socket || !session->handshake_complete)
	{
		if (handler)
		{
			handler(std::make_error_code(std::errc::not_connected), 0);
		}
		return;
	}

	session->socket->async_send_to(std::move(data), endpoint, std::move(handler));
}

auto secure_messaging_udp_server::set_receive_callback(
	std::function<void(const std::vector<uint8_t>&,
	                   const asio::ip::udp::endpoint&)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	receive_callback_ = std::move(callback);
}

auto secure_messaging_udp_server::set_error_callback(
	std::function<void(std::error_code)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	error_callback_ = std::move(callback);
}

auto secure_messaging_udp_server::set_client_connected_callback(
	std::function<void(const asio::ip::udp::endpoint&)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	client_connected_callback_ = std::move(callback);
}

auto secure_messaging_udp_server::set_client_disconnected_callback(
	std::function<void(const asio::ip::udp::endpoint&)> callback) -> void
{
	std::lock_guard<std::mutex> lock(callback_mutex_);
	client_disconnected_callback_ = std::move(callback);
}

} // namespace kcenon::network::core
