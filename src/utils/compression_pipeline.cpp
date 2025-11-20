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

#include "kcenon/network/utils/compression_pipeline.h"
#include "kcenon/network/integration/logger_integration.h"

#ifdef BUILD_LZ4_COMPRESSION
#include <lz4.h>
#include <lz4hc.h>
#endif

#ifdef BUILD_ZLIB_COMPRESSION
#include <zlib.h>
#endif

#include <cstring>

namespace network_system::utils
{

	class compression_pipeline::impl
	{
	public:
		explicit impl(compression_algorithm algo, size_t threshold)
			: algorithm_(algo), compression_threshold_(threshold)
		{
			NETWORK_LOG_DEBUG("[compression_pipeline] Created with algorithm=" +
							  std::to_string(static_cast<int>(algo)) +
							  ", threshold=" + std::to_string(threshold));
		}

		auto compress(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>
		{
			// If below threshold, return uncompressed
			if (input.size() < compression_threshold_)
			{
				NETWORK_LOG_TRACE("[compression_pipeline] Size below threshold, skipping compression");
				std::vector<uint8_t> result(input.begin(), input.end());
				return ok<std::vector<uint8_t>>(std::move(result));
			}

			if (algorithm_ == compression_algorithm::none)
			{
				std::vector<uint8_t> result(input.begin(), input.end());
				return ok<std::vector<uint8_t>>(std::move(result));
			}

#ifdef BUILD_LZ4_COMPRESSION
			if (algorithm_ == compression_algorithm::lz4)
			{
				return compress_lz4(input);
			}
#endif

#ifdef BUILD_ZLIB_COMPRESSION
			if (algorithm_ == compression_algorithm::gzip)
			{
				return compress_gzip(input);
			}
			if (algorithm_ == compression_algorithm::deflate)
			{
				return compress_deflate(input);
			}
#endif

			// Fallback: return uncompressed if algorithm not available
			NETWORK_LOG_WARN("[compression_pipeline] Compression algorithm not available, returning uncompressed");
			std::vector<uint8_t> result(input.begin(), input.end());
			return ok<std::vector<uint8_t>>(std::move(result));
		}

		auto decompress(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>
		{
			if (input.empty())
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::invalid_argument,
					"Input data is empty");
			}

			if (algorithm_ == compression_algorithm::none)
			{
				std::vector<uint8_t> result(input.begin(), input.end());
				return ok<std::vector<uint8_t>>(std::move(result));
			}

#ifdef BUILD_LZ4_COMPRESSION
			if (algorithm_ == compression_algorithm::lz4)
			{
				return decompress_lz4(input);
			}
#endif

#ifdef BUILD_ZLIB_COMPRESSION
			if (algorithm_ == compression_algorithm::gzip)
			{
				return decompress_gzip(input);
			}
			if (algorithm_ == compression_algorithm::deflate)
			{
				return decompress_deflate(input);
			}
#endif

			// Fallback: assume uncompressed
			NETWORK_LOG_WARN("[compression_pipeline] Decompression algorithm not available, returning as-is");
			std::vector<uint8_t> result(input.begin(), input.end());
			return ok<std::vector<uint8_t>>(std::move(result));
		}

		auto set_compression_threshold(size_t bytes) -> void
		{
			compression_threshold_ = bytes;
			NETWORK_LOG_DEBUG("[compression_pipeline] Set threshold=" + std::to_string(bytes));
		}

		auto get_compression_threshold() const -> size_t { return compression_threshold_; }

		auto get_algorithm() const -> compression_algorithm { return algorithm_; }

	private:
#ifdef BUILD_LZ4_COMPRESSION
		auto compress_lz4(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>
		{
			// Calculate maximum compressed size
			int max_compressed_size = LZ4_compressBound(static_cast<int>(input.size()));
			if (max_compressed_size <= 0)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::internal_error,
					"Failed to calculate LZ4 compressed size bound");
			}

			// Allocate buffer for compressed data + 4-byte header (original size)
			std::vector<uint8_t> compressed(max_compressed_size + 4);

			// Store original size in first 4 bytes (little-endian)
			uint32_t original_size = static_cast<uint32_t>(input.size());
			std::memcpy(compressed.data(), &original_size, 4);

			// Compress data
			int compressed_size = LZ4_compress_default(
				reinterpret_cast<const char*>(input.data()),
				reinterpret_cast<char*>(compressed.data() + 4),
				static_cast<int>(input.size()),
				max_compressed_size);

			if (compressed_size <= 0)
			{
				NETWORK_LOG_WARN("[compression_pipeline] LZ4 compression failed, returning uncompressed");
				std::vector<uint8_t> result(input.begin(), input.end());
				return ok<std::vector<uint8_t>>(std::move(result));
			}

			// If compressed size is not smaller, return uncompressed
			if (static_cast<size_t>(compressed_size + 4) >= input.size())
			{
				NETWORK_LOG_TRACE("[compression_pipeline] Compressed size not smaller, returning uncompressed");
				std::vector<uint8_t> result(input.begin(), input.end());
				return ok<std::vector<uint8_t>>(std::move(result));
			}

			// Resize to actual compressed size
			compressed.resize(compressed_size + 4);

			NETWORK_LOG_TRACE("[compression_pipeline] Compressed " +
							  std::to_string(input.size()) + " -> " +
							  std::to_string(compressed.size()) + " bytes (" +
							  std::to_string(100 - (compressed.size() * 100 / input.size())) + "% reduction)");

			return ok<std::vector<uint8_t>>(std::move(compressed));
		}

		auto decompress_lz4(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>
		{
			// Need at least 4 bytes for size header
			if (input.size() < 4)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::invalid_argument,
					"Compressed data too small");
			}

			// Read original size from first 4 bytes
			uint32_t original_size;
			std::memcpy(&original_size, input.data(), 4);

			// Sanity check on decompressed size (max 100MB)
			if (original_size > 100 * 1024 * 1024)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::invalid_argument,
					"Decompressed size too large: " + std::to_string(original_size));
			}

			// Allocate buffer for decompressed data
			std::vector<uint8_t> decompressed(original_size);

			// Decompress
			int decompressed_size = LZ4_decompress_safe(
				reinterpret_cast<const char*>(input.data() + 4),
				reinterpret_cast<char*>(decompressed.data()),
				static_cast<int>(input.size() - 4),
				static_cast<int>(original_size));

			if (decompressed_size < 0)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::internal_error,
					"LZ4 decompression failed");
			}

			if (static_cast<size_t>(decompressed_size) != original_size)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::internal_error,
					"Decompressed size mismatch");
			}

			NETWORK_LOG_TRACE("[compression_pipeline] Decompressed " +
							  std::to_string(input.size()) + " -> " +
							  std::to_string(decompressed.size()) + " bytes");

			return ok<std::vector<uint8_t>>(std::move(decompressed));
		}
#endif

#ifdef BUILD_ZLIB_COMPRESSION
		auto compress_gzip(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>
		{
			z_stream stream{};
			stream.zalloc = Z_NULL;
			stream.zfree = Z_NULL;
			stream.opaque = Z_NULL;

			// gzip initialization (windowBits = 15 + 16 for gzip)
			if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
							15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::internal_error,
					"Failed to initialize gzip compression");
			}

			stream.avail_in = static_cast<uInt>(input.size());
			stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));

			std::vector<uint8_t> compressed;
			compressed.resize(deflateBound(&stream, static_cast<uLong>(input.size())));

			stream.avail_out = static_cast<uInt>(compressed.size());
			stream.next_out = compressed.data();

			int ret = deflate(&stream, Z_FINISH);
			deflateEnd(&stream);

			if (ret != Z_STREAM_END)
			{
				NETWORK_LOG_WARN("[compression_pipeline] gzip compression failed, returning uncompressed");
				std::vector<uint8_t> result(input.begin(), input.end());
				return ok<std::vector<uint8_t>>(std::move(result));
			}

			compressed.resize(stream.total_out);

			// If compressed size is not smaller, return uncompressed
			if (compressed.size() >= input.size())
			{
				NETWORK_LOG_TRACE("[compression_pipeline] Compressed size not smaller, returning uncompressed");
				std::vector<uint8_t> result(input.begin(), input.end());
				return ok<std::vector<uint8_t>>(std::move(result));
			}

			NETWORK_LOG_TRACE("[compression_pipeline] gzip compressed " +
							std::to_string(input.size()) + " -> " +
							std::to_string(compressed.size()) + " bytes (" +
							std::to_string(100 - (compressed.size() * 100 / input.size())) + "% reduction)");

			return ok<std::vector<uint8_t>>(std::move(compressed));
		}

		auto decompress_gzip(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>
		{
			z_stream stream{};
			stream.zalloc = Z_NULL;
			stream.zfree = Z_NULL;
			stream.opaque = Z_NULL;

			// gzip initialization (windowBits = 15 + 16 for gzip)
			if (inflateInit2(&stream, 15 + 16) != Z_OK)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::internal_error,
					"Failed to initialize gzip decompression");
			}

			stream.avail_in = static_cast<uInt>(input.size());
			stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));

			// Start with 2x input size, will grow if needed
			std::vector<uint8_t> decompressed;
			decompressed.resize(input.size() * 2);

			stream.avail_out = static_cast<uInt>(decompressed.size());
			stream.next_out = decompressed.data();

			int ret;
			while ((ret = inflate(&stream, Z_NO_FLUSH)) == Z_OK)
			{
				// Need more output space
				size_t current_size = decompressed.size();
				decompressed.resize(current_size * 2);
				stream.avail_out = static_cast<uInt>(decompressed.size() - current_size);
				stream.next_out = decompressed.data() + current_size;
			}

			inflateEnd(&stream);

			if (ret != Z_STREAM_END)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::internal_error,
					"gzip decompression failed");
			}

			decompressed.resize(stream.total_out);

			NETWORK_LOG_TRACE("[compression_pipeline] gzip decompressed " +
							std::to_string(input.size()) + " -> " +
							std::to_string(decompressed.size()) + " bytes");

			return ok<std::vector<uint8_t>>(std::move(decompressed));
		}

		auto compress_deflate(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>
		{
			z_stream stream{};
			stream.zalloc = Z_NULL;
			stream.zfree = Z_NULL;
			stream.opaque = Z_NULL;

			// deflate initialization (windowBits = 15 for deflate)
			if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
							15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::internal_error,
					"Failed to initialize deflate compression");
			}

			stream.avail_in = static_cast<uInt>(input.size());
			stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));

			std::vector<uint8_t> compressed;
			compressed.resize(deflateBound(&stream, static_cast<uLong>(input.size())));

			stream.avail_out = static_cast<uInt>(compressed.size());
			stream.next_out = compressed.data();

			int ret = deflate(&stream, Z_FINISH);
			deflateEnd(&stream);

			if (ret != Z_STREAM_END)
			{
				NETWORK_LOG_WARN("[compression_pipeline] deflate compression failed, returning uncompressed");
				std::vector<uint8_t> result(input.begin(), input.end());
				return ok<std::vector<uint8_t>>(std::move(result));
			}

			compressed.resize(stream.total_out);

			// If compressed size is not smaller, return uncompressed
			if (compressed.size() >= input.size())
			{
				NETWORK_LOG_TRACE("[compression_pipeline] Compressed size not smaller, returning uncompressed");
				std::vector<uint8_t> result(input.begin(), input.end());
				return ok<std::vector<uint8_t>>(std::move(result));
			}

			NETWORK_LOG_TRACE("[compression_pipeline] deflate compressed " +
							std::to_string(input.size()) + " -> " +
							std::to_string(compressed.size()) + " bytes (" +
							std::to_string(100 - (compressed.size() * 100 / input.size())) + "% reduction)");

			return ok<std::vector<uint8_t>>(std::move(compressed));
		}

		auto decompress_deflate(std::span<const uint8_t> input) -> Result<std::vector<uint8_t>>
		{
			z_stream stream{};
			stream.zalloc = Z_NULL;
			stream.zfree = Z_NULL;
			stream.opaque = Z_NULL;

			// deflate initialization (windowBits = 15 for deflate)
			if (inflateInit2(&stream, 15) != Z_OK)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::internal_error,
					"Failed to initialize deflate decompression");
			}

			stream.avail_in = static_cast<uInt>(input.size());
			stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input.data()));

			// Start with 2x input size, will grow if needed
			std::vector<uint8_t> decompressed;
			decompressed.resize(input.size() * 2);

			stream.avail_out = static_cast<uInt>(decompressed.size());
			stream.next_out = decompressed.data();

			int ret;
			while ((ret = inflate(&stream, Z_NO_FLUSH)) == Z_OK)
			{
				// Need more output space
				size_t current_size = decompressed.size();
				decompressed.resize(current_size * 2);
				stream.avail_out = static_cast<uInt>(decompressed.size() - current_size);
				stream.next_out = decompressed.data() + current_size;
			}

			inflateEnd(&stream);

			if (ret != Z_STREAM_END)
			{
				return error<std::vector<uint8_t>>(
					error_codes::common_errors::internal_error,
					"deflate decompression failed");
			}

			decompressed.resize(stream.total_out);

			NETWORK_LOG_TRACE("[compression_pipeline] deflate decompressed " +
							std::to_string(input.size()) + " -> " +
							std::to_string(decompressed.size()) + " bytes");

			return ok<std::vector<uint8_t>>(std::move(decompressed));
		}
#endif

	private:
		compression_algorithm algorithm_;
		size_t compression_threshold_;
	};

	compression_pipeline::compression_pipeline(compression_algorithm algo,
											   size_t compression_threshold)
		: pimpl_(std::make_unique<impl>(algo, compression_threshold))
	{
	}

	compression_pipeline::~compression_pipeline() noexcept = default;

	auto compression_pipeline::compress(std::span<const uint8_t> input)
		-> Result<std::vector<uint8_t>>
	{
		return pimpl_->compress(input);
	}

	auto compression_pipeline::compress(const std::vector<uint8_t>& input)
		-> Result<std::vector<uint8_t>>
	{
		return pimpl_->compress(std::span<const uint8_t>(input));
	}

	auto compression_pipeline::decompress(std::span<const uint8_t> input)
		-> Result<std::vector<uint8_t>>
	{
		return pimpl_->decompress(input);
	}

	auto compression_pipeline::decompress(const std::vector<uint8_t>& input)
		-> Result<std::vector<uint8_t>>
	{
		return pimpl_->decompress(std::span<const uint8_t>(input));
	}

	auto compression_pipeline::set_compression_threshold(size_t bytes) -> void
	{
		pimpl_->set_compression_threshold(bytes);
	}

	auto compression_pipeline::get_compression_threshold() const -> size_t
	{
		return pimpl_->get_compression_threshold();
	}

	auto compression_pipeline::get_algorithm() const -> compression_algorithm
	{
		return pimpl_->get_algorithm();
	}

	auto make_compress_function(std::shared_ptr<compression_pipeline> pipeline)
		-> std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>
	{
		return [pipeline](const std::vector<uint8_t>& input) -> std::vector<uint8_t> {
			auto result = pipeline->compress(input);
			if (result.is_err())
			{
				NETWORK_LOG_ERROR("[compression] Compression failed: " + result.error().message);
				return input; // Return uncompressed on error
			}
			return result.value();
		};
	}

	auto make_decompress_function(std::shared_ptr<compression_pipeline> pipeline)
		-> std::function<std::vector<uint8_t>(const std::vector<uint8_t>&)>
	{
		return [pipeline](const std::vector<uint8_t>& input) -> std::vector<uint8_t> {
			auto result = pipeline->decompress(input);
			if (result.is_err())
			{
				NETWORK_LOG_ERROR("[compression] Decompression failed: " + result.error().message);
				return input; // Return as-is on error
			}
			return result.value();
		};
	}

} // namespace network_system::utils
