// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "internal/adapters/ws_client_adapter.h"

#include "internal/http/websocket_client.h"

namespace kcenon::network::internal::adapters {

ws_client_adapter::ws_client_adapter(
	std::string_view client_id,
	std::chrono::milliseconds ping_interval)
	: client_id_(client_id)
	, ping_interval_(ping_interval)
	, client_(std::make_shared<core::messaging_ws_client>(std::string(client_id)))
{
	setup_internal_callbacks();
}

ws_client_adapter::~ws_client_adapter()
{
	if (client_ && client_->is_running())
	{
		(void)client_->stop();
	}
}

// =========================================================================
// Path Configuration
// =========================================================================

auto ws_client_adapter::set_path(std::string_view path) -> void
{
	path_ = std::string(path);
}

// =========================================================================
// i_network_component interface implementation
// =========================================================================

auto ws_client_adapter::is_running() const -> bool
{
	return client_ && client_->is_running();
}

auto ws_client_adapter::wait_for_stop() -> void
{
	if (client_)
	{
		client_->wait_for_stop();
	}
}

// =========================================================================
// i_protocol_client interface implementation
// =========================================================================

auto ws_client_adapter::start(std::string_view host, uint16_t port) -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"ws_client_adapter::start"
		);
	}

	// Use the stored path (set via set_path, or default "/")
	return client_->start(host, port, path_);
}

auto ws_client_adapter::stop() -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"ws_client_adapter::stop"
		);
	}

	return client_->stop();
}

auto ws_client_adapter::send(std::vector<uint8_t>&& data) -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"ws_client_adapter::send"
		);
	}

	if (!client_->is_connected())
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Client is not connected - call start() first",
			"ws_client_adapter::send"
		);
	}

	// Send as binary WebSocket frame
	return client_->send_binary(std::move(data));
}

auto ws_client_adapter::is_connected() const -> bool
{
	return client_ && client_->is_connected();
}

auto ws_client_adapter::set_observer(std::shared_ptr<interfaces::connection_observer> observer) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	observer_ = std::move(observer);
}

auto ws_client_adapter::set_receive_callback(receive_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	receive_callback_ = std::move(callback);
}

auto ws_client_adapter::set_connected_callback(connected_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	connected_callback_ = std::move(callback);
}

auto ws_client_adapter::set_disconnected_callback(disconnected_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	disconnected_callback_ = std::move(callback);
}

auto ws_client_adapter::set_error_callback(error_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	error_callback_ = std::move(callback);
}

// =========================================================================
// Internal methods
// =========================================================================

auto ws_client_adapter::setup_internal_callbacks() -> void
{
	if (!client_)
	{
		return;
	}

	// Bridge WebSocket binary callback to i_protocol_client receive callback
	client_->set_binary_callback(
		[this](const std::vector<uint8_t>& data)
		{
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

	// Bridge WebSocket connected callback
	client_->set_connected_callback(
		[this]()
		{
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

	// Bridge WebSocket disconnected callback (ignore close code and reason)
	client_->set_disconnected_callback(
		[this](uint16_t /* code */, std::string_view /* reason */)
		{
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

	// Bridge WebSocket error callback
	client_->set_error_callback(
		[this](std::error_code ec)
		{
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
