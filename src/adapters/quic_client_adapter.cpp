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

#include "internal/adapters/quic_client_adapter.h"

#include "kcenon/network/detail/utils/result_types.h"

// Enable experimental API for QUIC
#ifndef NETWORK_USE_EXPERIMENTAL
#define NETWORK_USE_EXPERIMENTAL
#endif

#include "internal/experimental/quic_client.h"

using kcenon::network::ok;
using kcenon::network::error_void;
namespace error_codes = kcenon::network::error_codes;

namespace kcenon::network::internal::adapters {

quic_client_adapter::quic_client_adapter(std::string_view client_id)
	: client_id_(client_id)
	, client_(std::make_shared<core::messaging_quic_client>(client_id))
{
	setup_internal_callbacks();
}

quic_client_adapter::~quic_client_adapter()
{
	if (client_ && client_->is_running())
	{
		(void)client_->stop();
	}
}

// =========================================================================
// QUIC-specific Configuration
// =========================================================================

auto quic_client_adapter::set_alpn_protocols(const std::vector<std::string>& protocols) -> void
{
	alpn_protocols_ = protocols;
	if (client_)
	{
		client_->set_alpn_protocols(protocols);
	}
}

auto quic_client_adapter::set_ca_cert_path(std::string_view path) -> void
{
	ca_cert_path_ = std::string(path);
}

auto quic_client_adapter::set_client_cert(std::string_view cert_path, std::string_view key_path) -> void
{
	client_cert_path_ = std::string(cert_path);
	client_key_path_ = std::string(key_path);
}

auto quic_client_adapter::set_verify_server(bool verify) -> void
{
	verify_server_ = verify;
}

auto quic_client_adapter::set_max_idle_timeout(uint64_t timeout_ms) -> void
{
	max_idle_timeout_ms_ = timeout_ms;
}

// =========================================================================
// i_network_component interface implementation
// =========================================================================

auto quic_client_adapter::is_running() const -> bool
{
	return client_ && client_->is_running();
}

auto quic_client_adapter::wait_for_stop() -> void
{
	if (client_)
	{
		client_->wait_for_stop();
	}
}

// =========================================================================
// i_protocol_client interface implementation
// =========================================================================

auto quic_client_adapter::start(std::string_view host, uint16_t port) -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"quic_client_adapter::start"
		);
	}

	// Build configuration
	core::quic_client_config config;
	config.ca_cert_file = ca_cert_path_;
	config.client_cert_file = client_cert_path_;
	config.client_key_file = client_key_path_;
	config.verify_server = verify_server_;
	config.alpn_protocols = alpn_protocols_;
	config.max_idle_timeout_ms = max_idle_timeout_ms_;

	return client_->start_client(host, port, config);
}

auto quic_client_adapter::stop() -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"quic_client_adapter::stop"
		);
	}

	return client_->stop_client();
}

auto quic_client_adapter::send(std::vector<uint8_t>&& data) -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"quic_client_adapter::send"
		);
	}

	if (!client_->is_connected())
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Client is not connected",
			"quic_client_adapter::send"
		);
	}

	return client_->send(std::move(data));
}

auto quic_client_adapter::is_connected() const -> bool
{
	return client_ && client_->is_connected();
}

auto quic_client_adapter::set_observer(std::shared_ptr<interfaces::connection_observer> observer) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	observer_ = std::move(observer);
}

auto quic_client_adapter::set_receive_callback(receive_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	receive_callback_ = std::move(callback);
}

auto quic_client_adapter::set_connected_callback(connected_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	connected_callback_ = std::move(callback);
}

auto quic_client_adapter::set_disconnected_callback(disconnected_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	disconnected_callback_ = std::move(callback);
}

auto quic_client_adapter::set_error_callback(error_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	error_callback_ = std::move(callback);
}

// =========================================================================
// Internal methods
// =========================================================================

auto quic_client_adapter::setup_internal_callbacks() -> void
{
	if (!client_)
	{
		return;
	}

	// Bridge receive callback
	client_->set_receive_callback([this](const std::vector<uint8_t>& data) {
		std::shared_ptr<interfaces::connection_observer> observer_copy;
		receive_callback_t callback_copy;
		{
			std::lock_guard<std::mutex> lock(callbacks_mutex_);
			observer_copy = observer_;
			callback_copy = receive_callback_;
		}

		if (observer_copy)
		{
			observer_copy->on_receive(data);
		}
		if (callback_copy)
		{
			callback_copy(data);
		}
	});

	// Bridge connected callback
	client_->set_connected_callback([this]() {
		std::shared_ptr<interfaces::connection_observer> observer_copy;
		connected_callback_t callback_copy;
		{
			std::lock_guard<std::mutex> lock(callbacks_mutex_);
			observer_copy = observer_;
			callback_copy = connected_callback_;
		}

		if (observer_copy)
		{
			observer_copy->on_connected();
		}
		if (callback_copy)
		{
			callback_copy();
		}
	});

	// Bridge disconnected callback
	client_->set_disconnected_callback([this]() {
		std::shared_ptr<interfaces::connection_observer> observer_copy;
		disconnected_callback_t callback_copy;
		{
			std::lock_guard<std::mutex> lock(callbacks_mutex_);
			observer_copy = observer_;
			callback_copy = disconnected_callback_;
		}

		if (observer_copy)
		{
			observer_copy->on_disconnected();
		}
		if (callback_copy)
		{
			callback_copy();
		}
	});

	// Bridge error callback
	client_->set_error_callback([this](std::error_code ec) {
		std::shared_ptr<interfaces::connection_observer> observer_copy;
		error_callback_t callback_copy;
		{
			std::lock_guard<std::mutex> lock(callbacks_mutex_);
			observer_copy = observer_;
			callback_copy = error_callback_;
		}

		if (observer_copy)
		{
			observer_copy->on_error(ec);
		}
		if (callback_copy)
		{
			callback_copy(ec);
		}
	});
}

} // namespace kcenon::network::internal::adapters
