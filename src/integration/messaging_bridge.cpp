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
 * @file messaging_bridge.cpp
 * @brief Implementation of messaging_bridge for messaging_system compatibility
 *
 * @author kcenon
 * @date 2025-09-19
 */

// Suppress deprecation warnings from thread_system headers
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <kcenon/network/detail/config/feature_flags.h>

#include "internal/integration/messaging_bridge.h"
#include "kcenon/network/integration/thread_integration.h"
#include <kcenon/network/detail/utils/result_types.h>
#include <atomic>
#include <mutex>

#if KCENON_WITH_CONTAINER_SYSTEM
#include "container.h"
#endif

namespace kcenon::network::integration {

class messaging_bridge::impl {
public:
    impl() : initialized_(false) {
        metrics_.start_time = std::chrono::steady_clock::now();
    }

    std::atomic<bool> initialized_{false};
    mutable std::mutex metrics_mutex_;
    performance_metrics metrics_;
    mutable BridgeMetrics bridge_metrics_;

#if KCENON_WITH_CONTAINER_SYSTEM
    std::shared_ptr<container_module::value_container> active_container_;
    std::function<void(const container_module::value_container&)> container_handler_;
#endif

#if KCENON_WITH_THREAD_SYSTEM
    std::shared_ptr<kcenon::thread::thread_pool> thread_pool_;
#endif
    std::shared_ptr<thread_pool_interface> thread_pool_interface_;
};

messaging_bridge::messaging_bridge() : pimpl_(std::make_unique<impl>()) {
    pimpl_->metrics_.start_time = std::chrono::steady_clock::now();
}

messaging_bridge::~messaging_bridge() {
    if (is_initialized()) {
        shutdown();
    }
}

// INetworkBridge interface implementation

VoidResult messaging_bridge::initialize(const BridgeConfig& config) {
    std::lock_guard<std::mutex> lock(pimpl_->metrics_mutex_);

    if (pimpl_->initialized_.load()) {
        return error_void(
            error_codes::common_errors::already_exists,
            "messaging_bridge already initialized",
            "messaging_bridge::initialize");
    }

    // Process configuration properties
    auto enabled_it = config.properties.find("enabled");
    if (enabled_it != config.properties.end() && enabled_it->second == "false") {
        return error_void(
            error_codes::common_errors::invalid_argument,
            "Bridge is disabled in configuration",
            "messaging_bridge::initialize");
    }

    // Initialize bridge metrics
    pimpl_->bridge_metrics_.is_healthy = true;
    pimpl_->bridge_metrics_.last_activity = std::chrono::steady_clock::now();
    pimpl_->bridge_metrics_.custom_metrics.clear();

    pimpl_->initialized_.store(true);
    return ok();
}

VoidResult messaging_bridge::shutdown() {
    std::lock_guard<std::mutex> lock(pimpl_->metrics_mutex_);

    if (!pimpl_->initialized_.load()) {
        return ok(); // Already shut down, idempotent
    }

    pimpl_->initialized_.store(false);
    pimpl_->bridge_metrics_.is_healthy = false;

    return ok();
}

bool messaging_bridge::is_initialized() const {
    return pimpl_->initialized_.load();
}

BridgeMetrics messaging_bridge::get_metrics() const {
    std::lock_guard<std::mutex> lock(pimpl_->metrics_mutex_);

    // Update bridge metrics from performance metrics
    pimpl_->bridge_metrics_.is_healthy = pimpl_->initialized_.load();
    pimpl_->bridge_metrics_.last_activity = std::chrono::steady_clock::now();
    pimpl_->bridge_metrics_.custom_metrics["messages_sent"] = static_cast<double>(pimpl_->metrics_.messages_sent);
    pimpl_->bridge_metrics_.custom_metrics["messages_received"] = static_cast<double>(pimpl_->metrics_.messages_received);
    pimpl_->bridge_metrics_.custom_metrics["bytes_sent"] = static_cast<double>(pimpl_->metrics_.bytes_sent);
    pimpl_->bridge_metrics_.custom_metrics["bytes_received"] = static_cast<double>(pimpl_->metrics_.bytes_received);
    pimpl_->bridge_metrics_.custom_metrics["connections_active"] = static_cast<double>(pimpl_->metrics_.connections_active);
    pimpl_->bridge_metrics_.custom_metrics["avg_latency_ms"] = static_cast<double>(pimpl_->metrics_.avg_latency.count());

    return pimpl_->bridge_metrics_;
}

// messaging_bridge-specific methods (maintained for backward compatibility)

std::shared_ptr<kcenon::network::core::messaging_server> messaging_bridge::create_server(
    const std::string& server_id
) {
    return std::make_shared<kcenon::network::core::messaging_server>(server_id);
}

std::shared_ptr<kcenon::network::core::messaging_client> messaging_bridge::create_client(
    const std::string& client_id
) {
    return std::make_shared<kcenon::network::core::messaging_client>(client_id);
}

#if KCENON_WITH_CONTAINER_SYSTEM
void messaging_bridge::set_container(
    std::shared_ptr<container_module::value_container> container
) {
    pimpl_->active_container_ = container;
}

void messaging_bridge::set_container_message_handler(
    std::function<void(const container_module::value_container&)> handler
) {
    pimpl_->container_handler_ = handler;
}
#endif

#if KCENON_WITH_THREAD_SYSTEM
void messaging_bridge::set_thread_pool(
    std::shared_ptr<kcenon::thread::thread_pool> pool
) {
    pimpl_->thread_pool_ = pool;
}
#endif

messaging_bridge::performance_metrics messaging_bridge::get_performance_metrics() const {
    std::lock_guard<std::mutex> lock(pimpl_->metrics_mutex_);
    return pimpl_->metrics_;
}

void messaging_bridge::reset_metrics() {
    std::lock_guard<std::mutex> lock(pimpl_->metrics_mutex_);
    pimpl_->metrics_ = performance_metrics{};
    pimpl_->metrics_.start_time = std::chrono::steady_clock::now();
}

void messaging_bridge::set_thread_pool_interface(
    std::shared_ptr<thread_pool_interface> pool
) {
    pimpl_->thread_pool_interface_ = pool;
}

std::shared_ptr<thread_pool_interface> messaging_bridge::get_thread_pool_interface() const {
    if (!pimpl_->thread_pool_interface_) {
        // Return the global thread integration manager's pool
        return thread_integration_manager::instance().get_thread_pool();
    }
    return pimpl_->thread_pool_interface_;
}

} // namespace kcenon::network::integration

#pragma clang diagnostic pop