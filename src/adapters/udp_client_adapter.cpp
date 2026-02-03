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

#include "internal/adapters/udp_client_adapter.h"

#include "internal/core/messaging_udp_client.h"

namespace kcenon::network::internal::adapters {

udp_client_adapter::udp_client_adapter(std::string_view client_id)
	: client_id_(client_id)
	, client_(std::make_shared<core::messaging_udp_client>(std::string(client_id)))
{
	setup_internal_callbacks();
}

udp_client_adapter::~udp_client_adapter()
{
	if (client_ && client_->is_running())
	{
		(void)client_->stop();
	}
}

// =========================================================================
// i_network_component interface implementation
// =========================================================================

auto udp_client_adapter::is_running() const -> bool
{
	return client_ && client_->is_running();
}

auto udp_client_adapter::wait_for_stop() -> void
{
	if (client_)
	{
		client_->wait_for_stop();
	}
}

// =========================================================================
// i_protocol_client interface implementation
// =========================================================================

auto udp_client_adapter::start(std::string_view host, uint16_t port) -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"udp_client_adapter::start"
		);
	}

	auto result = client_->start(host, port);
	if (result.is_ok())
	{
		is_connected_.store(true, std::memory_order_release);

		// Invoke connected callback/observer since we've "connected" to the target
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
	}

	return result;
}

auto udp_client_adapter::stop() -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"udp_client_adapter::stop"
		);
	}

	bool was_connected = is_connected_.exchange(false, std::memory_order_acq_rel);
	auto result = client_->stop();

	// Invoke disconnected callback/observer if we were connected
	if (was_connected)
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
	}

	return result;
}

auto udp_client_adapter::send(std::vector<uint8_t>&& data) -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"udp_client_adapter::send"
		);
	}

	if (!is_connected_.load(std::memory_order_acquire))
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Client is not connected - call start() first",
			"udp_client_adapter::send"
		);
	}

	return client_->send(std::move(data));
}

auto udp_client_adapter::is_connected() const -> bool
{
	return is_connected_.load(std::memory_order_acquire);
}

auto udp_client_adapter::set_observer(std::shared_ptr<interfaces::connection_observer> observer) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	observer_ = std::move(observer);
}

auto udp_client_adapter::set_receive_callback(receive_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	receive_callback_ = std::move(callback);
}

auto udp_client_adapter::set_connected_callback(connected_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	connected_callback_ = std::move(callback);
}

auto udp_client_adapter::set_disconnected_callback(disconnected_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	disconnected_callback_ = std::move(callback);
}

auto udp_client_adapter::set_error_callback(error_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	error_callback_ = std::move(callback);
}

// =========================================================================
// Internal methods
// =========================================================================

auto udp_client_adapter::setup_internal_callbacks() -> void
{
	if (!client_)
	{
		return;
	}

	// Bridge UDP receive callback to i_protocol_client callback
	// Note: UDP receive includes endpoint info which we discard for the unified interface
	client_->set_receive_callback(
		[this](const std::vector<uint8_t>& data,
		       const interfaces::i_udp_client::endpoint_info& /* endpoint */)
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

	// Bridge UDP error callback
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
