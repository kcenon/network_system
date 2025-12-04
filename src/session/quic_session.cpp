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

#include "kcenon/network/session/quic_session.h"

#include "kcenon/network/integration/logger_integration.h"
#include "kcenon/network/metrics/network_metrics.h"
#include "internal/quic_socket.h"

namespace network_system::session
{

	quic_session::quic_session(std::shared_ptr<internal::quic_socket> socket,
	                           std::string_view session_id)
	    : session_id_(session_id), socket_(std::move(socket))
	{
		NETWORK_LOG_DEBUG("[quic_session] Created session: " + session_id_);
	}

	quic_session::~quic_session() noexcept
	{
		try
		{
			if (is_active_.load())
			{
				auto result = close(0);
				(void)result; // Ignore result in destructor
			}
		}
		catch (...)
		{
			// Destructor must not throw
		}
	}

	auto quic_session::session_id() const -> const std::string&
	{
		return session_id_;
	}

	auto quic_session::remote_endpoint() const -> asio::ip::udp::endpoint
	{
		std::lock_guard<std::mutex> lock(socket_mutex_);
		if (socket_)
		{
			return socket_->remote_endpoint();
		}
		return {};
	}

	auto quic_session::is_active() const noexcept -> bool
	{
		return is_active_.load(std::memory_order_relaxed);
	}

	auto quic_session::send(std::vector<uint8_t>&& data) -> VoidResult
	{
		if (!is_active_.load())
		{
			return error_void(error_codes::network_system::connection_closed,
			                  "Session is not active",
			                  "quic_session");
		}

		std::lock_guard<std::mutex> lock(socket_mutex_);
		if (!socket_)
		{
			return error_void(error_codes::network_system::connection_closed,
			                  "Socket is null",
			                  "quic_session");
		}

		// Report bytes sent metric
		metrics::metric_reporter::report_bytes_sent(data.size());

		return socket_->send_stream_data(default_stream_id_, std::move(data), false);
	}

	auto quic_session::send(std::string_view data) -> VoidResult
	{
		std::vector<uint8_t> bytes(data.begin(), data.end());
		return send(std::move(bytes));
	}

	auto quic_session::send_on_stream(uint64_t stream_id,
	                                  std::vector<uint8_t>&& data,
	                                  bool fin) -> VoidResult
	{
		if (!is_active_.load())
		{
			return error_void(error_codes::network_system::connection_closed,
			                  "Session is not active",
			                  "quic_session");
		}

		std::lock_guard<std::mutex> lock(socket_mutex_);
		if (!socket_)
		{
			return error_void(error_codes::network_system::connection_closed,
			                  "Socket is null",
			                  "quic_session");
		}

		metrics::metric_reporter::report_bytes_sent(data.size());

		return socket_->send_stream_data(stream_id, std::move(data), fin);
	}

	auto quic_session::create_stream() -> Result<uint64_t>
	{
		if (!is_active_.load())
		{
			return error<uint64_t>(error_codes::network_system::connection_closed,
			                       "Session is not active",
			                       "quic_session");
		}

		std::lock_guard<std::mutex> lock(socket_mutex_);
		if (!socket_)
		{
			return error<uint64_t>(error_codes::network_system::connection_closed,
			                       "Socket is null",
			                       "quic_session");
		}

		return socket_->create_stream(false);
	}

	auto quic_session::close(uint64_t error_code) -> VoidResult
	{
		if (!is_active_.exchange(false))
		{
			return ok();
		}

		NETWORK_LOG_INFO("[quic_session] Closing session: " + session_id_);

		VoidResult result = ok();

		{
			std::lock_guard<std::mutex> lock(socket_mutex_);
			if (socket_)
			{
				result = socket_->close(error_code, "Session closed");
			}
		}

		// Invoke close callback
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (close_callback_)
			{
				try
				{
					close_callback_();
				}
				catch (const std::exception& e)
				{
					NETWORK_LOG_ERROR("[quic_session] Exception in close callback: "
					                  + std::string(e.what()));
				}
			}
		}

		return result;
	}

	auto quic_session::stats() const -> core::quic_connection_stats
	{
		// Return empty stats for now - will be implemented with actual QUIC stats
		return core::quic_connection_stats{};
	}

	auto quic_session::set_receive_callback(
	    std::function<void(const std::vector<uint8_t>&)> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		receive_callback_ = std::move(callback);
	}

	auto quic_session::set_stream_receive_callback(
	    std::function<void(uint64_t, const std::vector<uint8_t>&, bool)> callback)
	    -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		stream_receive_callback_ = std::move(callback);
	}

	auto quic_session::set_close_callback(std::function<void()> callback) -> void
	{
		std::lock_guard<std::mutex> lock(callback_mutex_);
		close_callback_ = std::move(callback);
	}

	auto quic_session::start_session() -> void
	{
		if (!is_active_.load())
		{
			return;
		}

		auto weak_self = weak_from_this();

		std::lock_guard<std::mutex> lock(socket_mutex_);
		if (!socket_)
		{
			return;
		}

		// Set up stream data callback
		socket_->set_stream_data_callback(
		    [weak_self](uint64_t stream_id, std::span<const uint8_t> data, bool fin)
		    {
			    if (auto self = weak_self.lock())
			    {
				    self->on_stream_data(stream_id, data, fin);
			    }
		    });

		// Set up error callback
		socket_->set_error_callback([weak_self](std::error_code ec)
		                            {
			                            if (auto self = weak_self.lock())
			                            {
				                            self->on_error(ec);
			                            }
		                            });

		// Set up close callback
		socket_->set_close_callback(
		    [weak_self](uint64_t error_code, const std::string& reason)
		    {
			    if (auto self = weak_self.lock())
			    {
				    self->on_close(error_code, reason);
			    }
		    });

		NETWORK_LOG_INFO("[quic_session] Started session: " + session_id_);
	}

	auto quic_session::handle_packet(std::span<const uint8_t> data) -> void
	{
		// This is called by the server when a packet arrives for this session
		// The actual packet handling is done by quic_socket
		std::lock_guard<std::mutex> lock(socket_mutex_);
		if (socket_)
		{
			// The socket will process the packet internally
			// No direct call needed as socket handles its own receive loop
		}
	}

	auto quic_session::matches_connection_id(
	    const protocols::quic::connection_id& conn_id) const -> bool
	{
		std::lock_guard<std::mutex> lock(socket_mutex_);
		if (!socket_)
		{
			return false;
		}
		return socket_->local_connection_id() == conn_id ||
		       socket_->remote_connection_id() == conn_id;
	}

	auto quic_session::on_stream_data(uint64_t stream_id,
	                                  std::span<const uint8_t> data,
	                                  bool fin) -> void
	{
		if (!is_active_.load())
		{
			return;
		}

		// Report bytes received metric
		metrics::metric_reporter::report_bytes_received(data.size());

		NETWORK_LOG_DEBUG("[quic_session] Received " + std::to_string(data.size())
		                  + " bytes on stream " + std::to_string(stream_id));

		std::vector<uint8_t> data_copy(data.begin(), data.end());

		std::lock_guard<std::mutex> lock(callback_mutex_);

		// Call stream-specific callback if set
		if (stream_receive_callback_)
		{
			try
			{
				stream_receive_callback_(stream_id, data_copy, fin);
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR(
				    "[quic_session] Exception in stream receive callback: "
				    + std::string(e.what()));
			}
		}

		// Also call default receive callback for default stream
		if (stream_id == default_stream_id_ && receive_callback_)
		{
			try
			{
				receive_callback_(data_copy);
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR("[quic_session] Exception in receive callback: "
				                  + std::string(e.what()));
			}
		}
	}

	auto quic_session::on_error(std::error_code ec) -> void
	{
		NETWORK_LOG_ERROR("[quic_session] Error on session " + session_id_ + ": "
		                  + ec.message());

		// Close the session on error
		auto result = close(1);
		(void)result;
	}

	auto quic_session::on_close(uint64_t error_code, const std::string& reason)
	    -> void
	{
		NETWORK_LOG_INFO("[quic_session] Session " + session_id_ + " closed: "
		                 + reason + " (code: " + std::to_string(error_code) + ")");

		is_active_.store(false);

		// Invoke close callback
		{
			std::lock_guard<std::mutex> lock(callback_mutex_);
			if (close_callback_)
			{
				try
				{
					close_callback_();
				}
				catch (const std::exception& e)
				{
					NETWORK_LOG_ERROR("[quic_session] Exception in close callback: "
					                  + std::string(e.what()));
				}
			}
		}
	}

} // namespace network_system::session
