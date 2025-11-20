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

#ifdef BUILD_WITH_COMMON_SYSTEM
	#include <kcenon/common/patterns/result.h>
	#include <kcenon/common/error/error_codes.h>
#endif

#include <variant>
#include <string>

namespace network_system {

#ifdef BUILD_WITH_COMMON_SYSTEM
	// Use common_system Result<T> when available
	template<typename T>
	using Result = ::kcenon::common::Result<T>;

	using VoidResult = ::kcenon::common::VoidResult;

	using error_info = ::kcenon::common::error_info;

	// Error code namespace (includes common_errors and network_system)
	namespace error_codes = ::kcenon::common::error::codes;

	// Helper functions for creating Results
	template<typename T>
	inline Result<T> ok(T&& value) {
		return Result<T>(std::forward<T>(value));
	}

	inline VoidResult ok() {
		return VoidResult(std::monostate{});
	}

	template<typename T>
	inline Result<T> error(int code, const std::string& message,
	                      const std::string& source = "network_system",
	                      const std::string& details = "") {
		return Result<T>(error_info(code, message, source, details));
	}

	inline VoidResult error_void(int code, const std::string& message,
	                            const std::string& source = "network_system",
	                            const std::string& details = "") {
		return VoidResult(error_info(code, message, source, details));
	}

#else
	// Fallback: Simple Result<T> implementation when common_system is not available
	// This maintains API compatibility but with minimal error information

	struct simple_error {
		int code;
		std::string message;
		std::string source;
		std::string details;

		simple_error(int c, std::string msg, std::string src = "", std::string det = "")
			: code(c), message(std::move(msg)), source(std::move(src)), details(std::move(det)) {}
	};

	template<typename T>
	class Result {
	public:
		Result(T&& val) : data_(std::forward<T>(val)) {}
		Result(const simple_error& err) : data_(err) {}

		bool is_ok() const { return std::holds_alternative<T>(data_); }
		bool is_err() const { return !is_ok(); }

		const T& value() const { return std::get<T>(data_); }
		T& value() { return std::get<T>(data_); }

		const simple_error& error() const { return std::get<simple_error>(data_); }

		operator bool() const { return is_ok(); }

	private:
		std::variant<T, simple_error> data_;
	};

	using VoidResult = Result<std::monostate>;
	using error_info = simple_error;

	// Minimal error codes (fallback)
	namespace error_codes {
		namespace network_system {
			constexpr int connection_failed = -600;
			constexpr int connection_refused = -601;
			constexpr int connection_timeout = -602;
			constexpr int connection_closed = -603;
			constexpr int send_failed = -640;
			constexpr int receive_failed = -641;
			constexpr int server_not_started = -660;
			constexpr int server_already_running = -661;
			constexpr int bind_failed = -662;
		}
		namespace common_errors {
			constexpr int success = 0;
			constexpr int invalid_argument = -1;
			constexpr int not_found = -2;
			constexpr int permission_denied = -3;
			constexpr int timeout = -4;
			constexpr int cancelled = -5;
			constexpr int not_initialized = -6;
			constexpr int already_exists = -7;
			constexpr int out_of_memory = -8;
			constexpr int io_error = -9;
			constexpr int network_error = -10;
			constexpr int internal_error = -99;
		}
		// Legacy compatibility
		namespace common {
			constexpr int invalid_argument = common_errors::invalid_argument;
			constexpr int not_initialized = common_errors::not_initialized;
			constexpr int already_exists = common_errors::already_exists;
			constexpr int internal_error = -99;
		}
	}

	// Helper functions
	template<typename T>
	inline Result<T> ok(T&& value) {
		return Result<T>(std::forward<T>(value));
	}

	inline VoidResult ok() {
		return VoidResult(std::monostate{});
	}

	template<typename T>
	inline Result<T> error(int code, const std::string& message,
	                      const std::string& source = "network_system",
	                      const std::string& details = "") {
		return Result<T>(simple_error(code, message, source, details));
	}

	inline VoidResult error_void(int code, const std::string& message,
	                            const std::string& source = "network_system",
	                            const std::string& details = "") {
		return VoidResult(simple_error(code, message, source, details));
	}

#endif

} // namespace network_system
