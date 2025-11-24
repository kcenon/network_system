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

#include "kcenon/network/internal/http_error.h"
#include <sstream>
#include <iomanip>
#include <ctime>

namespace network_system::internal
{

auto get_error_status_text(http_error_code code) -> std::string_view
{
	switch (code)
	{
		// Client Errors (4xx)
	case http_error_code::bad_request:
		return "Bad Request";
	case http_error_code::unauthorized:
		return "Unauthorized";
	case http_error_code::payment_required:
		return "Payment Required";
	case http_error_code::forbidden:
		return "Forbidden";
	case http_error_code::not_found:
		return "Not Found";
	case http_error_code::method_not_allowed:
		return "Method Not Allowed";
	case http_error_code::not_acceptable:
		return "Not Acceptable";
	case http_error_code::proxy_authentication_required:
		return "Proxy Authentication Required";
	case http_error_code::request_timeout:
		return "Request Timeout";
	case http_error_code::conflict:
		return "Conflict";
	case http_error_code::gone:
		return "Gone";
	case http_error_code::length_required:
		return "Length Required";
	case http_error_code::precondition_failed:
		return "Precondition Failed";
	case http_error_code::payload_too_large:
		return "Payload Too Large";
	case http_error_code::uri_too_long:
		return "URI Too Long";
	case http_error_code::unsupported_media_type:
		return "Unsupported Media Type";
	case http_error_code::range_not_satisfiable:
		return "Range Not Satisfiable";
	case http_error_code::expectation_failed:
		return "Expectation Failed";
	case http_error_code::im_a_teapot:
		return "I'm a teapot";
	case http_error_code::misdirected_request:
		return "Misdirected Request";
	case http_error_code::unprocessable_entity:
		return "Unprocessable Entity";
	case http_error_code::locked:
		return "Locked";
	case http_error_code::failed_dependency:
		return "Failed Dependency";
	case http_error_code::too_early:
		return "Too Early";
	case http_error_code::upgrade_required:
		return "Upgrade Required";
	case http_error_code::precondition_required:
		return "Precondition Required";
	case http_error_code::too_many_requests:
		return "Too Many Requests";
	case http_error_code::request_header_fields_too_large:
		return "Request Header Fields Too Large";
	case http_error_code::unavailable_for_legal_reasons:
		return "Unavailable For Legal Reasons";

		// Server Errors (5xx)
	case http_error_code::internal_server_error:
		return "Internal Server Error";
	case http_error_code::not_implemented:
		return "Not Implemented";
	case http_error_code::bad_gateway:
		return "Bad Gateway";
	case http_error_code::service_unavailable:
		return "Service Unavailable";
	case http_error_code::gateway_timeout:
		return "Gateway Timeout";
	case http_error_code::http_version_not_supported:
		return "HTTP Version Not Supported";
	case http_error_code::variant_also_negotiates:
		return "Variant Also Negotiates";
	case http_error_code::insufficient_storage:
		return "Insufficient Storage";
	case http_error_code::loop_detected:
		return "Loop Detected";
	case http_error_code::not_extended:
		return "Not Extended";
	case http_error_code::network_authentication_required:
		return "Network Authentication Required";
	default:
		return "Unknown Error";
	}
}

// Helper to escape JSON strings
static auto escape_json_string(const std::string& input) -> std::string
{
	std::ostringstream oss;
	for (char c : input)
	{
		switch (c)
		{
		case '"':
			oss << "\\\"";
			break;
		case '\\':
			oss << "\\\\";
			break;
		case '\b':
			oss << "\\b";
			break;
		case '\f':
			oss << "\\f";
			break;
		case '\n':
			oss << "\\n";
			break;
		case '\r':
			oss << "\\r";
			break;
		case '\t':
			oss << "\\t";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20)
			{
				oss << "\\u" << std::hex << std::setw(4) << std::setfill('0')
					<< static_cast<int>(c);
			}
			else
			{
				oss << c;
			}
			break;
		}
	}
	return oss.str();
}

// Helper to escape HTML strings
static auto escape_html_string(const std::string& input) -> std::string
{
	std::ostringstream oss;
	for (char c : input)
	{
		switch (c)
		{
		case '&':
			oss << "&amp;";
			break;
		case '<':
			oss << "&lt;";
			break;
		case '>':
			oss << "&gt;";
			break;
		case '"':
			oss << "&quot;";
			break;
		case '\'':
			oss << "&#39;";
			break;
		default:
			oss << c;
			break;
		}
	}
	return oss.str();
}

auto http_error_response::build_json_error(const http_error& error) -> http_response
{
	http_response response;
	response.status_code = error.status_code();
	response.status_message = std::string(get_error_status_text(error.code));

	// Build RFC 7807 compliant JSON response
	std::ostringstream json;
	json << "{\n";
	json << "  \"type\": \"about:blank\",\n";
	json << "  \"title\": \"" << escape_json_string(response.status_message) << "\",\n";
	json << "  \"status\": " << response.status_code << ",\n";
	json << "  \"detail\": \"" << escape_json_string(error.detail.empty() ? error.message : error.detail)
		 << "\"";

	if (!error.request_id.empty())
	{
		json << ",\n  \"instance\": \"" << escape_json_string(error.request_id) << "\"";
	}

	// Add timestamp
	auto time_t_val = std::chrono::system_clock::to_time_t(error.timestamp);
	std::tm tm_val{};
#ifdef _WIN32
	gmtime_s(&tm_val, &time_t_val);
#else
	gmtime_r(&time_t_val, &tm_val);
#endif
	char time_buf[32];
	std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%SZ", &tm_val);
	json << ",\n  \"timestamp\": \"" << time_buf << "\"";

	json << "\n}\n";

	response.set_body_string(json.str());
	response.set_header("Content-Type", "application/problem+json; charset=utf-8");

	return response;
}

auto http_error_response::build_html_error(const http_error& error) -> http_response
{
	http_response response;
	response.status_code = error.status_code();
	response.status_message = std::string(get_error_status_text(error.code));

	std::ostringstream html;
	html << "<!DOCTYPE html>\n";
	html << "<html lang=\"en\">\n";
	html << "<head>\n";
	html << "  <meta charset=\"utf-8\">\n";
	html << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
	html << "  <title>" << response.status_code << " " << escape_html_string(response.status_message)
		 << "</title>\n";
	html << "  <style>\n";
	html << "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; "
		 << "margin: 40px auto; max-width: 650px; line-height: 1.6; color: #333; padding: 0 10px; }\n";
	html << "    h1 { color: #c00; border-bottom: 2px solid #eee; padding-bottom: 10px; }\n";
	html << "    .detail { background: #f9f9f9; padding: 15px; border-radius: 5px; }\n";
	html << "    .meta { color: #666; font-size: 0.9em; margin-top: 20px; }\n";
	html << "  </style>\n";
	html << "</head>\n";
	html << "<body>\n";
	html << "  <h1>" << response.status_code << " " << escape_html_string(response.status_message)
		 << "</h1>\n";

	if (!error.message.empty())
	{
		html << "  <p>" << escape_html_string(error.message) << "</p>\n";
	}

	if (!error.detail.empty())
	{
		html << "  <div class=\"detail\">\n";
		html << "    <strong>Details:</strong> " << escape_html_string(error.detail) << "\n";
		html << "  </div>\n";
	}

	html << "  <div class=\"meta\">\n";
	if (!error.request_id.empty())
	{
		html << "    <p>Request ID: " << escape_html_string(error.request_id) << "</p>\n";
	}
	html << "    <p><em>NetworkSystem HTTP Server</em></p>\n";
	html << "  </div>\n";
	html << "</body>\n";
	html << "</html>\n";

	response.set_body_string(html.str());
	response.set_header("Content-Type", "text/html; charset=utf-8");

	return response;
}

auto http_error_response::make_error(http_error_code code, const std::string& detail,
									 const std::string& request_id) -> http_error
{
	http_error error;
	error.code = code;
	error.message = std::string(get_error_status_text(code));
	error.detail = detail;
	error.request_id = request_id;
	error.timestamp = std::chrono::system_clock::now();
	return error;
}

} // namespace network_system::internal
