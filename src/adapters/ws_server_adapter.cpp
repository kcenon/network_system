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

#include "internal/adapters/ws_server_adapter.h"

// Suppress deprecation warnings for internal usage
#define NETWORK_SYSTEM_SUPPRESS_DEPRECATION_WARNINGS
#include "internal/http/websocket_server.h"
#undef NETWORK_SYSTEM_SUPPRESS_DEPRECATION_WARNINGS

namespace kcenon::network::internal::adapters {

// =========================================================================
// ws_session_wrapper implementation
// =========================================================================

ws_session_wrapper::ws_session_wrapper(std::shared_ptr<core::ws_connection> connection)
	: connection_(std::move(connection))
{
	// Cache the ID since it may be accessed frequently
	if (connection_)
	{
		id_cache_ = std::string(connection_->id());
	}
}

auto ws_session_wrapper::id() const -> std::string_view
{
	return id_cache_;
}

auto ws_session_wrapper::is_connected() const -> bool
{
	return connection_ && connection_->is_connected();
}

auto ws_session_wrapper::send(std::vector<uint8_t>&& data) -> VoidResult
{
	if (!connection_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Connection no longer available",
			"ws_session_wrapper::send"
		);
	}

	// Send as binary WebSocket frame
	return connection_->send_binary(std::move(data));
}

auto ws_session_wrapper::close() -> void
{
	if (connection_)
	{
		connection_->close();
	}
}

// =========================================================================
// ws_server_adapter implementation
// =========================================================================

ws_server_adapter::ws_server_adapter(std::string_view server_id)
	: server_id_(server_id)
	, server_(std::make_shared<core::messaging_ws_server>(std::string(server_id)))
{
	setup_internal_callbacks();
}

ws_server_adapter::~ws_server_adapter()
{
	if (server_ && server_->is_running())
	{
		(void)server_->stop();
	}
}

// =========================================================================
// Path Configuration
// =========================================================================

auto ws_server_adapter::set_path(std::string_view path) -> void
{
	path_ = std::string(path);
}

// =========================================================================
// i_network_component interface implementation
// =========================================================================

auto ws_server_adapter::is_running() const -> bool
{
	return server_ && server_->is_running();
}

auto ws_server_adapter::wait_for_stop() -> void
{
	if (server_)
	{
		server_->wait_for_stop();
	}
}

// =========================================================================
// i_protocol_server interface implementation
// =========================================================================

auto ws_server_adapter::start(uint16_t port) -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"ws_server_adapter::start"
		);
	}

	// Use the stored path (set via set_path, or default "/")
	return server_->start_server(port, path_);
}

auto ws_server_adapter::stop() -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"ws_server_adapter::stop"
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

auto ws_server_adapter::connection_count() const -> size_t
{
	return server_ ? server_->connection_count() : 0;
}

auto ws_server_adapter::set_connection_callback(connection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	connection_callback_ = std::move(callback);
}

auto ws_server_adapter::set_disconnection_callback(disconnection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	disconnection_callback_ = std::move(callback);
}

auto ws_server_adapter::set_receive_callback(receive_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	receive_callback_ = std::move(callback);
}

auto ws_server_adapter::set_error_callback(error_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	error_callback_ = std::move(callback);
}

// =========================================================================
// Internal methods
// =========================================================================

auto ws_server_adapter::setup_internal_callbacks() -> void
{
	if (!server_)
	{
		return;
	}

	// Bridge WebSocket connection callback
	server_->set_connection_callback(
		[this](std::shared_ptr<interfaces::i_websocket_session> ws_session)
		{
			// Cast to ws_connection for our wrapper
			auto ws_conn = std::dynamic_pointer_cast<core::ws_connection>(ws_session);
			if (!ws_conn)
			{
				return;
			}

			auto session = get_or_create_session(ws_conn);

			connection_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = connection_callback_;
			}

			if (callback_copy)
			{
				callback_copy(session);
			}
		});

	// Bridge WebSocket disconnection callback (ignore close code and reason)
	server_->set_disconnection_callback(
		[this](std::string_view session_id, uint16_t /* code */, std::string_view /* reason */)
		{
			// Remove from tracked sessions
			{
				std::lock_guard<std::mutex> lock(sessions_mutex_);
				sessions_.erase(std::string(session_id));
			}

			disconnection_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = disconnection_callback_;
			}

			if (callback_copy)
			{
				callback_copy(session_id);
			}
		});

	// Bridge WebSocket binary message callback
	server_->set_binary_callback(
		[this](std::string_view session_id, const std::vector<uint8_t>& data)
		{
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

	// Bridge WebSocket error callback
	server_->set_error_callback(
		[this](std::string_view session_id, std::error_code ec)
		{
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

auto ws_server_adapter::get_or_create_session(std::shared_ptr<core::ws_connection> connection)
	-> std::shared_ptr<interfaces::i_session>
{
	std::string session_id(connection->id());

	std::lock_guard<std::mutex> lock(sessions_mutex_);

	auto it = sessions_.find(session_id);
	if (it != sessions_.end())
	{
		return it->second;
	}

	// Create new session wrapper
	auto session = std::make_shared<ws_session_wrapper>(connection);
	sessions_[session_id] = session;

	return session;
}

} // namespace kcenon::network::internal::adapters
