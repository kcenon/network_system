// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace kcenon::network::internal
{
	/*!
	 * \struct ws_handshake_result
	 * \brief Result of a WebSocket handshake operation.
	 *
	 * Contains the success status, error message (if any), and parsed
	 * HTTP headers from the handshake request or response.
	 */
	struct ws_handshake_result
	{
		bool success;                                  ///< Whether the handshake was successful
		std::string error_message;                     ///< Error message if success is false
		std::map<std::string, std::string> headers;    ///< Parsed HTTP headers
	};

	/*!
	 * \class websocket_handshake
	 * \brief Implements WebSocket HTTP/1.1 upgrade handshake (RFC 6455).
	 *
	 * This class provides static methods for creating and validating WebSocket
	 * handshake requests and responses according to RFC 6455 Section 4.
	 */
	class websocket_handshake
	{
	public:
		/*!
		 * \brief Creates a WebSocket handshake request (client-side).
		 *
		 * Generates an HTTP/1.1 upgrade request with all required WebSocket
		 * headers including a randomly generated Sec-WebSocket-Key.
		 *
		 * \param host The target host (for Host header)
		 * \param path The request path (default: "/")
		 * \param port The target port (for Host header if non-standard)
		 * \param extra_headers Additional headers to include in the request
		 * \return Complete HTTP request as a string
		 */
		static auto create_client_handshake(
			std::string_view host, std::string_view path, uint16_t port,
			const std::map<std::string, std::string>& extra_headers = {})
			-> std::string;

		/*!
		 * \brief Validates a WebSocket handshake response (client-side).
		 *
		 * Verifies that the server's response is a valid WebSocket upgrade
		 * response with the correct Sec-WebSocket-Accept value.
		 *
		 * \param response The complete HTTP response from the server
		 * \param expected_key The Sec-WebSocket-Key that was sent in the request
		 * \return Handshake result with success status and parsed headers
		 */
		static auto validate_server_response(const std::string& response,
											  const std::string& expected_key)
			-> ws_handshake_result;

		/*!
		 * \brief Parses a WebSocket handshake request (server-side).
		 *
		 * Validates that the client's request is a valid WebSocket upgrade
		 * request with all required headers.
		 *
		 * \param request The complete HTTP request from the client
		 * \return Handshake result with success status and parsed headers
		 */
		static auto parse_client_request(const std::string& request)
			-> ws_handshake_result;

		/*!
		 * \brief Creates a WebSocket handshake response (server-side).
		 *
		 * Generates an HTTP/1.1 101 Switching Protocols response with the
		 * computed Sec-WebSocket-Accept header.
		 *
		 * \param client_key The Sec-WebSocket-Key from the client's request
		 * \return Complete HTTP response as a string
		 */
		static auto create_server_response(const std::string& client_key)
			-> std::string;

		/*!
		 * \brief Generates a random Sec-WebSocket-Key (client-side).
		 *
		 * Creates a Base64-encoded random 16-byte value for use in the
		 * client handshake request.
		 *
		 * \return Base64-encoded random key (24 characters)
		 */
		static auto generate_websocket_key() -> std::string;

		/*!
		 * \brief Calculates Sec-WebSocket-Accept from client key.
		 *
		 * Computes the accept value by concatenating the client key with the
		 * WebSocket GUID, hashing with SHA-1, and encoding as Base64.
		 *
		 * \param client_key The Sec-WebSocket-Key from the client
		 * \return Base64-encoded SHA-1 hash
		 */
		static auto calculate_accept_key(const std::string& client_key)
			-> std::string;

	private:
		/*!
		 * \brief Parses HTTP headers from a request or response.
		 *
		 * Extracts headers from the HTTP header section. Header names are
		 * converted to lowercase for case-insensitive lookup.
		 *
		 * \param http_message The complete HTTP message
		 * \return Map of header names (lowercase) to values
		 */
		static auto parse_headers(const std::string& http_message)
			-> std::map<std::string, std::string>;

		/*!
		 * \brief Extracts the status code from an HTTP response.
		 *
		 * Parses the HTTP status line to extract the numeric status code.
		 *
		 * \param response The complete HTTP response
		 * \return Status code (e.g., 101, 200, 400), or 0 if parsing fails
		 */
		static auto extract_status_code(const std::string& response) -> int;

		/*!
		 * \brief Encodes binary data to Base64.
		 *
		 * Standard Base64 encoding without padding removal.
		 *
		 * \param data The binary data to encode
		 * \return Base64-encoded string
		 */
		static auto base64_encode(const std::vector<uint8_t>& data)
			-> std::string;

		/*!
		 * \brief Computes SHA-1 hash of input data.
		 *
		 * Uses OpenSSL to compute the SHA-1 hash.
		 *
		 * \param data The input data to hash
		 * \return 20-byte SHA-1 hash
		 */
		static auto sha1_hash(const std::string& data) -> std::vector<uint8_t>;
	};

} // namespace kcenon::network::internal
