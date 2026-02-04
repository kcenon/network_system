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

#include "kcenon/network/detail/session/messaging_session.h"
#include "internal/integration/io_context_thread_manager.h"
#include "internal/integration/logger_integration.h"

#ifdef BUILD_TLS_SUPPORT
#include "kcenon/network/detail/session/secure_session.h"
#include <openssl/ssl.h>
#endif

#include <algorithm>
#include <chrono>

namespace kcenon::network::core
{

using tcp = asio::ip::tcp;

// =====================================================================
// Constructor / Destructor
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
unified_messaging_server<Protocol, TlsPolicy>::unified_messaging_server(
	std::string_view server_id)
	requires (!policy::is_tls_enabled_v<TlsPolicy>)
	: server_id_(server_id)
{
}

#ifdef BUILD_TLS_SUPPORT
template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
unified_messaging_server<Protocol, TlsPolicy>::unified_messaging_server(
	std::string_view server_id,
	const TlsPolicy& tls_config)
	requires policy::is_tls_enabled_v<TlsPolicy>
	: server_id_(server_id)
	, tls_config_(tls_config)
{
	ssl_context_ = std::make_unique<asio::ssl::context>(
		asio::ssl::context::tls_server);

	ssl_context_->set_options(
		asio::ssl::context::default_workarounds |
		asio::ssl::context::no_sslv2 |
		asio::ssl::context::no_sslv3 |
		asio::ssl::context::no_tlsv1 |
		asio::ssl::context::no_tlsv1_1 |
		asio::ssl::context::single_dh_use);

	SSL_CTX* native_ctx = ssl_context_->native_handle();
	if (native_ctx)
	{
		SSL_CTX_set_min_proto_version(native_ctx, TLS1_3_VERSION);
		SSL_CTX_set_max_proto_version(native_ctx, TLS1_3_VERSION);

		SSL_CTX_set_ciphersuites(native_ctx,
			"TLS_AES_256_GCM_SHA384:"
			"TLS_CHACHA20_POLY1305_SHA256:"
			"TLS_AES_128_GCM_SHA256");
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
	if (!tls_config_.ca_path.empty())
	{
		ssl_context_->load_verify_file(tls_config_.ca_path);
	}

	if (tls_config_.verify_peer)
	{
		ssl_context_->set_verify_mode(
			asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert);
	}
	else
	{
		ssl_context_->set_verify_mode(asio::ssl::verify_none);
	}

	NETWORK_LOG_INFO("[unified_messaging_server] SSL context initialized with TLS 1.3");
}
#endif

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
unified_messaging_server<Protocol, TlsPolicy>::~unified_messaging_server() noexcept
{
	if (is_running())
	{
		[[maybe_unused]] auto _ = stop_server();
	}

	try
	{
		{
			std::lock_guard<std::mutex> lock(acceptor_mutex_);
			if (acceptor_)
			{
				asio::error_code ec;
				acceptor_->cancel(ec);
				if (acceptor_->is_open())
				{
					acceptor_->close(ec);
				}
				acceptor_.reset();
			}
		}

		if (cleanup_timer_)
		{
			cleanup_timer_->cancel();
			cleanup_timer_.reset();
		}

		if (work_guard_)
		{
			work_guard_.reset();
		}

		if (io_context_)
		{
			io_context_->stop();
			io_context_.reset();
		}

#ifdef BUILD_TLS_SUPPORT
		if constexpr (is_secure)
		{
			ssl_context_.reset();
		}
#endif
	}
	catch (...)
	{
	}
}

// =====================================================================
// Lifecycle Management
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::start_server(
	uint16_t port) -> VoidResult
{
	if (!lifecycle_.try_start())
	{
		return error_void(
			error_codes::common_errors::already_exists,
			"Server is already running",
			"unified_messaging_server::start_server",
			"Server ID: " + server_id_);
	}

	stop_initiated_.store(false, std::memory_order_release);

	auto result = do_start_impl(port);
	if (result.is_err())
	{
		lifecycle_.mark_stopped();
	}

	return result;
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::stop_server() -> VoidResult
{
	if (!lifecycle_.prepare_stop())
	{
		return ok();
	}

	stop_initiated_.store(true, std::memory_order_release);
	auto result = do_stop_impl();
	lifecycle_.mark_stopped();
	return result;
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::wait_for_stop() -> void
{
	lifecycle_.wait_for_stop();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::is_running() const noexcept -> bool
{
	return lifecycle_.is_running();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::server_id() const -> const std::string&
{
	return server_id_;
}

// =====================================================================
// Callback Setters
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::set_connection_callback(
	connection_callback_t callback) -> void
{
	callbacks_.template set<to_index(callback_index::connection)>(std::move(callback));
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::set_disconnection_callback(
	disconnection_callback_t callback) -> void
{
	callbacks_.template set<to_index(callback_index::disconnection)>(std::move(callback));
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::set_receive_callback(
	receive_callback_t callback) -> void
{
	callbacks_.template set<to_index(callback_index::receive)>(std::move(callback));
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::set_error_callback(
	error_callback_t callback) -> void
{
	callbacks_.template set<to_index(callback_index::error)>(std::move(callback));
}

// =====================================================================
// Internal Callback Helpers
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::get_connection_callback() const
	-> connection_callback_t
{
	return callbacks_.template get<to_index(callback_index::connection)>();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::get_disconnection_callback() const
	-> disconnection_callback_t
{
	return callbacks_.template get<to_index(callback_index::disconnection)>();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::get_receive_callback() const
	-> receive_callback_t
{
	return callbacks_.template get<to_index(callback_index::receive)>();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::get_error_callback() const
	-> error_callback_t
{
	return callbacks_.template get<to_index(callback_index::error)>();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::invoke_connection_callback(
	session_ptr session) -> void
{
	callbacks_.template invoke<to_index(callback_index::connection)>(std::move(session));
}

// =====================================================================
// Internal Implementation Methods
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::do_start_impl(
	uint16_t port) -> VoidResult
{
	try
	{
		io_context_ = std::make_shared<asio::io_context>();

		work_guard_ = std::make_unique<
			asio::executor_work_guard<asio::io_context::executor_type>>(
			asio::make_work_guard(*io_context_));

		acceptor_ = std::make_unique<tcp::acceptor>(
			*io_context_, tcp::endpoint(tcp::v4(), port));

		cleanup_timer_ = std::make_unique<asio::steady_timer>(*io_context_);

		do_accept();
		start_cleanup_timer();

		io_context_future_ = integration::io_context_thread_manager::instance()
			.run_io_context(io_context_, "unified_server:" + server_id());

		if constexpr (is_secure)
		{
			NETWORK_LOG_INFO("[unified_messaging_server] Started listening on port " +
				std::to_string(port) + " (TLS/SSL enabled)");
		}
		else
		{
			NETWORK_LOG_INFO("[unified_messaging_server] Started listening on port " +
				std::to_string(port));
		}

		return ok();
	}
	catch (const std::system_error& e)
	{
		acceptor_.reset();
		cleanup_timer_.reset();
		work_guard_.reset();
		io_context_.reset();

		if (e.code() == asio::error::address_in_use ||
		    e.code() == std::errc::address_in_use)
		{
			return error_void(
				error_codes::network_system::bind_failed,
				"Failed to bind to port: address already in use",
				"unified_messaging_server::do_start_impl",
				"Port: " + std::to_string(port));
		}
		else if (e.code() == asio::error::access_denied ||
		         e.code() == std::errc::permission_denied)
		{
			return error_void(
				error_codes::network_system::bind_failed,
				"Failed to bind to port: permission denied",
				"unified_messaging_server::do_start_impl",
				"Port: " + std::to_string(port));
		}

		return error_void(
			error_codes::common_errors::internal_error,
			"Failed to start server: " + std::string(e.what()),
			"unified_messaging_server::do_start_impl",
			"Port: " + std::to_string(port));
	}
	catch (const std::exception& e)
	{
		acceptor_.reset();
		cleanup_timer_.reset();
		work_guard_.reset();
		io_context_.reset();

		return error_void(
			error_codes::common_errors::internal_error,
			"Failed to start server: " + std::string(e.what()),
			"unified_messaging_server::do_start_impl",
			"Port: " + std::to_string(port));
	}
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::do_stop_impl() -> VoidResult
{
	try
	{
		{
			std::lock_guard<std::mutex> lock(acceptor_mutex_);
			if (acceptor_)
			{
				asio::error_code ec;
				acceptor_->cancel(ec);
				if (acceptor_->is_open())
				{
					acceptor_->close(ec);
				}
			}
		}

		if (cleanup_timer_)
		{
			cleanup_timer_->cancel();
		}

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

		if (io_context_)
		{
			try
			{
				io_context_->restart();
				io_context_->poll();
			}
			catch (...)
			{
			}
		}

		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			sessions_.clear();
		}

		acceptor_.reset();
		cleanup_timer_.reset();
		io_context_.reset();

#ifdef BUILD_TLS_SUPPORT
		if constexpr (is_secure)
		{
			ssl_context_.reset();
		}
#endif

		NETWORK_LOG_INFO("[unified_messaging_server] Stopped.");
		return ok();
	}
	catch (const std::exception& e)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Failed to stop server: " + std::string(e.what()),
			"unified_messaging_server::do_stop_impl",
			"Server ID: " + server_id());
	}
}

// =====================================================================
// Internal Connection Handlers
// =====================================================================

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::do_accept() -> void
{
	std::lock_guard<std::mutex> lock(acceptor_mutex_);

	if (!is_running() || !acceptor_ || !acceptor_->is_open())
	{
		return;
	}

	auto self = this->shared_from_this();
	acceptor_->async_accept(
		[this, self](std::error_code ec, tcp::socket sock)
		{
			on_accept(ec, std::move(sock));
		});
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::on_accept(
	std::error_code ec,
	tcp::socket socket) -> void
{
	if (!is_running())
	{
		return;
	}

	if (ec)
	{
		NETWORK_LOG_ERROR("[unified_messaging_server] Accept error: " + ec.message());
#if KCENON_WITH_COMMON_SYSTEM
		connection_errors_.fetch_add(1, std::memory_order_relaxed);
		if (monitor_)
		{
			monitor_->record_metric("connection_errors",
				static_cast<double>(connection_errors_.load()));
		}
#endif
		return;
	}

	cleanup_dead_sessions();

	auto self = this->shared_from_this();
	auto receive_cb = get_receive_callback();
	auto disconnection_cb = get_disconnection_callback();
	auto error_cb = get_error_callback();

	if constexpr (is_secure)
	{
#ifdef BUILD_TLS_SUPPORT
		auto new_session = std::make_shared<session::secure_session>(
			std::move(socket), *ssl_context_, server_id_);

		if (receive_cb)
		{
			new_session->set_receive_callback(
				[self, new_session, receive_cb](const std::vector<uint8_t>& data)
				{
					receive_cb(new_session, data);
				});
		}

		if (disconnection_cb)
		{
			new_session->set_disconnection_callback(
				[self, disconnection_cb](const std::string& session_id)
				{
					disconnection_cb(session_id);
				});
		}

		if (error_cb)
		{
			new_session->set_error_callback(
				[self, new_session, error_cb](std::error_code err)
				{
					error_cb(new_session, err);
				});
		}

		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			sessions_.push_back(new_session);
		}

		new_session->start_session();
		invoke_connection_callback(new_session);
#endif
	}
	else
	{
		auto new_session = std::make_shared<session::messaging_session>(
			std::move(socket), server_id_);

		if (receive_cb)
		{
			new_session->set_receive_callback(
				[self, new_session, receive_cb](const std::vector<uint8_t>& data)
				{
					receive_cb(new_session, data);
				});
		}

		if (disconnection_cb)
		{
			new_session->set_disconnection_callback(
				[self, disconnection_cb](const std::string& session_id)
				{
					disconnection_cb(session_id);
				});
		}

		if (error_cb)
		{
			new_session->set_error_callback(
				[self, new_session, error_cb](std::error_code err)
				{
					error_cb(new_session, err);
				});
		}

		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			sessions_.push_back(new_session);
		}

		new_session->start_session();
		invoke_connection_callback(new_session);
	}

#if KCENON_WITH_COMMON_SYSTEM
	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		if (monitor_)
		{
			monitor_->record_metric("active_connections",
				static_cast<double>(sessions_.size()));
		}
	}
#endif

	do_accept();
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::cleanup_dead_sessions() -> void
{
	std::lock_guard<std::mutex> lock(sessions_mutex_);

	sessions_.erase(
		std::remove_if(sessions_.begin(), sessions_.end(),
			[](const auto& session)
			{
				return session && session->is_stopped();
			}),
		sessions_.end());

	NETWORK_LOG_DEBUG("[unified_messaging_server] Cleaned up dead sessions. Active: " +
		std::to_string(sessions_.size()));

#if KCENON_WITH_COMMON_SYSTEM
	if (monitor_)
	{
		monitor_->record_metric("active_connections",
			static_cast<double>(sessions_.size()));
	}
#endif
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::start_cleanup_timer() -> void
{
	if (!cleanup_timer_ || !is_running())
	{
		return;
	}

	cleanup_timer_->expires_after(std::chrono::seconds(30));

	auto self = this->shared_from_this();
	cleanup_timer_->async_wait(
		[this, self](const std::error_code& ec)
		{
			if (!ec && is_running())
			{
				cleanup_dead_sessions();
				start_cleanup_timer();
			}
		});
}

#if KCENON_WITH_COMMON_SYSTEM
template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::set_monitor(
	kcenon::common::interfaces::IMonitor* monitor) -> void
{
	monitor_ = monitor;
}

template <protocol::Protocol Protocol, policy::TlsPolicy TlsPolicy>
	requires std::same_as<Protocol, protocol::tcp_protocol>
auto unified_messaging_server<Protocol, TlsPolicy>::get_monitor() const
	-> kcenon::common::interfaces::IMonitor*
{
	return monitor_;
}
#endif

} // namespace kcenon::network::core
