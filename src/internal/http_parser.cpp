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

#include "network_system/internal/http_parser.h"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>

namespace network_system::internal
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
        auto method_opt = string_to_http_method(method_str);
        if (!method_opt)
        {
            return error<http_request>(-1, "Invalid HTTP method: " + method_str);
        }
        request.method = *method_opt;

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
        auto version_opt = string_to_http_version(version_str);
        if (!version_opt)
        {
            return error<http_request>(-1, "Invalid HTTP version: " + version_str);
        }
        request.version = *version_opt;

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
        auto version_opt = string_to_http_version(version_str);
        if (!version_opt)
        {
            return error<http_response>(-1, "Invalid HTTP version: " + version_str);
        }
        response.version = *version_opt;

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

        // Empty line to separate headers from body
        oss << CRLF;

        // Convert header section to bytes
        auto str = oss.str();
        std::vector<uint8_t> result(str.begin(), str.end());

        // Body - handle chunked encoding if enabled
        if (response.use_chunked_encoding && !response.body.empty())
        {
            // Chunked transfer encoding format:
            // [chunk-size in hex]\r\n
            // [chunk-data]\r\n
            // ...
            // 0\r\n
            // \r\n

            // For simplicity, send entire body as single chunk
            // In production, you might split into smaller chunks (e.g., 8KB)
            std::ostringstream chunk_oss;

            // Chunk size in hexadecimal
            chunk_oss << std::hex << response.body.size() << CRLF;

            auto chunk_header = chunk_oss.str();
            result.insert(result.end(), chunk_header.begin(), chunk_header.end());

            // Chunk data
            result.insert(result.end(), response.body.begin(), response.body.end());

            // CRLF after chunk data
            result.insert(result.end(), CRLF, CRLF + 2);

            // Final chunk (0-sized chunk)
            std::string final_chunk = "0\r\n\r\n";
            result.insert(result.end(), final_chunk.begin(), final_chunk.end());
        }
        else
        {
            // Normal body without chunking
            result.insert(result.end(), response.body.begin(), response.body.end());
        }

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

    auto http_parser::parse_cookies(const std::string& cookie_header)
        -> std::map<std::string, std::string>
    {
        std::map<std::string, std::string> cookies;

        // Cookie header format: "name1=value1; name2=value2; name3=value3"
        std::istringstream iss(cookie_header);
        std::string cookie_pair;

        while (std::getline(iss, cookie_pair, ';'))
        {
            // Trim leading/trailing whitespace
            auto start = cookie_pair.find_first_not_of(" \t");
            auto end = cookie_pair.find_last_not_of(" \t");

            if (start == std::string::npos)
            {
                continue;
            }

            cookie_pair = cookie_pair.substr(start, end - start + 1);

            // Split by '='
            auto eq_pos = cookie_pair.find('=');
            if (eq_pos != std::string::npos)
            {
                std::string name = cookie_pair.substr(0, eq_pos);
                std::string value = cookie_pair.substr(eq_pos + 1);

                // Trim name and value
                name.erase(0, name.find_first_not_of(" \t"));
                name.erase(name.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                cookies[name] = value;
            }
        }

        return cookies;
    }

    auto http_parser::parse_multipart_form_data(
        const std::vector<uint8_t>& body,
        const std::string& content_type,
        std::map<std::string, std::string>& form_data,
        std::map<std::string, multipart_file>& files) -> bool
    {
        // Extract boundary from Content-Type header
        // Format: "multipart/form-data; boundary=----WebKitFormBoundary..."
        auto boundary_pos = content_type.find("boundary=");
        if (boundary_pos == std::string::npos)
        {
            return false;
        }

        std::string boundary = content_type.substr(boundary_pos + 9);
        // Remove quotes if present
        if (!boundary.empty() && boundary.front() == '"')
        {
            boundary = boundary.substr(1, boundary.length() - 2);
        }

        // Multipart boundaries are prefixed with "--"
        std::string delimiter = "--" + boundary;
        std::string end_delimiter = delimiter + "--";

        // Convert body to string for easier processing
        std::string body_str(body.begin(), body.end());

        // Find first boundary
        size_t pos = body_str.find(delimiter);
        if (pos == std::string::npos)
        {
            return false;
        }

        pos += delimiter.length();

        // Process each part
        while (pos < body_str.length())
        {
            // Skip CRLF after boundary
            if (pos + 1 < body_str.length() && body_str[pos] == '\r' && body_str[pos + 1] == '\n')
            {
                pos += 2;
            }

            // Find next boundary
            size_t next_boundary = body_str.find(delimiter, pos);
            if (next_boundary == std::string::npos)
            {
                break;
            }

            // Extract part (headers + content)
            std::string part = body_str.substr(pos, next_boundary - pos);

            // Split headers and content
            size_t headers_end = part.find("\r\n\r\n");
            if (headers_end == std::string::npos)
            {
                pos = next_boundary + delimiter.length();
                continue;
            }

            std::string headers_section = part.substr(0, headers_end);
            std::string content = part.substr(headers_end + 4);

            // Remove trailing CRLF from content
            if (content.length() >= 2 && content[content.length() - 2] == '\r' &&
                content[content.length() - 1] == '\n')
            {
                content = content.substr(0, content.length() - 2);
            }

            // Parse Content-Disposition header
            std::string field_name;
            std::string filename;
            std::string content_type_part;

            std::istringstream headers_stream(headers_section);
            std::string header_line;

            while (std::getline(headers_stream, header_line))
            {
                // Remove \r if present
                if (!header_line.empty() && header_line.back() == '\r')
                {
                    header_line.pop_back();
                }

                if (header_line.empty())
                {
                    continue;
                }

                // Check for Content-Disposition header
                if (header_line.find("Content-Disposition:") == 0)
                {
                    // Extract field name
                    auto name_pos = header_line.find("name=\"");
                    if (name_pos != std::string::npos)
                    {
                        name_pos += 6;
                        auto name_end = header_line.find("\"", name_pos);
                        if (name_end != std::string::npos)
                        {
                            field_name = header_line.substr(name_pos, name_end - name_pos);
                        }
                    }

                    // Extract filename if present
                    auto filename_pos = header_line.find("filename=\"");
                    if (filename_pos != std::string::npos)
                    {
                        filename_pos += 10;
                        auto filename_end = header_line.find("\"", filename_pos);
                        if (filename_end != std::string::npos)
                        {
                            filename = header_line.substr(filename_pos, filename_end - filename_pos);
                        }
                    }
                }
                // Check for Content-Type header
                else if (header_line.find("Content-Type:") == 0)
                {
                    content_type_part = header_line.substr(13);
                    // Trim leading spaces
                    content_type_part.erase(0, content_type_part.find_first_not_of(" \t"));
                }
            }

            // Store the parsed data
            if (!field_name.empty())
            {
                if (!filename.empty())
                {
                    // This is a file upload
                    multipart_file file;
                    file.field_name = field_name;
                    file.filename = filename;
                    file.content_type = content_type_part.empty() ? "application/octet-stream" : content_type_part;
                    file.content.assign(content.begin(), content.end());

                    files[field_name] = std::move(file);
                }
                else
                {
                    // This is a regular form field
                    form_data[field_name] = content;
                }
            }

            // Move to next part
            pos = next_boundary + delimiter.length();

            // Check if this is the end delimiter
            if (body_str.substr(next_boundary, end_delimiter.length()) == end_delimiter)
            {
                break;
            }
        }

        return true;
    }

} // namespace network_system::internal
