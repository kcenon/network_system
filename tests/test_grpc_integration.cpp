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

#include <gtest/gtest.h>
#include <kcenon/network/protocols/grpc/server.h>
#include <kcenon/network/protocols/grpc/client.h>
#include <kcenon/network/protocols/grpc/frame.h>
#include <kcenon/network/protocols/grpc/status.h>
#include <kcenon/network/protocols/grpc/service_registry.h>
#include <kcenon/network/protocols/grpc/grpc_official_wrapper.h>

#include <chrono>
#include <thread>
#include <atomic>
#include <vector>

namespace grpc = kcenon::network::protocols::grpc;

// ============================================================================
// Full gRPC Stack Integration Tests
// ============================================================================

class GrpcFullStackTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GrpcFullStackTest, ServiceRegistryWithGenericService)
{
    // Create a service registry with reflection enabled
    grpc::registry_config config;
    config.enable_reflection = true;
    config.enable_health_check = true;

    grpc::service_registry registry(config);

    // Create and configure a generic service
    grpc::generic_service echo_service("echo.EchoService");

    // Register a unary method
    auto echo_handler = [](grpc::server_context& ctx,
                           const std::vector<uint8_t>& request)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        // Echo back the request
        return {grpc::grpc_status::ok_status(), request};
    };

    auto result = echo_service.register_unary_method(
        "Echo", echo_handler, "EchoRequest", "EchoResponse");
    EXPECT_TRUE(result.is_ok());

    // Register the service
    auto reg_result = registry.register_service(&echo_service);
    EXPECT_TRUE(reg_result.is_ok());

    // Verify service is registered
    auto* found = registry.find_service("echo.EchoService");
    ASSERT_NE(found, nullptr);

    // Verify method can be found
    auto method = registry.find_method("/echo.EchoService/Echo");
    ASSERT_TRUE(method.has_value());
    EXPECT_EQ(method->second->name, "Echo");
    EXPECT_EQ(method->second->type, grpc::method_type::unary);
}

TEST_F(GrpcFullStackTest, ServiceRegistryWithMultipleServices)
{
    grpc::service_registry registry;

    // Create multiple services
    grpc::generic_service service1("myapp.UserService");
    grpc::generic_service service2("myapp.OrderService");
    grpc::generic_service service3("myapp.PaymentService");

    // Register unary methods for each
    auto dummy_handler = [](grpc::server_context& ctx,
                            const std::vector<uint8_t>& request)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };

    service1.register_unary_method("GetUser", dummy_handler);
    service1.register_unary_method("CreateUser", dummy_handler);
    service1.register_unary_method("DeleteUser", dummy_handler);

    service2.register_unary_method("GetOrder", dummy_handler);
    service2.register_unary_method("CreateOrder", dummy_handler);

    service3.register_unary_method("ProcessPayment", dummy_handler);

    // Register all services
    EXPECT_TRUE(registry.register_service(&service1).is_ok());
    EXPECT_TRUE(registry.register_service(&service2).is_ok());
    EXPECT_TRUE(registry.register_service(&service3).is_ok());

    // Verify all services are registered
    auto names = registry.service_names();
    EXPECT_EQ(names.size(), 3u);

    // Verify methods can be found across services
    EXPECT_TRUE(registry.find_method("/myapp.UserService/GetUser").has_value());
    EXPECT_TRUE(registry.find_method("/myapp.OrderService/CreateOrder").has_value());
    EXPECT_TRUE(registry.find_method("/myapp.PaymentService/ProcessPayment").has_value());
}

TEST_F(GrpcFullStackTest, AllStreamingTypesRegistration)
{
    grpc::generic_service service("streaming.StreamService");

    // Unary handler
    auto unary = [](grpc::server_context& ctx, const std::vector<uint8_t>& req)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), req};
    };

    // Server streaming handler
    auto server_stream = [](grpc::server_context& ctx,
                            const std::vector<uint8_t>& req,
                            grpc::server_writer& writer) -> grpc::grpc_status {
        return grpc::grpc_status::ok_status();
    };

    // Client streaming handler
    auto client_stream = [](grpc::server_context& ctx, grpc::server_reader& reader)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };

    // Bidirectional streaming handler
    auto bidi_stream = [](grpc::server_context& ctx, grpc::server_reader_writer& stream)
        -> grpc::grpc_status {
        return grpc::grpc_status::ok_status();
    };

    // Register all types
    EXPECT_TRUE(service.register_unary_method("Unary", unary).is_ok());
    EXPECT_TRUE(service.register_server_streaming_method("ServerStream", server_stream).is_ok());
    EXPECT_TRUE(service.register_client_streaming_method("ClientStream", client_stream).is_ok());
    EXPECT_TRUE(service.register_bidi_streaming_method("BidiStream", bidi_stream).is_ok());

    // Verify method types
    const auto& desc = service.descriptor();
    EXPECT_EQ(desc.methods.size(), 4u);

    auto* unary_method = desc.find_method("Unary");
    ASSERT_NE(unary_method, nullptr);
    EXPECT_EQ(unary_method->type, grpc::method_type::unary);
    EXPECT_FALSE(unary_method->is_client_streaming());
    EXPECT_FALSE(unary_method->is_server_streaming());

    auto* server_method = desc.find_method("ServerStream");
    ASSERT_NE(server_method, nullptr);
    EXPECT_EQ(server_method->type, grpc::method_type::server_streaming);
    EXPECT_FALSE(server_method->is_client_streaming());
    EXPECT_TRUE(server_method->is_server_streaming());

    auto* client_method = desc.find_method("ClientStream");
    ASSERT_NE(client_method, nullptr);
    EXPECT_EQ(client_method->type, grpc::method_type::client_streaming);
    EXPECT_TRUE(client_method->is_client_streaming());
    EXPECT_FALSE(client_method->is_server_streaming());

    auto* bidi_method = desc.find_method("BidiStream");
    ASSERT_NE(bidi_method, nullptr);
    EXPECT_EQ(bidi_method->type, grpc::method_type::bidi_streaming);
    EXPECT_TRUE(bidi_method->is_client_streaming());
    EXPECT_TRUE(bidi_method->is_server_streaming());
}

// ============================================================================
// Health Check Integration Tests
// ============================================================================

class HealthCheckIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HealthCheckIntegrationTest, HealthServiceWithRegistry)
{
    grpc::registry_config config;
    config.enable_health_check = true;

    grpc::service_registry registry(config);

    // Create and register a service
    grpc::generic_service service("myapp.MyService");
    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& req)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };
    service.register_unary_method("DoSomething", handler);

    EXPECT_TRUE(registry.register_service(&service).is_ok());

    // Initially, service should be healthy (default)
    EXPECT_TRUE(registry.get_service_health("myapp.MyService"));

    // Set service to unhealthy
    EXPECT_TRUE(registry.set_service_health("myapp.MyService", false).is_ok());
    EXPECT_FALSE(registry.get_service_health("myapp.MyService"));

    // Set service back to healthy
    EXPECT_TRUE(registry.set_service_health("myapp.MyService", true).is_ok());
    EXPECT_TRUE(registry.get_service_health("myapp.MyService"));
}

TEST_F(HealthCheckIntegrationTest, HealthServiceStandalone)
{
    grpc::health_service health;

    // Check initial state
    EXPECT_EQ(health.get_status("unknown.Service"), grpc::health_status::service_unknown);

    // Set various statuses
    health.set_status("service.A", grpc::health_status::serving);
    health.set_status("service.B", grpc::health_status::not_serving);
    health.set_status("", grpc::health_status::serving);  // Server-wide status

    EXPECT_EQ(health.get_status("service.A"), grpc::health_status::serving);
    EXPECT_EQ(health.get_status("service.B"), grpc::health_status::not_serving);
    EXPECT_EQ(health.get_status(""), grpc::health_status::serving);

    // Clear and verify
    health.clear();
    EXPECT_EQ(health.get_status("service.A"), grpc::health_status::service_unknown);
    EXPECT_EQ(health.get_status("service.B"), grpc::health_status::service_unknown);
}

// ============================================================================
// Message Serialization Integration Tests
// ============================================================================

class MessageSerializationIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MessageSerializationIntegrationTest, MultipleMessagesInSequence)
{
    std::vector<grpc::grpc_message> messages;

    // Create multiple messages
    for (int i = 0; i < 100; ++i)
    {
        std::vector<uint8_t> data(i + 1, static_cast<uint8_t>(i % 256));
        messages.emplace_back(data, i % 2 == 0);
    }

    // Serialize and parse each
    for (size_t i = 0; i < messages.size(); ++i)
    {
        auto serialized = messages[i].serialize();
        auto parsed = grpc::grpc_message::parse(serialized);

        ASSERT_TRUE(parsed.is_ok()) << "Failed at message " << i;
        EXPECT_EQ(parsed.value().data, messages[i].data)
            << "Data mismatch at message " << i;
        EXPECT_EQ(parsed.value().compressed, messages[i].compressed)
            << "Compression flag mismatch at message " << i;
    }
}

TEST_F(MessageSerializationIntegrationTest, ConcatenatedMessages)
{
    // Simulate a stream of concatenated gRPC messages
    std::vector<uint8_t> stream;

    // Create and append 5 messages
    std::vector<grpc::grpc_message> originals;
    for (int i = 1; i <= 5; ++i)
    {
        std::vector<uint8_t> data(i * 10, static_cast<uint8_t>(i));
        grpc::grpc_message msg(data, false);
        originals.push_back(msg);

        auto serialized = msg.serialize();
        stream.insert(stream.end(), serialized.begin(), serialized.end());
    }

    // Parse messages from the stream
    size_t offset = 0;
    size_t msg_index = 0;

    while (offset < stream.size() && msg_index < originals.size())
    {
        // Need at least 5 bytes for header
        if (stream.size() - offset < 5) break;

        // Read length from header
        uint32_t length = (static_cast<uint32_t>(stream[offset + 1]) << 24) |
                          (static_cast<uint32_t>(stream[offset + 2]) << 16) |
                          (static_cast<uint32_t>(stream[offset + 3]) << 8) |
                          static_cast<uint32_t>(stream[offset + 4]);

        size_t frame_size = 5 + length;
        if (stream.size() - offset < frame_size) break;

        std::vector<uint8_t> frame(stream.begin() + offset,
                                   stream.begin() + offset + frame_size);
        auto parsed = grpc::grpc_message::parse(frame);

        ASSERT_TRUE(parsed.is_ok()) << "Failed at message " << msg_index;
        EXPECT_EQ(parsed.value().data, originals[msg_index].data);

        offset += frame_size;
        ++msg_index;
    }

    EXPECT_EQ(msg_index, 5u);
}

// ============================================================================
// Timeout Integration Tests
// ============================================================================

class TimeoutIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(TimeoutIntegrationTest, TimeoutPropagation)
{
    // Test that timeout values can be properly converted
    std::vector<std::pair<std::string, uint64_t>> test_cases = {
        {"100m", 100},         // 100 milliseconds
        {"1S", 1000},          // 1 second
        {"1M", 60000},         // 1 minute
        {"1H", 3600000},       // 1 hour
        {"500m", 500},         // 500 milliseconds
        {"30S", 30000},        // 30 seconds
        {"5M", 300000},        // 5 minutes
    };

    for (const auto& [input, expected] : test_cases)
    {
        uint64_t parsed = grpc::parse_timeout(input);
        EXPECT_EQ(parsed, expected) << "Failed for input: " << input;

        // Verify round-trip for values >= 1 second
        if (expected >= 1000)
        {
            std::string formatted = grpc::format_timeout(parsed);
            uint64_t reparsed = grpc::parse_timeout(formatted);
            EXPECT_EQ(reparsed, expected)
                << "Round-trip failed for: " << input
                << " -> " << formatted << " -> " << reparsed;
        }
    }
}

// ============================================================================
// Error Handling Integration Tests
// ============================================================================

class ErrorHandlingIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ErrorHandlingIntegrationTest, ServiceRegistrationErrors)
{
    grpc::service_registry registry;

    // Null service registration should fail
    auto result = registry.register_service(nullptr);
    EXPECT_TRUE(result.is_err());

    // Duplicate service registration should fail
    grpc::generic_service service("test.Service");
    EXPECT_TRUE(registry.register_service(&service).is_ok());
    EXPECT_TRUE(registry.register_service(&service).is_err());

    // Unregistering non-existent service should fail
    EXPECT_TRUE(registry.unregister_service("nonexistent.Service").is_err());
}

TEST_F(ErrorHandlingIntegrationTest, MethodRegistrationErrors)
{
    grpc::generic_service service("test.Service");

    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& req)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };

    // First registration should succeed
    EXPECT_TRUE(service.register_unary_method("Method1", handler).is_ok());

    // Duplicate method registration should fail
    EXPECT_TRUE(service.register_unary_method("Method1", handler).is_err());
}

TEST_F(ErrorHandlingIntegrationTest, MessageParsingErrors)
{
    // Empty data
    auto result1 = grpc::grpc_message::parse({});
    EXPECT_TRUE(result1.is_err());

    // Truncated header
    auto result2 = grpc::grpc_message::parse({0, 0, 0});
    EXPECT_TRUE(result2.is_err());

    // Length mismatch (header says 100 bytes, only 5 available)
    std::vector<uint8_t> bad_data = {0, 0, 0, 0, 100, 1, 2, 3, 4, 5};
    auto result3 = grpc::grpc_message::parse(bad_data);
    EXPECT_TRUE(result3.is_err());
}

// ============================================================================
// Thread Safety Integration Tests
// ============================================================================

class ThreadSafetyIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ThreadSafetyIntegrationTest, ConcurrentServiceLookup)
{
    grpc::service_registry registry;

    // Register multiple services
    std::vector<std::unique_ptr<grpc::generic_service>> services;
    for (int i = 0; i < 10; ++i)
    {
        auto service = std::make_unique<grpc::generic_service>(
            "test.Service" + std::to_string(i));

        auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& req)
            -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
            return {grpc::grpc_status::ok_status(), {}};
        };
        service->register_unary_method("Method", handler);

        registry.register_service(service.get());
        services.push_back(std::move(service));
    }

    // Concurrent lookups
    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < 10; ++t)
    {
        threads.emplace_back([&registry, &success_count, t]() {
            for (int i = 0; i < 100; ++i)
            {
                int service_idx = (t + i) % 10;
                std::string name = "test.Service" + std::to_string(service_idx);

                if (registry.find_service(name) != nullptr)
                {
                    ++success_count;
                }

                std::string method_path = "/" + name + "/Method";
                if (registry.find_method(method_path).has_value())
                {
                    ++success_count;
                }
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // All lookups should succeed
    EXPECT_EQ(success_count.load(), 2000);
}

TEST_F(ThreadSafetyIntegrationTest, ConcurrentHealthStatusUpdates)
{
    grpc::health_service health;

    // Concurrent updates
    std::vector<std::thread> threads;

    for (int t = 0; t < 10; ++t)
    {
        threads.emplace_back([&health, t]() {
            for (int i = 0; i < 100; ++i)
            {
                std::string service_name = "service." + std::to_string(t);
                auto status = (i % 2 == 0) ? grpc::health_status::serving
                                           : grpc::health_status::not_serving;
                health.set_status(service_name, status);
                health.get_status(service_name);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    // No crashes or deadlocks = success
    SUCCEED();
}
