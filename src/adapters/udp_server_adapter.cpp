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

#include "internal/adapters/udp_server_adapter.h"

#include "internal/core/messaging_udp_server.h"

#include <sstream>

namespace kcenon::network::internal::adapters {

// =========================================================================
// udp_endpoint_session implementation
// =========================================================================

udp_endpoint_session::udp_endpoint_session(
	std::string_view session_id,
	std::string_view address,
	uint16_t port,
	std::weak_ptr<core::messaging_udp_server> server)
	: session_id_(session_id)
	, address_(address)
	, port_(port)
	, server_(std::move(server))
{
}

auto udp_endpoint_session::id() const -> std::string_view
{
	return session_id_;
}

auto udp_endpoint_session::is_connected() const -> bool
{
	// UDP is connectionless, so we consider the session "connected"
	// as long as the server is still running
	auto server = server_.lock();
	return server && server->is_running();
}

auto udp_endpoint_session::send(std::vector<uint8_t>&& data) -> VoidResult
{
	auto server = server_.lock();
	if (!server)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server no longer available",
			"udp_endpoint_session::send"
		);
	}

	interfaces::i_udp_server::endpoint_info endpoint{address_, port_};
	return server->send_to(endpoint, std::move(data));
}

auto udp_endpoint_session::close() -> void
{
	// UDP is connectionless, so close is a no-op
	// The session will be removed from tracking when server stops
}

// =========================================================================
// udp_server_adapter implementation
// =========================================================================

udp_server_adapter::udp_server_adapter(std::string_view server_id)
	: server_id_(server_id)
	, server_(std::make_shared<core::messaging_udp_server>(std::string(server_id)))
{
	setup_internal_callbacks();
}

udp_server_adapter::~udp_server_adapter()
{
	if (server_ && server_->is_running())
	{
		(void)server_->stop();
	}
}

// =========================================================================
// i_network_component interface implementation
// =========================================================================

auto udp_server_adapter::is_running() const -> bool
{
	return server_ && server_->is_running();
}

auto udp_server_adapter::wait_for_stop() -> void
{
	if (server_)
	{
		server_->wait_for_stop();
	}
}

// =========================================================================
// i_protocol_server interface implementation
// =========================================================================

auto udp_server_adapter::start(uint16_t port) -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"udp_server_adapter::start"
		);
	}
	return server_->start(port);
}

auto udp_server_adapter::stop() -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"udp_server_adapter::stop"
		);
	}

	auto result = server_->stop();

	// Clear tracked sessions on stop
	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		sessions_.clear();
	}

	return result;
}

auto udp_server_adapter::connection_count() const -> size_t
{
	std::lock_guard<std::mutex> lock(sessions_mutex_);
	return sessions_.size();
}

auto udp_server_adapter::set_connection_callback(connection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	connection_callback_ = std::move(callback);
}

auto udp_server_adapter::set_disconnection_callback(disconnection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	disconnection_callback_ = std::move(callback);
}

auto udp_server_adapter::set_receive_callback(receive_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	receive_callback_ = std::move(callback);
}

auto udp_server_adapter::set_error_callback(error_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	error_callback_ = std::move(callback);
}

// =========================================================================
// Internal methods
// =========================================================================

auto udp_server_adapter::setup_internal_callbacks() -> void
{
	if (!server_)
	{
		return;
	}

	// Bridge UDP receive callback to i_protocol_server callback
	// Create virtual sessions for each unique endpoint
	server_->set_receive_callback(
		[this](const std::vector<uint8_t>& data,
		       const interfaces::i_udp_server::endpoint_info& endpoint)
		{
			// Get or create session for this endpoint
			auto session = get_or_create_session(endpoint.address, endpoint.port);
			std::string session_id(session->id());

			// Invoke the receive callback
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

	// Bridge UDP error callback
	server_->set_error_callback(
		[this](std::error_code ec)
		{
			// Server-level error, use empty session_id
			error_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = error_callback_;
			}
			if (callback_copy)
			{
				callback_copy("", ec);
			}
		});
}

auto udp_server_adapter::make_session_id(const std::string& address, uint16_t port) -> std::string
{
	std::ostringstream oss;
	oss << address << ":" << port;
	return oss.str();
}

auto udp_server_adapter::get_or_create_session(const std::string& address, uint16_t port)
	-> std::shared_ptr<interfaces::i_session>
{
	std::string session_id = make_session_id(address, port);

	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);

		auto it = sessions_.find(session_id);
		if (it != sessions_.end())
		{
			return it->second;
		}

		// Create new session
		auto session = std::make_shared<udp_endpoint_session>(
			session_id, address, port, server_);
		sessions_[session_id] = session;

		// Invoke connection callback for new session (outside of lock)
		connection_callback_t callback_copy;
		{
			std::lock_guard<std::mutex> cb_lock(callbacks_mutex_);
			callback_copy = connection_callback_;
		}

		if (callback_copy)
		{
			callback_copy(session);
		}

		return session;
	}
}

} // namespace kcenon::network::internal::adapters
