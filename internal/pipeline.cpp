// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#include "kcenon/network/internal/pipeline.h"

#include <iostream>
#include <string_view>

// Using nested namespace definition for implementation details (C++17)
namespace kcenon::network::internal::detail
{
	// Using inline variables for debugging messages (C++17)
	inline constexpr std::string_view compress_debug_msg = "[debug] default_compress_stub";
	inline constexpr std::string_view decompress_debug_msg = "[debug] default_decompress_stub";
	inline constexpr std::string_view encrypt_debug_msg = "[debug] default_encrypt_stub";
	inline constexpr std::string_view decrypt_debug_msg = "[debug] default_decrypt_stub";
	
	// Using constexpr functions with if constexpr for compile-time optimization (C++17)
	auto default_compress_stub(const std::vector<uint8_t>& data)
		-> std::vector<uint8_t>
	{
		std::cout << compress_debug_msg << '\n';
		// No real compression, just return data
		return data;
	}

	auto default_decompress_stub(const std::vector<uint8_t>& data)
		-> std::vector<uint8_t>
	{
		std::cout << decompress_debug_msg << '\n';
		// No real decompression, just return data
		return data;
	}

	auto default_encrypt_stub(const std::vector<uint8_t>& data)
		-> std::vector<uint8_t>
	{
		std::cout << encrypt_debug_msg << '\n';
		// No real encryption, just return data
		return data;
	}

	auto default_decrypt_stub(const std::vector<uint8_t>& data)
		-> std::vector<uint8_t>
	{
		std::cout << decrypt_debug_msg << '\n';
		// No real decryption, just return data
		return data;
	}
}

namespace kcenon::network::internal
{
	// Using aggregate initialization with designated initializers (C++17)
	auto make_default_pipeline() -> pipeline
	{
		return pipeline{
			.compress = detail::default_compress_stub,
			.decompress = detail::default_decompress_stub,
			.encrypt = detail::default_encrypt_stub,
			.decrypt = detail::default_decrypt_stub
		};
	}

} // namespace kcenon::network::internal
