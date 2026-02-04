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

#include <cstdint>
#include <memory>
#include <vector>

namespace kcenon::network::utils
{

	/*!
	 * \class buffer_pool
	 * \brief Thread-safe memory pool for reusable byte buffers
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe
	 * - Uses mutex protection for pool access
	 * - Safe for concurrent acquire/release operations
	 *
	 * ### Key Features
	 * - Pre-allocated buffer pool reduces allocations
	 * - Automatic buffer size adjustment
	 * - Configurable pool size and buffer capacity
	 * - RAII-based buffer management with custom deleter
	 *
	 * ### Usage Example
	 * \code
	 * auto pool = std::make_shared<buffer_pool>(
	 *     32,      // pool size: 32 buffers
	 *     8192     // default buffer capacity: 8KB
	 * );
	 *
	 * // Acquire a buffer
	 * auto buffer = pool->acquire(4096);  // Get buffer with at least 4KB
	 *
	 * // Use buffer...
	 * buffer->resize(data_size);
	 * std::copy(data, data + data_size, buffer->begin());
	 *
	 * // Buffer automatically returns to pool when shared_ptr is destroyed
	 * \endcode
	 */
	class buffer_pool : public std::enable_shared_from_this<buffer_pool>
	{
	public:
		/*!
		 * \brief Constructs a buffer pool
		 * \param pool_size Maximum number of buffers to cache
		 * \param default_capacity Default capacity for new buffers
		 */
		explicit buffer_pool(size_t pool_size = 32, size_t default_capacity = 8192);

		/*!
		 * \brief Destructor
		 */
		~buffer_pool() noexcept;

		/*!
		 * \brief Acquires a buffer from the pool
		 * \param min_capacity Minimum required capacity
		 * \return Shared pointer to buffer (automatically returned to pool when destroyed)
		 *
		 * If no suitable buffer is available in the pool, creates a new one.
		 * Buffer is returned to pool automatically when the shared_ptr is destroyed.
		 */
		auto acquire(size_t min_capacity = 0) -> std::shared_ptr<std::vector<uint8_t>>;

		/*!
		 * \brief Gets current pool statistics
		 * \return Pair of (available buffers, total allocated)
		 */
		auto get_stats() const -> std::pair<size_t, size_t>;

		/*!
		 * \brief Clears the pool and releases all cached buffers
		 */
		auto clear() -> void;

	private:
		/*!
		 * \brief Returns a buffer to the pool
		 * \param buffer Buffer to return
		 *
		 * Called automatically by custom deleter when shared_ptr is destroyed.
		 */
		auto release(std::vector<uint8_t>* buffer) -> void;

	private:
		class impl;
		std::unique_ptr<impl> pimpl_;
	};

} // namespace kcenon::network::utils
