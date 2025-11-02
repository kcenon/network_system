/**
 * BSD 3-Clause License
 * Copyright (c) 2024, Network System Project
 *
 * HTTP Client Demo
 *
 * This demo showcases the HTTP client functionality using a local HTTP server.
 *
 * IMPORTANT:
 * - This demo requires a local HTTP server running on http://localhost:8080
 * - Run 'simple_http_server' before running this demo
 * - HTTPS is not yet supported (HTTP only)
 * - All external dependencies have been removed to ensure local-only testing
 */

#include <iostream>
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <chrono>
#include <thread>
#include <future>
#include <iomanip>
#include <algorithm>
#include "network_system/network_system.h"
#include "network_system/core/http_client.h"

using network_system::core::http_client;
using network_system::internal::http_response;
using network_system::Result;

class http_demo {
public:
    http_demo() {
        http_client_ = std::make_shared<http_client>();
        setup_test_urls();
    }
    
    void run_demo() {
        std::cout << "=== Network System - HTTP Client Demo ===" << std::endl;
        
        test_basic_get_requests();
        test_post_requests();
        test_headers_and_authentication();
        test_file_operations();
        test_error_handling();
        test_concurrent_requests();
        test_performance_benchmark();
        
        std::cout << "\n=== HTTP Client Demo completed ===" << std::endl;
    }

private:
    std::shared_ptr<http_client> http_client_;
    std::map<std::string, std::string> test_urls_;
    
    void setup_test_urls() {
        // NOTE: All URLs use HTTP (not HTTPS) as HTTPS is not yet supported
        // These URLs assume a local HTTP server is running on port 8080
        test_urls_ = {
            {"base", "http://localhost:8080"},
            {"get", "http://localhost:8080/"},
            {"post", "http://localhost:8080/api/echo"},
            {"hello", "http://localhost:8080/api/hello"},
            {"users", "http://localhost:8080/users"}
        };
    }
    
    void test_basic_get_requests() {
        std::cout << "\n1. Basic GET Requests:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        // Simple GET request
        std::cout << "Testing simple GET request..." << std::endl;
        auto response = http_client_->get(test_urls_["get"]);
        if (const auto* http = get_response_ptr(response)) {
            std::cout << "✓ GET request successful" << std::endl;
            std::cout << "Response size: " << http->body.size() << " bytes" << std::endl;
            std::cout << "Status code: " << http->status_code << std::endl;
            std::cout << "Content-Type: " << get_header_value(*http, "Content-Type") << std::endl;

            // Show response body
            std::cout << "Response body: " << http->get_body_string() << std::endl;
        } else {
            const auto& error = response.error();
            std::cout << "✗ GET request failed: " << error.message << std::endl;
        }

        // GET with query parameters
        std::cout << "\nTesting GET with query parameters..." << std::endl;
        std::map<std::string, std::string> query_params = {
            {"name", "TestUser"}
        };

        auto param_response = http_client_->get(test_urls_["hello"], query_params);
        if (const auto* http = get_response_ptr(param_response)) {
            std::cout << "✓ GET with parameters successful" << std::endl;
            std::cout << "Status code: " << http->status_code << std::endl;
            std::cout << "Response: " << http->get_body_string() << std::endl;
        } else {
            const auto& error = param_response.error();
            std::cout << "✗ GET with parameters failed: " << error.message << std::endl;
        }

        // Path parameter test
        std::cout << "\nTesting path parameter..." << std::endl;
        auto path_response = http_client_->get(test_urls_["users"] + "/123");
        if (const auto* http = get_response_ptr(path_response)) {
            std::cout << "✓ Path parameter request successful" << std::endl;
            std::cout << "Response: " << http->get_body_string() << std::endl;
        } else {
            const auto& error = path_response.error();
            std::cout << "✗ Path parameter request failed: " << error.message << std::endl;
        }
    }
    
    void test_post_requests() {
        std::cout << "\n2. POST Requests:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        // Text POST
        std::cout << "Testing text POST request..." << std::endl;
        std::string text_data = "Hello from HTTP client demo!";

        std::map<std::string, std::string> text_headers = {
            {"Content-Type", "text/plain"}
        };

        auto post_response = http_client_->post(test_urls_["post"], text_data, text_headers);
        if (const auto* http = get_response_ptr(post_response)) {
            std::cout << "✓ Text POST successful" << std::endl;
            std::cout << "Status code: " << http->status_code << std::endl;
            std::cout << "Response: " << http->get_body_string() << std::endl;
        } else {
            const auto& error = post_response.error();
            std::cout << "✗ Text POST failed: " << error.message << std::endl;
        }

        // JSON POST
        std::cout << "\nTesting JSON POST request..." << std::endl;
        std::string json_data = R"({"message": "Test JSON data", "value": 42})";

        std::map<std::string, std::string> json_headers = {
            {"Content-Type", "application/json"}
        };

        auto json_response = http_client_->post(test_urls_["post"], json_data, json_headers);
        if (const auto* http = get_response_ptr(json_response)) {
            std::cout << "✓ JSON POST successful" << std::endl;
            std::cout << "Status code: " << http->status_code << std::endl;
            std::cout << "Response size: " << http->body.size() << " bytes" << std::endl;
        } else {
            const auto& error = json_response.error();
            std::cout << "✗ JSON POST failed: " << error.message << std::endl;
        }

        // Binary data POST
        std::cout << "\nTesting binary data POST..." << std::endl;
        std::vector<uint8_t> binary_data = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}; // PNG header

        std::map<std::string, std::string> binary_headers = {
            {"Content-Type", "application/octet-stream"}
        };

        auto binary_response = http_client_->post(test_urls_["post"], binary_data, binary_headers);
        if (const auto* http = get_response_ptr(binary_response)) {
            std::cout << "✓ Binary POST successful" << std::endl;
            std::cout << "Status code: " << http->status_code << std::endl;
        } else {
            const auto& error = binary_response.error();
            std::cout << "✗ Binary POST failed: " << error.message << std::endl;
        }
    }
    
    void test_headers_and_authentication() {
        std::cout << "\n3. Custom Headers:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        // Custom headers
        std::cout << "Testing custom headers..." << std::endl;
        std::map<std::string, std::string> custom_headers = {
            {"User-Agent", "NetworkSystem/1.0 HTTP Client Demo"},
            {"X-Custom-Header", "CustomValue"},
            {"Accept", "application/json"}
        };

        auto header_response = http_client_->get(test_urls_["get"], {}, custom_headers);
        if (const auto* http = get_response_ptr(header_response)) {
            std::cout << "✓ Custom headers request successful" << std::endl;
            std::cout << "Status code: " << http->status_code << std::endl;
        } else {
            const auto& error = header_response.error();
            std::cout << "✗ Custom headers request failed: " << error.message << std::endl;
        }

        // Test Accept header
        std::cout << "\nTesting Accept header..." << std::endl;
        std::map<std::string, std::string> accept_headers = {
            {"Accept", "text/plain"}
        };

        auto accept_response = http_client_->get(test_urls_["get"], {}, accept_headers);
        if (const auto* http = get_response_ptr(accept_response)) {
            std::cout << "✓ Accept header request successful" << std::endl;
            std::cout << "Content-Type: " << get_header_value(*http, "Content-Type") << std::endl;
        } else {
            const auto& error = accept_response.error();
            std::cout << "✗ Accept header request failed: " << error.message << std::endl;
        }
    }
    
    void test_file_operations() {
        std::cout << "\n4. Data Transfer:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        // Download data
        std::cout << "Testing data download..." << std::endl;
        auto download_response = http_client_->get(test_urls_["get"]);
        if (const auto* http = get_response_ptr(download_response)) {
            if (http->status_code == 200) {
                std::cout << "✓ Data download successful" << std::endl;
                std::cout << "Downloaded " << http->body.size() << " bytes" << std::endl;

                // Save to file (demo)
                std::string filename = "downloaded_data.txt";
                bool saved = save_response_to_file(*http, filename);
                if (saved) {
                    std::cout << "✓ Data saved as " << filename << std::endl;
                }
            } else {
                std::cout << "✗ Unexpected status code: " << http->status_code << std::endl;
            }
        } else {
            const auto& error = download_response.error();
            std::cout << "✗ Data download failed: " << error.message << std::endl;
        }

        // File upload simulation
        std::cout << "\nTesting data upload simulation..." << std::endl;
        std::string file_content = "This is test file content for upload simulation.\n";
        file_content += "Line 2: Multi-line content test\n";

        std::map<std::string, std::string> upload_headers = {
            {"Content-Type", "text/plain"}
        };

        auto upload_response = http_client_->post(test_urls_["post"], file_content, upload_headers);
        if (const auto* http = get_response_ptr(upload_response)) {
            std::cout << "✓ Data upload simulation successful" << std::endl;
            std::cout << "Status code: " << http->status_code << std::endl;
        } else {
            const auto& error = upload_response.error();
            std::cout << "✗ Data upload simulation failed: " << error.message << std::endl;
        }
    }
    
    void test_error_handling() {
        std::cout << "\n5. Error Handling:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        // Test 404 Not Found
        std::cout << "Testing 404 Not Found..." << std::endl;
        auto not_found_response = http_client_->get("http://localhost:8080/nonexistent");
        if (const auto* http = get_response_ptr(not_found_response)) {
            std::cout << "✓ Received status " << http->status_code;
            if (http->status_code == 404) {
                std::cout << " (correct 404)" << std::endl;
            } else {
                std::cout << " (expected 404)" << std::endl;
            }
        } else {
            const auto& error = not_found_response.error();
            std::cout << "✗ Request failed: " << error.message << std::endl;
        }

        // Test timeout
        std::cout << "\nTesting timeout handling..." << std::endl;
        http_client_->set_timeout(std::chrono::milliseconds(5000));  // 5 second timeout
        auto timeout_response = http_client_->get(test_urls_["get"]);
        if (timeout_response.is_ok()) {
            std::cout << "✓ Request completed within timeout" << std::endl;
        } else {
            const auto& error = timeout_response.error();
            std::cout << "✗ Request timed out or failed: " << error.message << std::endl;
        }

        // Reset timeout
        http_client_->set_timeout(std::chrono::milliseconds(10000));

        // Test invalid URL
        std::cout << "\nTesting invalid URL handling..." << std::endl;
        auto invalid_response = http_client_->get("http://invalid-domain-that-should-not-exist.com");
        if (invalid_response.is_err()) {
            const auto& error = invalid_response.error();
            std::cout << "✓ Invalid URL handled correctly: " << error.message << std::endl;
        } else {
            std::cout << "✗ Invalid URL should have failed" << std::endl;
        }

        // Test connection to local server
        std::cout << "\nTesting local server connection..." << std::endl;
        auto local_response = http_client_->get(test_urls_["get"]);
        if (const auto* http = get_response_ptr(local_response)) {
            std::cout << "✓ Local server connection successful" << std::endl;
            std::cout << "Status: " << http->status_code << std::endl;
        } else {
            const auto& error = local_response.error();
            std::cout << "✗ Local server connection failed: " << error.message << std::endl;
            std::cout << "(Make sure simple_http_server is running on port 8080)" << std::endl;
        }
    }
    
    void test_concurrent_requests() {
        std::cout << "\n6. Concurrent Requests:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        const int num_requests = 5;
        std::vector<std::future<bool>> futures;

        std::cout << "Starting " << num_requests << " concurrent requests..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_requests; ++i) {
            futures.push_back(std::async(std::launch::async, [this, i]() -> bool {
                auto client = std::make_shared<http_client>();
                std::map<std::string, std::string> params = {
                    {"request", std::to_string(i)}
                };

                auto response = client->get(test_urls_["hello"], params);
                if (const auto* http = get_response_ptr(response)) {
                    if (http->status_code == 200) {
                        std::cout << "  ✓ Concurrent request " << i << " completed" << std::endl;
                        return true;
                    }
                    std::cout << "  ✗ Concurrent request " << i << " returned status "
                              << http->status_code << std::endl;
                    return false;
                } else {
                    const auto& error = response.error();
                    std::cout << "  ✗ Concurrent request " << i << " failed: "
                              << error.message << std::endl;
                    return false;
                }
            }));
        }

        // Wait for all requests to complete
        int successful_requests = 0;
        for (auto& future : futures) {
            if (future.get()) {
                successful_requests++;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << "Concurrent requests completed:" << std::endl;
        std::cout << "  Successful: " << successful_requests << "/" << num_requests << std::endl;
        std::cout << "  Total time: " << duration.count() << " ms" << std::endl;
        if (num_requests > 0) {
            std::cout << "  Average time per request: " << duration.count() / num_requests << " ms" << std::endl;
        }
    }
    
    void test_performance_benchmark() {
        std::cout << "\n7. Performance Benchmark:" << std::endl;
        std::cout << std::string(40, '-') << std::endl;

        const int num_requests = 20;
        const std::string benchmark_url = test_urls_["get"];

        std::cout << "Running performance benchmark with " << num_requests << " requests..." << std::endl;

        std::vector<std::chrono::milliseconds> request_times;
        int successful_requests = 0;

        auto total_start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_requests; ++i) {
            auto request_start = std::chrono::high_resolution_clock::now();

            auto response = http_client_->get(benchmark_url);

            auto request_end = std::chrono::high_resolution_clock::now();
            auto request_time = std::chrono::duration_cast<std::chrono::milliseconds>(request_end - request_start);

            if (const auto* http = get_response_ptr(response)) {
                if (http->status_code == 200) {
                    successful_requests++;
                    request_times.push_back(request_time);
                }
            }

            // Small delay between requests
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        auto total_end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);

        // Calculate statistics
        if (!request_times.empty()) {
            auto min_time = *std::min_element(request_times.begin(), request_times.end());
            auto max_time = *std::max_element(request_times.begin(), request_times.end());

            long long total_request_time = 0;
            for (const auto& time : request_times) {
                total_request_time += time.count();
            }
            auto avg_time = total_request_time / request_times.size();

            std::cout << "Performance Results:" << std::endl;
            std::cout << "  Successful requests: " << successful_requests << "/" << num_requests << std::endl;
            std::cout << "  Success rate: " << std::fixed << std::setprecision(1)
                      << (double)successful_requests / num_requests * 100 << "%" << std::endl;
            std::cout << "  Total time: " << total_time.count() << " ms" << std::endl;
            std::cout << "  Average request time: " << avg_time << " ms" << std::endl;
            std::cout << "  Minimum request time: " << min_time.count() << " ms" << std::endl;
            std::cout << "  Maximum request time: " << max_time.count() << " ms" << std::endl;
            if (total_time.count() > 0) {
                std::cout << "  Requests per second: " << std::fixed << std::setprecision(2)
                          << (double)successful_requests / total_time.count() * 1000 << std::endl;
            }
        } else {
            std::cout << "No successful requests for performance analysis" << std::endl;
            std::cout << "(Make sure simple_http_server is running on port 8080)" << std::endl;
        }
    }
    
    static const http_response* get_response_ptr(const Result<http_response>& result) {
        return result.is_ok() ? &result.value() : nullptr;
    }
    
    std::string get_header_value(const http_response& response, const std::string& header_name) {
        auto it = response.headers.find(header_name);
        return (it != response.headers.end()) ? it->second : "";
    }
    
    bool save_response_to_file(const http_response& response, const std::string& filename) {
        // This is a simplified file save operation
        // In a real implementation, you would use proper file I/O
        std::cout << "  (File save simulation for " << filename << ")" << std::endl;
        return true;
    }
};

int main() {
    http_demo demo;
    demo.run_demo();
    return 0;
}
