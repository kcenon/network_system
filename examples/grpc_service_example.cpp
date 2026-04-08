/*****************************************************************************
BSD 3-Clause License

Copyright (c) 2024, 🍀☀🌕🌥 🌊
All rights reserved.
*****************************************************************************/

/**
 * @file grpc_service_example.cpp
 * @brief Demonstrates gRPC server and client using the facade API.
 * @example grpc_service_example.cpp
 *
 * @par Category
 * Protocol - gRPC service
 *
 * Demonstrates:
 * - grpc_server with unary method registration
 * - grpc_client connection and raw calls
 * - Server configuration and lifecycle
 *
 * @see kcenon::network::protocols::grpc::grpc_server
 * @see kcenon::network::protocols::grpc::grpc_client
 */

#include <kcenon/network/protocols/grpc/grpc.h>

#include <iostream>
#include <string>

using namespace kcenon::network;

int main()
{
	std::cout << "=== gRPC Service Example ===" << std::endl;

	// 1. Server configuration
	std::cout << "\n1. gRPC Server configuration:" << std::endl;
	protocols::grpc::grpc_server_config config;
	config.max_concurrent_streams = 100;
	config.num_threads = 4;
	config.keepalive_time = std::chrono::milliseconds(7200000);
	config.keepalive_timeout = std::chrono::milliseconds(20000);

	std::cout << "   Max concurrent streams: " << config.max_concurrent_streams << std::endl;
	std::cout << "   Worker threads: " << config.num_threads << std::endl;
	std::cout << "   Keepalive: " << config.keepalive_time.count() << "ms" << std::endl;

	// 2. Create server and register methods
	std::cout << "\n2. Creating gRPC server:" << std::endl;
	protocols::grpc::grpc_server server(config);

	auto reg_result = server.register_unary_method(
		"/example.Greeter/SayHello",
		[](const protocols::grpc::grpc_message& request,
		   protocols::grpc::server_context& ctx) -> kcenon::common::Result<protocols::grpc::grpc_message>
		{
			std::cout << "   [Handler] Received request (" << request.data.size()
					  << " bytes)" << std::endl;

			protocols::grpc::grpc_message response;
			std::string body = "Hello from gRPC server!";
			response.data.assign(body.begin(), body.end());
			return kcenon::common::ok(std::move(response));
		});

	if (reg_result.is_ok())
	{
		std::cout << "   Method '/example.Greeter/SayHello' registered" << std::endl;
	}
	else
	{
		std::cout << "   Method registration: " << reg_result.error().message << std::endl;
	}

	// 3. Client configuration
	std::cout << "\n3. gRPC Client:" << std::endl;
	std::cout << "   Target: localhost:50051" << std::endl;
	std::cout << "   (Connection requires a running server)" << std::endl;

	// 4. API overview
	std::cout << "\n4. gRPC API overview:" << std::endl;
	std::cout << "   Server: register_unary_method(), start(port), stop()" << std::endl;
	std::cout << "   Client: connect(), call_raw(method, request, options)" << std::endl;
	std::cout << "   Message: grpc_message { data: vector<byte> }" << std::endl;

	std::cout << "\nDone." << std::endl;
	return 0;
}
