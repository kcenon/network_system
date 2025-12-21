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

#include "kcenon/network/internal/http_parser.h"
#include "kcenon/network/integration/logger_integration.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

namespace kcenon::network::internal
{
    namespace
    {
        constexpr char CRLF[] = "\r\n";
        constexpr char HEADER_SEPARATOR[] = ": ";
    }

    // Helper functions

    auto http_parser::trim(std::string_view str) -> std::string_view
    {
        const auto start = std::find_if(str.begin(), str.end(),
            [](unsigned char c) { return !std::isspace(c); });

        const auto end = std::find_if(str.rbegin(), str.rend(),
            [](unsigned char c) { return !std::isspace(c); }).base();

        return (start < end) ? std::string_view(&*start, std::distance(start, end)) : std::string_view{};
    }

    auto http_parser::split_line(std::string_view data) -> std::pair<std::string_view, std::string_view>
    {
        auto pos = data.find("\r\n");
        if (pos == std::string_view::npos)
        {
            return {data, std::string_view{}};
        }

        return {data.substr(0, pos), data.substr(pos + 2)};
    }

    auto http_parser::parse_request_line(std::string_view line) -> Result<http_request>
    {
        http_request request;

        // Parse: METHOD URI HTTP/VERSION
        auto first_space = line.find(' ');
        if (first_space == std::string_view::npos)
        {
            return error<http_request>(-1, "Invalid request line: no spaces found");
        }

        auto method_str = std::string(line.substr(0, first_space));
        auto method_result = string_to_http_method(method_str);
        if (method_result.is_err())
        {
            return error<http_request>(method_result.error().code, method_result.error().message);
        }
        request.method = method_result.value();

        line = line.substr(first_space + 1);
        auto second_space = line.find(' ');
        if (second_space == std::string_view::npos)
        {
            return error<http_request>(-1, "Invalid request line: missing HTTP version");
        }

        auto uri_with_query = std::string(line.substr(0, second_space));

        // Parse query parameters from URI
        auto query_pos = uri_with_query.find('?');
        if (query_pos != std::string::npos)
        {
            request.uri = uri_with_query.substr(0, query_pos);
            auto query_string = uri_with_query.substr(query_pos + 1);
            request.query_params = parse_query_string(query_string);
        }
        else
        {
            request.uri = uri_with_query;
        }

        auto version_str = std::string(line.substr(second_space + 1));
        auto version_result = string_to_http_version(version_str);
        if (version_result.is_err())
        {
            return error<http_request>(version_result.error().code, version_result.error().message);
        }
        request.version = version_result.value();

        return ok(std::move(request));
    }

    auto http_parser::parse_status_line(std::string_view line) -> Result<http_response>
    {
        http_response response;

        // Parse: HTTP/VERSION STATUS_CODE STATUS_MESSAGE
        auto first_space = line.find(' ');
        if (first_space == std::string_view::npos)
        {
            return error<http_response>(-1, "Invalid status line: no spaces found");
        }

        auto version_str = std::string(line.substr(0, first_space));
        auto version_result = string_to_http_version(version_str);
        if (version_result.is_err())
        {
            return error<http_response>(version_result.error().code, version_result.error().message);
        }
        response.version = version_result.value();

        line = line.substr(first_space + 1);
        auto second_space = line.find(' ');

        std::string status_code_str;
        if (second_space == std::string_view::npos)
        {
            // No status message, only status code
            status_code_str = std::string(line);
            response.status_message = get_status_message(response.status_code);
        }
        else
        {
            status_code_str = std::string(line.substr(0, second_space));
            response.status_message = std::string(line.substr(second_space + 1));
        }

        try
        {
            response.status_code = std::stoi(status_code_str);
        }
        catch (const std::exception&)
        {
            return error<http_response>(-1, "Invalid status code: " + status_code_str);
        }

        // If status message is empty, use default
        if (response.status_message.empty())
        {
            response.status_message = get_status_message(response.status_code);
        }

        return ok(std::move(response));
    }

    auto http_parser::parse_headers(std::string_view headers_section,
                                     std::map<std::string, std::string>& headers) -> bool
    {
        while (!headers_section.empty())
        {
            auto [line, rest] = split_line(headers_section);
            headers_section = rest;

            if (line.empty())
            {
                break;  // Empty line marks end of headers
            }

            auto colon_pos = line.find(':');
            if (colon_pos == std::string_view::npos)
            {
                return false;  // Invalid header
            }

            auto name = trim(line.substr(0, colon_pos));
            auto value = trim(line.substr(colon_pos + 1));

            headers[std::string(name)] = std::string(value);
        }

        return true;
    }

    // Parse request

    auto http_parser::parse_request(const std::vector<uint8_t>& data) -> Result<http_request>
    {
        std::string_view str_view(reinterpret_cast<const char*>(data.data()), data.size());
        return parse_request(str_view);
    }

    auto http_parser::parse_request(std::string_view data) -> Result<http_request>
    {
        // Split request line
        auto [request_line, rest] = split_line(data);
        if (request_line.empty())
        {
            return error<http_request>(-1, "Empty HTTP request");
        }

        // Parse request line
        auto request_result = parse_request_line(request_line);
        if (request_result.is_err())
        {
            return request_result;
        }

        auto request = std::move(request_result.value());

        // Find end of headers (empty line)
        auto headers_end = rest.find("\r\n\r\n");
        if (headers_end == std::string_view::npos)
        {
            // No body, all remaining data is headers
            if (!parse_headers(rest, request.headers))
            {
                return error<http_request>(-1, "Failed to parse headers");
            }
        }
        else
        {
            // Parse headers
            auto headers_section = rest.substr(0, headers_end);
            if (!parse_headers(headers_section, request.headers))
            {
                return error<http_request>(-1, "Failed to parse headers");
            }

            // Parse body
            auto body_start = rest.substr(headers_end + 4);
            if (!body_start.empty())
            {
                request.body.assign(body_start.begin(), body_start.end());
            }
        }

        return ok(std::move(request));
    }

    // Parse response

    auto http_parser::parse_response(const std::vector<uint8_t>& data) -> Result<http_response>
    {
        std::string_view str_view(reinterpret_cast<const char*>(data.data()), data.size());
        return parse_response(str_view);
    }

    auto http_parser::parse_response(std::string_view data) -> Result<http_response>
    {
        // Split status line
        auto [status_line, rest] = split_line(data);
        if (status_line.empty())
        {
            return error<http_response>(-1, "Empty HTTP response");
        }

        // Parse status line
        auto response_result = parse_status_line(status_line);
        if (response_result.is_err())
        {
            return response_result;
        }

        auto response = std::move(response_result.value());

        // Find end of headers (empty line)
        auto headers_end = rest.find("\r\n\r\n");
        if (headers_end == std::string_view::npos)
        {
            // No body, all remaining data is headers
            if (!parse_headers(rest, response.headers))
            {
                return error<http_response>(-1, "Failed to parse headers");
            }
        }
        else
        {
            // Parse headers
            auto headers_section = rest.substr(0, headers_end);
            if (!parse_headers(headers_section, response.headers))
            {
                return error<http_response>(-1, "Failed to parse headers");
            }

            // Parse body
            auto body_start = rest.substr(headers_end + 4);
            if (!body_start.empty())
            {
                response.body.assign(body_start.begin(), body_start.end());
            }
        }

        return ok(std::move(response));
    }

    // Serialize request

    auto http_parser::serialize_request(const http_request& request) -> std::vector<uint8_t>
    {
        std::ostringstream oss;

        // Request line: METHOD URI HTTP/VERSION
        oss << http_method_to_string(request.method) << " ";
        oss << request.uri;

        // Add query parameters if present
        if (!request.query_params.empty())
        {
            oss << "?" << build_query_string(request.query_params);
        }

        oss << " " << http_version_to_string(request.version) << CRLF;

        // Headers
        for (const auto& [name, value] : request.headers)
        {
            oss << name << HEADER_SEPARATOR << value << CRLF;
        }

        // Empty line to separate headers from body
        oss << CRLF;

        // Body
        auto str = oss.str();
        std::vector<uint8_t> result(str.begin(), str.end());
        result.insert(result.end(), request.body.begin(), request.body.end());

        return result;
    }

    // Serialize response

    auto http_parser::serialize_response(const http_response& response) -> std::vector<uint8_t>
    {
        // Use chunked encoding if requested
        if (response.use_chunked_encoding && response.version == http_version::HTTP_1_1)
        {
            return serialize_chunked_response(response);
        }

        std::ostringstream oss;

        // Status line: HTTP/VERSION STATUS_CODE STATUS_MESSAGE
        oss << http_version_to_string(response.version) << " ";
        oss << response.status_code << " ";
        oss << response.status_message << CRLF;

        // Headers
        for (const auto& [name, value] : response.headers)
        {
            oss << name << HEADER_SEPARATOR << value << CRLF;
        }

        // Set-Cookie headers
        for (const auto& cookie : response.set_cookies)
        {
            oss << "Set-Cookie" << HEADER_SEPARATOR << cookie.to_header_value() << CRLF;
        }

        // Empty line to separate headers from body
        oss << CRLF;

        // Body
        auto str = oss.str();
        std::vector<uint8_t> result(str.begin(), str.end());
        result.insert(result.end(), response.body.begin(), response.body.end());

        return result;
    }

    // Chunked encoding helper

    auto http_parser::serialize_chunked_response(const http_response& response) -> std::vector<uint8_t>
    {
        std::ostringstream oss;

        // Status line: HTTP/VERSION STATUS_CODE STATUS_MESSAGE
        oss << http_version_to_string(response.version) << " ";
        oss << response.status_code << " ";
        oss << response.status_message << CRLF;

        // Headers (excluding Content-Length, adding Transfer-Encoding)
        for (const auto& [name, value] : response.headers)
        {
            // Skip Content-Length header for chunked encoding
            if (name != "Content-Length" && name != "content-length")
            {
                oss << name << HEADER_SEPARATOR << value << CRLF;
            }
        }

        // Add Transfer-Encoding: chunked header
        oss << "Transfer-Encoding" << HEADER_SEPARATOR << "chunked" << CRLF;

        // Set-Cookie headers
        for (const auto& cookie : response.set_cookies)
        {
            oss << "Set-Cookie" << HEADER_SEPARATOR << cookie.to_header_value() << CRLF;
        }

        // Empty line to separate headers from body
        oss << CRLF;

        // Convert header to bytes
        auto str = oss.str();
        std::vector<uint8_t> result(str.begin(), str.end());

        // Encode body as chunks
        constexpr size_t CHUNK_SIZE = 8192;  // 8KB chunks
        size_t offset = 0;

        while (offset < response.body.size())
        {
            size_t chunk_size = std::min(CHUNK_SIZE, response.body.size() - offset);

            // Chunk size in hexadecimal + CRLF
            std::ostringstream chunk_header;
            chunk_header << std::hex << chunk_size << CRLF;
            std::string chunk_header_str = chunk_header.str();
            result.insert(result.end(), chunk_header_str.begin(), chunk_header_str.end());

            // Chunk data
            result.insert(result.end(),
                         response.body.begin() + offset,
                         response.body.begin() + offset + chunk_size);

            // CRLF after chunk data
            result.push_back('\r');
            result.push_back('\n');

            offset += chunk_size;
        }

        // Last chunk: "0\r\n\r\n"
        std::string last_chunk = "0\r\n\r\n";
        result.insert(result.end(), last_chunk.begin(), last_chunk.end());

        NETWORK_LOG_DEBUG("[http_parser] Serialized chunked response: " +
                         std::to_string(response.body.size()) + " bytes in " +
                         std::to_string((response.body.size() + CHUNK_SIZE - 1) / CHUNK_SIZE) + " chunks");

        return result;
    }

    // URL encoding/decoding

    auto http_parser::url_encode(const std::string& value) -> std::string
    {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex << std::uppercase;

        for (unsigned char c : value)
        {
            // Keep alphanumeric and other safe characters
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            {
                escaped << c;
            }
            else
            {
                // Encode as %XX
                escaped << '%' << std::setw(2) << int(c);
            }
        }

        return escaped.str();
    }

    auto http_parser::url_decode(const std::string& value) -> std::string
    {
        std::ostringstream decoded;

        for (std::size_t i = 0; i < value.length(); ++i)
        {
            if (value[i] == '%')
            {
                if (i + 2 < value.length())
                {
                    // Decode %XX
                    std::string hex = value.substr(i + 1, 2);
                    int decoded_char = std::stoi(hex, nullptr, 16);
                    decoded << static_cast<char>(decoded_char);
                    i += 2;
                }
            }
            else if (value[i] == '+')
            {
                // + can be used for spaces
                decoded << ' ';
            }
            else
            {
                decoded << value[i];
            }
        }

        return decoded.str();
    }

    // Query string parsing

    auto http_parser::parse_query_string(const std::string& query_string)
        -> std::map<std::string, std::string>
    {
        std::map<std::string, std::string> params;
        std::istringstream iss(query_string);
        std::string pair;

        while (std::getline(iss, pair, '&'))
        {
            auto eq_pos = pair.find('=');
            if (eq_pos != std::string::npos)
            {
                auto key = url_decode(pair.substr(0, eq_pos));
                auto value = url_decode(pair.substr(eq_pos + 1));
                params[key] = value;
            }
            else
            {
                // Parameter without value
                params[url_decode(pair)] = "";
            }
        }

        return params;
    }

    auto http_parser::build_query_string(const std::map<std::string, std::string>& params)
        -> std::string
    {
        std::ostringstream oss;
        bool first = true;

        for (const auto& [key, value] : params)
        {
            if (!first)
            {
                oss << '&';
            }
            first = false;

            oss << url_encode(key);
            if (!value.empty())
            {
                oss << '=' << url_encode(value);
            }
        }

        return oss.str();
    }

    auto http_parser::parse_cookies(http_request& request) -> void
    {
        auto cookie_header = request.get_header("Cookie");
        if (!cookie_header) {
            return;  // No Cookie header present
        }

        const std::string& cookie_str = *cookie_header;
        size_t pos = 0;

        while (pos < cookie_str.size()) {
            // Skip whitespace
            while (pos < cookie_str.size() && (cookie_str[pos] == ' ' || cookie_str[pos] == '\t')) {
                pos++;
            }

            // Find the next semicolon or end of string
            size_t semi_pos = cookie_str.find(';', pos);
            if (semi_pos == std::string::npos) {
                semi_pos = cookie_str.size();
            }

            // Extract and parse the name=value pair
            std::string pair = cookie_str.substr(pos, semi_pos - pos);

            // Find the '=' separator
            size_t eq_pos = pair.find('=');
            if (eq_pos != std::string::npos) {
                std::string name = pair.substr(0, eq_pos);
                std::string value = pair.substr(eq_pos + 1);

                // Trim whitespace from name and value
                auto trim = [](std::string& s) {
                    // Trim leading whitespace
                    size_t start = 0;
                    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
                        start++;
                    }
                    // Trim trailing whitespace
                    size_t end = s.size();
                    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
                        end--;
                    }
                    s = s.substr(start, end - start);
                };

                trim(name);
                trim(value);

                if (!name.empty()) {
                    request.cookies[name] = value;
                }
            }

            pos = semi_pos + 1;
        }
    }

    auto http_parser::parse_multipart_form_data(http_request& request) -> VoidResult
    {
        // Get Content-Type header
        auto content_type = request.get_header("Content-Type");
        if (!content_type) {
            return error<std::monostate>(-1, "Missing Content-Type header");
        }

        // Extract boundary from Content-Type header
        // Format: "multipart/form-data; boundary=----WebKitFormBoundary..."
        std::string boundary;
        size_t boundary_pos = content_type->find("boundary=");
        if (boundary_pos == std::string::npos) {
            return error<std::monostate>(-1, "Missing boundary in Content-Type header");
        }

        boundary = "--" + content_type->substr(boundary_pos + 9);

        // Remove quotes if present
        if (!boundary.empty() && boundary.front() == '"') {
            boundary = boundary.substr(1);
        }
        if (!boundary.empty() && boundary.back() == '"') {
            boundary.pop_back();
        }

        // Parse multipart body
        const std::vector<uint8_t>& body = request.body;
        std::string body_str(body.begin(), body.end());

        size_t pos = 0;
        const std::string boundary_delimiter = boundary;
        const std::string boundary_end = boundary + "--";

        while (pos < body_str.size()) {
            // Find the next boundary
            size_t boundary_start = body_str.find(boundary_delimiter, pos);
            if (boundary_start == std::string::npos) {
                break;
            }

            // Move past the boundary and CRLF
            pos = boundary_start + boundary_delimiter.size();
            if (pos + 2 <= body_str.size() && body_str.substr(pos, 2) == "\r\n") {
                pos += 2;
            }

            // Check if this is the final boundary
            if (pos >= 2 && body_str.substr(pos - 2, 2) == "--") {
                break;  // Final boundary found
            }

            // Parse part headers
            std::map<std::string, std::string> part_headers;
            size_t headers_end = body_str.find("\r\n\r\n", pos);
            if (headers_end == std::string::npos) {
                break;
            }

            std::string headers_section = body_str.substr(pos, headers_end - pos);
            pos = headers_end + 4;  // Skip "\r\n\r\n"

            // Parse Content-Disposition header
            size_t disp_start = headers_section.find("Content-Disposition:");
            if (disp_start == std::string::npos) {
                continue;
            }

            size_t disp_end = headers_section.find("\r\n", disp_start);
            std::string disposition = headers_section.substr(
                disp_start + 20,  // Length of "Content-Disposition:"
                disp_end == std::string::npos ? std::string::npos : disp_end - disp_start - 20
            );

            // Extract field name
            std::string field_name;
            size_t name_pos = disposition.find("name=\"");
            if (name_pos != std::string::npos) {
                size_t name_start = name_pos + 6;
                size_t name_end = disposition.find('"', name_start);
                if (name_end != std::string::npos) {
                    field_name = disposition.substr(name_start, name_end - name_start);
                }
            }

            // Check if this is a file upload
            size_t filename_pos = disposition.find("filename=\"");
            bool is_file = (filename_pos != std::string::npos);

            // Find the part content (until next boundary)
            size_t next_boundary = body_str.find(boundary_delimiter, pos);
            if (next_boundary == std::string::npos) {
                next_boundary = body_str.size();
            }

            // Extract content (remove trailing \r\n before boundary)
            size_t content_end = next_boundary;
            if (content_end >= 2 && body_str.substr(content_end - 2, 2) == "\r\n") {
                content_end -= 2;
            }

            if (is_file) {
                // Extract filename
                std::string filename;
                size_t filename_start = filename_pos + 10;
                size_t filename_end = disposition.find('"', filename_start);
                if (filename_end != std::string::npos) {
                    filename = disposition.substr(filename_start, filename_end - filename_start);
                }

                // Extract Content-Type if present
                std::string content_type_value = "application/octet-stream";
                size_t ct_pos = headers_section.find("Content-Type:");
                if (ct_pos != std::string::npos) {
                    size_t ct_start = ct_pos + 13;
                    size_t ct_end = headers_section.find("\r\n", ct_start);
                    content_type_value = headers_section.substr(
                        ct_start,
                        ct_end == std::string::npos ? std::string::npos : ct_end - ct_start
                    );
                    // Trim whitespace
                    size_t first = content_type_value.find_first_not_of(" \t");
                    size_t last = content_type_value.find_last_not_of(" \t\r\n");
                    if (first != std::string::npos) {
                        content_type_value = content_type_value.substr(first, last - first + 1);
                    }
                }

                // Create multipart_file
                multipart_file file;
                file.field_name = field_name;
                file.filename = filename;
                file.content_type = content_type_value;
                file.content.assign(body_str.begin() + pos, body_str.begin() + content_end);

                request.files[field_name] = std::move(file);
            } else {
                // Regular form field
                std::string value = body_str.substr(pos, content_end - pos);
                request.form_data[field_name] = value;
            }

            pos = next_boundary;
        }

        return ok(std::monostate{});
    }

} // namespace kcenon::network::internal
