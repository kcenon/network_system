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

#include "network_system/core/session_manager.h"

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace network_system::core
{
	// Forward declaration
	class ws_connection;

	/*!
	 * \class ws_session_manager
	 * \brief Thread-safe WebSocket session lifecycle management.
	 *
	 * This manager provides the same connection management features as
	 * session_manager but for WebSocket connections instead of TCP sessions.
	 * It maintains type safety and clear separation while reusing the same
	 * configuration structure.
	 *
	 * Features:
	 * - Thread-safe connection tracking
	 * - Connection limit enforcement
	 * - Backpressure signaling
	 * - Connection metrics
	 * - Automatic connection ID generation
	 *
	 * Thread Safety:
	 * - All public methods are thread-safe
	 * - Uses shared_mutex for concurrent reads and exclusive writes
	 * - Atomic counters for metrics
	 *
	 * Usage Example:
	 * \code
	 * session_config config;
	 * config.max_sessions = 1000;
	 * auto manager = std::make_shared<ws_session_manager>(config);
	 *
	 * // Check before accepting new connection
	 * if (manager->can_accept_connection()) {
	 *     auto conn = create_ws_connection();
	 *     auto conn_id = manager->add_connection(conn);
	 *     if (!conn_id.empty()) {
	 *         // Connection accepted
	 *     }
	 * }
	 * \endcode
	 */
	class ws_session_manager
	{
	public:
		using ws_connection_ptr = std::shared_ptr<ws_connection>;

		/*!
		 * \brief Constructs a WebSocket session manager.
		 *
		 * \param config Session management configuration
		 */
		explicit ws_session_manager(const session_config& config = session_config())
			: config_(config)
			, connection_count_(0)
			, total_accepted_(0)
			, total_rejected_(0)
		{
		}

		/*!
		 * \brief Checks if a new connection can be accepted.
		 *
		 * \return True if under max_sessions limit
		 */
		[[nodiscard]] auto can_accept_connection() const -> bool
		{
			return connection_count_.load(std::memory_order_acquire) <
				   config_.max_sessions;
		}

		/*!
		 * \brief Checks if backpressure should be applied.
		 *
		 * Backpressure is activated when the connection count exceeds
		 * the configured threshold percentage of max_sessions.
		 *
		 * \return True if connection count exceeds backpressure threshold
		 */
		[[nodiscard]] auto is_backpressure_active() const -> bool
		{
			if (!config_.enable_backpressure)
			{
				return false;
			}

			auto count = connection_count_.load(std::memory_order_acquire);
			auto threshold = static_cast<size_t>(config_.max_sessions *
												  config_.backpressure_threshold);
			return count >= threshold;
		}

		/*!
		 * \brief Adds a connection to the manager.
		 *
		 * Thread-safe operation that adds a connection with automatic or
		 * provided ID. If max_sessions limit is reached, the connection
		 * is rejected.
		 *
		 * \param conn Connection to add
		 * \param conn_id Optional connection ID (auto-generated if empty)
		 * \return Connection ID that was used (empty string if rejected)
		 */
		auto add_connection(ws_connection_ptr conn, const std::string& conn_id = "")
			-> std::string
		{
			if (!can_accept_connection())
			{
				total_rejected_.fetch_add(1, std::memory_order_relaxed);
				return ""; // Rejected
			}

			std::unique_lock<std::shared_mutex> lock(connections_mutex_);
			std::string id = conn_id.empty() ? generate_connection_id() : conn_id;

			active_connections_[id] = conn;
			connection_count_.fetch_add(1, std::memory_order_release);
			total_accepted_.fetch_add(1, std::memory_order_relaxed);

			return id; // Return the ID that was used
		}

		/*!
		 * \brief Removes a connection from the manager.
		 *
		 * Thread-safe operation that removes a connection by ID.
		 *
		 * \param conn_id Connection ID to remove
		 * \return True if connection was found and removed
		 */
		auto remove_connection(const std::string& conn_id) -> bool
		{
			std::unique_lock<std::shared_mutex> lock(connections_mutex_);

			auto it = active_connections_.find(conn_id);
			if (it != active_connections_.end())
			{
				active_connections_.erase(it);
				connection_count_.fetch_sub(1, std::memory_order_release);
				return true;
			}
			return false;
		}

		/*!
		 * \brief Gets a connection by ID.
		 *
		 * Thread-safe read operation.
		 *
		 * \param conn_id Connection ID to lookup
		 * \return Shared pointer to connection, or nullptr if not found
		 */
		[[nodiscard]] auto get_connection(const std::string& conn_id) const
			-> ws_connection_ptr
		{
			std::shared_lock<std::shared_mutex> lock(connections_mutex_);
			auto it = active_connections_.find(conn_id);
			return (it != active_connections_.end()) ? it->second : nullptr;
		}

		/*!
		 * \brief Gets all active connections.
		 *
		 * Thread-safe operation that returns a snapshot of all connections.
		 *
		 * \return Vector of all active connections
		 */
		[[nodiscard]] auto get_all_connections() const -> std::vector<ws_connection_ptr>
		{
			std::shared_lock<std::shared_mutex> lock(connections_mutex_);
			std::vector<ws_connection_ptr> conns;
			conns.reserve(active_connections_.size());
			for (const auto& [id, conn] : active_connections_)
			{
				conns.push_back(conn);
			}
			return conns;
		}

		/*!
		 * \brief Gets all connection IDs.
		 *
		 * Thread-safe operation that returns a list of all connection IDs.
		 *
		 * \return Vector of connection IDs
		 */
		[[nodiscard]] auto get_all_connection_ids() const -> std::vector<std::string>
		{
			std::shared_lock<std::shared_mutex> lock(connections_mutex_);
			std::vector<std::string> ids;
			ids.reserve(active_connections_.size());
			for (const auto& [id, conn] : active_connections_)
			{
				ids.push_back(id);
			}
			return ids;
		}

		/*!
		 * \brief Gets the current connection count.
		 *
		 * \return Number of active connections
		 */
		[[nodiscard]] auto get_connection_count() const -> size_t
		{
			return connection_count_.load(std::memory_order_acquire);
		}

		/*!
		 * \brief Gets the total number of accepted connections.
		 *
		 * \return Total accepted connection count since creation
		 */
		[[nodiscard]] auto get_total_accepted() const -> uint64_t
		{
			return total_accepted_.load(std::memory_order_relaxed);
		}

		/*!
		 * \brief Gets the total number of rejected connections.
		 *
		 * \return Total rejected connection count since creation
		 */
		[[nodiscard]] auto get_total_rejected() const -> uint64_t
		{
			return total_rejected_.load(std::memory_order_relaxed);
		}

		/*!
		 * \brief Clears all connections.
		 *
		 * Thread-safe operation that removes all connections.
		 */
		auto clear_all_connections() -> void
		{
			std::unique_lock<std::shared_mutex> lock(connections_mutex_);
			active_connections_.clear();
			connection_count_.store(0, std::memory_order_release);
		}

		/*!
		 * \brief Generates a unique connection ID.
		 *
		 * This is a static method that can be used to generate IDs externally.
		 *
		 * \return Generated connection ID
		 */
		static auto generate_connection_id() -> std::string
		{
			static std::atomic<uint64_t> counter{0};
			return "ws_conn_" +
				   std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
		}

	private:
		session_config config_;
		mutable std::shared_mutex connections_mutex_;
		std::unordered_map<std::string, ws_connection_ptr> active_connections_;

		std::atomic<size_t> connection_count_;
		std::atomic<uint64_t> total_accepted_;
		std::atomic<uint64_t> total_rejected_;
	};

} // namespace network_system::core
