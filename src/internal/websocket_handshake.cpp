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

#include "network_system/internal/websocket_handshake.h"

#include <algorithm>
#include <random>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/sha.h>

namespace network_system::internal
{
	namespace
	{
		// WebSocket GUID constant from RFC 6455
		constexpr std::string_view WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

		// Base64 encoding table
		constexpr std::string_view BASE64_CHARS =
			"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

		/*!
		 * \brief Converts a string to lowercase.
		 */
		auto to_lower(std::string str) -> std::string
		{
			std::transform(str.begin(), str.end(), str.begin(),
						   [](unsigned char c) { return std::tolower(c); });
			return str;
		}

		/*!
		 * \brief Trims whitespace from both ends of a string.
		 */
		auto trim(std::string_view str) -> std::string
		{
			const auto start = str.find_first_not_of(" \t\r\n");
			if (start == std::string_view::npos)
			{
				return {};
			}

			const auto end = str.find_last_not_of(" \t\r\n");
			return std::string(str.substr(start, end - start + 1));
		}

	} // anonymous namespace

	auto websocket_handshake::base64_encode(const std::vector<uint8_t>& data)
		-> std::string
	{
		std::string result;
		result.reserve(((data.size() + 2) / 3) * 4);

		int val = 0;
		int valb = -6;

		for (uint8_t c : data)
		{
			val = (val << 8) + c;
			valb += 8;
			while (valb >= 0)
			{
				result.push_back(BASE64_CHARS[(val >> valb) & 0x3F]);
				valb -= 6;
			}
		}

		if (valb > -6)
		{
			result.push_back(BASE64_CHARS[((val << 8) >> (valb + 8)) & 0x3F]);
		}

		while (result.size() % 4)
		{
			result.push_back('=');
		}

		return result;
	}

	auto websocket_handshake::sha1_hash(const std::string& data)
		-> std::vector<uint8_t>
	{
		std::vector<uint8_t> hash(SHA_DIGEST_LENGTH);
		SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(),
			 hash.data());
		return hash;
	}

	auto websocket_handshake::generate_websocket_key() -> std::string
	{
		// Generate 16 random bytes
		static thread_local std::random_device rd;
		static thread_local std::mt19937 gen(rd());
		static thread_local std::uniform_int_distribution<uint32_t> dis(0, 255);

		std::vector<uint8_t> random_bytes(16);
		for (auto& byte : random_bytes)
		{
			byte = static_cast<uint8_t>(dis(gen));
		}

		return base64_encode(random_bytes);
	}

	auto websocket_handshake::calculate_accept_key(const std::string& client_key)
		-> std::string
	{
		// Concatenate client key with GUID
		std::string combined = client_key;
		combined.append(WEBSOCKET_GUID);

		// Hash with SHA-1
		auto hash = sha1_hash(combined);

		// Encode as Base64
		return base64_encode(hash);
	}

	auto websocket_handshake::parse_headers(const std::string& http_message)
		-> std::map<std::string, std::string>
	{
		std::map<std::string, std::string> headers;

		// Find the end of headers (empty line)
		const auto header_end = http_message.find("\r\n\r\n");
		if (header_end == std::string::npos)
		{
			return headers;
		}

		std::istringstream stream(http_message.substr(0, header_end));
		std::string line;

		// Skip the first line (request/status line)
		std::getline(stream, line);

		// Parse headers
		while (std::getline(stream, line))
		{
			// Remove trailing \r if present
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}

			const auto colon_pos = line.find(':');
			if (colon_pos != std::string::npos)
			{
				auto name = to_lower(trim(line.substr(0, colon_pos)));
				auto value = trim(line.substr(colon_pos + 1));
				headers[name] = value;
			}
		}

		return headers;
	}

	auto websocket_handshake::extract_status_code(const std::string& response)
		-> int
	{
		// HTTP response format: "HTTP/1.1 101 Switching Protocols\r\n"
		const auto first_line_end = response.find("\r\n");
		if (first_line_end == std::string::npos)
		{
			return 0;
		}

		const auto status_line = response.substr(0, first_line_end);
		std::istringstream stream(status_line);

		std::string http_version;
		int status_code = 0;

		stream >> http_version >> status_code;
		return status_code;
	}

	auto websocket_handshake::create_client_handshake(
		std::string_view host, std::string_view path, uint16_t port,
		const std::map<std::string, std::string>& extra_headers) -> std::string
	{
		std::ostringstream request;

		// Request line
		request << "GET " << path << " HTTP/1.1\r\n";

		// Host header (include port if non-standard)
		request << "Host: " << host;
		if (port != 80 && port != 443)
		{
			request << ":" << port;
		}
		request << "\r\n";

		// Required WebSocket headers
		request << "Upgrade: websocket\r\n";
		request << "Connection: Upgrade\r\n";
		request << "Sec-WebSocket-Key: " << generate_websocket_key() << "\r\n";
		request << "Sec-WebSocket-Version: 13\r\n";

		// Extra headers
		for (const auto& [name, value] : extra_headers)
		{
			request << name << ": " << value << "\r\n";
		}

		// Empty line to end headers
		request << "\r\n";

		return request.str();
	}

	auto websocket_handshake::validate_server_response(
		const std::string& response, const std::string& expected_key)
		-> ws_handshake_result
	{
		ws_handshake_result result;
		result.success = false;

		// Check status code
		const int status_code = extract_status_code(response);
		if (status_code != 101)
		{
			result.error_message =
				"Invalid status code: " + std::to_string(status_code);
			return result;
		}

		// Parse headers
		result.headers = parse_headers(response);

		// Validate Upgrade header
		auto it = result.headers.find("upgrade");
		if (it == result.headers.end() || to_lower(it->second) != "websocket")
		{
			result.error_message = "Missing or invalid Upgrade header";
			return result;
		}

		// Validate Connection header
		it = result.headers.find("connection");
		if (it == result.headers.end() || to_lower(it->second) != "upgrade")
		{
			result.error_message = "Missing or invalid Connection header";
			return result;
		}

		// Validate Sec-WebSocket-Accept header
		it = result.headers.find("sec-websocket-accept");
		if (it == result.headers.end())
		{
			result.error_message = "Missing Sec-WebSocket-Accept header";
			return result;
		}

		const std::string expected_accept = calculate_accept_key(expected_key);
		if (it->second != expected_accept)
		{
			result.error_message = "Invalid Sec-WebSocket-Accept value";
			return result;
		}

		result.success = true;
		return result;
	}

	auto websocket_handshake::parse_client_request(const std::string& request)
		-> ws_handshake_result
	{
		ws_handshake_result result;
		result.success = false;

		// Parse headers
		result.headers = parse_headers(request);

		// Validate request line (should start with "GET")
		if (request.substr(0, 3) != "GET")
		{
			result.error_message = "Invalid HTTP method (expected GET)";
			return result;
		}

		// Validate Upgrade header
		auto it = result.headers.find("upgrade");
		if (it == result.headers.end() || to_lower(it->second) != "websocket")
		{
			result.error_message = "Missing or invalid Upgrade header";
			return result;
		}

		// Validate Connection header
		it = result.headers.find("connection");
		if (it == result.headers.end() || to_lower(it->second) != "upgrade")
		{
			result.error_message = "Missing or invalid Connection header";
			return result;
		}

		// Validate Sec-WebSocket-Key header
		it = result.headers.find("sec-websocket-key");
		if (it == result.headers.end() || it->second.empty())
		{
			result.error_message = "Missing or empty Sec-WebSocket-Key header";
			return result;
		}

		// Validate Sec-WebSocket-Version header
		it = result.headers.find("sec-websocket-version");
		if (it == result.headers.end() || it->second != "13")
		{
			result.error_message = "Missing or invalid Sec-WebSocket-Version header";
			return result;
		}

		result.success = true;
		return result;
	}

	auto websocket_handshake::create_server_response(const std::string& client_key)
		-> std::string
	{
		std::ostringstream response;

		// Status line
		response << "HTTP/1.1 101 Switching Protocols\r\n";

		// Required headers
		response << "Upgrade: websocket\r\n";
		response << "Connection: Upgrade\r\n";
		response << "Sec-WebSocket-Accept: " << calculate_accept_key(client_key)
				 << "\r\n";

		// Empty line to end headers
		response << "\r\n";

		return response.str();
	}

} // namespace network_system::internal
