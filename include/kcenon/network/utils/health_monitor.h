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

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <mutex>

#include <asio.hpp>

#include "kcenon/network/core/messaging_client.h"

namespace kcenon::network::utils
{

	/*!
	 * \struct connection_health
	 * \brief Contains connection health metrics
	 */
	struct connection_health
	{
		bool is_alive{true};                             /*!< Connection alive status */
		std::chrono::milliseconds last_response_time{0}; /*!< Last response time */
		size_t missed_heartbeats{0};                     /*!< Number of missed heartbeats */
		double packet_loss_rate{0.0};                    /*!< Packet loss rate (0.0-1.0) */
		std::chrono::steady_clock::time_point last_heartbeat; /*!< Last heartbeat timestamp */
	};

	/*!
	 * \class health_monitor
	 * \brief Monitors connection health with heartbeat mechanism
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe
	 * - Health metrics are protected by mutex
	 * - Heartbeat thread runs independently
	 *
	 * ### Key Features
	 * - Periodic heartbeat messages
	 * - Automatic dead connection detection
	 * - Connection quality metrics (latency, packet loss)
	 * - Health status callbacks
	 * - Configurable heartbeat interval
	 *
	 * ### Usage Example
	 * \code
	 * auto client = std::make_shared<messaging_client>("client_id");
	 * client->start_client("localhost", 8080);
	 *
	 * auto monitor = std::make_shared<health_monitor>(
	 *     std::chrono::seconds(30)  // heartbeat every 30 seconds
	 * );
	 *
	 * monitor->set_health_callback([](const connection_health& health) {
	 *     if (!health.is_alive) {
	 *         std::cerr << "Connection is dead!\n";
	 *     }
	 * });
	 *
	 * monitor->start_monitoring(client);
	 *
	 * // ... later ...
	 * auto health = monitor->get_health();
	 * std::cout << "Missed heartbeats: " << health.missed_heartbeats << "\n";
	 *
	 * monitor->stop_monitoring();
	 * \endcode
	 */
	class health_monitor : public std::enable_shared_from_this<health_monitor>
	{
	public:
		/*!
		 * \brief Constructs a health monitor
		 * \param heartbeat_interval Interval between heartbeat checks (default: 30s)
		 * \param max_missed_heartbeats Maximum missed heartbeats before marking as dead (default: 3)
		 */
		explicit health_monitor(
			std::chrono::seconds heartbeat_interval = std::chrono::seconds(30),
			size_t max_missed_heartbeats = 3);

		/*!
		 * \brief Destructor - stops monitoring if active
		 */
		~health_monitor() noexcept;

		/*!
		 * \brief Starts monitoring the given client
		 * \param client Client to monitor
		 *
		 * Begins periodic heartbeat checks. If client doesn't respond
		 * within the timeout, increments missed_heartbeats counter.
		 */
		auto start_monitoring(std::shared_ptr<core::messaging_client> client) -> void;

		/*!
		 * \brief Stops monitoring
		 *
		 * Cancels the heartbeat timer and stops the monitoring thread.
		 */
		auto stop_monitoring() -> void;

		/*!
		 * \brief Gets current health status
		 * \return Current connection health metrics
		 */
		[[nodiscard]] auto get_health() const -> connection_health;

		/*!
		 * \brief Sets callback for health status changes
		 * \param callback Function called when health status changes
		 *
		 * The callback receives the current health metrics.
		 */
		auto set_health_callback(
			std::function<void(const connection_health&)> callback) -> void;

		/*!
		 * \brief Checks if monitoring is active
		 * \return true if monitoring, false otherwise
		 */
		[[nodiscard]] auto is_monitoring() const noexcept -> bool;

	private:
		/*!
		 * \brief Performs a single heartbeat check
		 *
		 * Sends a heartbeat message and waits for response.
		 * Updates health metrics based on the result.
		 */
		auto do_heartbeat() -> void;

		/*!
		 * \brief Schedules the next heartbeat
		 */
		auto schedule_next_heartbeat() -> void;

		/*!
		 * \brief Updates health metrics
		 * \param success Whether heartbeat was successful
		 * \param response_time Response time in milliseconds
		 */
		auto update_health(bool success, std::chrono::milliseconds response_time) -> void;

	private:
		std::shared_ptr<core::messaging_client> client_; /*!< Monitored client */

		std::unique_ptr<asio::io_context> io_context_; /*!< IO context for timer */
		std::unique_ptr<asio::executor_work_guard<asio::io_context::executor_type>>
			work_guard_; /*!< Work guard to keep io_context running */
		std::unique_ptr<asio::steady_timer> heartbeat_timer_; /*!< Heartbeat timer */
		std::future<void> io_future_; /*!< Future for io_context execution */

		std::chrono::seconds heartbeat_interval_; /*!< Heartbeat interval */
		size_t max_missed_heartbeats_; /*!< Max missed heartbeats before dead */

		mutable std::mutex health_mutex_; /*!< Protects health metrics */
		connection_health health_; /*!< Current health metrics */

		std::atomic<bool> is_monitoring_{false}; /*!< Monitoring active flag */
		std::atomic<size_t> total_heartbeats_{0}; /*!< Total heartbeats sent */
		std::atomic<size_t> failed_heartbeats_{0}; /*!< Failed heartbeats */

		std::function<void(const connection_health&)> health_callback_; /*!< Health callback */
	};

} // namespace kcenon::network::utils
