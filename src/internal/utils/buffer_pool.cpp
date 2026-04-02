// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "buffer_pool.h"
#include "internal/integration/logger_integration.h"

#include <atomic>
#include <deque>
#include <mutex>

namespace kcenon::network::utils
{

	class buffer_pool::impl
	{
	public:
		explicit impl(size_t pool_size, size_t default_capacity)
			: pool_size_(pool_size), default_capacity_(default_capacity), total_allocated_(0)
		{
			NETWORK_LOG_DEBUG("[buffer_pool] Created with pool_size=" +
							  std::to_string(pool_size) +
							  ", default_capacity=" + std::to_string(default_capacity));
		}

		auto acquire(size_t min_capacity, buffer_pool* parent) -> std::shared_ptr<std::vector<uint8_t>>
		{
			std::unique_lock<std::mutex> lock(mutex_);

			// Try to find a suitable buffer in the pool
			for (auto it = available_buffers_.begin(); it != available_buffers_.end(); ++it)
			{
				if ((*it)->capacity() >= min_capacity)
				{
					auto buffer = std::move(*it);
					available_buffers_.erase(it);

					// Clear the buffer but keep capacity
					buffer->clear();

					NETWORK_LOG_TRACE("[buffer_pool] Acquired buffer from pool, capacity=" +
									  std::to_string(buffer->capacity()));

					// Return with custom deleter that returns buffer to pool
					return std::shared_ptr<std::vector<uint8_t>>(
						buffer.release(), [parent](std::vector<uint8_t>* buf) {
							if (parent)
							{
								parent->release(buf);
							}
							else
							{
								delete buf;
							}
						});
				}
			}

			// No suitable buffer found, create a new one
			size_t capacity = std::max(min_capacity, default_capacity_);
			auto buffer = std::make_unique<std::vector<uint8_t>>();
			buffer->reserve(capacity);

			total_allocated_.fetch_add(1);

			NETWORK_LOG_TRACE("[buffer_pool] Created new buffer, capacity=" +
							  std::to_string(capacity) +
							  ", total_allocated=" + std::to_string(total_allocated_.load()));

			// Return with custom deleter
			return std::shared_ptr<std::vector<uint8_t>>(
				buffer.release(), [parent](std::vector<uint8_t>* buf) {
					if (parent)
					{
						parent->release(buf);
					}
					else
					{
						delete buf;
					}
				});
		}

		auto release(std::vector<uint8_t>* buffer) -> void
		{
			if (!buffer)
				return;

			std::unique_lock<std::mutex> lock(mutex_);

			// If pool is full, just delete the buffer
			if (available_buffers_.size() >= pool_size_)
			{
				NETWORK_LOG_TRACE("[buffer_pool] Pool full, deleting buffer");
				delete buffer;
				total_allocated_.fetch_sub(1);
				return;
			}

			// Return buffer to pool
			available_buffers_.emplace_back(buffer);

			NETWORK_LOG_TRACE("[buffer_pool] Returned buffer to pool, available=" +
							  std::to_string(available_buffers_.size()));
		}

		auto get_stats() const -> std::pair<size_t, size_t>
		{
			std::lock_guard<std::mutex> lock(mutex_);
			return {available_buffers_.size(), total_allocated_.load()};
		}

		auto clear() -> void
		{
			std::unique_lock<std::mutex> lock(mutex_);

			size_t cleared = available_buffers_.size();
			available_buffers_.clear();

			total_allocated_.fetch_sub(cleared);

			NETWORK_LOG_INFO("[buffer_pool] Cleared " + std::to_string(cleared) + " buffers");
		}

	private:
		size_t pool_size_;
		size_t default_capacity_;
		std::atomic<size_t> total_allocated_;

		mutable std::mutex mutex_;
		std::deque<std::unique_ptr<std::vector<uint8_t>>> available_buffers_;
	};

	buffer_pool::buffer_pool(size_t pool_size, size_t default_capacity)
		: pimpl_(std::make_unique<impl>(pool_size, default_capacity))
	{
	}

	buffer_pool::~buffer_pool() noexcept
	{
		try
		{
			pimpl_->clear();
		}
		catch (...)
		{
			// Destructor must not throw
		}
	}

	auto buffer_pool::acquire(size_t min_capacity) -> std::shared_ptr<std::vector<uint8_t>>
	{
		return pimpl_->acquire(min_capacity, this);
	}

	auto buffer_pool::release(std::vector<uint8_t>* buffer) -> void
	{
		pimpl_->release(buffer);
	}

	auto buffer_pool::get_stats() const -> std::pair<size_t, size_t>
	{
		return pimpl_->get_stats();
	}

	auto buffer_pool::clear() -> void { pimpl_->clear(); }

} // namespace kcenon::network::utils
