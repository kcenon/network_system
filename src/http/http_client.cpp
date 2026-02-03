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

#include "internal/http/http_client.h"
#include <condition_variable>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>

namespace kcenon::network::core {
namespace {
// Default timeout: 30 seconds
constexpr auto DEFAULT_TIMEOUT_MS = std::chrono::milliseconds(30000);
} // namespace

// URL parsing implementation

auto http_url::parse(const std::string &url) -> Result<http_url> {
  http_url result;

  // Regular expression for URL parsing
  // Format: scheme://[user:pass@]host[:port][/path][?query]
  // Static to avoid recompilation and ensure thread-safety (C++11 magic
  // statics)
  static const std::regex url_regex(
      R"(^(https?):\/\/([^:\/\s]+)(?::(\d+))?(\/[^\?]*)?(?:\?(.*))?$)",
      std::regex::icase);

  std::smatch matches;
  if (!std::regex_match(url, matches, url_regex)) {
    return error<http_url>(-1, "Invalid URL format: " + url);
  }

  // Extract components
  result.scheme = matches[1].str();
  result.host = matches[2].str();

  // Parse port
  if (matches[3].matched) {
    try {
      result.port = static_cast<unsigned short>(std::stoi(matches[3].str()));
    } catch (const std::exception &) {
      return error<http_url>(-1, "Invalid port number in URL");
    }
  } else {
    result.port = result.get_default_port();
  }

  // Parse path
  result.path = matches[4].matched ? matches[4].str() : "/";

  // Parse query string
  if (matches[5].matched) {
    result.query = internal::http_parser::parse_query_string(matches[5].str());
  }

  return ok(std::move(result));
}

auto http_url::get_default_port() const -> unsigned short {
  if (scheme == "https") {
    return 443;
  }
  return 80; // default for http
}

// HTTP client implementation

http_client::http_client() : timeout_(DEFAULT_TIMEOUT_MS) {}

http_client::http_client(std::chrono::milliseconds timeout_ms)
    : timeout_(timeout_ms) {}

auto http_client::get(const std::string &url,
                      const std::map<std::string, std::string> &query,
                      const std::map<std::string, std::string> &headers)
    -> Result<internal::http_response> {
  return request(internal::http_method::HTTP_GET, url, {}, headers, query);
}

auto http_client::post(const std::string &url, const std::string &body,
                       const std::map<std::string, std::string> &headers)
    -> Result<internal::http_response> {
  std::vector<uint8_t> body_bytes(body.begin(), body.end());
  return request(internal::http_method::HTTP_POST, url, body_bytes, headers,
                 {});
}

auto http_client::post(const std::string &url, const std::vector<uint8_t> &body,
                       const std::map<std::string, std::string> &headers)
    -> Result<internal::http_response> {
  return request(internal::http_method::HTTP_POST, url, body, headers, {});
}

auto http_client::put(const std::string &url, const std::string &body,
                      const std::map<std::string, std::string> &headers)
    -> Result<internal::http_response> {
  std::vector<uint8_t> body_bytes(body.begin(), body.end());
  return request(internal::http_method::HTTP_PUT, url, body_bytes, headers, {});
}

auto http_client::del(const std::string &url,
                      const std::map<std::string, std::string> &headers)
    -> Result<internal::http_response> {
  return request(internal::http_method::HTTP_DELETE, url, {}, headers, {});
}

auto http_client::head(const std::string &url,
                       const std::map<std::string, std::string> &headers)
    -> Result<internal::http_response> {
  return request(internal::http_method::HTTP_HEAD, url, {}, headers, {});
}

auto http_client::patch(const std::string &url, const std::string &body,
                        const std::map<std::string, std::string> &headers)
    -> Result<internal::http_response> {
  std::vector<uint8_t> body_bytes(body.begin(), body.end());
  return request(internal::http_method::HTTP_PATCH, url, body_bytes, headers,
                 {});
}

auto http_client::set_timeout(std::chrono::milliseconds timeout_ms) -> void {
  timeout_ = timeout_ms;
}

auto http_client::get_timeout() const -> std::chrono::milliseconds {
  return timeout_;
}

auto http_client::build_request(
    internal::http_method method, const http_url &url_info,
    const std::vector<uint8_t> &body,
    const std::map<std::string, std::string> &headers)
    -> internal::http_request {
  internal::http_request request;
  request.method = method;
  request.uri = url_info.path;
  request.version = internal::http_version::HTTP_1_1;
  request.query_params = url_info.query;
  request.body = body;

  // Add custom headers
  for (const auto &[name, value] : headers) {
    request.set_header(name, value);
  }

  // Add required headers
  request.set_header("Host", url_info.host);
  request.set_header("Connection", "close");
  request.set_header("Accept", "*/*");

  // Add Content-Length if body is present
  if (!body.empty()) {
    request.set_header("Content-Length", std::to_string(body.size()));
  }

  // Add User-Agent if not present
  if (!request.get_header("User-Agent")) {
    request.set_header("User-Agent", "NetworkSystem-HTTP-Client/1.0");
  }

  return request;
}

auto http_client::request(internal::http_method method, const std::string &url,
                          const std::vector<uint8_t> &body,
                          const std::map<std::string, std::string> &headers,
                          const std::map<std::string, std::string> &query)
    -> Result<internal::http_response> {
  // Parse URL
  auto url_result = http_url::parse(url);
  if (url_result.is_err()) {
    return error<internal::http_response>(-1, "Failed to parse URL: " +
                                                  url_result.error().message);
  }

  auto url_info = std::move(url_result.value());

  // Merge query parameters
  for (const auto &[key, value] : query) {
    url_info.query[key] = value;
  }

  // Check for HTTPS (not supported yet)
  if (url_info.scheme == "https") {
    return error<internal::http_response>(
        -1, "HTTPS not supported yet. Use HTTP for now.");
  }

  // Build HTTP request
  auto http_request = build_request(method, url_info, body, headers);

  // Serialize request
  auto request_bytes = internal::http_parser::serialize_request(http_request);

  // Create messaging client for this request
  auto client = std::make_shared<messaging_client>(
      "http_client_" +
      std::to_string(
          std::chrono::system_clock::now().time_since_epoch().count()));

  // Response data collection
  std::vector<uint8_t> response_data;
  std::mutex response_mutex;
  std::condition_variable response_cv;
  bool response_complete = false;
  bool has_error = false;
  std::string error_message;

  // Set up receive callback
  client->set_receive_callback([&](const std::vector<uint8_t> &data) {
    std::lock_guard<std::mutex> lock(response_mutex);
    response_data.insert(response_data.end(), data.begin(), data.end());

    // Check if we have received the complete response
    // For simplicity, we check for end of headers + Content-Length
    // or connection close
    std::string_view response_str(
        reinterpret_cast<const char *>(response_data.data()),
        response_data.size());

    auto headers_end = response_str.find("\r\n\r\n");
    if (headers_end != std::string_view::npos) {
      // We have headers, check Content-Length
      auto headers_section = response_str.substr(0, headers_end);

      // Simple Content-Length extraction
      auto cl_pos = headers_section.find("Content-Length:");
      if (cl_pos == std::string_view::npos) {
        cl_pos = headers_section.find("content-length:");
      }

      if (cl_pos != std::string_view::npos) {
        auto cl_start = headers_section.find(':', cl_pos) + 1;
        auto cl_end = headers_section.find('\r', cl_start);
        auto cl_str = headers_section.substr(cl_start, cl_end - cl_start);

        // Trim whitespace
        while (!cl_str.empty() && std::isspace(cl_str.front()))
          cl_str.remove_prefix(1);
        while (!cl_str.empty() && std::isspace(cl_str.back()))
          cl_str.remove_suffix(1);

        try {
          std::size_t content_length = std::stoull(std::string(cl_str));
          std::size_t body_start = headers_end + 4;

          if (response_data.size() >= body_start + content_length) {
            response_complete = true;
            response_cv.notify_one();
          }
        } catch (...) {
          // Invalid Content-Length, treat as complete
          response_complete = true;
          response_cv.notify_one();
        }
      } else {
        // No Content-Length, will rely on connection close
      }
    }
  });

  // Set up error callback
  client->set_error_callback([&](std::error_code ec) {
    std::lock_guard<std::mutex> lock(response_mutex);
    if (!response_complete) {
      // Connection closed, mark as complete if we have any data
      if (!response_data.empty()) {
        response_complete = true;
      } else {
        has_error = true;
        error_message = ec.message();
      }
      response_cv.notify_one();
    }
  });

  // Connect to server
  auto connect_result = client->start_client(url_info.host, url_info.port);
  if (connect_result.is_err()) {
    return error<internal::http_response>(
        -1, "Failed to connect to " + url_info.host + ":" +
                std::to_string(url_info.port) + ": " +
                connect_result.error().message);
  }

  // Wait for connection (give it a moment to establish)
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Send request
  auto send_result = client->send_packet(std::move(request_bytes));
  if (send_result.is_err()) {
    (void)client->stop_client();
    return error<internal::http_response>(-1, "Failed to send request: " +
                                                  send_result.error().message);
  }

  // Wait for response with timeout
  {
    std::unique_lock<std::mutex> lock(response_mutex);
    if (!response_cv.wait_for(lock, timeout_,
                              [&] { return response_complete || has_error; })) {
      (void)client->stop_client();
      return error<internal::http_response>(-1, "Request timeout");
    }
  }

  // Stop client
  (void)client->stop_client();

  // Check for errors
  if (has_error) {
    return error<internal::http_response>(-1,
                                          "Request failed: " + error_message);
  }

  // Parse response
  auto response_result = internal::http_parser::parse_response(response_data);
  if (response_result.is_err()) {
    return error<internal::http_response>(
        -1, "Failed to parse response: " + response_result.error().message);
  }

  return response_result;
}

} // namespace kcenon::network::core
