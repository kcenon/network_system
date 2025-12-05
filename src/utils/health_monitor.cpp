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

#include "kcenon/network/utils/health_monitor.h"

#include "kcenon/network/integration/logger_integration.h"
#include "kcenon/network/integration/monitoring_integration.h"
#include "kcenon/network/integration/thread_integration.h"

#include <sstream>

namespace network_system::utils
{

	health_monitor::health_monitor(std::chrono::seconds heartbeat_interval,
								   size_t max_missed_heartbeats)
		: heartbeat_interval_(heartbeat_interval)
		, max_missed_heartbeats_(max_missed_heartbeats)
	{
		health_.last_heartbeat = std::chrono::steady_clock::now();
		NETWORK_LOG_INFO("[health_monitor] Created with interval=" +
			std::to_string(heartbeat_interval.count()) + "s, max_missed=" +
			std::to_string(max_missed_heartbeats));
	}

	health_monitor::~health_monitor() noexcept
	{
		try
		{
			stop_monitoring();
		}
		catch (...)
		{
			// Destructor must not throw
		}
	}

	auto health_monitor::start_monitoring(
		std::shared_ptr<core::messaging_client> client) -> void
	{
		if (is_monitoring_.load())
		{
			NETWORK_LOG_WARN("[health_monitor] Already monitoring");
			return;
		}

		if (!client)
		{
			NETWORK_LOG_ERROR("[health_monitor] Cannot monitor null client");
			return;
		}

		client_ = client;

		// Create io_context and work guard
		io_context_ = std::make_unique<asio::io_context>();
		work_guard_ = std::make_unique<asio::executor_work_guard<asio::io_context::executor_type>>(
			asio::make_work_guard(*io_context_)
		);

		// Create heartbeat timer
		heartbeat_timer_ = std::make_unique<asio::steady_timer>(*io_context_);

		is_monitoring_.store(true);

		// Reset metrics
		total_heartbeats_.store(0);
		failed_heartbeats_.store(0);
		{
			std::lock_guard<std::mutex> lock(health_mutex_);
			health_.is_alive = true;
			health_.missed_heartbeats = 0;
			health_.packet_loss_rate = 0.0;
			health_.last_heartbeat = std::chrono::steady_clock::now();
		}

		// Submit io_context::run() to thread pool
		io_future_ = integration::thread_integration_manager::instance().submit_task(
			[this]()
			{
				try
				{
					io_context_->run();
				}
				catch (const std::exception& e)
				{
					NETWORK_LOG_ERROR("[health_monitor] Exception in io_context run: " +
						std::string(e.what()));
				}
			}
		);

		// Schedule first heartbeat
		schedule_next_heartbeat();

		NETWORK_LOG_INFO("[health_monitor] Started monitoring");
	}

	auto health_monitor::stop_monitoring() -> void
	{
		if (!is_monitoring_.exchange(false))
		{
			return;
		}

		// Cancel timer
		if (heartbeat_timer_)
		{
			heartbeat_timer_->cancel();
		}

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

		// Wait for io_context task to complete
		if (io_future_.valid())
		{
			io_future_.wait();
		}

		// Release resources
		heartbeat_timer_.reset();
		io_context_.reset();
		client_.reset();

		NETWORK_LOG_INFO("[health_monitor] Stopped monitoring");
	}

	auto health_monitor::get_health() const -> connection_health
	{
		std::lock_guard<std::mutex> lock(health_mutex_);
		return health_;
	}

	auto health_monitor::set_health_callback(
		std::function<void(const connection_health&)> callback) -> void
	{
		health_callback_ = std::move(callback);
	}

	auto health_monitor::is_monitoring() const noexcept -> bool
	{
		return is_monitoring_.load();
	}

	auto health_monitor::do_heartbeat() -> void
	{
		if (!is_monitoring_.load() || !client_)
		{
			return;
		}

		auto start_time = std::chrono::steady_clock::now();

		// Check if client is still connected
		bool is_connected = client_->is_connected();

		total_heartbeats_.fetch_add(1);

		if (is_connected)
		{
			// Send heartbeat packet (simple ping message)
			std::vector<uint8_t> heartbeat_data = {
				0xFF, 0xFE,  // Magic bytes for heartbeat
				0x01,        // Heartbeat type
				0x00         // Padding
			};

			auto send_result = client_->send_packet(std::move(heartbeat_data));

			auto end_time = std::chrono::steady_clock::now();
			auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
				end_time - start_time);

			if (!send_result.is_err())
			{
				// Heartbeat successful
				update_health(true, response_time);
				NETWORK_LOG_DEBUG("[health_monitor] Heartbeat successful, response_time=" +
					std::to_string(response_time.count()) + "ms");
			}
			else
			{
				// Heartbeat failed
				failed_heartbeats_.fetch_add(1);
				update_health(false, response_time);
				NETWORK_LOG_WARN("[health_monitor] Heartbeat failed: " +
					send_result.error().message);
			}
		}
		else
		{
			// Client disconnected
			failed_heartbeats_.fetch_add(1);
			update_health(false, std::chrono::milliseconds(0));
			NETWORK_LOG_WARN("[health_monitor] Client is disconnected");
		}

		// Schedule next heartbeat if still monitoring
		if (is_monitoring_.load())
		{
			schedule_next_heartbeat();
		}
	}

	auto health_monitor::schedule_next_heartbeat() -> void
	{
		if (!heartbeat_timer_ || !is_monitoring_.load())
		{
			return;
		}

		heartbeat_timer_->expires_after(heartbeat_interval_);

		auto self = shared_from_this();
		heartbeat_timer_->async_wait(
			[this, self](const std::error_code& ec)
			{
				if (!ec && is_monitoring_.load())
				{
					do_heartbeat();
				}
			}
		);
	}

	auto health_monitor::update_health(bool success,
									   std::chrono::milliseconds response_time) -> void
	{
		std::lock_guard<std::mutex> lock(health_mutex_);

		health_.last_heartbeat = std::chrono::steady_clock::now();
		health_.last_response_time = response_time;

		if (success)
		{
			// Reset missed heartbeats on success
			health_.missed_heartbeats = 0;
			health_.is_alive = true;
		}
		else
		{
			// Increment missed heartbeats
			health_.missed_heartbeats++;

			// Mark as dead if exceeded threshold
			if (health_.missed_heartbeats >= max_missed_heartbeats_)
			{
				health_.is_alive = false;
				NETWORK_LOG_ERROR("[health_monitor] Connection marked as DEAD (missed " +
					std::to_string(health_.missed_heartbeats) + " heartbeats)");
			}
		}

		// Calculate packet loss rate
		auto total = total_heartbeats_.load();
		auto failed = failed_heartbeats_.load();
		if (total > 0)
		{
			health_.packet_loss_rate = static_cast<double>(failed) / static_cast<double>(total);
		}

		// Report metrics to monitoring system (optional)
		try
		{
			auto& monitor_mgr = integration::monitoring_integration_manager::instance();
			auto monitor = monitor_mgr.get_monitoring();
			if (monitor && client_)
			{
				// Use client pointer address as connection identifier
				std::ostringstream oss;
				oss << "client_" << static_cast<const void*>(client_.get());

				monitor->report_health(
					oss.str(),
					health_.is_alive,
					static_cast<double>(health_.last_response_time.count()),
					health_.missed_heartbeats,
					health_.packet_loss_rate
				);
			}
		}
		catch (...)
		{
			// Monitoring failure is not critical, continue normal operation
		}

		// Invoke health callback if set
		if (health_callback_)
		{
			health_callback_(health_);
		}
	}

} // namespace network_system::utils
