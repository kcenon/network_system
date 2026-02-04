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

#include "internal/core/messaging_server.h"
#include "kcenon/network/detail/session/messaging_session.h"
#include "kcenon/network/integration/logger_integration.h"
#include "kcenon/network/integration/io_context_thread_manager.h"

namespace kcenon::network::core
{

	using tcp = asio::ip::tcp;

	messaging_server::messaging_server(const std::string& server_id)
		: server_id_(server_id)
	{
	}

	messaging_server::~messaging_server() { stop_server(); }

	auto messaging_server::start_server(unsigned short port) -> void
	{
		// If already running, do nothing
		if (is_running_.load())
		{
			return;
		}
		is_running_.store(true);

		// Create io_context as shared_ptr (required by io_context_thread_manager)
		io_context_ = std::make_shared<asio::io_context>();

		// Create work guard to keep io_context running until explicitly stopped
		work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
			io_context_->get_executor());

		acceptor_ = std::make_unique<tcp::acceptor>(
			*io_context_, tcp::endpoint(tcp::v4(), port));

		// Prepare promise/future for wait_for_stop()
		stop_promise_.emplace();
		stop_future_ = stop_promise_->get_future();

		// Post the accept operation to io_context BEFORE starting io_context::run
		// This ensures the operation is queued and ready when the thread pool worker
		// picks up the io_context::run task, avoiding potential race conditions
		// detected by ThreadSanitizer
		asio::post(*io_context_, [this]() {
			do_accept();
		});

		// Run io_context using the centralized thread manager instead of direct std::thread
		io_context_future_ = integration::io_context_thread_manager::instance()
			.run_io_context(io_context_, "messaging_server:" + server_id_);

		NETWORK_LOG_INFO("[messaging_server] Started listening on port " + std::to_string(port));
	}

	auto messaging_server::stop_server() -> void
	{
		if (!is_running_.load())
		{
			return;
		}
		is_running_.store(false);

		// Close the acceptor
		if (acceptor_ && acceptor_->is_open())
		{
			asio::error_code ec;
			acceptor_->close(ec);
		}

		// Stop all active sessions
		for (auto& sess : sessions_)
		{
			if (sess)
			{
				sess->stop_session();
			}
		}
		sessions_.clear();

		// Release work guard to allow io_context to complete
		work_guard_.reset();

		// Stop io_context through the centralized manager
		if (io_context_)
		{
			integration::io_context_thread_manager::instance().stop_io_context(io_context_);
		}

		// Wait for io_context task to complete
		if (io_context_future_.valid())
		{
			io_context_future_.wait();
		}

		// Signal the promise for wait_for_stop()
		if (stop_promise_.has_value())
		{
			stop_promise_->set_value();
			stop_promise_.reset();
		}

		NETWORK_LOG_INFO("[messaging_server] Stopped.");
	}

	auto messaging_server::wait_for_stop() -> void
	{
		if (stop_future_.valid())
		{
			stop_future_.wait();
		}
	}

	auto messaging_server::do_accept() -> void
	{
		auto self = shared_from_this();
		acceptor_->async_accept(
			[this, self](std::error_code ec, tcp::socket sock)
			{ on_accept(ec, std::move(sock)); });
	}

	auto messaging_server::on_accept(std::error_code ec, tcp::socket socket)
		-> void
	{
		if (!is_running_.load())
		{
			return;
		}
		if (ec)
		{
			NETWORK_LOG_ERROR("[messaging_server] Accept error: " + ec.message());
			return;
		}

		// Create a new messaging_session
		auto new_session = std::make_shared<messaging_session>(
			std::move(socket), server_id_);

		// Track it in our sessions_ vector
		sessions_.push_back(new_session);

		// Start the session
		new_session->start_session();

		// Accept next connection
		do_accept();
	}

} // namespace kcenon::network::core