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
#include <kcenon/network/detail/protocols/grpc/service_registry.h>

namespace grpc = kcenon::network::protocols::grpc;

// ============================================================================
// Utility Functions Tests
// ============================================================================

class MethodPathParseTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MethodPathParseTest, ValidPath)
{
    auto result = grpc::parse_method_path("/helloworld.Greeter/SayHello");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "helloworld.Greeter");
    EXPECT_EQ(result->second, "SayHello");
}

TEST_F(MethodPathParseTest, ValidPathWithNestedPackage)
{
    auto result = grpc::parse_method_path("/com.example.api.v1.UserService/GetUser");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->first, "com.example.api.v1.UserService");
    EXPECT_EQ(result->second, "GetUser");
}

TEST_F(MethodPathParseTest, EmptyPath)
{
    auto result = grpc::parse_method_path("");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MethodPathParseTest, NoLeadingSlash)
{
    auto result = grpc::parse_method_path("helloworld.Greeter/SayHello");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MethodPathParseTest, NoMethodSeparator)
{
    auto result = grpc::parse_method_path("/helloworld.Greeter");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MethodPathParseTest, EmptyMethod)
{
    auto result = grpc::parse_method_path("/helloworld.Greeter/");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MethodPathParseTest, EmptyService)
{
    auto result = grpc::parse_method_path("//SayHello");
    EXPECT_FALSE(result.has_value());
}

TEST_F(MethodPathParseTest, BuildMethodPath)
{
    auto path = grpc::build_method_path("helloworld.Greeter", "SayHello");
    EXPECT_EQ(path, "/helloworld.Greeter/SayHello");
}

// ============================================================================
// Method Descriptor Tests
// ============================================================================

class MethodDescriptorTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(MethodDescriptorTest, UnaryMethod)
{
    grpc::method_descriptor desc;
    desc.name = "SayHello";
    desc.type = grpc::method_type::unary;

    EXPECT_FALSE(desc.is_client_streaming());
    EXPECT_FALSE(desc.is_server_streaming());
}

TEST_F(MethodDescriptorTest, ServerStreamingMethod)
{
    grpc::method_descriptor desc;
    desc.name = "ListMessages";
    desc.type = grpc::method_type::server_streaming;

    EXPECT_FALSE(desc.is_client_streaming());
    EXPECT_TRUE(desc.is_server_streaming());
}

TEST_F(MethodDescriptorTest, ClientStreamingMethod)
{
    grpc::method_descriptor desc;
    desc.name = "SendMessages";
    desc.type = grpc::method_type::client_streaming;

    EXPECT_TRUE(desc.is_client_streaming());
    EXPECT_FALSE(desc.is_server_streaming());
}

TEST_F(MethodDescriptorTest, BidiStreamingMethod)
{
    grpc::method_descriptor desc;
    desc.name = "Chat";
    desc.type = grpc::method_type::bidi_streaming;

    EXPECT_TRUE(desc.is_client_streaming());
    EXPECT_TRUE(desc.is_server_streaming());
}

// ============================================================================
// Service Descriptor Tests
// ============================================================================

class ServiceDescriptorTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ServiceDescriptorTest, FullName)
{
    grpc::service_descriptor desc;
    desc.package = "helloworld";
    desc.name = "Greeter";

    EXPECT_EQ(desc.full_name(), "helloworld.Greeter");
}

TEST_F(ServiceDescriptorTest, FullNameNoPackage)
{
    grpc::service_descriptor desc;
    desc.name = "Greeter";

    EXPECT_EQ(desc.full_name(), "Greeter");
}

TEST_F(ServiceDescriptorTest, FindMethod)
{
    grpc::service_descriptor desc;
    desc.name = "Greeter";

    grpc::method_descriptor method1;
    method1.name = "SayHello";
    desc.methods.push_back(method1);

    grpc::method_descriptor method2;
    method2.name = "SayGoodbye";
    desc.methods.push_back(method2);

    auto* found = desc.find_method("SayHello");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "SayHello");

    found = desc.find_method("SayGoodbye");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "SayGoodbye");

    found = desc.find_method("NonExistent");
    EXPECT_EQ(found, nullptr);
}

// ============================================================================
// Generic Service Tests
// ============================================================================

class GenericServiceTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(GenericServiceTest, Construction)
{
    grpc::generic_service service("helloworld.Greeter");
    const auto& desc = service.descriptor();

    EXPECT_EQ(desc.package, "helloworld");
    EXPECT_EQ(desc.name, "Greeter");
    EXPECT_EQ(desc.full_name(), "helloworld.Greeter");
}

TEST_F(GenericServiceTest, ConstructionNoPackage)
{
    grpc::generic_service service("Greeter");
    const auto& desc = service.descriptor();

    EXPECT_EQ(desc.package, "");
    EXPECT_EQ(desc.name, "Greeter");
}

TEST_F(GenericServiceTest, RegisterUnaryMethod)
{
    grpc::generic_service service("test.Service");

    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), request};
    };

    auto result = service.register_unary_method("Echo", handler);
    EXPECT_TRUE(result.is_ok());

    const auto& desc = service.descriptor();
    EXPECT_EQ(desc.methods.size(), 1);
    EXPECT_EQ(desc.methods[0].name, "Echo");
    EXPECT_EQ(desc.methods[0].type, grpc::method_type::unary);

    auto* retrieved = service.get_unary_handler("Echo");
    EXPECT_NE(retrieved, nullptr);
}

TEST_F(GenericServiceTest, RegisterDuplicateMethod)
{
    grpc::generic_service service("test.Service");

    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };

    auto result1 = service.register_unary_method("Echo", handler);
    EXPECT_TRUE(result1.is_ok());

    auto result2 = service.register_unary_method("Echo", handler);
    EXPECT_TRUE(result2.is_err());
}

TEST_F(GenericServiceTest, RegisterServerStreamingMethod)
{
    grpc::generic_service service("test.Service");

    auto handler = [](grpc::server_context& ctx,
                      const std::vector<uint8_t>& request,
                      grpc::server_writer& writer) -> grpc::grpc_status {
        return grpc::grpc_status::ok_status();
    };

    auto result = service.register_server_streaming_method(
        "StreamData", handler, "StreamRequest", "StreamResponse");
    EXPECT_TRUE(result.is_ok());

    const auto& desc = service.descriptor();
    auto* method = desc.find_method("StreamData");
    ASSERT_NE(method, nullptr);
    EXPECT_EQ(method->type, grpc::method_type::server_streaming);
    EXPECT_EQ(method->input_type, "StreamRequest");
    EXPECT_EQ(method->output_type, "StreamResponse");
}

TEST_F(GenericServiceTest, RegisterClientStreamingMethod)
{
    grpc::generic_service service("test.Service");

    auto handler = [](grpc::server_context& ctx, grpc::server_reader& reader)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };

    auto result = service.register_client_streaming_method("ReceiveData", handler);
    EXPECT_TRUE(result.is_ok());

    auto* retrieved = service.get_client_streaming_handler("ReceiveData");
    EXPECT_NE(retrieved, nullptr);
}

TEST_F(GenericServiceTest, RegisterBidiStreamingMethod)
{
    grpc::generic_service service("test.Service");

    auto handler = [](grpc::server_context& ctx, grpc::server_reader_writer& stream)
        -> grpc::grpc_status {
        return grpc::grpc_status::ok_status();
    };

    auto result = service.register_bidi_streaming_method("Chat", handler);
    EXPECT_TRUE(result.is_ok());

    auto* retrieved = service.get_bidi_streaming_handler("Chat");
    EXPECT_NE(retrieved, nullptr);
}

TEST_F(GenericServiceTest, GetNonexistentHandler)
{
    grpc::generic_service service("test.Service");

    EXPECT_EQ(service.get_unary_handler("NonExistent"), nullptr);
    EXPECT_EQ(service.get_server_streaming_handler("NonExistent"), nullptr);
    EXPECT_EQ(service.get_client_streaming_handler("NonExistent"), nullptr);
    EXPECT_EQ(service.get_bidi_streaming_handler("NonExistent"), nullptr);
}

TEST_F(GenericServiceTest, MoveConstruction)
{
    grpc::generic_service service1("test.Service");
    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };
    service1.register_unary_method("Echo", handler);

    grpc::generic_service service2(std::move(service1));

    EXPECT_EQ(service2.descriptor().full_name(), "test.Service");
    EXPECT_NE(service2.get_unary_handler("Echo"), nullptr);
}

// ============================================================================
// Service Registry Tests
// ============================================================================

class ServiceRegistryTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ServiceRegistryTest, Construction)
{
    grpc::service_registry registry;
    EXPECT_TRUE(registry.services().empty());
    EXPECT_FALSE(registry.is_reflection_enabled());
}

TEST_F(ServiceRegistryTest, ConstructionWithReflection)
{
    grpc::registry_config config;
    config.enable_reflection = true;

    grpc::service_registry registry(config);
    EXPECT_TRUE(registry.is_reflection_enabled());
}

TEST_F(ServiceRegistryTest, RegisterService)
{
    grpc::service_registry registry;
    grpc::generic_service service("test.Service");

    auto result = registry.register_service(&service);
    EXPECT_TRUE(result.is_ok());

    auto services = registry.services();
    EXPECT_EQ(services.size(), 1);
    EXPECT_EQ(services[0], &service);
}

TEST_F(ServiceRegistryTest, RegisterMultipleServices)
{
    grpc::service_registry registry;
    grpc::generic_service service1("test.Service1");
    grpc::generic_service service2("test.Service2");

    EXPECT_TRUE(registry.register_service(&service1).is_ok());
    EXPECT_TRUE(registry.register_service(&service2).is_ok());

    auto services = registry.services();
    EXPECT_EQ(services.size(), 2);
}

TEST_F(ServiceRegistryTest, RegisterDuplicateService)
{
    grpc::service_registry registry;
    grpc::generic_service service1("test.Service");
    grpc::generic_service service2("test.Service");

    EXPECT_TRUE(registry.register_service(&service1).is_ok());
    EXPECT_TRUE(registry.register_service(&service2).is_err());
}

TEST_F(ServiceRegistryTest, RegisterNullService)
{
    grpc::service_registry registry;

    auto result = registry.register_service(nullptr);
    EXPECT_TRUE(result.is_err());
}

TEST_F(ServiceRegistryTest, UnregisterService)
{
    grpc::service_registry registry;
    grpc::generic_service service("test.Service");

    registry.register_service(&service);
    EXPECT_EQ(registry.services().size(), 1);

    auto result = registry.unregister_service("test.Service");
    EXPECT_TRUE(result.is_ok());
    EXPECT_TRUE(registry.services().empty());
}

TEST_F(ServiceRegistryTest, UnregisterNonexistentService)
{
    grpc::service_registry registry;

    auto result = registry.unregister_service("test.NonExistent");
    EXPECT_TRUE(result.is_err());
}

TEST_F(ServiceRegistryTest, FindService)
{
    grpc::service_registry registry;
    grpc::generic_service service("test.Service");

    registry.register_service(&service);

    auto* found = registry.find_service("test.Service");
    EXPECT_EQ(found, &service);

    found = registry.find_service("test.NonExistent");
    EXPECT_EQ(found, nullptr);
}

TEST_F(ServiceRegistryTest, ServiceNames)
{
    grpc::service_registry registry;
    grpc::generic_service service1("test.Service1");
    grpc::generic_service service2("test.Service2");

    registry.register_service(&service1);
    registry.register_service(&service2);

    auto names = registry.service_names();
    EXPECT_EQ(names.size(), 2);
    EXPECT_TRUE(std::find(names.begin(), names.end(), "test.Service1") != names.end());
    EXPECT_TRUE(std::find(names.begin(), names.end(), "test.Service2") != names.end());
}

TEST_F(ServiceRegistryTest, FindMethod)
{
    grpc::service_registry registry;
    grpc::generic_service service("test.Service");

    auto handler = [](grpc::server_context& ctx, const std::vector<uint8_t>& request)
        -> std::pair<grpc::grpc_status, std::vector<uint8_t>> {
        return {grpc::grpc_status::ok_status(), {}};
    };
    service.register_unary_method("Echo", handler);

    registry.register_service(&service);

    auto found = registry.find_method("/test.Service/Echo");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->first, &service);
    EXPECT_EQ(found->second->name, "Echo");
}

TEST_F(ServiceRegistryTest, FindMethodInvalidPath)
{
    grpc::service_registry registry;

    auto found = registry.find_method("invalid_path");
    EXPECT_FALSE(found.has_value());
}

TEST_F(ServiceRegistryTest, FindMethodNonexistentService)
{
    grpc::service_registry registry;

    auto found = registry.find_method("/nonexistent.Service/Method");
    EXPECT_FALSE(found.has_value());
}

TEST_F(ServiceRegistryTest, FindMethodNonexistentMethod)
{
    grpc::service_registry registry;
    grpc::generic_service service("test.Service");

    registry.register_service(&service);

    auto found = registry.find_method("/test.Service/NonExistent");
    EXPECT_FALSE(found.has_value());
}

TEST_F(ServiceRegistryTest, SetServiceHealth)
{
    grpc::registry_config config;
    config.enable_health_check = true;
    grpc::service_registry registry(config);

    grpc::generic_service service("test.Service");
    registry.register_service(&service);

    EXPECT_TRUE(registry.get_service_health("test.Service"));

    auto result = registry.set_service_health("test.Service", false);
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(registry.get_service_health("test.Service"));
}

TEST_F(ServiceRegistryTest, SetHealthNonexistentService)
{
    grpc::service_registry registry;

    auto result = registry.set_service_health("nonexistent.Service", true);
    EXPECT_TRUE(result.is_err());
}

TEST_F(ServiceRegistryTest, MoveConstruction)
{
    grpc::service_registry registry1;
    grpc::generic_service service("test.Service");
    registry1.register_service(&service);

    grpc::service_registry registry2(std::move(registry1));

    auto* found = registry2.find_service("test.Service");
    EXPECT_EQ(found, &service);
}

// ============================================================================
// Health Service Tests
// ============================================================================

class HealthServiceTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(HealthServiceTest, Construction)
{
    grpc::health_service service;
    const auto& desc = service.descriptor();

    EXPECT_EQ(desc.full_name(), "grpc.health.v1.Health");
    EXPECT_EQ(desc.methods.size(), 2);
}

TEST_F(HealthServiceTest, SetAndGetStatus)
{
    grpc::health_service service;

    service.set_status("test.Service", grpc::health_status::serving);
    EXPECT_EQ(service.get_status("test.Service"), grpc::health_status::serving);

    service.set_status("test.Service", grpc::health_status::not_serving);
    EXPECT_EQ(service.get_status("test.Service"), grpc::health_status::not_serving);
}

TEST_F(HealthServiceTest, GetUnknownService)
{
    grpc::health_service service;

    EXPECT_EQ(service.get_status("unknown.Service"), grpc::health_status::service_unknown);
}

TEST_F(HealthServiceTest, ServerWideStatus)
{
    grpc::health_service service;

    service.set_status("", grpc::health_status::serving);
    EXPECT_EQ(service.get_status(""), grpc::health_status::serving);
}

TEST_F(HealthServiceTest, Clear)
{
    grpc::health_service service;

    service.set_status("test.Service1", grpc::health_status::serving);
    service.set_status("test.Service2", grpc::health_status::serving);

    service.clear();

    EXPECT_EQ(service.get_status("test.Service1"), grpc::health_status::service_unknown);
    EXPECT_EQ(service.get_status("test.Service2"), grpc::health_status::service_unknown);
}

TEST_F(HealthServiceTest, MoveConstruction)
{
    grpc::health_service service1;
    service1.set_status("test.Service", grpc::health_status::serving);

    grpc::health_service service2(std::move(service1));

    EXPECT_EQ(service2.get_status("test.Service"), grpc::health_status::serving);
}
