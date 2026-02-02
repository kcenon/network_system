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

#pragma once

#include "internal/tcp/tcp_socket.h"
#include "kcenon/network/integration/io_context_thread_manager.h"

#ifdef BUILD_TLS_SUPPORT
#include "internal/tcp/secure_tcp_socket.h"
#endif

#include <chrono>

namespace kcenon::network::core
{

using tcp = asio::ip::tcp;

// =====================================================================
// Constructor / Destructor
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
unified_messaging_client<Protocol, TlsPolicy>::unified_messaging_client(
	std::string_view client_id)
	: client_id_(client_id)
{
}

#ifdef BUILD_TLS_SUPPORT
template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
unified_messaging_client<Protocol, TlsPolicy>::unified_messaging_client(
	std::string_view client_id,
	const TlsPolicy& tls_config)
	requires policy::is_tls_enabled_v<TlsPolicy>
	: client_id_(client_id)
	, tls_config_(tls_config)
{
}
#endif

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
unified_messaging_client<Protocol, TlsPolicy>::~unified_messaging_client() noexcept
{
	if (is_running())
	{
		[[maybe_unused]] auto _ = stop_client();
	}
}

// =====================================================================
// Lifecycle Management
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::start_client(
	std::string_view host,
	uint16_t port) -> VoidResult
{
	if (host.empty())
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Host cannot be empty",
			"unified_messaging_client::start_client");
	}

	if (!lifecycle_.try_start())
	{
		return error_void(
			error_codes::common_errors::already_exists,
			"Client is already running",
			"unified_messaging_client::start_client",
			"Client ID: " + client_id_);
	}

	is_connected_.store(false, std::memory_order_release);
	stop_initiated_.store(false, std::memory_order_release);

	auto result = do_start_impl(host, port);
	if (result.is_err())
	{
		lifecycle_.mark_stopped();
	}

	return result;
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::stop_client() -> VoidResult
{
	if (!lifecycle_.prepare_stop())
	{
		// If not running or already stopping, return ok (idempotent)
		return ok();
	}

	stop_initiated_.store(true, std::memory_order_release);
	auto result = do_stop_impl();
	invoke_disconnected_callback();
	lifecycle_.mark_stopped();
	return result;
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::wait_for_stop() -> void
{
	lifecycle_.wait_for_stop();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::is_running() const noexcept -> bool
{
	return lifecycle_.is_running();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::is_connected() const noexcept -> bool
{
	return is_connected_.load(std::memory_order_acquire);
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::client_id() const -> const std::string&
{
	return client_id_;
}

// =====================================================================
// Data Transfer
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::send_packet(
	std::vector<uint8_t>&& data) -> VoidResult
{
	if (!is_connected())
	{
		return error_void(
			error_codes::network_system::connection_closed,
			"Client is not connected",
			"unified_messaging_client::send_packet",
			"Client ID: " + client_id_);
	}

	if (data.empty())
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Data cannot be empty",
			"unified_messaging_client::send_packet");
	}

	return do_send_impl(std::move(data));
}

// =====================================================================
// Callback Setters
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::set_receive_callback(
	receive_callback_t callback) -> void
{
	callbacks_.template set<to_index(callback_index::receive)>(std::move(callback));
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::set_connected_callback(
	connected_callback_t callback) -> void
{
	callbacks_.template set<to_index(callback_index::connected)>(std::move(callback));
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::set_disconnected_callback(
	disconnected_callback_t callback) -> void
{
	callbacks_.template set<to_index(callback_index::disconnected)>(std::move(callback));
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::set_error_callback(
	error_callback_t callback) -> void
{
	callbacks_.template set<to_index(callback_index::error)>(std::move(callback));
}

// =====================================================================
// Internal Callback Helpers
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::set_connected(bool connected) -> void
{
	is_connected_.store(connected, std::memory_order_release);
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::invoke_receive_callback(
	const std::vector<uint8_t>& data) -> void
{
	callbacks_.template invoke<to_index(callback_index::receive)>(data);
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::invoke_connected_callback() -> void
{
	callbacks_.template invoke<to_index(callback_index::connected)>();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::invoke_disconnected_callback() -> void
{
	callbacks_.template invoke<to_index(callback_index::disconnected)>();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::invoke_error_callback(
	std::error_code ec) -> void
{
	callbacks_.template invoke<to_index(callback_index::error)>(ec);
}

// =====================================================================
// Internal Implementation Methods
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::do_start_impl(
	std::string_view host,
	uint16_t port) -> VoidResult
{
	try
	{
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			socket_.reset();
		}

		io_context_ = std::shared_ptr<asio::io_context>(
			new asio::io_context(),
			[](asio::io_context*) { /* no-op deleter - intentional leak */ });

		work_guard_ = std::make_unique<
			asio::executor_work_guard<asio::io_context::executor_type>>(
			asio::make_work_guard(*io_context_));

#ifdef BUILD_TLS_SUPPORT
		if constexpr (is_secure)
		{
			ssl_context_ = std::make_unique<asio::ssl::context>(
				asio::ssl::context::tlsv12_client);

			if (tls_config_.verify_peer)
			{
				ssl_context_->set_verify_mode(asio::ssl::verify_peer);
				if (!tls_config_.ca_path.empty())
				{
					ssl_context_->load_verify_file(tls_config_.ca_path);
				}
				else
				{
					ssl_context_->set_default_verify_paths();
				}
			}
			else
			{
				ssl_context_->set_verify_mode(asio::ssl::verify_none);
			}

			if (!tls_config_.cert_path.empty())
			{
				ssl_context_->use_certificate_chain_file(tls_config_.cert_path);
			}
			if (!tls_config_.key_path.empty())
			{
				ssl_context_->use_private_key_file(
					tls_config_.key_path,
					asio::ssl::context::pem);
			}
		}
#endif

		io_context_future_ = integration::io_context_thread_manager::instance()
			.run_io_context(io_context_, "unified_client:" + client_id());

		do_connect(host, port);

		return ok();
	}
	catch (const std::exception& e)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Failed to start client: " + std::string(e.what()),
			"unified_messaging_client::do_start_impl",
			"Client ID: " + client_id() + ", Host: " + std::string(host));
	}
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::do_stop_impl() -> VoidResult
{
	is_connected_.store(false, std::memory_order_release);

	try
	{
		std::shared_ptr<socket_type> local_socket;
		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			local_socket = std::move(socket_);
		}

		if (local_socket)
		{
			local_socket->close();
		}

		if (io_context_)
		{
			std::shared_ptr<asio::ip::tcp::resolver> resolver_to_cancel;
			std::shared_ptr<asio::ip::tcp::socket> socket_to_close;
			{
				std::lock_guard<std::mutex> lock(pending_mutex_);
				resolver_to_cancel = pending_resolver_;
				socket_to_close = pending_socket_;
			}

			if (resolver_to_cancel || socket_to_close)
			{
				auto close_promise = std::make_shared<std::promise<void>>();
				auto close_future = close_promise->get_future();

				asio::post(*io_context_,
					[resolver_to_cancel, socket_to_close, close_promise]() {
						if (resolver_to_cancel)
						{
							resolver_to_cancel->cancel();
						}
						if (socket_to_close)
						{
							asio::error_code ec;
							socket_to_close->close(ec);
						}
						close_promise->set_value();
					});

				close_future.wait_for(std::chrono::milliseconds(100));
			}
		}

		if (work_guard_)
		{
			work_guard_.reset();
		}

		if (io_context_)
		{
			integration::io_context_thread_manager::instance().stop_io_context(
				io_context_);
		}

		if (io_context_future_.valid())
		{
			io_context_future_.wait();
		}

		{
			std::lock_guard<std::mutex> lock(pending_mutex_);
			pending_resolver_.reset();
			pending_socket_.reset();
		}

#ifdef BUILD_TLS_SUPPORT
		if constexpr (is_secure)
		{
			ssl_context_.reset();
		}
#endif

		io_context_.reset();

		return ok();
	}
	catch (const std::exception& e)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Failed to stop client: " + std::string(e.what()),
			"unified_messaging_client::do_stop_impl",
			"Client ID: " + client_id());
	}
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::do_send_impl(
	std::vector<uint8_t>&& data) -> VoidResult
{
	auto local_socket = get_socket();

	if (!local_socket)
	{
		return error_void(
			error_codes::network_system::connection_closed,
			"Socket not available",
			"unified_messaging_client::do_send_impl",
			"Client ID: " + client_id());
	}

	local_socket->async_send(
		std::move(data),
		[](std::error_code, std::size_t) {
			// Silently ignore errors
		});

	return ok();
}

// =====================================================================
// Internal Connection Handlers
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::do_connect(
	std::string_view host,
	uint16_t port) -> void
{
	auto resolver = std::make_shared<tcp::resolver>(*io_context_);
	{
		std::lock_guard<std::mutex> lock(pending_mutex_);
		pending_resolver_ = resolver;
	}

	auto self = this->shared_from_this();

	resolver->async_resolve(
		std::string(host),
		std::to_string(port),
		[this, self, resolver](
			std::error_code ec,
			tcp::resolver::results_type results) {
			if (stop_initiated_.load(std::memory_order_acquire))
			{
				return;
			}

			{
				std::lock_guard<std::mutex> lock(pending_mutex_);
				if (pending_resolver_.get() == resolver.get())
				{
					pending_resolver_.reset();
				}
			}

			if (ec)
			{
				on_connection_failed(ec);
				return;
			}

			if (stop_initiated_.load(std::memory_order_acquire))
			{
				return;
			}

			auto raw_socket = std::make_shared<tcp::socket>(*io_context_);
			{
				std::lock_guard<std::mutex> lock(pending_mutex_);
				pending_socket_ = raw_socket;
			}

			asio::async_connect(
				*raw_socket,
				results,
				[this, self, raw_socket](
					std::error_code connect_ec,
					[[maybe_unused]] const tcp::endpoint& endpoint) {
					if (stop_initiated_.load(std::memory_order_acquire))
					{
						return;
					}

					{
						std::lock_guard<std::mutex> lock(pending_mutex_);
						if (pending_socket_.get() == raw_socket.get())
						{
							pending_socket_.reset();
						}
					}

					if (connect_ec)
					{
						on_connection_failed(connect_ec);
						return;
					}

					if (stop_initiated_.load(std::memory_order_acquire))
					{
						return;
					}

					if constexpr (is_secure)
					{
#ifdef BUILD_TLS_SUPPORT
						auto secure_socket = std::make_shared<internal::secure_tcp_socket>(
							std::move(*raw_socket),
							*ssl_context_);
						{
							std::lock_guard<std::mutex> lock(socket_mutex_);
							socket_ = secure_socket;
						}

						secure_socket->async_handshake(
							asio::ssl::stream_base::client,
							[this, self](std::error_code handshake_ec) {
								on_handshake_complete(handshake_ec);
							});
#endif
					}
					else
					{
						{
							std::lock_guard<std::mutex> lock(socket_mutex_);
							socket_ = std::make_shared<internal::tcp_socket>(
								std::move(*raw_socket));
						}
						on_connect(connect_ec);
					}
				});
		});
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::on_connect(
	std::error_code ec) -> void
{
	if (ec)
	{
		return;
	}

	set_connected(true);
	invoke_connected_callback();

	auto self = this->shared_from_this();
	auto local_socket = get_socket();

	if (local_socket)
	{
		local_socket->set_receive_callback_view(
			[this, self](std::span<const uint8_t> chunk) {
				on_receive(chunk);
			});
		local_socket->set_error_callback(
			[this, self](std::error_code err) {
				on_error(err);
			});
		local_socket->start_read();
	}
}

#ifdef BUILD_TLS_SUPPORT
template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::on_handshake_complete(
	std::error_code ec) -> void
	requires is_secure
{
	if (ec)
	{
		on_connection_failed(ec);
		return;
	}

	set_connected(true);
	invoke_connected_callback();

	auto self = this->shared_from_this();
	auto local_socket = get_socket();

	if (local_socket)
	{
		local_socket->set_receive_callback_view(
			[this, self](std::span<const uint8_t> chunk) {
				on_receive(chunk);
			});
		local_socket->set_error_callback(
			[this, self](std::error_code err) {
				on_error(err);
			});
		local_socket->start_read();
	}
}
#endif

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::on_receive(
	std::span<const uint8_t> data) -> void
{
	if (!is_connected())
	{
		return;
	}

	std::vector<uint8_t> data_copy(data.begin(), data.end());
	invoke_receive_callback(data_copy);
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::on_error(
	std::error_code ec) -> void
{
	invoke_error_callback(ec);

	if (is_connected())
	{
		invoke_disconnected_callback();
	}

	set_connected(false);
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::on_connection_failed(
	std::error_code ec) -> void
{
	set_connected(false);
	invoke_error_callback(ec);
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_client<Protocol, TlsPolicy>::get_socket() const
	-> std::shared_ptr<socket_type>
{
	std::lock_guard<std::mutex> lock(socket_mutex_);
	return socket_;
}

} // namespace kcenon::network::core
