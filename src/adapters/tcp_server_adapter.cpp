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

#include "internal/adapters/tcp_server_adapter.h"

#include "internal/core/messaging_server.h"
#include "kcenon/network/detail/session/messaging_session.h"

namespace kcenon::network::internal::adapters {

tcp_server_adapter::tcp_server_adapter(std::string_view server_id)
	: server_id_(server_id)
	, server_(std::make_shared<core::messaging_server>(std::string(server_id)))
{
	setup_internal_callbacks();
}

tcp_server_adapter::~tcp_server_adapter()
{
	if (server_ && server_->is_running())
	{
		(void)server_->stop_server();
	}
}

// =========================================================================
// i_network_component interface implementation
// =========================================================================

auto tcp_server_adapter::is_running() const -> bool
{
	return server_ && server_->is_running();
}

auto tcp_server_adapter::wait_for_stop() -> void
{
	if (server_)
	{
		server_->wait_for_stop();
	}
}

// =========================================================================
// i_protocol_server interface implementation
// =========================================================================

auto tcp_server_adapter::start(uint16_t port) -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"tcp_server_adapter::start"
		);
	}
	return server_->start_server(port);
}

auto tcp_server_adapter::stop() -> VoidResult
{
	if (!server_)
	{
		return error_void(
			error_codes::common_errors::internal_error,
			"Server not initialized",
			"tcp_server_adapter::stop"
		);
	}

	auto result = server_->stop_server();

	// Clear tracked sessions on stop
	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		sessions_.clear();
	}

	return result;
}

auto tcp_server_adapter::connection_count() const -> size_t
{
	std::lock_guard<std::mutex> lock(sessions_mutex_);
	return sessions_.size();
}

auto tcp_server_adapter::set_connection_callback(connection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	connection_callback_ = std::move(callback);
}

auto tcp_server_adapter::set_disconnection_callback(disconnection_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	disconnection_callback_ = std::move(callback);
}

auto tcp_server_adapter::set_receive_callback(receive_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	receive_callback_ = std::move(callback);
}

auto tcp_server_adapter::set_error_callback(error_callback_t callback) -> void
{
	std::lock_guard<std::mutex> lock(callbacks_mutex_);
	error_callback_ = std::move(callback);
}

// =========================================================================
// Internal methods
// =========================================================================

auto tcp_server_adapter::setup_internal_callbacks() -> void
{
	if (!server_)
	{
		return;
	}

	// Bridge legacy connection callback to i_protocol_server callback
	server_->set_connection_callback(
		[this](std::shared_ptr<session::messaging_session> session)
		{
			if (!session)
			{
				return;
			}

			// Track the session
			auto i_session_ptr = track_session(session);

			// Invoke the interface callback
			connection_callback_t callback_copy;
			{
				std::lock_guard<std::mutex> lock(callbacks_mutex_);
				callback_copy = connection_callback_;
			}
			if (callback_copy)
			{
				callback_copy(std::move(i_session_ptr));
			}
		});

	// Bridge legacy disconnection callback
	server_->set_disconnection_callback(
		[this](const std::string& session_id)
		{
			// Remove from tracked sessions
			{
				std::lock_guard<std::mutex> lock(sessions_mutex_);
				sessions_.erase(session_id);
			}

			// Invoke the interface callback
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

	// Bridge legacy receive callback
	server_->set_receive_callback(
		[this](std::shared_ptr<session::messaging_session> session,
		       const std::vector<uint8_t>& data)
		{
			if (!session)
			{
				return;
			}

			// Get session ID (messaging_session inherits from i_session)
			std::string session_id(session->id());

			// Invoke the interface callback
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

	// Bridge legacy error callback
	server_->set_error_callback(
		[this](std::shared_ptr<session::messaging_session> session,
		       std::error_code ec)
		{
			if (!session)
			{
				return;
			}

			// Get session ID
			std::string session_id(session->id());

			// Invoke the interface callback
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

auto tcp_server_adapter::track_session(std::shared_ptr<session::messaging_session> session)
	-> std::shared_ptr<interfaces::i_session>
{
	if (!session)
	{
		return nullptr;
	}

	std::string session_id(session->id());

	{
		std::lock_guard<std::mutex> lock(sessions_mutex_);
		sessions_[session_id] = session;
	}

	// messaging_session inherits from i_session, so we can return it directly
	return session;
}

} // namespace kcenon::network::internal::adapters
