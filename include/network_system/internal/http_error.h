/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, kcenon
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

#include "http_types.h"
#include <string>
#include <string_view>
#include <chrono>

namespace network_system::internal
{

	/*!
	 * \enum http_error_code
	 * \brief Standard HTTP error codes (RFC 7231)
	 */
	enum class http_error_code
	{
		// Client Errors (4xx)
		bad_request = 400,
		unauthorized = 401,
		payment_required = 402,
		forbidden = 403,
		not_found = 404,
		method_not_allowed = 405,
		not_acceptable = 406,
		proxy_authentication_required = 407,
		request_timeout = 408,
		conflict = 409,
		gone = 410,
		length_required = 411,
		precondition_failed = 412,
		payload_too_large = 413,
		uri_too_long = 414,
		unsupported_media_type = 415,
		range_not_satisfiable = 416,
		expectation_failed = 417,
		im_a_teapot = 418,
		misdirected_request = 421,
		unprocessable_entity = 422,
		locked = 423,
		failed_dependency = 424,
		too_early = 425,
		upgrade_required = 426,
		precondition_required = 428,
		too_many_requests = 429,
		request_header_fields_too_large = 431,
		unavailable_for_legal_reasons = 451,

		// Server Errors (5xx)
		internal_server_error = 500,
		not_implemented = 501,
		bad_gateway = 502,
		service_unavailable = 503,
		gateway_timeout = 504,
		http_version_not_supported = 505,
		variant_also_negotiates = 506,
		insufficient_storage = 507,
		loop_detected = 508,
		not_extended = 510,
		network_authentication_required = 511
	};

	/*!
	 * \struct http_error
	 * \brief Structured HTTP error information
	 *
	 * ### Structure
	 * - code: HTTP error code
	 * - message: Short error message
	 * - detail: Detailed description (safe for clients)
	 * - request_id: Request identifier for tracing
	 * - timestamp: When the error occurred
	 */
	struct http_error
	{
		http_error_code code = http_error_code::internal_server_error;
		std::string message;
		std::string detail;
		std::string request_id;
		std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now();

		/*!
		 * \brief Get status code as integer
		 * \return HTTP status code
		 */
		auto status_code() const -> int { return static_cast<int>(code); }

		/*!
		 * \brief Check if this is a client error (4xx)
		 * \return true if client error
		 */
		auto is_client_error() const -> bool
		{
			int status = status_code();
			return status >= 400 && status < 500;
		}

		/*!
		 * \brief Check if this is a server error (5xx)
		 * \return true if server error
		 */
		auto is_server_error() const -> bool
		{
			int status = status_code();
			return status >= 500 && status < 600;
		}
	};

	/*!
	 * \enum parse_error_type
	 * \brief Types of HTTP parsing errors
	 */
	enum class parse_error_type
	{
		invalid_method,
		invalid_uri,
		invalid_version,
		invalid_header,
		incomplete_headers,
		incomplete_body,
		body_too_large,
		header_too_large,
		malformed_request
	};

	/*!
	 * \struct parse_error
	 * \brief Detailed HTTP parsing error information
	 *
	 * ### Structure
	 * - error_type: Type of parsing error
	 * - line_number: Line where error occurred (1-based)
	 * - column_number: Column where error occurred (1-based)
	 * - context: Relevant portion of the malformed request
	 * - message: Human-readable error message
	 */
	struct parse_error
	{
		parse_error_type error_type = parse_error_type::malformed_request;
		size_t line_number = 0;
		size_t column_number = 0;
		std::string context;
		std::string message;

		/*!
		 * \brief Convert parse error to http_error
		 * \return Corresponding HTTP error
		 */
		auto to_http_error() const -> http_error
		{
			http_error err;
			err.code = http_error_code::bad_request;
			err.message = "Bad Request";
			err.detail = message;
			if (!context.empty())
			{
				err.detail += " near: " + context;
			}
			return err;
		}
	};

	/*!
	 * \brief Get status text for HTTP error code
	 * \param code HTTP error code
	 * \return Status text (e.g., "Not Found")
	 */
	auto get_error_status_text(http_error_code code) -> std::string_view;

	/*!
	 * \class http_error_response
	 * \brief Builder for HTTP error responses
	 *
	 * Supports building error responses in JSON and HTML formats.
	 * Follows RFC 7807 (Problem Details for HTTP APIs) for JSON format.
	 */
	class http_error_response
	{
	public:
		/*!
		 * \brief Build JSON format error response (RFC 7807)
		 * \param error Error details
		 * \return HTTP response with JSON body
		 */
		static auto build_json_error(const http_error& error) -> http_response;

		/*!
		 * \brief Build HTML format error response
		 * \param error Error details
		 * \return HTTP response with HTML body
		 */
		static auto build_html_error(const http_error& error) -> http_response;

		/*!
		 * \brief Create http_error from error code
		 * \param code HTTP error code
		 * \param detail Optional detailed message
		 * \param request_id Optional request ID
		 * \return Constructed http_error
		 */
		static auto make_error(http_error_code code, const std::string& detail = "",
							   const std::string& request_id = "") -> http_error;
	};

} // namespace network_system::internal
