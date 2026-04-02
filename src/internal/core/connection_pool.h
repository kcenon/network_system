// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string>

#include "internal/core/messaging_client.h"
#include "kcenon/network/detail/utils/result_types.h"

namespace kcenon::network::core
{

	/*!
	 * \class connection_pool
	 * \brief Manages a pool of reusable client connections to reduce
	 *        connection overhead and improve performance.
	 *
	 * ### Key Features
	 * - Pre-creates a fixed number of connections at initialization
	 * - Provides thread-safe acquire/release semantics for borrowing connections
	 * - Blocks when all connections are in use until one becomes available
	 * - Automatically reconnects clients if connection is lost
	 * - Tracks active connection count for monitoring
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe
	 * - Uses mutex and condition variable for synchronization
	 * - Multiple threads can safely acquire/release connections concurrently
	 *
	 * ### Usage Example
	 * \code
	 * auto pool = std::make_shared<connection_pool>("localhost", 5555, 10);
	 * auto result = pool->initialize();
	 * if (!result) {
	 *     std::cerr << "Failed to initialize pool\n";
	 *     return;
	 * }
	 *
	 * // Acquire a connection
	 * auto client = pool->acquire();
	 * client->send_packet(data);
	 *
	 * // Release back to pool when done
	 * pool->release(std::move(client));
	 * \endcode
	 */
	class connection_pool
	{
	public:
		/*!
		 * \brief Constructs a connection pool.
		 * \param host The remote host to connect to
		 * \param port The remote port to connect to
		 * \param pool_size Number of connections to maintain in the pool (default: 10)
		 * \param acquire_timeout Maximum time to wait for an available connection
		 *        (default: 30 seconds). Use std::chrono::seconds::zero() for no
		 *        timeout.
		 */
		connection_pool(
			std::string host, unsigned short port, size_t pool_size = 10,
			std::chrono::seconds acquire_timeout = std::chrono::seconds(30));

		/*!
		 * \brief Destructor. Stops all connections and cleans up resources.
		 */
		~connection_pool() noexcept;

		/*!
		 * \brief Initializes the pool by creating and connecting all clients.
		 * \return Result<void> - Success if all connections established, or error
		 *
		 * This method should be called after construction and before using the pool.
		 * It pre-creates all connections to avoid latency on first use.
		 */
		auto initialize() -> VoidResult;

		/*!
		 * \brief Acquires a connection from the pool.
		 * \return A unique_ptr to messaging_client, or nullptr on timeout/shutdown
		 *
		 * This method blocks if no connections are available, up to the
		 * configured acquire timeout. Returns nullptr if the timeout elapses
		 * or the pool is shutting down.
		 */
		auto acquire() -> std::unique_ptr<messaging_client>;

		/*!
		 * \brief Releases a connection back to the pool.
		 * \param client The client to return to the pool
		 *
		 * The client is checked for connectivity and reconnected if necessary
		 * before being returned to the available queue.
		 */
		auto release(std::unique_ptr<messaging_client> client) -> void;

		/*!
		 * \brief Gets the number of active (borrowed) connections.
		 * \return Number of connections currently in use
		 */
		[[nodiscard]] auto active_count() const noexcept -> size_t
		{
			return active_count_.load(std::memory_order_relaxed);
		}

		/*!
		 * \brief Gets the total pool size.
		 * \return Total number of connections in the pool
		 */
		[[nodiscard]] auto pool_size() const noexcept -> size_t
		{
			return pool_size_;
		}

	private:
		std::string host_;              /*!< Remote host to connect to */
		unsigned short port_;           /*!< Remote port to connect to */
		size_t pool_size_;              /*!< Total number of connections */
		std::chrono::seconds acquire_timeout_;
												/*!< Max wait time for acquire() */

		std::queue<std::unique_ptr<messaging_client>> available_;
													/*!< Available connections */
		std::atomic<size_t> active_count_{0};
													/*!< Active connection count */
		std::mutex mutex_;              /*!< Protects available_ queue */
		std::condition_variable cv_;    /*!< Signals when connection available */
		std::atomic<bool> is_shutdown_{false};
													/*!< Shutdown flag */
	};

} // namespace kcenon::network::core
