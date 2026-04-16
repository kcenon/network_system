/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024-2025, kcenon
All rights reserved.
*****************************************************************************/

/**
 * @file service_registry_test.cpp
 * @brief Unit tests for gRPC service_registry, service descriptors,
 *        generic_service, health_service, and utility functions
 *
 * Tests validate:
 * - method_type enum values
 * - method_descriptor streaming predicates
 * - service_descriptor construction, find_method, full_name
 * - registry_config defaults
 * - service_registry construction and initial state
 * - service_registry move semantics
 * - generic_service construction and method registration
 * - health_service construction and status management
 * - health_status enum values
 * - parse_method_path utility function
 * - build_method_path utility function
 */

#include "kcenon/network/detail/protocols/grpc/service_registry.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace kcenon::network::protocols::grpc;

// ============================================================================
// method_descriptor Tests
// ============================================================================

TEST(MethodDescriptorTest, UnaryMethodIsNotStreaming)
{
	method_descriptor desc;
	desc.type = method_type::unary;

	EXPECT_FALSE(desc.is_client_streaming());
	EXPECT_FALSE(desc.is_server_streaming());
}

TEST(MethodDescriptorTest, ServerStreamingMethod)
{
	method_descriptor desc;
	desc.type = method_type::server_streaming;

	EXPECT_FALSE(desc.is_client_streaming());
	EXPECT_TRUE(desc.is_server_streaming());
}

TEST(MethodDescriptorTest, ClientStreamingMethod)
{
	method_descriptor desc;
	desc.type = method_type::client_streaming;

	EXPECT_TRUE(desc.is_client_streaming());
	EXPECT_FALSE(desc.is_server_streaming());
}

TEST(MethodDescriptorTest, BidiStreamingMethod)
{
	method_descriptor desc;
	desc.type = method_type::bidi_streaming;

	EXPECT_TRUE(desc.is_client_streaming());
	EXPECT_TRUE(desc.is_server_streaming());
}

TEST(MethodDescriptorTest, DefaultTypeIsUnary)
{
	method_descriptor desc;

	EXPECT_EQ(desc.type, method_type::unary);
}

TEST(MethodDescriptorTest, FieldAssignment)
{
	method_descriptor desc;
	desc.name = "SayHello";
	desc.full_name = "/helloworld.Greeter/SayHello";
	desc.type = method_type::unary;
	desc.input_type = "HelloRequest";
	desc.output_type = "HelloReply";

	EXPECT_EQ(desc.name, "SayHello");
	EXPECT_EQ(desc.full_name, "/helloworld.Greeter/SayHello");
	EXPECT_EQ(desc.input_type, "HelloRequest");
	EXPECT_EQ(desc.output_type, "HelloReply");
}

// ============================================================================
// service_descriptor Tests
// ============================================================================

TEST(ServiceDescriptorTest, EmptyDescriptor)
{
	service_descriptor desc;

	EXPECT_TRUE(desc.name.empty());
	EXPECT_TRUE(desc.package.empty());
	EXPECT_TRUE(desc.methods.empty());
}

TEST(ServiceDescriptorTest, FullNameWithPackage)
{
	service_descriptor desc;
	desc.name = "Greeter";
	desc.package = "helloworld";

	EXPECT_EQ(desc.full_name(), "helloworld.Greeter");
}

TEST(ServiceDescriptorTest, FullNameWithoutPackage)
{
	service_descriptor desc;
	desc.name = "Greeter";
	desc.package = "";

	EXPECT_EQ(desc.full_name(), "Greeter");
}

TEST(ServiceDescriptorTest, FindMethodExists)
{
	service_descriptor desc;
	method_descriptor method;
	method.name = "SayHello";
	method.full_name = "/helloworld.Greeter/SayHello";
	desc.methods.push_back(method);

	auto result = desc.find_method("SayHello");

	ASSERT_NE(result, nullptr);
	EXPECT_EQ(result->name, "SayHello");
}

TEST(ServiceDescriptorTest, FindMethodNotFound)
{
	service_descriptor desc;
	method_descriptor method;
	method.name = "SayHello";
	desc.methods.push_back(method);

	auto result = desc.find_method("DoSomething");

	EXPECT_EQ(result, nullptr);
}

TEST(ServiceDescriptorTest, FindMethodEmptyMethods)
{
	service_descriptor desc;

	auto result = desc.find_method("SayHello");

	EXPECT_EQ(result, nullptr);
}

TEST(ServiceDescriptorTest, FindMethodMultipleMethods)
{
	service_descriptor desc;

	method_descriptor m1;
	m1.name = "SayHello";
	m1.type = method_type::unary;
	desc.methods.push_back(m1);

	method_descriptor m2;
	m2.name = "ListFeatures";
	m2.type = method_type::server_streaming;
	desc.methods.push_back(m2);

	auto result = desc.find_method("ListFeatures");
	ASSERT_NE(result, nullptr);
	EXPECT_EQ(result->name, "ListFeatures");
	EXPECT_EQ(result->type, method_type::server_streaming);
}

// ============================================================================
// registry_config Tests
// ============================================================================

TEST(RegistryConfigTest, DefaultValues)
{
	registry_config config;

	EXPECT_FALSE(config.enable_reflection);
	EXPECT_FALSE(config.enable_health_check);
	EXPECT_EQ(config.health_service_name, "grpc.health.v1.Health");
}

// ============================================================================
// service_registry Tests
// ============================================================================

TEST(ServiceRegistryTest, DefaultConstruction)
{
	service_registry registry;

	EXPECT_TRUE(registry.services().empty());
	EXPECT_TRUE(registry.service_names().empty());
	EXPECT_FALSE(registry.is_reflection_enabled());
}

TEST(ServiceRegistryTest, ConstructWithReflectionEnabled)
{
	registry_config config;
	config.enable_reflection = true;

	service_registry registry(config);

	EXPECT_TRUE(registry.is_reflection_enabled());
}

TEST(ServiceRegistryTest, FindServiceNotRegistered)
{
	service_registry registry;

	auto result = registry.find_service("nonexistent.Service");

	EXPECT_EQ(result, nullptr);
}

TEST(ServiceRegistryTest, FindMethodNotRegistered)
{
	service_registry registry;

	auto result = registry.find_method("/nonexistent.Service/Method");

	EXPECT_FALSE(result.has_value());
}

TEST(ServiceRegistryTest, MoveConstruct)
{
	service_registry original;
	service_registry moved(std::move(original));

	EXPECT_TRUE(moved.services().empty());
}

TEST(ServiceRegistryTest, MoveAssign)
{
	service_registry r1;
	service_registry r2;

	r2 = std::move(r1);
	EXPECT_TRUE(r2.services().empty());
}

TEST(ServiceRegistryTest, UnregisterNonexistentService)
{
	service_registry registry;

	auto result = registry.unregister_service("nonexistent.Service");
	EXPECT_TRUE(result.is_err());
}

TEST(ServiceRegistryTest, GetServiceHealthDefault)
{
	service_registry registry;

	// Non-existent service should return false
	EXPECT_FALSE(registry.get_service_health("nonexistent.Service"));
}

// ============================================================================
// generic_service Tests
// ============================================================================

TEST(GenericServiceTest, Construction)
{
	generic_service service("mypackage.MyService");

	auto& desc = service.descriptor();
	EXPECT_EQ(desc.full_name(), "mypackage.MyService");
	EXPECT_TRUE(service.is_ready());
}

TEST(GenericServiceTest, RegisterUnaryMethod)
{
	generic_service service("test.Service");

	auto result = service.register_unary_method(
		"Echo",
		[](server_context&, const std::vector<uint8_t>& request)
			-> std::pair<grpc_status, std::vector<uint8_t>> {
			return {grpc_status::ok_status(), request};
		});

	EXPECT_TRUE(result.is_ok());
}

TEST(GenericServiceTest, GetUnaryHandlerRegistered)
{
	generic_service service("test.Service");

	service.register_unary_method(
		"Echo",
		[](server_context&, const std::vector<uint8_t>& request)
			-> std::pair<grpc_status, std::vector<uint8_t>> {
			return {grpc_status::ok_status(), request};
		});

	auto handler = service.get_unary_handler("Echo");
	EXPECT_NE(handler, nullptr);
}

TEST(GenericServiceTest, GetUnaryHandlerNotRegistered)
{
	generic_service service("test.Service");

	auto handler = service.get_unary_handler("NonExistent");
	EXPECT_EQ(handler, nullptr);
}

TEST(GenericServiceTest, RegisterServerStreamingMethod)
{
	generic_service service("test.Service");

	auto result = service.register_server_streaming_method(
		"ListItems",
		[](server_context&, const std::vector<uint8_t>&, server_writer&) -> grpc_status {
			return grpc_status::ok_status();
		});

	EXPECT_TRUE(result.is_ok());
}

TEST(GenericServiceTest, GetServerStreamingHandlerRegistered)
{
	generic_service service("test.Service");

	service.register_server_streaming_method(
		"ListItems",
		[](server_context&, const std::vector<uint8_t>&, server_writer&) -> grpc_status {
			return grpc_status::ok_status();
		});

	auto handler = service.get_server_streaming_handler("ListItems");
	EXPECT_NE(handler, nullptr);
}

TEST(GenericServiceTest, GetServerStreamingHandlerNotRegistered)
{
	generic_service service("test.Service");

	auto handler = service.get_server_streaming_handler("NonExistent");
	EXPECT_EQ(handler, nullptr);
}

TEST(GenericServiceTest, MoveConstruct)
{
	generic_service original("test.Service");
	original.register_unary_method(
		"Echo",
		[](server_context&, const std::vector<uint8_t>& request)
			-> std::pair<grpc_status, std::vector<uint8_t>> {
			return {grpc_status::ok_status(), request};
		});

	generic_service moved(std::move(original));
	EXPECT_NE(moved.get_unary_handler("Echo"), nullptr);
}

TEST(GenericServiceTest, DescriptorMethodsAfterRegistration)
{
	generic_service service("test.Service");

	service.register_unary_method("Echo",
		[](server_context&, const std::vector<uint8_t>& request)
			-> std::pair<grpc_status, std::vector<uint8_t>> {
			return {grpc_status::ok_status(), request};
		},
		"EchoRequest", "EchoResponse");

	auto& desc = service.descriptor();
	EXPECT_FALSE(desc.methods.empty());

	auto method = desc.find_method("Echo");
	ASSERT_NE(method, nullptr);
	EXPECT_EQ(method->name, "Echo");
	EXPECT_EQ(method->input_type, "EchoRequest");
	EXPECT_EQ(method->output_type, "EchoResponse");
}

// ============================================================================
// health_service Tests
// ============================================================================

TEST(HealthServiceTest, Construction)
{
	health_service service;

	auto& desc = service.descriptor();
	EXPECT_FALSE(desc.name.empty());
}

TEST(HealthServiceTest, DefaultStatusIsUnknown)
{
	health_service service;

	auto status = service.get_status("some.service");
	EXPECT_EQ(status, health_status::service_unknown);
}

TEST(HealthServiceTest, SetAndGetStatus)
{
	health_service service;

	service.set_status("my.service", health_status::serving);
	EXPECT_EQ(service.get_status("my.service"), health_status::serving);

	service.set_status("my.service", health_status::not_serving);
	EXPECT_EQ(service.get_status("my.service"), health_status::not_serving);
}

TEST(HealthServiceTest, SetOverallStatus)
{
	health_service service;

	service.set_status("", health_status::serving);
	EXPECT_EQ(service.get_status(""), health_status::serving);
}

TEST(HealthServiceTest, ClearStatuses)
{
	health_service service;

	service.set_status("my.service", health_status::serving);
	service.clear();

	EXPECT_EQ(service.get_status("my.service"), health_status::service_unknown);
}

TEST(HealthServiceTest, MoveConstruct)
{
	health_service original;
	original.set_status("svc", health_status::serving);

	health_service moved(std::move(original));
	EXPECT_EQ(moved.get_status("svc"), health_status::serving);
}

// ============================================================================
// health_status Enum Tests
// ============================================================================

TEST(HealthStatusEnumTest, Values)
{
	EXPECT_NE(health_status::unknown, health_status::serving);
	EXPECT_NE(health_status::serving, health_status::not_serving);
	EXPECT_NE(health_status::not_serving, health_status::service_unknown);
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST(ParseMethodPathTest, ValidPath)
{
	auto result = parse_method_path("/helloworld.Greeter/SayHello");

	ASSERT_TRUE(result.has_value());
	EXPECT_EQ(result->first, "helloworld.Greeter");
	EXPECT_EQ(result->second, "SayHello");
}

TEST(ParseMethodPathTest, EmptyPath)
{
	auto result = parse_method_path("");

	EXPECT_FALSE(result.has_value());
}

TEST(ParseMethodPathTest, MissingLeadingSlash)
{
	auto result = parse_method_path("helloworld.Greeter/SayHello");

	EXPECT_FALSE(result.has_value());
}

TEST(ParseMethodPathTest, NoMethodSeparator)
{
	auto result = parse_method_path("/helloworld.Greeter");

	EXPECT_FALSE(result.has_value());
}

TEST(ParseMethodPathTest, EmptyService)
{
	auto result = parse_method_path("//Method");

	EXPECT_FALSE(result.has_value());
}

TEST(ParseMethodPathTest, EmptyMethod)
{
	auto result = parse_method_path("/Service/");

	EXPECT_FALSE(result.has_value());
}

TEST(BuildMethodPathTest, BasicPath)
{
	auto path = build_method_path("helloworld.Greeter", "SayHello");

	EXPECT_EQ(path, "/helloworld.Greeter/SayHello");
}

TEST(BuildMethodPathTest, SimpleServiceName)
{
	auto path = build_method_path("Echo", "Ping");

	EXPECT_EQ(path, "/Echo/Ping");
}
