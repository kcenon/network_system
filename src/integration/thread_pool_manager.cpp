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

#include "network_system/integration/thread_pool_manager.h"
#include "network_system/integration/logger_integration.h"
#include <kcenon/thread/interfaces/thread_context.h>
#include <kcenon/thread/core/thread_worker.h>
#include <mutex>
#include <unordered_map>
#include <stdexcept>

namespace network_system::integration
{

	// Implementation class using Pimpl idiom for ABI stability
	class thread_pool_manager::impl
	{
	public:
		impl() = default;

		auto initialize(size_t io_pool_count, size_t pipeline_workers, size_t utility_workers)
			-> bool
		{
			std::lock_guard<std::mutex> lock(mutex_);

			if (initialized_)
			{
				NETWORK_LOG_WARN("[thread_pool_manager] Already initialized");
				return false;
			}

			try
			{
				kcenon::thread::thread_context ctx;

				// Create pipeline pool (typed, priority-based)
				pipeline_pool_ = std::make_shared<
					kcenon::thread::typed_thread_pool_t<pipeline_priority>>("pipeline_pool",
																			 ctx);

				// Add workers to pipeline pool BEFORE starting
				// Workers handle all priority levels by passing all enum values
				std::vector<pipeline_priority> all_priorities = {pipeline_priority::REALTIME,
																  pipeline_priority::HIGH,
																  pipeline_priority::NORMAL,
																  pipeline_priority::LOW,
																  pipeline_priority::BACKGROUND};

				for (size_t i = 0; i < pipeline_workers; ++i)
				{
					auto worker =
						std::make_unique<kcenon::thread::typed_thread_worker_t<pipeline_priority>>(
							all_priorities);
					auto enqueue_result = pipeline_pool_->enqueue(std::move(worker));
					if (enqueue_result.has_error())
					{
						throw std::runtime_error("Failed to enqueue pipeline worker: " +
												 enqueue_result.get_error().message());
					}
				}

				// Start pipeline pool
				auto result = pipeline_pool_->start();
				if (result.has_error())
				{
					throw std::runtime_error("Failed to start pipeline pool: " +
											 result.get_error().message());
				}

				// Create utility pool
				utility_pool_ =
					std::make_shared<kcenon::thread::thread_pool>("utility_pool", ctx);

				// Add workers to utility pool BEFORE starting
				for (size_t i = 0; i < utility_workers; ++i)
				{
					auto worker = std::make_unique<kcenon::thread::thread_worker>();
					auto enqueue_result = utility_pool_->enqueue(std::move(worker));
					if (enqueue_result.has_error())
					{
						throw std::runtime_error("Failed to enqueue utility worker: " +
												 enqueue_result.get_error().message());
					}
				}

				// Start utility pool
				result = utility_pool_->start();
				if (result.has_error())
				{
					throw std::runtime_error("Failed to start utility pool: " +
											 result.get_error().message());
				}

				// Reserve space for I/O pools
				io_pools_.reserve(io_pool_count);

				// Store worker counts
				pipeline_workers_ = pipeline_workers;

				initialized_ = true;

				NETWORK_LOG_INFO(
					"[thread_pool_manager] Initialized: pipeline_workers=" +
					std::to_string(pipeline_workers) +
					", utility_workers=" + std::to_string(utility_workers));

				return true;
			}
			catch (const std::exception& e)
			{
				NETWORK_LOG_ERROR("[thread_pool_manager] Initialization failed: " +
								  std::string(e.what()));
				cleanup();
				return false;
			}
		}

		void shutdown()
		{
			std::lock_guard<std::mutex> lock(mutex_);

			if (!initialized_)
			{
				return;
			}

			NETWORK_LOG_INFO("[thread_pool_manager] Shutting down...");

			cleanup();
			initialized_ = false;

			NETWORK_LOG_INFO("[thread_pool_manager] Shutdown complete");
		}

		auto create_io_pool(const std::string& component_name)
			-> std::shared_ptr<kcenon::thread::thread_pool>
		{
			std::lock_guard<std::mutex> lock(mutex_);

			if (!initialized_)
			{
				throw std::runtime_error(
					"thread_pool_manager not initialized. Call initialize() first.");
			}

			// Create new pool with 1 worker
			kcenon::thread::thread_context ctx;
			auto pool = std::make_shared<kcenon::thread::thread_pool>("io_pool:" + component_name,
																	   ctx);

			// Add exactly 1 worker for I/O BEFORE starting
			auto worker = std::make_unique<kcenon::thread::thread_worker>();
			auto enqueue_result = pool->enqueue(std::move(worker));
			if (enqueue_result.has_error())
			{
				throw std::runtime_error("Failed to enqueue I/O worker for " + component_name +
										 ": " + enqueue_result.get_error().message());
			}

			// Start the pool
			auto result = pool->start();
			if (result.has_error())
			{
				throw std::runtime_error("Failed to start I/O pool for " + component_name + ": " +
										 result.get_error().message());
			}

			// Store pool
			io_pools_[component_name] = pool;

			NETWORK_LOG_DEBUG("[thread_pool_manager] Created I/O pool for: " + component_name);

			return pool;
		}

		auto get_pipeline_pool()
			-> std::shared_ptr<kcenon::thread::typed_thread_pool_t<pipeline_priority>>
		{
			std::lock_guard<std::mutex> lock(mutex_);

			if (!initialized_)
			{
				throw std::runtime_error(
					"thread_pool_manager not initialized. Call initialize() first.");
			}

			return pipeline_pool_;
		}

		auto get_utility_pool() -> std::shared_ptr<kcenon::thread::thread_pool>
		{
			std::lock_guard<std::mutex> lock(mutex_);

			if (!initialized_)
			{
				throw std::runtime_error(
					"thread_pool_manager not initialized. Call initialize() first.");
			}

			return utility_pool_;
		}

		[[nodiscard]] auto get_statistics() const -> thread_pool_manager::statistics
		{
			std::lock_guard<std::mutex> lock(mutex_);

			statistics stats;
			stats.is_initialized = initialized_;
			stats.total_io_pools = io_pools_.size();

			if (!initialized_)
			{
				return stats;
			}

			// Collect I/O pool stats
			// Note: thread_pool doesn't expose get_active_thread_count(), so we count threads
			for (const auto& [name, pool] : io_pools_)
			{
				if (pool)
				{
					stats.active_io_tasks += pool->get_thread_count();
				}
			}

			// Pipeline stats
			if (pipeline_pool_)
			{
				// typed_thread_pool_t doesn't have get_pending_task_count or get_thread_count
				// We'll leave queue size as 0 for now
				stats.pipeline_queue_size = 0;
				stats.pipeline_workers = pipeline_workers_;
			}

			// Utility stats
			if (utility_pool_)
			{
				stats.utility_queue_size = utility_pool_->get_pending_task_count();
				stats.utility_workers = utility_pool_->get_thread_count();
			}

			return stats;
		}

		[[nodiscard]] auto is_initialized() const -> bool
		{
			std::lock_guard<std::mutex> lock(mutex_);
			return initialized_;
		}

	private:
		void cleanup()
		{
			// Stop all I/O pools
			for (auto& [name, pool] : io_pools_)
			{
				if (pool)
				{
					pool->stop();
				}
			}
			io_pools_.clear();

			// Stop pipeline pool
			if (pipeline_pool_)
			{
				pipeline_pool_->stop();
				pipeline_pool_.reset();
			}

			// Stop utility pool
			if (utility_pool_)
			{
				utility_pool_->stop();
				utility_pool_.reset();
			}
		}

		mutable std::mutex mutex_;
		bool initialized_ = false;
		size_t pipeline_workers_ = 0; // Store pipeline worker count for statistics

		std::shared_ptr<kcenon::thread::typed_thread_pool_t<pipeline_priority>> pipeline_pool_;
		std::shared_ptr<kcenon::thread::thread_pool> utility_pool_;
		std::unordered_map<std::string, std::shared_ptr<kcenon::thread::thread_pool>> io_pools_;
	};

	// Singleton implementation
	auto thread_pool_manager::instance() -> thread_pool_manager&
	{
		static thread_pool_manager instance;
		return instance;
	}

	thread_pool_manager::~thread_pool_manager()
	{
		if (pimpl_)
		{
			pimpl_->shutdown();
		}
	}

	auto thread_pool_manager::initialize(size_t io_pool_count,
										 size_t pipeline_workers,
										 size_t utility_workers) -> bool
	{
		if (!pimpl_)
		{
			pimpl_ = std::make_unique<impl>();
		}
		return pimpl_->initialize(io_pool_count, pipeline_workers, utility_workers);
	}

	auto thread_pool_manager::is_initialized() const -> bool
	{
		return pimpl_ && pimpl_->is_initialized();
	}

	void thread_pool_manager::shutdown()
	{
		if (pimpl_)
		{
			pimpl_->shutdown();
		}
	}

	auto thread_pool_manager::create_io_pool(const std::string& component_name)
		-> std::shared_ptr<kcenon::thread::thread_pool>
	{
		if (!pimpl_)
		{
			throw std::runtime_error("thread_pool_manager not initialized");
		}
		return pimpl_->create_io_pool(component_name);
	}

	auto thread_pool_manager::get_pipeline_pool()
		-> std::shared_ptr<kcenon::thread::typed_thread_pool_t<pipeline_priority>>
	{
		if (!pimpl_)
		{
			throw std::runtime_error("thread_pool_manager not initialized");
		}
		return pimpl_->get_pipeline_pool();
	}

	auto thread_pool_manager::get_utility_pool() -> std::shared_ptr<kcenon::thread::thread_pool>
	{
		if (!pimpl_)
		{
			throw std::runtime_error("thread_pool_manager not initialized");
		}
		return pimpl_->get_utility_pool();
	}

	auto thread_pool_manager::get_statistics() const -> thread_pool_manager::statistics
	{
		if (!pimpl_)
		{
			return statistics{};
		}
		return pimpl_->get_statistics();
	}

} // namespace network_system::integration
