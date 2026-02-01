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

#include <kcenon/network/integration/observability_bridge.h>
#include <stdexcept>

namespace kcenon::network::integration {

ObservabilityBridge::ObservabilityBridge(
    std::shared_ptr<logger_interface> logger,
    std::shared_ptr<monitoring_interface> monitor,
    BackendType backend_type)
    : logger_(std::move(logger))
    , monitor_(std::move(monitor))
    , backend_type_(backend_type) {
    if (!logger_) {
        throw std::invalid_argument("ObservabilityBridge requires non-null logger");
    }
    if (!monitor_) {
        throw std::invalid_argument("ObservabilityBridge requires non-null monitor");
    }
}

ObservabilityBridge::~ObservabilityBridge() {
    if (initialized_.load()) {
        shutdown();
    }
}

VoidResult ObservabilityBridge::initialize(const BridgeConfig& config) {
    if (initialized_.load()) {
        return error_void(
            error_codes::common_errors::already_exists,
            "ObservabilityBridge already initialized",
            "ObservabilityBridge::initialize");
    }

    if (!logger_) {
        return error_void(
            error_codes::common_errors::invalid_argument,
            "Logger is null",
            "ObservabilityBridge::initialize");
    }

    if (!monitor_) {
        return error_void(
            error_codes::common_errors::invalid_argument,
            "Monitor is null",
            "ObservabilityBridge::initialize");
    }

    // Check if bridge is enabled (default: true)
    auto enabled_it = config.properties.find("enabled");
    if (enabled_it != config.properties.end() && enabled_it->second == "false") {
        return error_void(
            error_codes::common_errors::invalid_argument,
            "Bridge is disabled in configuration",
            "ObservabilityBridge::initialize");
    }

    // Check if monitoring is enabled (default: true)
    auto monitoring_enabled_it = config.properties.find("enable_monitoring");
    monitoring_enabled_ = (monitoring_enabled_it == config.properties.end() ||
                          monitoring_enabled_it->second != "false");

    // Initialize metrics
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    cached_metrics_.is_healthy = true;
    cached_metrics_.last_activity = std::chrono::steady_clock::now();
    cached_metrics_.custom_metrics["backend_type"] = static_cast<double>(backend_type_);
    cached_metrics_.custom_metrics["monitoring_enabled"] = monitoring_enabled_ ? 1.0 : 0.0;
    cached_metrics_.custom_metrics["logger_available"] = logger_ ? 1.0 : 0.0;
    cached_metrics_.custom_metrics["monitor_available"] = monitor_ ? 1.0 : 0.0;

    initialized_.store(true);
    return ok();
}

VoidResult ObservabilityBridge::shutdown() {
    if (!initialized_.load()) {
        return ok(); // Idempotent: already shut down
    }

    // Flush logger before shutdown
    if (logger_) {
        try {
            logger_->flush();
        } catch (...) {
            // Ignore flush errors during shutdown
        }
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

bool ObservabilityBridge::is_initialized() const {
    return initialized_.load() && logger_ && monitor_;
}

BridgeMetrics ObservabilityBridge::get_metrics() const {
    std::lock_guard<std::mutex> lock(metrics_mutex_);

    if (!initialized_.load() || !logger_ || !monitor_) {
        BridgeMetrics metrics;
        metrics.is_healthy = false;
        metrics.last_activity = cached_metrics_.last_activity;
        return metrics;
    }

    // Update metrics with current state
    BridgeMetrics metrics = cached_metrics_;
    metrics.is_healthy = (logger_ && monitor_);
    metrics.last_activity = std::chrono::steady_clock::now();
    metrics.custom_metrics["backend_type"] = static_cast<double>(backend_type_);
    metrics.custom_metrics["monitoring_enabled"] = monitoring_enabled_ ? 1.0 : 0.0;
    metrics.custom_metrics["logger_available"] = logger_ ? 1.0 : 0.0;
    metrics.custom_metrics["monitor_available"] = monitor_ ? 1.0 : 0.0;

    // Update cached metrics for next call
    cached_metrics_ = metrics;

    return metrics;
}

std::shared_ptr<logger_interface> ObservabilityBridge::get_logger() const {
    return logger_;
}

std::shared_ptr<monitoring_interface> ObservabilityBridge::get_monitor() const {
    return monitor_;
}

ObservabilityBridge::BackendType ObservabilityBridge::get_backend_type() const {
    return backend_type_;
}

#if KCENON_WITH_COMMON_SYSTEM
std::shared_ptr<ObservabilityBridge> ObservabilityBridge::from_common_system(
    std::shared_ptr<::kcenon::common::interfaces::ILogger> logger,
    std::shared_ptr<::kcenon::common::interfaces::IMonitor> monitor) {
    if (!logger) {
        throw std::invalid_argument("ObservabilityBridge::from_common_system requires non-null logger");
    }
    if (!monitor) {
        throw std::invalid_argument("ObservabilityBridge::from_common_system requires non-null monitor");
    }

    // Adapt common_system logger and monitor to network_system interfaces
    auto adapted_logger = std::make_shared<common_system_logger_adapter>();

    // For monitor, we use basic_monitoring since there's no direct adapter yet
    // This is a known limitation - future work could create a proper adapter
    auto adapted_monitor = std::make_shared<basic_monitoring>();

    return std::make_shared<ObservabilityBridge>(
        adapted_logger,
        adapted_monitor,
        BackendType::CommonSystem);
}
#endif

} // namespace kcenon::network::integration
