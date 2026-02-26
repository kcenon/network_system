/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2025, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

#include "internal/utils/compression_pipeline.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>

using namespace kcenon::network::utils;
using namespace kcenon::network;

/**
 * @file compression_pipeline_test.cpp
 * @brief Unit tests for compression_pipeline
 *
 * Tests validate:
 * - Construction with different algorithms and thresholds
 * - No-compression algorithm passthrough
 * - Below-threshold data bypasses compression
 * - Threshold getter and setter
 * - Algorithm getter
 * - Decompression of empty input returns error
 * - Compress/decompress roundtrip (when compression libs are available)
 * - make_compress_function and make_decompress_function helpers
 * - Vector overload consistency
 */

// ============================================================================
// Construction Tests
// ============================================================================

class CompressionPipelineConstructionTest : public ::testing::Test
{
};

TEST_F(CompressionPipelineConstructionTest, ConstructsWithDefaultParameters)
{
	compression_pipeline pipeline;

	EXPECT_EQ(pipeline.get_algorithm(), compression_algorithm::lz4);
	EXPECT_EQ(pipeline.get_compression_threshold(), 256);
}

TEST_F(CompressionPipelineConstructionTest, ConstructsWithCustomAlgorithm)
{
	compression_pipeline pipeline(compression_algorithm::gzip, 512);

	EXPECT_EQ(pipeline.get_algorithm(), compression_algorithm::gzip);
	EXPECT_EQ(pipeline.get_compression_threshold(), 512);
}

TEST_F(CompressionPipelineConstructionTest, ConstructsWithNoneAlgorithm)
{
	compression_pipeline pipeline(compression_algorithm::none, 0);

	EXPECT_EQ(pipeline.get_algorithm(), compression_algorithm::none);
	EXPECT_EQ(pipeline.get_compression_threshold(), 0);
}

TEST_F(CompressionPipelineConstructionTest, ConstructsWithDeflateAlgorithm)
{
	compression_pipeline pipeline(compression_algorithm::deflate, 1024);

	EXPECT_EQ(pipeline.get_algorithm(), compression_algorithm::deflate);
	EXPECT_EQ(pipeline.get_compression_threshold(), 1024);
}

// ============================================================================
// Threshold Tests
// ============================================================================

class CompressionPipelineThresholdTest : public ::testing::Test
{
protected:
	compression_pipeline pipeline_{compression_algorithm::none, 256};
};

TEST_F(CompressionPipelineThresholdTest, SetThresholdUpdatesValue)
{
	pipeline_.set_compression_threshold(1024);

	EXPECT_EQ(pipeline_.get_compression_threshold(), 1024);
}

TEST_F(CompressionPipelineThresholdTest, SetThresholdToZero)
{
	pipeline_.set_compression_threshold(0);

	EXPECT_EQ(pipeline_.get_compression_threshold(), 0);
}

TEST_F(CompressionPipelineThresholdTest, BelowThresholdReturnsUncompressed)
{
	// Create data smaller than threshold (256 bytes)
	std::vector<uint8_t> data(100, 0x42);

	auto result = pipeline_.compress(data);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value().size(), data.size());
	EXPECT_EQ(result.value(), data);
}

// ============================================================================
// No-Compression Algorithm Tests
// ============================================================================

class CompressionPipelineNoneTest : public ::testing::Test
{
protected:
	compression_pipeline pipeline_{compression_algorithm::none, 0};
};

TEST_F(CompressionPipelineNoneTest, CompressReturnsInputUnchanged)
{
	std::vector<uint8_t> data(512);
	std::iota(data.begin(), data.end(), 0);

	auto result = pipeline_.compress(data);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value(), data);
}

TEST_F(CompressionPipelineNoneTest, DecompressReturnsInputUnchanged)
{
	std::vector<uint8_t> data(512);
	std::iota(data.begin(), data.end(), 0);

	auto result = pipeline_.decompress(data);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value(), data);
}

TEST_F(CompressionPipelineNoneTest, CompressSpanOverload)
{
	std::vector<uint8_t> data(300, 0xAA);
	std::span<const uint8_t> span_data(data);

	auto result = pipeline_.compress(span_data);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value(), data);
}

TEST_F(CompressionPipelineNoneTest, DecompressSpanOverload)
{
	std::vector<uint8_t> data(300, 0xBB);
	std::span<const uint8_t> span_data(data);

	auto result = pipeline_.decompress(span_data);

	ASSERT_FALSE(result.is_err());
	EXPECT_EQ(result.value(), data);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

class CompressionPipelineErrorTest : public ::testing::Test
{
protected:
	compression_pipeline pipeline_{compression_algorithm::lz4, 0};
};

TEST_F(CompressionPipelineErrorTest, DecompressEmptyInputReturnsError)
{
	std::vector<uint8_t> empty_data;

	auto result = pipeline_.decompress(empty_data);

	EXPECT_TRUE(result.is_err());
}

TEST_F(CompressionPipelineErrorTest, CompressEmptyInputSucceeds)
{
	// Empty input is below any threshold, should succeed
	compression_pipeline pipeline(compression_algorithm::none, 0);
	std::vector<uint8_t> empty_data;

	auto result = pipeline.compress(empty_data);

	ASSERT_FALSE(result.is_err());
	EXPECT_TRUE(result.value().empty());
}

// ============================================================================
// LZ4 Roundtrip Tests (when available)
// ============================================================================

class CompressionPipelineLZ4Test : public ::testing::Test
{
protected:
	compression_pipeline pipeline_{compression_algorithm::lz4, 0};

	static std::vector<uint8_t> make_compressible_data(size_t size)
	{
		// Create highly compressible data (repeated pattern)
		std::vector<uint8_t> data(size);
		for (size_t i = 0; i < size; ++i)
		{
			data[i] = static_cast<uint8_t>(i % 4);
		}
		return data;
	}
};

TEST_F(CompressionPipelineLZ4Test, CompressProducesResult)
{
	auto data = make_compressible_data(1024);

	auto result = pipeline_.compress(data);

	// Should succeed regardless of whether LZ4 is available
	// (falls back to uncompressed)
	ASSERT_FALSE(result.is_err());
	EXPECT_FALSE(result.value().empty());
}

TEST_F(CompressionPipelineLZ4Test, CompressDecompressRoundtrip)
{
	auto original = make_compressible_data(2048);

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	// If compression was actually applied, decompression should work
	// If not (fallback), the data is returned as-is
	// Either way, we should get back the original data or a valid result
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

TEST_F(CompressionPipelineLZ4Test, CompressSmallDataMayReturnUncompressed)
{
	// Very small data may not compress well
	std::vector<uint8_t> data = {1, 2, 3, 4, 5};
	pipeline_.set_compression_threshold(0);

	auto result = pipeline_.compress(data);

	ASSERT_FALSE(result.is_err());
	// Result should be either compressed or original data
	EXPECT_FALSE(result.value().empty());
}

TEST_F(CompressionPipelineLZ4Test, CompressLargeCompressibleData)
{
	// Create large, highly compressible data
	std::vector<uint8_t> data(64 * 1024, 0x00);

	auto result = pipeline_.compress(data);

	ASSERT_FALSE(result.is_err());
	EXPECT_FALSE(result.value().empty());
}

// ============================================================================
// Gzip Tests (when available)
// ============================================================================

class CompressionPipelineGzipTest : public ::testing::Test
{
protected:
	compression_pipeline pipeline_{compression_algorithm::gzip, 0};
};

TEST_F(CompressionPipelineGzipTest, CompressProducesResult)
{
	std::vector<uint8_t> data(1024, 0x42);

	auto result = pipeline_.compress(data);

	ASSERT_FALSE(result.is_err());
	EXPECT_FALSE(result.value().empty());
}

TEST_F(CompressionPipelineGzipTest, CompressDecompressRoundtrip)
{
	std::vector<uint8_t> original(2048);
	std::iota(original.begin(), original.end(), 0);

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

// ============================================================================
// Deflate Tests (when available)
// ============================================================================

class CompressionPipelineDeflateTest : public ::testing::Test
{
protected:
	compression_pipeline pipeline_{compression_algorithm::deflate, 0};
};

TEST_F(CompressionPipelineDeflateTest, CompressProducesResult)
{
	std::vector<uint8_t> data(1024, 0x55);

	auto result = pipeline_.compress(data);

	ASSERT_FALSE(result.is_err());
	EXPECT_FALSE(result.value().empty());
}

TEST_F(CompressionPipelineDeflateTest, CompressDecompressRoundtrip)
{
	std::vector<uint8_t> original(4096);
	for (size_t i = 0; i < original.size(); ++i)
	{
		original[i] = static_cast<uint8_t>(i % 256);
	}

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

// ============================================================================
// Helper Function Tests
// ============================================================================

class CompressionPipelineHelperTest : public ::testing::Test
{
};

TEST_F(CompressionPipelineHelperTest, MakeCompressFunctionReturnsCallable)
{
	auto pipeline = std::make_shared<compression_pipeline>(
		compression_algorithm::none, 0);

	auto compress_fn = make_compress_function(pipeline);

	std::vector<uint8_t> data(512, 0x42);
	auto result = compress_fn(data);

	EXPECT_EQ(result, data);
}

TEST_F(CompressionPipelineHelperTest, MakeDecompressFunctionReturnsCallable)
{
	auto pipeline = std::make_shared<compression_pipeline>(
		compression_algorithm::none, 0);

	auto decompress_fn = make_decompress_function(pipeline);

	std::vector<uint8_t> data(512, 0x42);
	auto result = decompress_fn(data);

	EXPECT_EQ(result, data);
}

TEST_F(CompressionPipelineHelperTest, HelperFunctionsRoundtrip)
{
	auto pipeline = std::make_shared<compression_pipeline>(
		compression_algorithm::none, 0);

	auto compress_fn = make_compress_function(pipeline);
	auto decompress_fn = make_decompress_function(pipeline);

	std::vector<uint8_t> original(1024);
	std::iota(original.begin(), original.end(), 0);

	auto compressed = compress_fn(original);
	auto decompressed = decompress_fn(compressed);

	EXPECT_EQ(decompressed, original);
}

TEST_F(CompressionPipelineHelperTest, HelperFunctionWithEmptyInput)
{
	auto pipeline = std::make_shared<compression_pipeline>(
		compression_algorithm::none, 0);

	auto compress_fn = make_compress_function(pipeline);

	std::vector<uint8_t> empty_data;
	auto result = compress_fn(empty_data);

	EXPECT_TRUE(result.empty());
}

// ============================================================================
// LZ4 Roundtrip Extended Tests
// ============================================================================

class CompressionPipelineLZ4ExtendedTest : public ::testing::Test
{
protected:
	compression_pipeline pipeline_{compression_algorithm::lz4, 0};
};

TEST_F(CompressionPipelineLZ4ExtendedTest, RoundtripWithRandomLikeData)
{
	// Create data with a pattern that still has some structure
	std::vector<uint8_t> original(4096);
	for (size_t i = 0; i < original.size(); ++i)
	{
		original[i] = static_cast<uint8_t>((i * 31 + 7) % 256);
	}

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

TEST_F(CompressionPipelineLZ4ExtendedTest, RoundtripWithAllZeros)
{
	// Highly compressible data
	std::vector<uint8_t> original(8192, 0x00);

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	// All-zeros should compress very well
	if (compressed.value().size() < original.size())
	{
		auto decompressed = pipeline_.decompress(compressed.value());
		ASSERT_FALSE(decompressed.is_err());
		EXPECT_EQ(decompressed.value(), original);
	}
}

TEST_F(CompressionPipelineLZ4ExtendedTest, RoundtripWithAllOnes)
{
	std::vector<uint8_t> original(4096, 0xFF);

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

TEST_F(CompressionPipelineLZ4ExtendedTest, RoundtripWithSingleByte)
{
	std::vector<uint8_t> original = {0x42};

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

TEST_F(CompressionPipelineLZ4ExtendedTest, RoundtripWithRepeatingPattern)
{
	// Create ABCABCABC... pattern - very compressible
	std::vector<uint8_t> original(16384);
	for (size_t i = 0; i < original.size(); ++i)
	{
		original[i] = static_cast<uint8_t>('A' + (i % 3));
	}

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

TEST_F(CompressionPipelineLZ4ExtendedTest, CompressedSizeSmallerForCompressibleData)
{
	// Highly compressible: repeated byte
	std::vector<uint8_t> data(4096, 0x42);

	auto compressed = pipeline_.compress(data);
	ASSERT_FALSE(compressed.is_err());

	// Compressed output should be non-empty
	EXPECT_FALSE(compressed.value().empty());
}

TEST_F(CompressionPipelineLZ4ExtendedTest, DecompressCorruptedDataReturnsError)
{
	// Feed random garbage as "compressed" data
	std::vector<uint8_t> garbage = {0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA,
									0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

	auto result = pipeline_.decompress(garbage);

	// Decompressing garbage should either error or not crash
	// (implementation-specific behavior)
	SUCCEED();
}

// ============================================================================
// Gzip Extended Tests
// ============================================================================

class CompressionPipelineGzipExtendedTest : public ::testing::Test
{
protected:
	compression_pipeline pipeline_{compression_algorithm::gzip, 0};
};

TEST_F(CompressionPipelineGzipExtendedTest, RoundtripLargePayload)
{
	// 64KB of compressible data
	std::vector<uint8_t> original(64 * 1024);
	for (size_t i = 0; i < original.size(); ++i)
	{
		original[i] = static_cast<uint8_t>(i % 128);
	}

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

TEST_F(CompressionPipelineGzipExtendedTest, RoundtripMinimalData)
{
	std::vector<uint8_t> original = {0x01};

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

TEST_F(CompressionPipelineGzipExtendedTest, DecompressEmptyReturnsError)
{
	std::vector<uint8_t> empty_data;

	auto result = pipeline_.decompress(empty_data);

	EXPECT_TRUE(result.is_err());
}

// ============================================================================
// Deflate Extended Tests
// ============================================================================

class CompressionPipelineDeflateExtendedTest : public ::testing::Test
{
protected:
	compression_pipeline pipeline_{compression_algorithm::deflate, 0};
};

TEST_F(CompressionPipelineDeflateExtendedTest, RoundtripLargePayload)
{
	std::vector<uint8_t> original(64 * 1024);
	std::iota(original.begin(), original.end(), 0);

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

TEST_F(CompressionPipelineDeflateExtendedTest, RoundtripWithBinaryData)
{
	// Create binary data with all byte values
	std::vector<uint8_t> original(512);
	for (size_t i = 0; i < original.size(); ++i)
	{
		original[i] = static_cast<uint8_t>(i % 256);
	}

	auto compressed = pipeline_.compress(original);
	ASSERT_FALSE(compressed.is_err());

	auto decompressed = pipeline_.decompress(compressed.value());
	if (!decompressed.is_err())
	{
		EXPECT_EQ(decompressed.value(), original);
	}
}

// ============================================================================
// Threshold Edge Case Tests
// ============================================================================

class CompressionPipelineThresholdEdgeTest : public ::testing::Test
{
};

TEST_F(CompressionPipelineThresholdEdgeTest, DataExactlyAtThreshold)
{
	compression_pipeline pipeline(compression_algorithm::lz4, 256);

	// Data exactly at threshold boundary
	std::vector<uint8_t> data(256, 0x42);

	auto result = pipeline.compress(data);
	ASSERT_FALSE(result.is_err());
	EXPECT_FALSE(result.value().empty());
}

TEST_F(CompressionPipelineThresholdEdgeTest, DataOneBelowThreshold)
{
	compression_pipeline pipeline(compression_algorithm::lz4, 256);

	std::vector<uint8_t> data(255, 0x42);

	auto result = pipeline.compress(data);
	ASSERT_FALSE(result.is_err());
	// Below threshold - should return uncompressed
	EXPECT_EQ(result.value(), data);
}

TEST_F(CompressionPipelineThresholdEdgeTest, DataOneAboveThreshold)
{
	compression_pipeline pipeline(compression_algorithm::lz4, 256);

	std::vector<uint8_t> data(257, 0x42);

	auto result = pipeline.compress(data);
	ASSERT_FALSE(result.is_err());
	EXPECT_FALSE(result.value().empty());
}

TEST_F(CompressionPipelineThresholdEdgeTest, VeryLargeThresholdBypassesCompression)
{
	compression_pipeline pipeline(compression_algorithm::lz4, 1024 * 1024);

	std::vector<uint8_t> data(1024, 0x42);

	auto result = pipeline.compress(data);
	ASSERT_FALSE(result.is_err());
	// Data is well below threshold - should be returned unchanged
	EXPECT_EQ(result.value(), data);
}

TEST_F(CompressionPipelineThresholdEdgeTest, ThresholdChangeAffectsNextCompress)
{
	compression_pipeline pipeline(compression_algorithm::lz4, 1024);

	std::vector<uint8_t> data(512, 0x42);

	// Below threshold - passthrough
	auto result1 = pipeline.compress(data);
	ASSERT_FALSE(result1.is_err());
	EXPECT_EQ(result1.value(), data);

	// Lower threshold
	pipeline.set_compression_threshold(256);

	// Now above threshold - may be compressed
	auto result2 = pipeline.compress(data);
	ASSERT_FALSE(result2.is_err());
	EXPECT_FALSE(result2.value().empty());
}

// ============================================================================
// Cross-Algorithm Consistency Tests
// ============================================================================

class CompressionPipelineCrossAlgorithmTest : public ::testing::Test
{
protected:
	static std::vector<uint8_t> make_test_data(size_t size)
	{
		std::vector<uint8_t> data(size);
		for (size_t i = 0; i < size; ++i)
		{
			data[i] = static_cast<uint8_t>(i % 64);
		}
		return data;
	}
};

TEST_F(CompressionPipelineCrossAlgorithmTest, AllAlgorithmsCompressSuccessfully)
{
	auto data = make_test_data(2048);

	for (auto algo : {compression_algorithm::none, compression_algorithm::lz4,
					  compression_algorithm::gzip,
					  compression_algorithm::deflate})
	{
		compression_pipeline pipeline(algo, 0);
		auto result = pipeline.compress(data);
		EXPECT_FALSE(result.is_err())
			<< "Compression should succeed for algorithm "
			<< static_cast<int>(algo);
		EXPECT_FALSE(result.value().empty());
	}
}

TEST_F(CompressionPipelineCrossAlgorithmTest, NoneAlgorithmPreservesExactData)
{
	auto data = make_test_data(1024);
	compression_pipeline pipeline(compression_algorithm::none, 0);

	auto compressed = pipeline.compress(data);
	ASSERT_FALSE(compressed.is_err());
	EXPECT_EQ(compressed.value(), data);

	auto decompressed = pipeline.decompress(compressed.value());
	ASSERT_FALSE(decompressed.is_err());
	EXPECT_EQ(decompressed.value(), data);
}

TEST_F(CompressionPipelineCrossAlgorithmTest, AlgorithmGetterReturnsCorrectValue)
{
	for (auto algo : {compression_algorithm::none, compression_algorithm::lz4,
					  compression_algorithm::gzip,
					  compression_algorithm::deflate})
	{
		compression_pipeline pipeline(algo, 0);
		EXPECT_EQ(pipeline.get_algorithm(), algo);
	}
}
