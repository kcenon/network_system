// BSD 3-Clause License
//
// Copyright (c) 2021-2025, 🍀☀🌕🌥 🌊
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <kcenon/network/integration/thread_pool_bridge.h>

#if KCENON_WITH_COMMON_SYSTEM
#include <kcenon/network/integration/thread_pool_adapters.h>
#endif

#include <stdexcept>

namespace kcenon::network::integration {

ThreadPoolBridge::ThreadPoolBridge(
    std::shared_ptr<thread_pool_interface> pool,
    BackendType backend_type)
    : pool_(std::move(pool)), backend_type_(backend_type) {
    if (!pool_) {
        throw std::invalid_argument("ThreadPoolBridge requires non-null thread pool");
    }
}

ThreadPoolBridge::~ThreadPoolBridge() {
    if (initialized_.load()) {
        shutdown();
    }
}

VoidResult ThreadPoolBridge::initialize(const BridgeConfig& config) {
    if (initialized_.load()) {
        return error_void(
            error_codes::common_errors::already_exists,
            "ThreadPoolBridge already initialized",
            "ThreadPoolBridge::initialize");
    }

    if (!pool_) {
        return error_void(
            error_codes::common_errors::invalid_argument,
            "Thread pool is null",
            "ThreadPoolBridge::initialize");
    }

    if (!pool_->is_running()) {
        return error_void(
            error_codes::common_errors::not_initialized,
            "Thread pool is not running",
            "ThreadPoolBridge::initialize");
    }

    // Check if bridge is enabled (default: true)
    auto enabled_it = config.properties.find("enabled");
    if (enabled_it != config.properties.end() && enabled_it->second == "false") {
        return error_void(
            error_codes::common_errors::invalid_argument,
            "Bridge is disabled in configuration",
            "ThreadPoolBridge::initialize");
    }

    // Initialize metrics
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    cached_metrics_.is_healthy = true;
    cached_metrics_.last_activity = std::chrono::steady_clock::now();
    cached_metrics_.custom_metrics["worker_threads"] = static_cast<double>(pool_->worker_count());
    cached_metrics_.custom_metrics["pending_tasks"] = static_cast<double>(pool_->pending_tasks());
    cached_metrics_.custom_metrics["backend_type"] = static_cast<double>(backend_type_);

    initialized_.store(true);
    return ok();
}

VoidResult ThreadPoolBridge::shutdown() {
    if (!initialized_.load()) {
        return ok(); // Idempotent: already shut down
    }

    // Update metrics to reflect shutdown state
    {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        cached_metrics_.is_healthy = false;
        cached_metrics_.last_activity = std::chrono::steady_clock::now();
    }

    initialized_.store(false);
    return ok();
}

bool ThreadPoolBridge::is_initialized() const {
    return initialized_.load() && pool_ && pool_->is_running();
}

BridgeMetrics ThreadPoolBridge::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    if (!initialized_.load() || !pool_) {
        BridgeMetrics metrics;
        metrics.is_healthy = false;
        metrics.last_activity = cached_metrics_.last_activity;
        return metrics;
    }

    // Update metrics with current thread pool state
    BridgeMetrics metrics = cached_metrics_;
    metrics.is_healthy = pool_->is_running();
    metrics.last_activity = std::chrono::steady_clock::now();
    metrics.custom_metrics["worker_threads"] = static_cast<double>(pool_->worker_count());
    metrics.custom_metrics["pending_tasks"] = static_cast<double>(pool_->pending_tasks());
    metrics.custom_metrics["backend_type"] = static_cast<double>(backend_type_);

    // Update cached metrics for next call
    cached_metrics_ = metrics;

    return metrics;
}

std::shared_ptr<thread_pool_interface> ThreadPoolBridge::get_thread_pool() const {
    return pool_;
}

ThreadPoolBridge::BackendType ThreadPoolBridge::get_backend_type() const {
    return backend_type_;
}

std::shared_ptr<ThreadPoolBridge> ThreadPoolBridge::from_thread_system(
    const std::string& pool_name) {
    (void)pool_name; // Pool name is informational only in current implementation

    auto pool = thread_integration_manager::instance().get_thread_pool();
    if (!pool) {
        throw std::runtime_error("Failed to get thread pool from thread_integration_manager");
    }

    return std::make_shared<ThreadPoolBridge>(pool, BackendType::ThreadSystem);
}

#if KCENON_WITH_COMMON_SYSTEM
std::shared_ptr<ThreadPoolBridge> ThreadPoolBridge::from_common_system(
    std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor) {
    if (!executor) {
        throw std::invalid_argument("ThreadPoolBridge::from_common_system requires non-null executor");
    }

    // Adapt common_system executor to thread_pool_interface
    auto adapted_pool = std::make_shared<common_to_network_thread_adapter>(std::move(executor));

    return std::make_shared<ThreadPoolBridge>(adapted_pool, BackendType::CommonSystem);
}
#endif

} // namespace kcenon::network::integration
