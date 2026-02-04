/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "internal/adapters/quic_server_adapter.h"

#include "kcenon/network/utils/result_types.h"
#include "kcenon/network/session/quic_session.h"

// Enable experimental API for QUIC
#ifndef NETWORK_USE_EXPERIMENTAL
#define NETWORK_USE_EXPERIMENTAL
#endif

#include "internal/experimental/quic_server.h"

using kcenon::network::ok;
using kcenon::network::error_void;
namespace error_codes = kcenon::network::error_codes;

namespace kcenon::network::internal::adapters {

// =========================================================================
// quic_session_wrapper implementation
// =========================================================================

quic_session_wrapper::quic_session_wrapper(
	std::string_view session_id,
	std::shared_ptr<session::quic_session> session)
	: session_id_(session_id)
	, session_(std::move(session))
{
}

auto quic_session_wrapper::id() const -> std::string_view
{
	return session_id_;
}

auto quic_session_wrapper::is_connected() const -> bool
{
	return is_connected_.load(std::memory_order_acquire) &&
	       session_ && session_->is_active();
}

auto quic_session_wrapper::send(std::vector<uint8_t>&& data) -> VoidResult
{
	if (!is_connected_.load(std::memory_order_acquire))
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Session is not connected",
			"quic_session_wrapper::send"
		);
	}

	if (!session_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Session not initialized",
			"quic_session_wrapper::send"
		);
	}

	// Send using the interface send method
	return session_->send(std::move(data));
}

auto quic_session_wrapper::close() -> void
{
	is_connected_.store(false, std::memory_order_release);
	if (session_)
	{
		(void)session_->close(0); // 0 = no error
	}
}

// =========================================================================
// quic_server_adapter implementation
// =========================================================================

quic_server_adapter::quic_server_adapter(std::string_view server_id)
	: server_id_(server_id)
	, server_(std::make_shared<core::messaging_quic_server>(server_id))
{
	setup_internal_callbacks();
}

quic_server_adapter::~quic_server_adapter()
{
	if (server_ && is_running_.load(std::memory_order_acquire))
	{
		(void)server_->stop();
	}
}

// =========================================================================
// QUIC-specific Configuration
// =========================================================================

auto quic_server_adapter::set_cert_path(std::string_view path) -> void
{
	cert_path_ = std::string(path);
}

auto quic_server_adapter::set_key_path(std::string_view path) -> void
{
	key_path_ = std::string(path);
}

auto quic_server_adapter::set_alpn_protocols(const std::vector<std::string>& protocols) -> void
{
	alpn_protocols_ = protocols;
}

auto quic_server_adapter::set_ca_cert_path(std::string_view path) -> void
{
	ca_cert_path_ = std::string(path);
}

auto quic_server_adapter::set_require_client_cert(bool require) -> void
{
	require_client_cert_ = require;
}

auto quic_server_adapter::set_max_idle_timeout(uint64_t timeout_ms) -> void
{
	max_idle_timeout_ms_ = timeout_ms;
}

auto quic_server_adapter::set_max_connections(size_t max) -> void
{
	max_connections_ = max;
}

// =========================================================================
// i_network_component interface implementation
// =========================================================================

auto quic_server_adapter::is_running() const -> bool
{
	return is_running_.load(std::memory_order_acquire);
}

auto quic_server_adapter::wait_for_stop() -> void
{
	if (server_)
	{
		server_->wait_for_stop();
	}
}

// =========================================================================
// i_protocol_server interface implementation
// =========================================================================

auto quic_server_adapter::start(uint16_t port) -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"quic_server_adapter::start"
		);
	}

	// Build configuration
	core::quic_server_config config;
	config.cert_file = cert_path_;
	config.key_file = key_path_;
	config.ca_cert_file = ca_cert_path_;
	config.require_client_cert = require_client_cert_;
	config.alpn_protocols = alpn_protocols_;
	config.max_idle_timeout_ms = max_idle_timeout_ms_;
	config.max_connections = max_connections_;

	auto result = server_->start_server(port, config);
	if (result.is_ok())
	{
		is_running_.store(true, std::memory_order_release);
	}
	return result;
}

auto quic_server_adapter::stop() -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"quic_server_adapter::stop"
		);
	}

	auto result = server_->stop_server();
	is_running_.store(false, std::memory_order_release);

	// Clear tracked sessions on stop
	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		sessions_.clear();
	}

	return result;
}

auto quic_server_adapter::connection_count() const -> size_t
{
	std::lock_guard<std::mutex> lock(sessions_mutex_);
	return sessions_.size();
}

auto quic_server_adapter::set_connection_callback(connection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	connection_callback_ = std::move(callback);
}

auto quic_server_adapter::set_disconnection_callback(disconnection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	disconnection_callback_ = std::move(callback);
}

auto quic_server_adapter::set_receive_callback(receive_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	receive_callback_ = std::move(callback);
}

auto quic_server_adapter::set_error_callback(error_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	error_callback_ = std::move(callback);
}

// =========================================================================
// Internal methods
// =========================================================================

auto quic_server_adapter::setup_internal_callbacks() -> void
{
	if (!server_)
	{
		return;
	}

	// Bridge connection callback using the legacy callback type (takes quic_session directly)
	core::messaging_quic_server::connection_callback_t conn_cb =
		[this](std::shared_ptr<session::quic_session> quic_sess) {
			if (!quic_sess)
			{
				return;
			}

			auto wrapper = get_or_create_wrapper(quic_sess);

			connection_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = connection_callback_;
			}

			if (callback_copy)
			{
				callback_copy(wrapper);
			}
		};
	server_->set_connection_callback(conn_cb);

	// Bridge disconnection callback
	server_->set_disconnection_callback(
		[this](std::string_view session_id) {
			disconnection_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = disconnection_callback_;
			}

			if (callback_copy)
			{
				callback_copy(session_id);
			}

			remove_wrapper(std::string(session_id));
		});

	// Bridge receive callback
	server_->set_receive_callback(
		[this](std::string_view session_id, const std::vector<uint8_t>& data) {
			receive_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = receive_callback_;
			}

			if (callback_copy)
			{
				callback_copy(session_id, data);
			}
		});

	// Bridge error callback
	server_->set_error_callback(
		[this](std::string_view session_id, std::error_code ec) {
			error_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = error_callback_;
			}

			if (callback_copy)
			{
				callback_copy(session_id, ec);
			}
		});
}

auto quic_server_adapter::get_or_create_wrapper(std::shared_ptr<session::quic_session> session)
	-> std::shared_ptr<quic_session_wrapper>
{
	const std::string session_id = session->session_id();

	std::lock_guard<std::mutex> lock(sessions_mutex_);

	auto it = sessions_.find(session_id);
	if (it != sessions_.end())
	{
		return it->second;
	}

	auto wrapper = std::make_shared<quic_session_wrapper>(session_id, session);
	sessions_[session_id] = wrapper;
	return wrapper;
}

auto quic_server_adapter::remove_wrapper(const std::string& session_id) -> void
{
	std::lock_guard<std::mutex> lock(sessions_mutex_);
	sessions_.erase(session_id);
}

} // namespace kcenon::network::internal::adapters
