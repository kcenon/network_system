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

#pragma once

/*!
 * \file service_registry.h
 * \brief gRPC service registration mechanism
 *
 * This file provides interfaces for registering gRPC services, supporting both
 * protoc code-generated services and dynamically registered services.
 * It includes reflection support for debugging and service discovery.
 *
 * \see docs/adr/ADR-001-grpc-official-library-wrapper.md
 */

#include "kcenon/network/detail/protocols/grpc/server.h"
#include "kcenon/network/detail/protocols/grpc/status.h"
#include "kcenon/network/detail/utils/result_types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Forward declarations for gRPC types
#if NETWORK_GRPC_OFFICIAL
namespace grpc {
class Service;
class Server;
class ServerBuilder;
class ServerContext;
class ByteBuffer;
namespace reflection {
namespace v1alpha {
class ServerReflectionRequest;
class ServerReflectionResponse;
}
}
}
#endif

namespace kcenon::network::protocols::grpc
{

// ============================================================================
// Method Types and Descriptors
// ============================================================================

/*!
 * \enum method_type
 * \brief Type of RPC method
 */
enum class method_type
{
    unary,              //!< Unary RPC (single request, single response)
    server_streaming,   //!< Server streaming (single request, multiple responses)
    client_streaming,   //!< Client streaming (multiple requests, single response)
    bidi_streaming      //!< Bidirectional streaming (multiple requests and responses)
};

/*!
 * \struct method_descriptor
 * \brief Describes a single RPC method within a service
 */
struct method_descriptor
{
    //! Method name (without service prefix)
    std::string name;

    //! Full method path (e.g., "/package.Service/Method")
    std::string full_name;

    //! Type of RPC method
    method_type type = method_type::unary;

    //! Input message type name (for reflection)
    std::string input_type;

    //! Output message type name (for reflection)
    std::string output_type;

    //! Whether the method requires client streaming
    auto is_client_streaming() const -> bool
    {
        return type == method_type::client_streaming ||
               type == method_type::bidi_streaming;
    }

    //! Whether the method provides server streaming
    auto is_server_streaming() const -> bool
    {
        return type == method_type::server_streaming ||
               type == method_type::bidi_streaming;
    }
};

/*!
 * \struct service_descriptor
 * \brief Describes a gRPC service and its methods
 */
struct service_descriptor
{
    //! Service name (e.g., "helloworld.Greeter")
    std::string name;

    //! Package name (e.g., "helloworld")
    std::string package;

    //! List of methods in this service
    std::vector<method_descriptor> methods;

    /*!
     * \brief Find a method by name
     * \param method_name Method name to find
     * \return Pointer to method descriptor or nullptr
     */
    auto find_method(std::string_view method_name) const
        -> const method_descriptor*
    {
        for (const auto& method : methods)
        {
            if (method.name == method_name)
            {
                return &method;
            }
        }
        return nullptr;
    }

    /*!
     * \brief Get full service name including package
     * \return Full service name
     */
    auto full_name() const -> std::string
    {
        if (package.empty())
        {
            return name;
        }
        return package + "." + name;
    }
};

// ============================================================================
// Service Interface
// ============================================================================

/*!
 * \class service_base
 * \brief Base class for all gRPC service implementations
 *
 * This class provides the common interface for gRPC services, whether they
 * are dynamically registered or generated by protoc.
 */
class service_base
{
public:
    virtual ~service_base() = default;

    /*!
     * \brief Get the service descriptor
     * \return Reference to service descriptor
     */
    virtual auto descriptor() const -> const service_descriptor& = 0;

    /*!
     * \brief Check if this service is ready to handle requests
     * \return True if service is ready
     */
    virtual auto is_ready() const -> bool { return true; }

#if NETWORK_GRPC_OFFICIAL
    /*!
     * \brief Get the underlying gRPC service (for official gRPC)
     * \return Pointer to gRPC service or nullptr if not applicable
     */
    virtual auto grpc_service() -> ::grpc::Service* { return nullptr; }
#endif
};

// ============================================================================
// Generic Service (Dynamic Registration)
// ============================================================================

/*!
 * \class generic_service
 * \brief A service that allows dynamic method registration
 *
 * Use this class when you need to register methods at runtime without
 * protobuf definitions. Supports all RPC types.
 *
 * Example:
 * \code
 * generic_service service("mypackage.MyService");
 *
 * service.register_unary_method("Echo",
 *     [](server_context& ctx, const std::vector<uint8_t>& request)
 *         -> std::pair<grpc_status, std::vector<uint8_t>> {
 *         return {grpc_status::ok_status(), request};
 *     });
 *
 * registry.register_service(&service);
 * \endcode
 */
class generic_service : public service_base
{
public:
    /*!
     * \brief Construct a generic service
     * \param service_name Full service name (e.g., "package.ServiceName")
     */
    explicit generic_service(std::string service_name);

    ~generic_service() override;

    // Non-copyable
    generic_service(const generic_service&) = delete;
    generic_service& operator=(const generic_service&) = delete;

    // Movable
    generic_service(generic_service&&) noexcept;
    generic_service& operator=(generic_service&&) noexcept;

    /*!
     * \brief Get the service descriptor
     * \return Reference to service descriptor
     */
    auto descriptor() const -> const service_descriptor& override;

    /*!
     * \brief Register a unary method handler
     * \param method_name Method name (without service prefix)
     * \param handler Handler function
     * \param input_type Optional input type name for reflection
     * \param output_type Optional output type name for reflection
     * \return Success or error
     */
    auto register_unary_method(
        const std::string& method_name,
        unary_handler handler,
        const std::string& input_type = "",
        const std::string& output_type = "") -> VoidResult;

    /*!
     * \brief Register a server streaming method handler
     * \param method_name Method name
     * \param handler Handler function
     * \param input_type Optional input type name for reflection
     * \param output_type Optional output type name for reflection
     * \return Success or error
     */
    auto register_server_streaming_method(
        const std::string& method_name,
        server_streaming_handler handler,
        const std::string& input_type = "",
        const std::string& output_type = "") -> VoidResult;

    /*!
     * \brief Register a client streaming method handler
     * \param method_name Method name
     * \param handler Handler function
     * \param input_type Optional input type name for reflection
     * \param output_type Optional output type name for reflection
     * \return Success or error
     */
    auto register_client_streaming_method(
        const std::string& method_name,
        client_streaming_handler handler,
        const std::string& input_type = "",
        const std::string& output_type = "") -> VoidResult;

    /*!
     * \brief Register a bidirectional streaming method handler
     * \param method_name Method name
     * \param handler Handler function
     * \param input_type Optional input type name for reflection
     * \param output_type Optional output type name for reflection
     * \return Success or error
     */
    auto register_bidi_streaming_method(
        const std::string& method_name,
        bidi_streaming_handler handler,
        const std::string& input_type = "",
        const std::string& output_type = "") -> VoidResult;

    /*!
     * \brief Get unary handler for a method
     * \param method_name Method name
     * \return Handler or nullptr
     */
    auto get_unary_handler(const std::string& method_name) const
        -> const unary_handler*;

    /*!
     * \brief Get server streaming handler for a method
     * \param method_name Method name
     * \return Handler or nullptr
     */
    auto get_server_streaming_handler(const std::string& method_name) const
        -> const server_streaming_handler*;

    /*!
     * \brief Get client streaming handler for a method
     * \param method_name Method name
     * \return Handler or nullptr
     */
    auto get_client_streaming_handler(const std::string& method_name) const
        -> const client_streaming_handler*;

    /*!
     * \brief Get bidirectional streaming handler for a method
     * \param method_name Method name
     * \return Handler or nullptr
     */
    auto get_bidi_streaming_handler(const std::string& method_name) const
        -> const bidi_streaming_handler*;

#if NETWORK_GRPC_OFFICIAL
    auto grpc_service() -> ::grpc::Service* override;
#endif

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

// ============================================================================
// Protoc Service Adapter
// ============================================================================

#if NETWORK_GRPC_OFFICIAL

/*!
 * \class protoc_service_adapter
 * \brief Adapter for protoc-generated gRPC services
 *
 * This class wraps a protoc-generated service to work with the
 * network_system service registry. It automatically generates
 * the service descriptor from the protobuf reflection data.
 *
 * Example:
 * \code
 * // Create your protoc-generated service
 * auto my_service = std::make_unique<helloworld::Greeter::AsyncService>();
 *
 * // Wrap it in the adapter
 * protoc_service_adapter adapter(std::move(my_service), "helloworld.Greeter");
 *
 * // Register with the registry
 * registry.register_service(&adapter);
 * \endcode
 */
class protoc_service_adapter : public service_base
{
public:
    /*!
     * \brief Construct adapter from protoc-generated service
     * \param service Protoc-generated service
     * \param service_name Full service name
     */
    protoc_service_adapter(std::unique_ptr<::grpc::Service> service,
                           std::string service_name);

    /*!
     * \brief Construct adapter taking ownership of raw service pointer
     * \param service Raw service pointer (takes ownership)
     * \param service_name Full service name
     */
    protoc_service_adapter(::grpc::Service* service,
                           std::string service_name);

    ~protoc_service_adapter() override;

    // Non-copyable
    protoc_service_adapter(const protoc_service_adapter&) = delete;
    protoc_service_adapter& operator=(const protoc_service_adapter&) = delete;

    // Movable
    protoc_service_adapter(protoc_service_adapter&&) noexcept;
    protoc_service_adapter& operator=(protoc_service_adapter&&) noexcept;

    auto descriptor() const -> const service_descriptor& override;

    auto grpc_service() -> ::grpc::Service* override;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

#endif // NETWORK_GRPC_OFFICIAL

// ============================================================================
// Service Registry
// ============================================================================

/*!
 * \struct registry_config
 * \brief Configuration for service registry
 */
struct registry_config
{
    //! Enable reflection service for debugging
    bool enable_reflection = false;

    //! Enable health checking service
    bool enable_health_check = false;

    //! Health check service name (default: "grpc.health.v1.Health")
    std::string health_service_name = "grpc.health.v1.Health";
};

/*!
 * \class service_registry
 * \brief Central registry for managing gRPC services
 *
 * The service registry manages all registered services and provides:
 * - Service registration and lookup
 * - Method routing
 * - Reflection support for debugging tools
 * - Health checking integration
 *
 * Example:
 * \code
 * service_registry registry({.enable_reflection = true});
 *
 * // Register services
 * generic_service echo_service("echo.EchoService");
 * echo_service.register_unary_method("Echo", echo_handler);
 * registry.register_service(&echo_service);
 *
 * // Use with gRPC server
 * grpc_server server;
 * registry.configure_server(server);
 * server.start(50051);
 * \endcode
 */
class service_registry
{
public:
    /*!
     * \brief Construct service registry
     * \param config Registry configuration
     */
    explicit service_registry(const registry_config& config = {});

    ~service_registry();

    // Non-copyable
    service_registry(const service_registry&) = delete;
    service_registry& operator=(const service_registry&) = delete;

    // Movable
    service_registry(service_registry&&) noexcept;
    service_registry& operator=(service_registry&&) noexcept;

    /*!
     * \brief Register a service
     * \param service Service to register (registry does not take ownership)
     * \return Success or error
     */
    auto register_service(service_base* service) -> VoidResult;

    /*!
     * \brief Unregister a service
     * \param service_name Full service name
     * \return Success or error
     */
    auto unregister_service(const std::string& service_name) -> VoidResult;

    /*!
     * \brief Find a service by name
     * \param service_name Full service name
     * \return Pointer to service or nullptr
     */
    auto find_service(const std::string& service_name) const -> service_base*;

    /*!
     * \brief Get all registered services
     * \return List of registered services
     */
    auto services() const -> std::vector<service_base*>;

    /*!
     * \brief Get list of all service names
     * \return List of service names
     */
    auto service_names() const -> std::vector<std::string>;

    /*!
     * \brief Find a method by full path
     * \param full_method_path Full method path (e.g., "/package.Service/Method")
     * \return Pair of service and method descriptor, or nullopt
     */
    auto find_method(const std::string& full_method_path) const
        -> std::optional<std::pair<service_base*, const method_descriptor*>>;

    /*!
     * \brief Check if reflection is enabled
     * \return True if reflection is enabled
     */
    auto is_reflection_enabled() const -> bool;

    /*!
     * \brief Configure a gRPC server with registered services
     * \param server Server to configure
     * \return Success or error
     */
    auto configure_server(grpc_server& server) -> VoidResult;

#if NETWORK_GRPC_OFFICIAL
    /*!
     * \brief Configure a gRPC server builder with registered services
     * \param builder Server builder to configure
     * \return Success or error
     */
    auto configure_server_builder(::grpc::ServerBuilder& builder) -> VoidResult;

    /*!
     * \brief Enable gRPC reflection plugin
     * \param builder Server builder
     * \return Success or error
     */
    auto enable_reflection(::grpc::ServerBuilder& builder) -> VoidResult;
#endif

    /*!
     * \brief Set health status for a service
     * \param service_name Service name (empty for server-wide status)
     * \param serving True if the service is serving
     * \return Success or error
     */
    auto set_service_health(const std::string& service_name, bool serving)
        -> VoidResult;

    /*!
     * \brief Get health status for a service
     * \param service_name Service name (empty for server-wide status)
     * \return True if serving, false otherwise
     */
    auto get_service_health(const std::string& service_name) const -> bool;

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

// ============================================================================
// Health Checking Support
// ============================================================================

/*!
 * \enum health_status
 * \brief Health status for a service
 */
enum class health_status
{
    unknown,        //!< Status unknown
    serving,        //!< Service is serving
    not_serving,    //!< Service is not serving
    service_unknown //!< Service is not registered
};

/*!
 * \class health_service
 * \brief Implementation of gRPC health checking protocol
 *
 * Implements the standard gRPC health checking protocol as defined in:
 * https://github.com/grpc/grpc/blob/master/doc/health-checking.md
 */
class health_service : public service_base
{
public:
    health_service();
    ~health_service() override;

    // Non-copyable
    health_service(const health_service&) = delete;
    health_service& operator=(const health_service&) = delete;

    // Movable
    health_service(health_service&&) noexcept;
    health_service& operator=(health_service&&) noexcept;

    auto descriptor() const -> const service_descriptor& override;

    /*!
     * \brief Set health status for a service
     * \param service_name Service name (empty string for overall status)
     * \param status Health status
     */
    auto set_status(const std::string& service_name, health_status status) -> void;

    /*!
     * \brief Get health status for a service
     * \param service_name Service name
     * \return Health status
     */
    auto get_status(const std::string& service_name) const -> health_status;

    /*!
     * \brief Clear all health statuses
     */
    auto clear() -> void;

#if NETWORK_GRPC_OFFICIAL
    auto grpc_service() -> ::grpc::Service* override;
#endif

private:
    class impl;
    std::unique_ptr<impl> impl_;
};

// ============================================================================
// Utility Functions
// ============================================================================

/*!
 * \brief Parse full method path into service and method names
 * \param full_path Full method path (e.g., "/package.Service/Method")
 * \return Pair of service name and method name, or nullopt if invalid
 */
auto parse_method_path(std::string_view full_path)
    -> std::optional<std::pair<std::string, std::string>>;

/*!
 * \brief Build full method path from service and method names
 * \param service_name Full service name (e.g., "package.Service")
 * \param method_name Method name
 * \return Full method path
 */
auto build_method_path(std::string_view service_name,
                       std::string_view method_name) -> std::string;

} // namespace kcenon::network::protocols::grpc
