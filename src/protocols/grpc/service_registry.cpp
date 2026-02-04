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

/*!
 * \file service_registry.cpp
 * \brief gRPC service registration mechanism implementation
 *
 * This file implements the service registration interfaces for both
 * protoc-generated and dynamically registered gRPC services.
 */

#include "kcenon/network/detail/protocols/grpc/service_registry.h"
#include "kcenon/network/detail/protocols/grpc/server.h"

#include <algorithm>
#include <mutex>
#include <shared_mutex>

#if NETWORK_GRPC_OFFICIAL
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/health_check_service_interface.h>
#endif

namespace kcenon::network::protocols::grpc
{

// ============================================================================
// Utility Functions Implementation
// ============================================================================

auto parse_method_path(std::string_view full_path)
    -> std::optional<std::pair<std::string, std::string>>
{
    // Expected format: "/package.Service/Method"
    if (full_path.empty() || full_path[0] != '/')
    {
        return std::nullopt;
    }

    // Remove leading slash
    auto path = full_path.substr(1);

    // Find the separator between service and method
    auto pos = path.rfind('/');
    if (pos == std::string_view::npos || pos == 0 || pos == path.size() - 1)
    {
        return std::nullopt;
    }

    std::string service_name(path.substr(0, pos));
    std::string method_name(path.substr(pos + 1));

    return std::make_pair(std::move(service_name), std::move(method_name));
}

auto build_method_path(std::string_view service_name,
                       std::string_view method_name) -> std::string
{
    std::string result;
    result.reserve(1 + service_name.size() + 1 + method_name.size());
    result += '/';
    result += service_name;
    result += '/';
    result += method_name;
    return result;
}

// ============================================================================
// generic_service Implementation
// ============================================================================

class generic_service::impl
{
public:
    explicit impl(std::string service_name)
    {
        // Parse package and service name
        auto dot_pos = service_name.rfind('.');
        if (dot_pos != std::string::npos)
        {
            descriptor_.package = service_name.substr(0, dot_pos);
            descriptor_.name = service_name.substr(dot_pos + 1);
        }
        else
        {
            descriptor_.name = std::move(service_name);
        }
    }

    auto descriptor() const -> const service_descriptor&
    {
        return descriptor_;
    }

    auto register_unary_method(
        const std::string& method_name,
        unary_handler handler,
        const std::string& input_type,
        const std::string& output_type) -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (unary_handlers_.find(method_name) != unary_handlers_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered: " + method_name,
                "generic_service");
        }

        // Create method descriptor
        method_descriptor method;
        method.name = method_name;
        method.full_name = build_method_path(descriptor_.full_name(), method_name);
        method.type = method_type::unary;
        method.input_type = input_type;
        method.output_type = output_type;

        descriptor_.methods.push_back(std::move(method));
        unary_handlers_[method_name] = std::move(handler);

        return ok();
    }

    auto register_server_streaming_method(
        const std::string& method_name,
        server_streaming_handler handler,
        const std::string& input_type,
        const std::string& output_type) -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (server_streaming_handlers_.find(method_name) != server_streaming_handlers_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered: " + method_name,
                "generic_service");
        }

        method_descriptor method;
        method.name = method_name;
        method.full_name = build_method_path(descriptor_.full_name(), method_name);
        method.type = method_type::server_streaming;
        method.input_type = input_type;
        method.output_type = output_type;

        descriptor_.methods.push_back(std::move(method));
        server_streaming_handlers_[method_name] = std::move(handler);

        return ok();
    }

    auto register_client_streaming_method(
        const std::string& method_name,
        client_streaming_handler handler,
        const std::string& input_type,
        const std::string& output_type) -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (client_streaming_handlers_.find(method_name) != client_streaming_handlers_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered: " + method_name,
                "generic_service");
        }

        method_descriptor method;
        method.name = method_name;
        method.full_name = build_method_path(descriptor_.full_name(), method_name);
        method.type = method_type::client_streaming;
        method.input_type = input_type;
        method.output_type = output_type;

        descriptor_.methods.push_back(std::move(method));
        client_streaming_handlers_[method_name] = std::move(handler);

        return ok();
    }

    auto register_bidi_streaming_method(
        const std::string& method_name,
        bidi_streaming_handler handler,
        const std::string& input_type,
        const std::string& output_type) -> VoidResult
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (bidi_streaming_handlers_.find(method_name) != bidi_streaming_handlers_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Method already registered: " + method_name,
                "generic_service");
        }

        method_descriptor method;
        method.name = method_name;
        method.full_name = build_method_path(descriptor_.full_name(), method_name);
        method.type = method_type::bidi_streaming;
        method.input_type = input_type;
        method.output_type = output_type;

        descriptor_.methods.push_back(std::move(method));
        bidi_streaming_handlers_[method_name] = std::move(handler);

        return ok();
    }

    auto get_unary_handler(const std::string& method_name) const
        -> const unary_handler*
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = unary_handlers_.find(method_name);
        return it != unary_handlers_.end() ? &it->second : nullptr;
    }

    auto get_server_streaming_handler(const std::string& method_name) const
        -> const server_streaming_handler*
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = server_streaming_handlers_.find(method_name);
        return it != server_streaming_handlers_.end() ? &it->second : nullptr;
    }

    auto get_client_streaming_handler(const std::string& method_name) const
        -> const client_streaming_handler*
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = client_streaming_handlers_.find(method_name);
        return it != client_streaming_handlers_.end() ? &it->second : nullptr;
    }

    auto get_bidi_streaming_handler(const std::string& method_name) const
        -> const bidi_streaming_handler*
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = bidi_streaming_handlers_.find(method_name);
        return it != bidi_streaming_handlers_.end() ? &it->second : nullptr;
    }

private:
    service_descriptor descriptor_;
    mutable std::mutex mutex_;

    std::unordered_map<std::string, unary_handler> unary_handlers_;
    std::unordered_map<std::string, server_streaming_handler> server_streaming_handlers_;
    std::unordered_map<std::string, client_streaming_handler> client_streaming_handlers_;
    std::unordered_map<std::string, bidi_streaming_handler> bidi_streaming_handlers_;
};

generic_service::generic_service(std::string service_name)
    : impl_(std::make_unique<impl>(std::move(service_name)))
{
}

generic_service::~generic_service() = default;

generic_service::generic_service(generic_service&&) noexcept = default;
generic_service& generic_service::operator=(generic_service&&) noexcept = default;

auto generic_service::descriptor() const -> const service_descriptor&
{
    return impl_->descriptor();
}

auto generic_service::register_unary_method(
    const std::string& method_name,
    unary_handler handler,
    const std::string& input_type,
    const std::string& output_type) -> VoidResult
{
    return impl_->register_unary_method(method_name, std::move(handler),
                                        input_type, output_type);
}

auto generic_service::register_server_streaming_method(
    const std::string& method_name,
    server_streaming_handler handler,
    const std::string& input_type,
    const std::string& output_type) -> VoidResult
{
    return impl_->register_server_streaming_method(method_name, std::move(handler),
                                                   input_type, output_type);
}

auto generic_service::register_client_streaming_method(
    const std::string& method_name,
    client_streaming_handler handler,
    const std::string& input_type,
    const std::string& output_type) -> VoidResult
{
    return impl_->register_client_streaming_method(method_name, std::move(handler),
                                                   input_type, output_type);
}

auto generic_service::register_bidi_streaming_method(
    const std::string& method_name,
    bidi_streaming_handler handler,
    const std::string& input_type,
    const std::string& output_type) -> VoidResult
{
    return impl_->register_bidi_streaming_method(method_name, std::move(handler),
                                                 input_type, output_type);
}

auto generic_service::get_unary_handler(const std::string& method_name) const
    -> const unary_handler*
{
    return impl_->get_unary_handler(method_name);
}

auto generic_service::get_server_streaming_handler(const std::string& method_name) const
    -> const server_streaming_handler*
{
    return impl_->get_server_streaming_handler(method_name);
}

auto generic_service::get_client_streaming_handler(const std::string& method_name) const
    -> const client_streaming_handler*
{
    return impl_->get_client_streaming_handler(method_name);
}

auto generic_service::get_bidi_streaming_handler(const std::string& method_name) const
    -> const bidi_streaming_handler*
{
    return impl_->get_bidi_streaming_handler(method_name);
}

#if NETWORK_GRPC_OFFICIAL
auto generic_service::grpc_service() -> ::grpc::Service*
{
    // Generic service doesn't have a native gRPC service
    // It uses the async generic service API
    return nullptr;
}
#endif

// ============================================================================
// protoc_service_adapter Implementation
// ============================================================================

#if NETWORK_GRPC_OFFICIAL

class protoc_service_adapter::impl
{
public:
    impl(std::unique_ptr<::grpc::Service> service, std::string service_name)
        : service_(std::move(service))
        , owns_service_(true)
    {
        init_descriptor(std::move(service_name));
    }

    impl(::grpc::Service* service, std::string service_name)
        : service_(std::unique_ptr<::grpc::Service>(service))
        , owns_service_(true)
    {
        init_descriptor(std::move(service_name));
    }

    ~impl()
    {
        if (!owns_service_)
        {
            // Release without deleting if we don't own it
            service_.release();
        }
    }

    auto descriptor() const -> const service_descriptor&
    {
        return descriptor_;
    }

    auto grpc_service() -> ::grpc::Service*
    {
        return service_.get();
    }

private:
    void init_descriptor(std::string service_name)
    {
        // Parse package and service name
        auto dot_pos = service_name.rfind('.');
        if (dot_pos != std::string::npos)
        {
            descriptor_.package = service_name.substr(0, dot_pos);
            descriptor_.name = service_name.substr(dot_pos + 1);
        }
        else
        {
            descriptor_.name = std::move(service_name);
        }

        // Note: Method descriptors would be populated from protobuf reflection
        // if available. For now, we leave them empty as the protoc-generated
        // service handles its own method routing.
    }

    std::unique_ptr<::grpc::Service> service_;
    bool owns_service_;
    service_descriptor descriptor_;
};

protoc_service_adapter::protoc_service_adapter(
    std::unique_ptr<::grpc::Service> service,
    std::string service_name)
    : impl_(std::make_unique<impl>(std::move(service), std::move(service_name)))
{
}

protoc_service_adapter::protoc_service_adapter(
    ::grpc::Service* service,
    std::string service_name)
    : impl_(std::make_unique<impl>(service, std::move(service_name)))
{
}

protoc_service_adapter::~protoc_service_adapter() = default;

protoc_service_adapter::protoc_service_adapter(protoc_service_adapter&&) noexcept = default;
protoc_service_adapter& protoc_service_adapter::operator=(protoc_service_adapter&&) noexcept = default;

auto protoc_service_adapter::descriptor() const -> const service_descriptor&
{
    return impl_->descriptor();
}

auto protoc_service_adapter::grpc_service() -> ::grpc::Service*
{
    return impl_->grpc_service();
}

#endif // NETWORK_GRPC_OFFICIAL

// ============================================================================
// service_registry Implementation
// ============================================================================

class service_registry::impl
{
public:
    explicit impl(const registry_config& config)
        : config_(config)
    {
    }

    auto register_service(service_base* service) -> VoidResult
    {
        if (service == nullptr)
        {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Service cannot be null",
                "service_registry");
        }

        std::lock_guard<std::shared_mutex> lock(mutex_);

        const auto& desc = service->descriptor();
        auto full_name = desc.full_name();

        if (services_.find(full_name) != services_.end())
        {
            return error_void(
                error_codes::common_errors::already_exists,
                "Service already registered: " + full_name,
                "service_registry");
        }

        services_[full_name] = service;

        // Set initial health status
        if (config_.enable_health_check)
        {
            health_status_[full_name] = true;
        }

        return ok();
    }

    auto unregister_service(const std::string& service_name) -> VoidResult
    {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        auto it = services_.find(service_name);
        if (it == services_.end())
        {
            return error_void(
                error_codes::common_errors::not_found,
                "Service not found: " + service_name,
                "service_registry");
        }

        services_.erase(it);
        health_status_.erase(service_name);

        return ok();
    }

    auto find_service(const std::string& service_name) const -> service_base*
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = services_.find(service_name);
        return it != services_.end() ? it->second : nullptr;
    }

    auto services() const -> std::vector<service_base*>
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::vector<service_base*> result;
        result.reserve(services_.size());
        for (const auto& [name, service] : services_)
        {
            result.push_back(service);
        }
        return result;
    }

    auto service_names() const -> std::vector<std::string>
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        std::vector<std::string> result;
        result.reserve(services_.size());
        for (const auto& [name, service] : services_)
        {
            result.push_back(name);
        }
        return result;
    }

    auto find_method(const std::string& full_method_path) const
        -> std::optional<std::pair<service_base*, const method_descriptor*>>
    {
        auto parsed = parse_method_path(full_method_path);
        if (!parsed)
        {
            return std::nullopt;
        }

        const auto& [service_name, method_name] = *parsed;

        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = services_.find(service_name);
        if (it == services_.end())
        {
            return std::nullopt;
        }

        auto* service = it->second;
        const auto* method = service->descriptor().find_method(method_name);
        if (method == nullptr)
        {
            return std::nullopt;
        }

        return std::make_pair(service, method);
    }

    auto is_reflection_enabled() const -> bool
    {
        return config_.enable_reflection;
    }

    auto configure_server(grpc_server& server) -> VoidResult
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        for (const auto& [name, service] : services_)
        {
            // Check if service is a generic_service with handlers
            auto* generic = dynamic_cast<generic_service*>(service);
            if (generic != nullptr)
            {
                const auto& desc = generic->descriptor();
                for (const auto& method : desc.methods)
                {
                    VoidResult result = ok();
                    switch (method.type)
                    {
                        case method_type::unary:
                        {
                            auto* handler = generic->get_unary_handler(method.name);
                            if (handler != nullptr)
                            {
                                result = server.register_unary_method(
                                    method.full_name, *handler);
                            }
                            break;
                        }
                        case method_type::server_streaming:
                        {
                            auto* handler = generic->get_server_streaming_handler(method.name);
                            if (handler != nullptr)
                            {
                                result = server.register_server_streaming_method(
                                    method.full_name, *handler);
                            }
                            break;
                        }
                        case method_type::client_streaming:
                        {
                            auto* handler = generic->get_client_streaming_handler(method.name);
                            if (handler != nullptr)
                            {
                                result = server.register_client_streaming_method(
                                    method.full_name, *handler);
                            }
                            break;
                        }
                        case method_type::bidi_streaming:
                        {
                            auto* handler = generic->get_bidi_streaming_handler(method.name);
                            if (handler != nullptr)
                            {
                                result = server.register_bidi_streaming_method(
                                    method.full_name, *handler);
                            }
                            break;
                        }
                    }

                    if (result.is_err())
                    {
                        return result;
                    }
                }
            }
            else
            {
                // For non-generic services, use register_service
                auto result = server.register_service(
                    dynamic_cast<grpc_service*>(service));
                if (result.is_err())
                {
                    return result;
                }
            }
        }

        return ok();
    }

#if NETWORK_GRPC_OFFICIAL
    auto configure_server_builder(::grpc::ServerBuilder& builder) -> VoidResult
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        for (const auto& [name, service] : services_)
        {
            auto* grpc_svc = service->grpc_service();
            if (grpc_svc != nullptr)
            {
                builder.RegisterService(grpc_svc);
            }
        }

        if (config_.enable_reflection)
        {
            enable_reflection(builder);
        }

        return ok();
    }

    auto enable_reflection(::grpc::ServerBuilder& builder) -> VoidResult
    {
        ::grpc::reflection::InitProtoReflectionServerBuilderPlugin();
        return ok();
    }
#endif

    auto set_service_health(const std::string& service_name, bool serving)
        -> VoidResult
    {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        if (!service_name.empty() &&
            services_.find(service_name) == services_.end())
        {
            return error_void(
                error_codes::common_errors::not_found,
                "Service not found: " + service_name,
                "service_registry");
        }

        health_status_[service_name] = serving;
        return ok();
    }

    auto get_service_health(const std::string& service_name) const -> bool
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);

        auto it = health_status_.find(service_name);
        return it != health_status_.end() ? it->second : false;
    }

private:
    registry_config config_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, service_base*> services_;
    std::unordered_map<std::string, bool> health_status_;
};

service_registry::service_registry(const registry_config& config)
    : impl_(std::make_unique<impl>(config))
{
}

service_registry::~service_registry() = default;

service_registry::service_registry(service_registry&&) noexcept = default;
service_registry& service_registry::operator=(service_registry&&) noexcept = default;

auto service_registry::register_service(service_base* service) -> VoidResult
{
    return impl_->register_service(service);
}

auto service_registry::unregister_service(const std::string& service_name) -> VoidResult
{
    return impl_->unregister_service(service_name);
}

auto service_registry::find_service(const std::string& service_name) const
    -> service_base*
{
    return impl_->find_service(service_name);
}

auto service_registry::services() const -> std::vector<service_base*>
{
    return impl_->services();
}

auto service_registry::service_names() const -> std::vector<std::string>
{
    return impl_->service_names();
}

auto service_registry::find_method(const std::string& full_method_path) const
    -> std::optional<std::pair<service_base*, const method_descriptor*>>
{
    return impl_->find_method(full_method_path);
}

auto service_registry::is_reflection_enabled() const -> bool
{
    return impl_->is_reflection_enabled();
}

auto service_registry::configure_server(grpc_server& server) -> VoidResult
{
    return impl_->configure_server(server);
}

#if NETWORK_GRPC_OFFICIAL
auto service_registry::configure_server_builder(::grpc::ServerBuilder& builder)
    -> VoidResult
{
    return impl_->configure_server_builder(builder);
}

auto service_registry::enable_reflection(::grpc::ServerBuilder& builder)
    -> VoidResult
{
    return impl_->enable_reflection(builder);
}
#endif

auto service_registry::set_service_health(const std::string& service_name,
                                          bool serving) -> VoidResult
{
    return impl_->set_service_health(service_name, serving);
}

auto service_registry::get_service_health(const std::string& service_name) const
    -> bool
{
    return impl_->get_service_health(service_name);
}

// ============================================================================
// health_service Implementation
// ============================================================================

class health_service::impl
{
public:
    impl()
    {
        descriptor_.name = "Health";
        descriptor_.package = "grpc.health.v1";

        method_descriptor check_method;
        check_method.name = "Check";
        check_method.full_name = "/grpc.health.v1.Health/Check";
        check_method.type = method_type::unary;
        check_method.input_type = "grpc.health.v1.HealthCheckRequest";
        check_method.output_type = "grpc.health.v1.HealthCheckResponse";
        descriptor_.methods.push_back(std::move(check_method));

        method_descriptor watch_method;
        watch_method.name = "Watch";
        watch_method.full_name = "/grpc.health.v1.Health/Watch";
        watch_method.type = method_type::server_streaming;
        watch_method.input_type = "grpc.health.v1.HealthCheckRequest";
        watch_method.output_type = "grpc.health.v1.HealthCheckResponse";
        descriptor_.methods.push_back(std::move(watch_method));
    }

    auto descriptor() const -> const service_descriptor&
    {
        return descriptor_;
    }

    auto set_status(const std::string& service_name, health_status status) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_map_[service_name] = status;
    }

    auto get_status(const std::string& service_name) const -> health_status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = status_map_.find(service_name);
        if (it == status_map_.end())
        {
            return health_status::service_unknown;
        }
        return it->second;
    }

    auto clear() -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_map_.clear();
    }

private:
    service_descriptor descriptor_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, health_status> status_map_;
};

health_service::health_service()
    : impl_(std::make_unique<impl>())
{
}

health_service::~health_service() = default;

health_service::health_service(health_service&&) noexcept = default;
health_service& health_service::operator=(health_service&&) noexcept = default;

auto health_service::descriptor() const -> const service_descriptor&
{
    return impl_->descriptor();
}

auto health_service::set_status(const std::string& service_name,
                                health_status status) -> void
{
    impl_->set_status(service_name, status);
}

auto health_service::get_status(const std::string& service_name) const
    -> health_status
{
    return impl_->get_status(service_name);
}

auto health_service::clear() -> void
{
    impl_->clear();
}

#if NETWORK_GRPC_OFFICIAL
auto health_service::grpc_service() -> ::grpc::Service*
{
    // Health service uses the standard gRPC health check implementation
    // when available. For now, return nullptr as we handle it via generic.
    return nullptr;
}
#endif

} // namespace kcenon::network::protocols::grpc
