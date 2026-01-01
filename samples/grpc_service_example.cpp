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

/**
 * @file grpc_service_example.cpp
 * @brief Example demonstrating gRPC service registration and management
 *
 * This example shows how to:
 * - Create and configure a service registry
 * - Register generic services with multiple methods
 * - Handle different RPC types (unary, streaming)
 * - Implement health checking
 * - Route requests to appropriate handlers
 */

#include <kcenon/network/protocols/grpc/service_registry.h>
#include <kcenon/network/protocols/grpc/frame.h>
#include <kcenon/network/protocols/grpc/status.h>
#include <kcenon/network/protocols/grpc/server.h>

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

namespace grpc = kcenon::network::protocols::grpc;

// ============================================================================
// Mock server_context for example demonstration
// ============================================================================

/**
 * @brief Simple mock implementation of server_context for testing/examples
 *
 * In production, this would be provided by the gRPC server infrastructure.
 */
class mock_server_context : public grpc::server_context
{
public:
    auto client_metadata() const -> const grpc::grpc_metadata& override
    {
        return metadata_;
    }

    auto add_trailing_metadata(const std::string& key,
                               const std::string& value) -> void override
    {
        trailing_metadata_.emplace_back(key, value);
    }

    auto set_trailing_metadata(grpc::grpc_metadata metadata) -> void override
    {
        trailing_metadata_ = std::move(metadata);
    }

    auto is_cancelled() const -> bool override
    {
        return cancelled_;
    }

    auto deadline() const
        -> std::optional<std::chrono::system_clock::time_point> override
    {
        return deadline_;
    }

    auto peer() const -> std::string override
    {
        return peer_;
    }

    auto auth_context() const -> std::string override
    {
        return auth_context_;
    }

private:
    grpc::grpc_metadata metadata_;
    grpc::grpc_metadata trailing_metadata_;
    bool cancelled_ = false;
    std::optional<std::chrono::system_clock::time_point> deadline_;
    std::string peer_ = "127.0.0.1:12345";
    std::string auth_context_;
};

// ============================================================================
// Example Handlers
// ============================================================================

/**
 * @brief Simple echo handler that returns the request as-is
 */
auto echo_handler(grpc::server_context& ctx, const std::vector<uint8_t>& request)
    -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
{
    std::cout << "[Echo] Received " << request.size() << " bytes" << std::endl;
    return {grpc::grpc_status::ok_status(), request};
}

/**
 * @brief Handler that reverses the input bytes
 */
auto reverse_handler(grpc::server_context& ctx, const std::vector<uint8_t>& request)
    -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
{
    std::cout << "[Reverse] Processing " << request.size() << " bytes" << std::endl;

    if (request.empty()) {
        return {
            grpc::grpc_status::error_status(
                grpc::status_code::invalid_argument,
                "Request cannot be empty"),
            {}
        };
    }

    std::vector<uint8_t> reversed(request.rbegin(), request.rend());
    return {grpc::grpc_status::ok_status(), reversed};
}

/**
 * @brief Handler that returns current timestamp
 */
auto timestamp_handler(grpc::server_context& ctx, const std::vector<uint8_t>& request)
    -> std::pair<grpc::grpc_status, std::vector<uint8_t>>
{
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    std::string timestamp = std::to_string(ms);
    std::vector<uint8_t> response(timestamp.begin(), timestamp.end());

    std::cout << "[Timestamp] Returning: " << timestamp << std::endl;
    return {grpc::grpc_status::ok_status(), response};
}

/**
 * @brief Server streaming handler that sends multiple chunks
 */
auto stream_handler(grpc::server_context& ctx,
                    const std::vector<uint8_t>& request,
                    grpc::server_writer& writer) -> grpc::grpc_status
{
    std::cout << "[Stream] Starting stream for " << request.size() << " byte request" << std::endl;

    for (int i = 0; i < 5; ++i) {
        std::string chunk = "Chunk " + std::to_string(i + 1);
        std::vector<uint8_t> data(chunk.begin(), chunk.end());

        auto result = writer.write(data);
        if (result.is_err()) {
            return grpc::grpc_status::error_status(
                grpc::status_code::internal,
                "Failed to write chunk " + std::to_string(i + 1));
        }

        std::cout << "[Stream] Sent: " << chunk << std::endl;
    }

    return grpc::grpc_status::ok_status();
}

// ============================================================================
// Demo Functions
// ============================================================================

void demo_service_registration()
{
    std::cout << "\n=== Service Registration Demo ===" << std::endl;

    // Create registry with health check and reflection
    grpc::registry_config config;
    config.enable_health_check = true;
    config.enable_reflection = true;

    grpc::service_registry registry(config);

    // Create Echo service
    grpc::generic_service echo_service("demo.EchoService");
    echo_service.register_unary_method("Echo", echo_handler, "EchoRequest", "EchoResponse");
    echo_service.register_unary_method("Reverse", reverse_handler);

    // Create Utility service
    grpc::generic_service util_service("demo.UtilityService");
    util_service.register_unary_method("GetTimestamp", timestamp_handler);
    util_service.register_server_streaming_method("StreamData", stream_handler);

    // Register services
    auto result1 = registry.register_service(&echo_service);
    auto result2 = registry.register_service(&util_service);

    if (result1.is_ok() && result2.is_ok()) {
        std::cout << "Successfully registered services:" << std::endl;
        for (const auto& name : registry.service_names()) {
            std::cout << "  - " << name << std::endl;
        }
    }

    // Verify method lookup
    auto method = registry.find_method("/demo.EchoService/Echo");
    if (method.has_value()) {
        std::cout << "Found method: " << method->second->name
                  << " (type: " << static_cast<int>(method->second->type) << ")"
                  << std::endl;
    }
}

void demo_message_framing()
{
    std::cout << "\n=== Message Framing Demo ===" << std::endl;

    // Create a sample payload
    std::string payload = "Hello, gRPC!";
    std::vector<uint8_t> data(payload.begin(), payload.end());

    // Create gRPC message
    grpc::grpc_message msg(data, false);

    std::cout << "Original data size: " << data.size() << " bytes" << std::endl;
    std::cout << "Serialized size: " << msg.serialized_size() << " bytes" << std::endl;
    std::cout << "Compressed: " << (msg.compressed ? "yes" : "no") << std::endl;

    // Serialize
    auto serialized = msg.serialize();

    // Parse back
    auto parsed = grpc::grpc_message::parse(serialized);
    if (parsed.is_ok()) {
        std::string recovered(parsed.value().data.begin(), parsed.value().data.end());
        std::cout << "Recovered data: " << recovered << std::endl;
    }
}

void demo_health_checking()
{
    std::cout << "\n=== Health Checking Demo ===" << std::endl;

    grpc::health_service health;

    // Set initial status
    health.set_status("demo.EchoService", grpc::health_status::serving);
    health.set_status("demo.UtilityService", grpc::health_status::serving);
    health.set_status("", grpc::health_status::serving);  // Server-wide status

    std::cout << "Initial health status:" << std::endl;
    std::cout << "  demo.EchoService: " << (health.get_status("demo.EchoService") == grpc::health_status::serving ? "SERVING" : "NOT_SERVING") << std::endl;
    std::cout << "  demo.UtilityService: " << (health.get_status("demo.UtilityService") == grpc::health_status::serving ? "SERVING" : "NOT_SERVING") << std::endl;

    // Simulate service going down
    health.set_status("demo.UtilityService", grpc::health_status::not_serving);

    std::cout << "\nAfter UtilityService goes down:" << std::endl;
    std::cout << "  demo.EchoService: " << (health.get_status("demo.EchoService") == grpc::health_status::serving ? "SERVING" : "NOT_SERVING") << std::endl;
    std::cout << "  demo.UtilityService: " << (health.get_status("demo.UtilityService") == grpc::health_status::serving ? "SERVING" : "NOT_SERVING") << std::endl;

    // Unknown service
    auto unknown = health.get_status("unknown.Service");
    std::cout << "\nUnknown service status: " << (unknown == grpc::health_status::service_unknown ? "SERVICE_UNKNOWN" : "OTHER") << std::endl;
}

void demo_status_codes()
{
    std::cout << "\n=== Status Codes Demo ===" << std::endl;

    // OK status
    auto ok = grpc::grpc_status::ok_status();
    std::cout << "OK status: " << ok.code_string() << " (is_ok=" << ok.is_ok() << ")" << std::endl;

    // Error statuses
    std::vector<grpc::status_code> codes = {
        grpc::status_code::invalid_argument,
        grpc::status_code::not_found,
        grpc::status_code::permission_denied,
        grpc::status_code::internal,
        grpc::status_code::unavailable
    };

    std::cout << "\nCommon error codes:" << std::endl;
    for (auto code : codes) {
        auto status = grpc::grpc_status::error_status(code, "Example error");
        std::cout << "  " << grpc::status_code_to_string(code)
                  << " (code=" << static_cast<int>(code) << ")" << std::endl;
    }

    // Status with details
    grpc::grpc_status detailed(
        grpc::status_code::invalid_argument,
        "Validation failed",
        "field 'email' is required"
    );
    std::cout << "\nStatus with details:" << std::endl;
    std::cout << "  Code: " << detailed.code_string() << std::endl;
    std::cout << "  Message: " << detailed.message << std::endl;
    if (detailed.details.has_value()) {
        std::cout << "  Details: " << detailed.details.value() << std::endl;
    }
}

void demo_timeout_parsing()
{
    std::cout << "\n=== Timeout Parsing Demo ===" << std::endl;

    std::vector<std::string> timeouts = {"100m", "1S", "30S", "1M", "5M", "1H"};

    std::cout << "Parsing gRPC timeout formats:" << std::endl;
    for (const auto& t : timeouts) {
        uint64_t ms = grpc::parse_timeout(t);
        std::cout << "  " << t << " -> " << ms << " ms" << std::endl;
    }

    std::cout << "\nFormatting milliseconds to gRPC format:" << std::endl;
    std::vector<uint64_t> values = {500, 1000, 30000, 60000, 3600000};
    for (auto v : values) {
        std::string formatted = grpc::format_timeout(v);
        std::cout << "  " << v << " ms -> " << formatted << std::endl;
    }
}

void demo_handler_invocation()
{
    std::cout << "\n=== Handler Invocation Demo ===" << std::endl;

    // Create service
    grpc::generic_service service("demo.TestService");
    service.register_unary_method("Echo", echo_handler);
    service.register_unary_method("Reverse", reverse_handler);

    // Simulate request
    std::string request_data = "Test Data";
    std::vector<uint8_t> request(request_data.begin(), request_data.end());

    // Get and invoke handler
    auto* handler = service.get_unary_handler("Echo");
    if (handler) {
        mock_server_context ctx;
        auto [status, response] = (*handler)(ctx, request);

        if (status.is_ok()) {
            std::string result(response.begin(), response.end());
            std::cout << "Echo result: " << result << std::endl;
        }
    }

    handler = service.get_unary_handler("Reverse");
    if (handler) {
        mock_server_context ctx;
        auto [status, response] = (*handler)(ctx, request);

        if (status.is_ok()) {
            std::string result(response.begin(), response.end());
            std::cout << "Reverse result: " << result << std::endl;
        }
    }

    // Non-existent handler
    auto* missing = service.get_unary_handler("NonExistent");
    std::cout << "NonExistent handler: " << (missing ? "found" : "not found") << std::endl;
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "gRPC Service Example" << std::endl;
    std::cout << "===================" << std::endl;

    demo_service_registration();
    demo_message_framing();
    demo_health_checking();
    demo_status_codes();
    demo_timeout_parsing();
    demo_handler_invocation();

    std::cout << "\n=== Demo Complete ===" << std::endl;
    return 0;
}
