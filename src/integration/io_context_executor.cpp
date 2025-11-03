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

#include "network_system/integration/io_context_executor.h"
#include "network_system/integration/logger_integration.h"
#include <atomic>
#include <future>
#include <stdexcept>

namespace network_system::integration
{

	class io_context_executor::impl
	{
	public:
		impl(std::shared_ptr<kcenon::thread::thread_pool> pool,
			 asio::io_context& io_context,
			 const std::string& component_name)
			: pool_(std::move(pool)), io_context_(io_context), component_name_(component_name)
		{
		}

		void start()
		{
			if (running_.exchange(true))
			{
				NETWORK_LOG_WARN("[io_context_executor] Already running: " + component_name_);
				return;
			}

			if (!pool_ || !pool_->is_running())
			{
				running_.store(false);
				throw std::runtime_error("Thread pool not running for " + component_name_);
			}

			// Submit io_context::run() to thread pool using submit_task
			bool submitted = pool_->submit_task(
				[this]()
				{
					try
					{
						NETWORK_LOG_DEBUG("[io_context_executor] Starting io_context: " +
										  component_name_);

						io_context_.run();

						NETWORK_LOG_DEBUG("[io_context_executor] io_context stopped: " +
										  component_name_);
					}
					catch (const std::exception& e)
					{
						NETWORK_LOG_ERROR("[io_context_executor] Exception in " +
										  component_name_ + ": " + e.what());
					}
					catch (...)
					{
						NETWORK_LOG_ERROR("[io_context_executor] Unknown exception in " +
										  component_name_);
					}
					running_.store(false);
				});

			if (!submitted)
			{
				running_.store(false);
				throw std::runtime_error("Failed to submit io_context task for " + component_name_);
			}

			NETWORK_LOG_INFO("[io_context_executor] Started: " + component_name_);
		}

		void stop()
		{
			if (!running_.load())
			{
				return; // Already stopped
			}

			NETWORK_LOG_DEBUG("[io_context_executor] Stopping: " + component_name_);

			// Stop io_context
			if (!io_context_.stopped())
			{
				io_context_.stop();
			}

			// Wait for pool to finish (pool manages the actual thread)
			// The running_ flag will be cleared by the task itself
			while (running_.load())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			NETWORK_LOG_INFO("[io_context_executor] Stopped: " + component_name_);
		}

		[[nodiscard]] auto is_running() const -> bool { return running_.load(); }

		[[nodiscard]] auto get_pool() const -> std::shared_ptr<kcenon::thread::thread_pool>
		{
			return pool_;
		}

		[[nodiscard]] auto get_component_name() const -> std::string { return component_name_; }

	private:
		std::shared_ptr<kcenon::thread::thread_pool> pool_;
		asio::io_context& io_context_;
		std::string component_name_;
		std::atomic<bool> running_{false};
	};

	io_context_executor::io_context_executor(std::shared_ptr<kcenon::thread::thread_pool> pool,
											 asio::io_context& io_context,
											 const std::string& component_name)
		: pimpl_(std::make_unique<impl>(std::move(pool), io_context, component_name))
	{
	}

	io_context_executor::~io_context_executor()
	{
		if (pimpl_ && pimpl_->is_running())
		{
			pimpl_->stop();
		}
	}

	void io_context_executor::start() { pimpl_->start(); }

	void io_context_executor::stop() { pimpl_->stop(); }

	auto io_context_executor::is_running() const -> bool { return pimpl_->is_running(); }

	auto io_context_executor::get_pool() const -> std::shared_ptr<kcenon::thread::thread_pool>
	{
		return pimpl_->get_pool();
	}

	auto io_context_executor::get_component_name() const -> std::string
	{
		return pimpl_->get_component_name();
	}

} // namespace network_system::integration
