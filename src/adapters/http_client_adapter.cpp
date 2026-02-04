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

#include "internal/adapters/http_client_adapter.h"

#include "kcenon/network/utils/result_types.h"

// Suppress deprecation warnings for internal usage
#define NETWORK_SYSTEM_SUPPRESS_DEPRECATION_WARNINGS
#include "internal/http/http_client.h"
#undef NETWORK_SYSTEM_SUPPRESS_DEPRECATION_WARNINGS

#include <sstream>

using kcenon::network::ok;
using kcenon::network::error_void;
namespace error_codes = kcenon::network::error_codes;

namespace kcenon::network::internal::adapters {

http_client_adapter::http_client_adapter(
	std::string_view client_id,
	std::chrono::milliseconds timeout)
	: client_id_(client_id)
	, timeout_(timeout)
	, client_(std::make_shared<core::http_client>(timeout))
{
}

http_client_adapter::~http_client_adapter()
{
	is_running_.store(false, std::memory_order_release);
}

// =========================================================================
// Path Configuration
// =========================================================================

auto http_client_adapter::set_path(std::string_view path) -> void
{
	path_ = std::string(path);
}

auto http_client_adapter::set_use_ssl(bool use_ssl) -> void
{
	use_ssl_ = use_ssl;
}

// =========================================================================
// i_network_component interface implementation
// =========================================================================

auto http_client_adapter::is_running() const -> bool
{
	return is_running_.load(std::memory_order_acquire);
}

auto http_client_adapter::wait_for_stop() -> void
{
	// HTTP client is stateless, nothing to wait for
}

// =========================================================================
// i_protocol_client interface implementation
// =========================================================================

auto http_client_adapter::start(std::string_view host, uint16_t port) -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"http_client_adapter::start"
		);
	}

	// Store the connection details for building URLs
	host_ = std::string(host);
	port_ = port;
	is_running_.store(true, std::memory_order_release);

	// Notify connected callback (HTTP is "connected" when we have a target)
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
	}

	return ok();
}

auto http_client_adapter::stop() -> VoidResult
{
	if (!is_running_.load(std::memory_order_acquire))
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Client is not running",
			"http_client_adapter::stop"
		);
	}

	is_running_.store(false, std::memory_order_release);

	// Notify disconnected callback
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

	return ok();
}

auto http_client_adapter::send(std::vector<uint8_t>&& data) -> VoidResult
{
	if (!client_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Client not initialized",
			"http_client_adapter::send"
		);
	}

	if (!is_running_.load(std::memory_order_acquire))
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Client is not running - call start() first",
			"http_client_adapter::send"
		);
	}

	// Build the full URL
	const std::string url = build_url();

	// Perform HTTP POST request with the binary data
	auto result = client_->post(url, data);
	if (result.is_err())
	{
		notify_error(std::make_error_code(std::errc::io_error));
		return error_void(
			error_codes::common_errors::internal_error,
			"HTTP POST request failed: " + result.error().message,
			"http_client_adapter::send"
		);
	}

	// Notify receive callback with response body
	const auto& response = result.value();
	notify_receive(response.body);

	return ok();
}

auto http_client_adapter::is_connected() const -> bool
{
	return is_running_.load(std::memory_order_acquire) && !host_.empty();
}

auto http_client_adapter::set_observer(std::shared_ptr<interfaces::connection_observer> observer) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	observer_ = std::move(observer);
}

auto http_client_adapter::set_receive_callback(receive_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	receive_callback_ = std::move(callback);
}

auto http_client_adapter::set_connected_callback(connected_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	connected_callback_ = std::move(callback);
}

auto http_client_adapter::set_disconnected_callback(disconnected_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	disconnected_callback_ = std::move(callback);
}

auto http_client_adapter::set_error_callback(error_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	error_callback_ = std::move(callback);
}

// =========================================================================
// Internal methods
// =========================================================================

auto http_client_adapter::build_url() const -> std::string
{
	std::ostringstream oss;
	oss << (use_ssl_ ? "https://" : "http://")
		<< host_ << ":" << port_ << path_;
	return oss.str();
}

auto http_client_adapter::notify_receive(const std::vector<uint8_t>& data) -> void
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
}

auto http_client_adapter::notify_error(std::error_code ec) -> void
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
}

} // namespace kcenon::network::internal::adapters
