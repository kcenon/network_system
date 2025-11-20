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

#include "kcenon/network/core/connection_pool.h"
#include "kcenon/network/integration/logger_integration.h"

namespace network_system::core
{

	connection_pool::connection_pool(std::string host, unsigned short port,
									 size_t pool_size)
		: host_(std::move(host)), port_(port), pool_size_(pool_size)
	{
	}

	connection_pool::~connection_pool() noexcept
	{
		try
		{
			// Set shutdown flag
			is_shutdown_.store(true);

			// Notify all waiting threads
			cv_.notify_all();

			// Stop all connections
			std::lock_guard<std::mutex> lock(mutex_);
			while (!available_.empty())
			{
				auto client = std::move(available_.front());
				available_.pop();
				if (client)
				{
					client->stop_client();
				}
			}
		}
		catch (...)
		{
			// Destructor must not throw
		}
	}

	auto connection_pool::initialize() -> VoidResult
	{
		NETWORK_LOG_INFO("[connection_pool] Initializing pool with " +
			std::to_string(pool_size_) + " connections to " + host_ + ":" +
			std::to_string(port_));

		for (size_t i = 0; i < pool_size_; ++i)
		{
			auto client = std::make_unique<messaging_client>(
				"pool_client_" + std::to_string(i));

			// Connect to server
			auto result = client->start_client(host_, port_);
			if (result.is_err())
			{
				return error_void(
					result.error().code,
					"Failed to create pool connection " + std::to_string(i) +
						": " + result.error().message,
					"connection_pool::initialize",
					"Host: " + host_ + ", Port: " + std::to_string(port_)
				);
			}

			// Add to available queue
			std::lock_guard<std::mutex> lock(mutex_);
			available_.push(std::move(client));
		}

		NETWORK_LOG_INFO("[connection_pool] Successfully initialized " +
			std::to_string(pool_size_) + " connections");
		return ok();
	}

	auto connection_pool::acquire() -> std::unique_ptr<messaging_client>
	{
		std::unique_lock<std::mutex> lock(mutex_);

		// Wait until a connection is available or shutdown
		cv_.wait(lock, [this] {
			return !available_.empty() || is_shutdown_.load();
		});

		// Check if shutdown
		if (is_shutdown_.load())
		{
			return nullptr;
		}

		// Get connection from queue
		auto client = std::move(available_.front());
		available_.pop();
		active_count_.fetch_add(1, std::memory_order_relaxed);

		NETWORK_LOG_DEBUG("[connection_pool] Acquired connection. Active: " +
			std::to_string(active_count_.load()) + "/" +
			std::to_string(pool_size_));

		return client;
	}

	auto connection_pool::release(std::unique_ptr<messaging_client> client)
		-> void
	{
		if (!client)
		{
			return;
		}

		// Check if client is still connected, reconnect if necessary
		if (!client->is_connected())
		{
			NETWORK_LOG_WARN("[connection_pool] Connection lost, reconnecting...");

			auto result = client->start_client(host_, port_);
			if (result.is_err())
			{
				NETWORK_LOG_ERROR("[connection_pool] Failed to reconnect: " +
					result.error().message);
				// Drop this connection and decrement active count
				active_count_.fetch_sub(1, std::memory_order_relaxed);
				return;
			}
		}

		// Return to pool
		{
			std::lock_guard<std::mutex> lock(mutex_);
			available_.push(std::move(client));
		}

		active_count_.fetch_sub(1, std::memory_order_relaxed);

		// Notify one waiting thread
		cv_.notify_one();

		NETWORK_LOG_DEBUG("[connection_pool] Released connection. Active: " +
			std::to_string(active_count_.load()) + "/" +
			std::to_string(pool_size_));
	}

} // namespace network_system::core
