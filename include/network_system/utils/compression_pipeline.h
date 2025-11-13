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

#include "network_system/utils/result_types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace network_system::utils
{

	/*!
	 * \enum compression_algorithm
	 * \brief Supported compression algorithms
	 */
	enum class compression_algorithm
	{
		none,    /*!< No compression */
		lz4,     /*!< LZ4 fast compression */
		gzip,    /*!< GZIP compression (RFC 1952) */
		deflate  /*!< DEFLATE compression (RFC 1951) */
	};

	/*!
	 * \class compression_pipeline
	 * \brief Message compression and decompression pipeline
	 *
	 * ### Thread Safety
	 * - All public methods are thread-safe
	 * - Can be used from multiple threads concurrently
	 *
	 * ### Key Features
	 * - LZ4 fast compression algorithm
	 * - Configurable compression threshold
	 * - Automatic size threshold handling
	 * - Error handling via Result<T> pattern
	 *
	 * ### Usage Example
	 * \code
	 * auto pipeline = std::make_shared<compression_pipeline>(
	 *     compression_algorithm::lz4,
	 *     256  // Don't compress messages smaller than 256 bytes
	 * );
	 *
	 * // Compress data
	 * std::vector<uint8_t> data = {...};
	 * auto result = pipeline->compress(data);
	 * if (!result.is_err()) {
	 *     auto compressed = result.value();
	 *     // Send compressed data...
	 * }
	 *
	 * // Decompress data
	 * auto decompressed = pipeline->decompress(compressed);
	 * \endcode
	 */
	class compression_pipeline
	{
	public:
		/*!
		 * \brief Constructs a compression pipeline
		 * \param algo Compression algorithm to use
		 * \param compression_threshold Minimum size to compress (bytes)
		 */
		explicit compression_pipeline(
			compression_algorithm algo = compression_algorithm::lz4,
			size_t compression_threshold = 256);

		/*!
		 * \brief Destructor
		 */
		~compression_pipeline() noexcept;

		/*!
		 * \brief Compresses input data
		 * \param input Data to compress
		 * \return Compressed data or error
		 *
		 * If input size is below threshold, returns uncompressed data.
		 * If compression fails or produces larger output, returns uncompressed data.
		 */
		auto compress(std::span<const uint8_t> input)
			-> Result<std::vector<uint8_t>>;

		/*!
		 * \brief Compresses input data (vector overload)
		 * \param input Data to compress
		 * \return Compressed data or error
		 */
		auto compress(const std::vector<uint8_t>& input)
			-> Result<std::vector<uint8_t>>;

		/*!
		 * \brief Decompresses input data
		 * \param input Data to decompress
		 * \return Decompressed data or error
		 */
		auto decompress(std::span<const uint8_t> input)
			-> Result<std::vector<uint8_t>>;

		/*!
		 * \brief Decompresses input data (vector overload)
		 * \param input Data to decompress
		 * \return Decompressed data or error
		 */
		auto decompress(const std::vector<uint8_t>& input)
			-> Result<std::vector<uint8_t>>;

		/*!
		 * \brief Sets compression threshold
		 * \param bytes Minimum size to compress
		 */
		auto set_compression_threshold(size_t bytes) -> void;

		/*!
		 * \brief Gets current compression threshold
		 * \return Threshold in bytes
		 */
		auto get_compression_threshold() const -> size_t;

		/*!
		 * \brief Gets current algorithm
		 * \return Compression algorithm
		 */
		auto get_algorithm() const -> compression_algorithm;

	private:
		class impl;
		std::unique_ptr<impl> pimpl_;
	};

	/*!
	 * \brief Creates a compression function for pipeline integration
	 * \param pipeline Compression pipeline to use
	 * \return Function object for compress operation
	 */
	auto make_compress_function(std::shared_ptr<compression_pipeline> pipeline)
		-> std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>;

	/*!
	 * \brief Creates a decompression function for pipeline integration
	 * \param pipeline Compression pipeline to use
	 * \return Function object for decompress operation
	 */
	auto make_decompress_function(std::shared_ptr<compression_pipeline> pipeline)
		-> std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>;

} // namespace network_system::utils
