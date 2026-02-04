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

#include "internal/adapters/http_server_adapter.h"

#include "kcenon/network/detail/utils/result_types.h"

// Suppress deprecation warnings for internal usage
#define NETWORK_SYSTEM_SUPPRESS_DEPRECATION_WARNINGS
#include "internal/http/http_server.h"
#undef NETWORK_SYSTEM_SUPPRESS_DEPRECATION_WARNINGS

#include <sstream>

using kcenon::network::ok;
using kcenon::network::error_void;
namespace error_codes = kcenon::network::error_codes;

namespace kcenon::network::internal::adapters {

// =========================================================================
// http_request_session implementation
// =========================================================================

http_request_session::http_request_session(
	std::string_view session_id,
	std::string_view client_address,
	uint16_t client_port,
	std::weak_ptr<core::http_server> server)
	: session_id_(session_id)
	, client_address_(client_address)
	, client_port_(client_port)
	, server_(std::move(server))
{
}

auto http_request_session::id() const -> std::string_view
{
	return session_id_;
}

auto http_request_session::is_connected() const -> bool
{
	return is_connected_.load(std::memory_order_acquire);
}

auto http_request_session::send(std::vector<uint8_t>&& data) -> VoidResult
{
	if (!is_connected_.load(std::memory_order_acquire))
	{
		return error_void(
			error_codes::common_errors::invalid_argument,
			"Session is not connected",
			"http_request_session::send"
		);
	}

	std::lock_guard<std::mutex> lock(data_mutex_);
	response_data_ = std::move(data);

	return ok();
}

auto http_request_session::close() -> void
{
	is_connected_.store(false, std::memory_order_release);
}

auto http_request_session::set_response_data(std::vector<uint8_t> data) -> void
{
	std::lock_guard<std::mutex> lock(data_mutex_);
	response_data_ = std::move(data);
}

auto http_request_session::get_response_data() const -> const std::vector<uint8_t>&
{
	std::lock_guard<std::mutex> lock(data_mutex_);
	return response_data_;
}

// =========================================================================
// http_server_adapter implementation
// =========================================================================

http_server_adapter::http_server_adapter(std::string_view server_id)
	: server_id_(server_id)
	, server_(std::make_shared<core::http_server>(std::string(server_id)))
{
	setup_internal_routes();
}

http_server_adapter::~http_server_adapter()
{
	if (server_ && is_running_.load(std::memory_order_acquire))
	{
		(void)server_->stop();
	}
}

// =========================================================================
// i_network_component interface implementation
// =========================================================================

auto http_server_adapter::is_running() const -> bool
{
	return is_running_.load(std::memory_order_acquire);
}

auto http_server_adapter::wait_for_stop() -> void
{
	if (server_)
	{
		server_->wait_for_stop();
	}
}

// =========================================================================
// i_protocol_server interface implementation
// =========================================================================

auto http_server_adapter::start(uint16_t port) -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"http_server_adapter::start"
		);
	}

	auto result = server_->start(port);
	if (result.is_ok())
	{
		is_running_.store(true, std::memory_order_release);
	}
	return result;
}

auto http_server_adapter::stop() -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"http_server_adapter::stop"
		);
	}

	auto result = server_->stop();
	is_running_.store(false, std::memory_order_release);

	// Clear tracked sessions on stop
	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		sessions_.clear();
	}

	return result;
}

auto http_server_adapter::connection_count() const -> size_t
{
	std::lock_guard<std::mutex> lock(sessions_mutex_);
	return sessions_.size();
}

auto http_server_adapter::set_connection_callback(connection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	connection_callback_ = std::move(callback);
}

auto http_server_adapter::set_disconnection_callback(disconnection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	disconnection_callback_ = std::move(callback);
}

auto http_server_adapter::set_receive_callback(receive_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	receive_callback_ = std::move(callback);
}

auto http_server_adapter::set_error_callback(error_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	error_callback_ = std::move(callback);
}

// =========================================================================
// Internal methods
// =========================================================================

auto http_server_adapter::setup_internal_routes() -> void
{
	if (!server_)
	{
		return;
	}

	// Register a catch-all POST handler for unified protocol interface
	// The i_protocol_client.send() performs HTTP POST, so we handle POST requests
	server_->post("/:path", [this](const core::http_request_context& ctx) {
		// Create a session for this request
		const auto request_id = request_counter_.fetch_add(1, std::memory_order_relaxed);
		const auto session_id = make_session_id("client", 0, request_id);
		auto session = std::make_shared<http_request_session>(
			session_id, "client", 0, server_);

		// Track the session
		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			sessions_[session_id] = session;
		}

		// Notify connection callback
		{
			connection_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = connection_callback_;
			}
			if (callback_copy)
			{
				callback_copy(session);
			}
		}

		// Notify receive callback with request body
		{
			receive_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = receive_callback_;
			}
			if (callback_copy)
			{
				callback_copy(session_id, ctx.request.body);
			}
		}

		// Build response from session data
		network::internal::http_response response;
		response.status_code = 200;
		response.body = session->get_response_data();
		response.set_header("Content-Type", "application/octet-stream");

		// Notify disconnection callback
		{
			disconnection_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = disconnection_callback_;
			}
			if (callback_copy)
			{
				callback_copy(session_id);
			}
		}

		// Remove session
		remove_session(session_id);

		return response;
	});

	// Also register root POST handler
	server_->post("/", [this](const core::http_request_context& ctx) {
		const auto request_id = request_counter_.fetch_add(1, std::memory_order_relaxed);
		const auto session_id = make_session_id("client", 0, request_id);
		auto session = std::make_shared<http_request_session>(
			session_id, "client", 0, server_);

		{
			std::lock_guard<std::mutex> lock(sessions_mutex_);
			sessions_[session_id] = session;
		}

		{
			connection_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = connection_callback_;
			}
			if (callback_copy)
			{
				callback_copy(session);
			}
		}

		{
			receive_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = receive_callback_;
			}
			if (callback_copy)
			{
				callback_copy(session_id, ctx.request.body);
			}
		}

		network::internal::http_response response;
		response.status_code = 200;
		response.body = session->get_response_data();
		response.set_header("Content-Type", "application/octet-stream");

		{
			disconnection_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = disconnection_callback_;
			}
			if (callback_copy)
			{
				callback_copy(session_id);
			}
		}

		remove_session(session_id);

		return response;
	});
}

auto http_server_adapter::make_session_id(
	const std::string& address,
	uint16_t port,
	uint64_t request_id) -> std::string
{
	std::ostringstream oss;
	oss << address << ":" << port << "#" << request_id;
	return oss.str();
}

auto http_server_adapter::get_or_create_session(
	const std::string& address,
	uint16_t port) -> std::shared_ptr<http_request_session>
{
	const auto request_id = request_counter_.fetch_add(1, std::memory_order_relaxed);
	const auto session_id = make_session_id(address, port, request_id);

	std::lock_guard<std::mutex> lock(sessions_mutex_);

	auto session = std::make_shared<http_request_session>(
		session_id, address, port, server_);
	sessions_[session_id] = session;

	return session;
}

auto http_server_adapter::remove_session(const std::string& session_id) -> void
{
	std::lock_guard<std::mutex> lock(sessions_mutex_);
	sessions_.erase(session_id);
}

} // namespace kcenon::network::internal::adapters
