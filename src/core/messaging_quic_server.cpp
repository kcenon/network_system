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

#include <kcenon/network/config/feature_flags.h>

#include "kcenon/network/core/messaging_quic_server.h"

#include "kcenon/network/core/network_context.h"
#include "kcenon/network/integration/logger_integration.h"
#include "kcenon/network/metrics/network_metrics.h"
#include "kcenon/network/protocols/quic/packet.h"
#include "kcenon/network/session/quic_session.h"
#include "internal/quic_socket.h"

#include <random>
#include <sstream>

namespace kcenon::network::core
{

	messaging_quic_server::messaging_quic_server(std::string_view server_id)
	    : messaging_quic_server_base<messaging_quic_server>(server_id)
	{
	}

	auto messaging_quic_server::start_server(unsigned short port) -> VoidResult
	{
		// Use default config with no TLS (for development/testing only)
		return start_server(port, quic_server_config{});
	}

	auto messaging_quic_server::start_server(unsigned short port,
	                                         const quic_server_config& config)
	    -> VoidResult
	{
		config_ = config;
		// Use base class start_server which calls do_start
		return messaging_quic_server_base<messaging_quic_server>::start_server(port);
	}

	auto messaging_quic_server::do_start(unsigned short port) -> VoidResult
	{
		try
		{
			// Create io_context
			io_context_ = std::make_unique<asio::io_context>();
			work_guard_ = std::make_unique<
			    asio::executor_work_guard<asio::io_context::executor_type>>(
			    asio::make_work_guard(*io_context_));

			// Create UDP socket
			udp_socket_ = std::make_unique<asio::ip::udp::socket>(
			    *io_context_,
			    asio::ip::udp::endpoint(asio::ip::udp::v4(), port));

			// Create cleanup timer
			cleanup_timer_ = std::make_unique<asio::steady_timer>(*io_context_);

			// Start receiving packets
			start_receive();

			// Start periodic cleanup timer
			start_cleanup_timer();

			// Get thread pool from network context
			thread_pool_ = network_context::instance().get_thread_pool();
			if (!thread_pool_)
			{
				thread_pool_ = std::make_shared<integration::basic_thread_pool>(2);
			}

			// Submit io_context run task to thread pool
			io_context_future_ = thread_pool_->submit(
			    [this]()
			    {
				    try
				    {
					    NETWORK_LOG_INFO(
					        "[messaging_quic_server] Starting io_context on thread pool");
					    io_context_->run();
					    NETWORK_LOG_INFO(
					        "[messaging_quic_server] io_context stopped");
				    }
				    catch (const std::exception& e)
				    {
					    NETWORK_LOG_ERROR(
					        "[messaging_quic_server] Exception in io_context: "
					        + std::string(e.what()));
				    }
			    });

			NETWORK_LOG_INFO("[messaging_quic_server] Started listening on port "
			                 + std::to_string(port));
			return ok();
		}
		catch (const std::system_error& e)
		{
			if (e.code() == asio::error::address_in_use ||
			    e.code() == std::errc::address_in_use)
			{
				return error_void(error_codes::network_system::bind_failed,
				                  "Failed to bind to port: address already in use",
				                  "messaging_quic_server::do_start",
				                  "Port: " + std::to_string(port));
			}
			else if (e.code() == asio::error::access_denied ||
			         e.code() == std::errc::permission_denied)
			{
				return error_void(error_codes::network_system::bind_failed,
				                  "Failed to bind to port: permission denied",
				                  "messaging_quic_server::do_start",
				                  "Port: " + std::to_string(port));
			}

			return error_void(error_codes::common_errors::internal_error,
			                  "Failed to start server: " + std::string(e.what()),
			                  "messaging_quic_server::do_start",
			                  "Port: " + std::to_string(port));
		}
		catch (const std::exception& e)
		{
			return error_void(error_codes::common_errors::internal_error,
			                  "Failed to start server: " + std::string(e.what()),
			                  "messaging_quic_server::do_start",
			                  "Port: " + std::to_string(port));
		}
	}

	auto messaging_quic_server::do_stop() -> VoidResult
	{
		try
		{
			// Cancel cleanup timer
			if (cleanup_timer_)
			{
				cleanup_timer_->cancel();
			}

			// Close UDP socket
			if (udp_socket_)
			{
				asio::error_code ec;
				udp_socket_->cancel(ec);
				if (udp_socket_->is_open())
				{
					udp_socket_->close(ec);
				}
			}

			// Stop all sessions
			disconnect_all(0);

			// Release work guard
			if (work_guard_)
			{
				work_guard_.reset();
			}

			// Stop io_context
			if (io_context_)
			{
				io_context_->stop();
			}

			// Wait for io_context task
			if (io_context_future_.valid())
			{
				io_context_future_.wait();
			}

			// Release resources
			udp_socket_.reset();
			cleanup_timer_.reset();
			thread_pool_.reset();
			io_context_.reset();

			NETWORK_LOG_INFO("[messaging_quic_server] Stopped.");
			return ok();
		}
		catch (const std::exception& e)
		{
			return error_void(error_codes::common_errors::internal_error,
			                  "Failed to stop server: " + std::string(e.what()),
			                  "messaging_quic_server::do_stop",
			                  "Server ID: " + server_id_);
		}
	}

	auto messaging_quic_server::sessions() const
	    -> std::vector<std::shared_ptr<session::quic_session>>
	{
		std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
		std::vector<std::shared_ptr<session::quic_session>> result;
		result.reserve(sessions_.size());
		for (const auto& [id, session] : sessions_)
		{
			result.push_back(session);
		}
		return result;
	}

	auto messaging_quic_server::get_session(const std::string& session_id)
	    -> std::shared_ptr<session::quic_session>
	{
		std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
		auto it = sessions_.find(session_id);
		if (it != sessions_.end())
		{
			return it->second;
		}
		return nullptr;
	}

	auto messaging_quic_server::session_count() const -> size_t
	{
		std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
		return sessions_.size();
	}

	auto messaging_quic_server::disconnect_session(const std::string& session_id,
	                                               uint64_t error_code)
	    -> VoidResult
	{
		std::shared_ptr<session::quic_session> session;
		{
			std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
			auto it = sessions_.find(session_id);
			if (it == sessions_.end())
			{
				return error_void(error_codes::common_errors::not_found,
				                  "Session not found",
				                  "messaging_quic_server::disconnect_session",
				                  "Session ID: " + session_id);
			}
			session = it->second;
			sessions_.erase(it);
		}

		if (session)
		{
			return session->close(error_code);
		}
		return ok();
	}

	auto messaging_quic_server::disconnect_all(uint64_t error_code) -> void
	{
		std::vector<std::shared_ptr<session::quic_session>> sessions_to_close;
		{
			std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
			sessions_to_close.reserve(sessions_.size());
			for (auto& [id, session] : sessions_)
			{
				sessions_to_close.push_back(session);
			}
			sessions_.clear();
		}

		for (auto& session : sessions_to_close)
		{
			if (session)
			{
				auto result = session->close(error_code);
				(void)result;
			}
		}
	}

	auto messaging_quic_server::broadcast(std::vector<uint8_t>&& data)
	    -> VoidResult
	{
		auto sessions_list = sessions();
		for (auto& session : sessions_list)
		{
			if (session && session->is_active())
			{
				std::vector<uint8_t> data_copy(data);
				auto result = session->send(std::move(data_copy));
				if (result.is_err())
				{
					NETWORK_LOG_WARN("[messaging_quic_server] Failed to send to session "
					                 + session->session_id() + ": "
					                 + result.error().message);
				}
			}
		}
		return ok();
	}

	auto messaging_quic_server::multicast(
	    const std::vector<std::string>& session_ids,
	    std::vector<uint8_t>&& data) -> VoidResult
	{
		for (const auto& session_id : session_ids)
		{
			auto session = get_session(session_id);
			if (session && session->is_active())
			{
				std::vector<uint8_t> data_copy(data);
				auto result = session->send(std::move(data_copy));
				if (result.is_err())
				{
					NETWORK_LOG_WARN("[messaging_quic_server] Failed to send to session "
					                 + session_id + ": " + result.error().message);
				}
			}
		}
		return ok();
	}

#if KCENON_WITH_COMMON_SYSTEM
	auto messaging_quic_server::set_monitor(
	    kcenon::common::interfaces::IMonitor* monitor) -> void
	{
		monitor_ = monitor;
	}

	auto messaging_quic_server::get_monitor() const
	    -> kcenon::common::interfaces::IMonitor*
	{
		return monitor_;
	}
#endif

	auto messaging_quic_server::start_receive() -> void
	{
		if (!is_running() || !udp_socket_)
		{
			return;
		}

		auto self = shared_from_this();
		udp_socket_->async_receive_from(
		    asio::buffer(recv_buffer_),
		    recv_endpoint_,
		    [this, self](std::error_code ec, std::size_t bytes_received)
		    {
			    if (!is_running())
			    {
				    return;
			    }

			    if (ec)
			    {
				    if (ec != asio::error::operation_aborted)
				    {
					    NETWORK_LOG_ERROR(
					        "[messaging_quic_server] Receive error: " + ec.message());

					    invoke_error_callback(ec);
				    }
				    return;
			    }

			    // Handle the received packet
			    std::span<const uint8_t> packet_data(recv_buffer_.data(),
			                                         bytes_received);
			    handle_packet(packet_data, recv_endpoint_);

			    // Continue receiving
			    start_receive();
		    });
	}

	auto messaging_quic_server::handle_packet(std::span<const uint8_t> data,
	                                          const asio::ip::udp::endpoint& from)
	    -> void
	{
		if (data.empty())
		{
			return;
		}

		// Parse packet header to get destination connection ID
		auto header_result = protocols::quic::packet_parser::parse_header(data);
		if (header_result.is_err())
		{
			NETWORK_LOG_DEBUG("[messaging_quic_server] Invalid packet from "
			                  + from.address().to_string());
			return;
		}

		// Extract destination connection ID from header
		protocols::quic::connection_id dcid;
		std::visit(
		    [&dcid](auto&& hdr)
		    {
			    dcid = hdr.dest_conn_id;
		    },
		    header_result.value().first);

		// Find or create session for this connection
		auto session = find_or_create_session(dcid, from);
		if (!session)
		{
			NETWORK_LOG_DEBUG(
			    "[messaging_quic_server] Could not find or create session for packet");
			return;
		}

		// Delegate packet handling to session
		session->handle_packet(data);
	}

	auto messaging_quic_server::find_or_create_session(
	    const protocols::quic::connection_id& dcid,
	    const asio::ip::udp::endpoint& endpoint)
	    -> std::shared_ptr<session::quic_session>
	{
		// First, check existing sessions
		{
			std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
			for (const auto& [id, session] : sessions_)
			{
				if (session->matches_connection_id(dcid))
				{
					return session;
				}
			}
		}

		// Check connection limit
		if (session_count() >= config_.max_connections)
		{
			NETWORK_LOG_WARN(
			    "[messaging_quic_server] Connection limit reached, rejecting new connection");
			return nullptr;
		}

		// Create new session for this connection
		auto session_id = generate_session_id();

		// Create QUIC socket for the new session
		asio::ip::udp::socket session_socket(
		    *io_context_, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0));
		session_socket.connect(endpoint);

		auto quic_socket = std::make_shared<internal::quic_socket>(
		    std::move(session_socket), internal::quic_role::server);

		// Accept the connection with TLS config
		if (!config_.cert_file.empty() && !config_.key_file.empty())
		{
			auto accept_result =
			    quic_socket->accept(config_.cert_file, config_.key_file);
			if (accept_result.is_err())
			{
				NETWORK_LOG_ERROR(
				    "[messaging_quic_server] Failed to accept connection: "
				    + accept_result.error().message);
				return nullptr;
			}
		}

		auto session =
		    std::make_shared<session::quic_session>(quic_socket, session_id);

		// Set up session callbacks
		auto self = weak_from_this();

		session->set_receive_callback(
		    [self, session_id](const std::vector<uint8_t>& data)
		    {
			    if (auto server = self.lock())
			    {
				    auto sess = server->get_session(session_id);
				    if (sess)
				    {
					    server->invoke_receive_callback(sess, data);
				    }
			    }
		    });

		session->set_stream_receive_callback(
		    [self, session_id](uint64_t stream_id,
		                       const std::vector<uint8_t>& data,
		                       bool fin)
		    {
			    if (auto server = self.lock())
			    {
				    auto sess = server->get_session(session_id);
				    if (sess)
				    {
					    server->invoke_stream_receive_callback(sess, stream_id, data, fin);
				    }
			    }
		    });

		session->set_close_callback(
		    [self, session_id]()
		    {
			    if (auto server = self.lock())
			    {
				    server->on_session_close(session_id);
			    }
		    });

		// Add session to map
		{
			std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
			sessions_[session_id] = session;
		}

		// Start the session
		session->start_session();

		// Report metrics
		metrics::metric_reporter::report_connection_accepted();
		metrics::metric_reporter::report_active_connections(session_count());

		// Invoke connection callback via base class
		invoke_connection_callback(session);

#if KCENON_WITH_COMMON_SYSTEM
		if (monitor_)
		{
			monitor_->record_metric("active_connections",
			                        static_cast<double>(session_count()));
		}
#endif

		NETWORK_LOG_INFO("[messaging_quic_server] New session created: "
		                 + session_id + " from " + endpoint.address().to_string());

		return session;
	}

	auto messaging_quic_server::generate_session_id() -> std::string
	{
		auto counter = session_counter_.fetch_add(1);
		std::ostringstream oss;
		oss << server_id_ << "-" << counter;
		return oss.str();
	}

	auto messaging_quic_server::on_session_close(const std::string& session_id)
	    -> void
	{
		std::shared_ptr<session::quic_session> session;
		{
			std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
			auto it = sessions_.find(session_id);
			if (it != sessions_.end())
			{
				session = it->second;
				sessions_.erase(it);
			}
		}

		if (session)
		{
			// Report metrics
			metrics::metric_reporter::report_active_connections(session_count());

			// Invoke disconnection callback via base class
			invoke_disconnection_callback(session);

#if KCENON_WITH_COMMON_SYSTEM
			if (monitor_)
			{
				monitor_->record_metric("active_connections",
				                        static_cast<double>(session_count()));
			}
#endif

			NETWORK_LOG_INFO("[messaging_quic_server] Session closed: "
			                 + session_id);
		}
	}

	auto messaging_quic_server::start_cleanup_timer() -> void
	{
		if (!cleanup_timer_ || !is_running())
		{
			return;
		}

		// Schedule cleanup every 30 seconds
		cleanup_timer_->expires_after(std::chrono::seconds(30));

		auto self = shared_from_this();
		cleanup_timer_->async_wait(
		    [this, self](const std::error_code& ec)
		    {
			    if (!ec && is_running())
			    {
				    cleanup_dead_sessions();
				    start_cleanup_timer(); // Reschedule
			    }
		    });
	}

	auto messaging_quic_server::cleanup_dead_sessions() -> void
	{
		std::vector<std::string> dead_session_ids;

		{
			std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
			for (const auto& [id, session] : sessions_)
			{
				if (!session || !session->is_active())
				{
					dead_session_ids.push_back(id);
				}
			}
		}

		for (const auto& id : dead_session_ids)
		{
			on_session_close(id);
		}

		if (!dead_session_ids.empty())
		{
			NETWORK_LOG_DEBUG(
			    "[messaging_quic_server] Cleaned up "
			    + std::to_string(dead_session_ids.size())
			    + " dead sessions. Active: " + std::to_string(session_count()));
		}
	}

} // namespace kcenon::network::core
