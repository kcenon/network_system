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

#include <kcenon/network/integration/network_system_bridge.h>
#include <kcenon/network/integration/thread_system_adapter.h>

#if KCENON_WITH_COMMON_SYSTEM
#include <kcenon/network/integration/common_system_adapter.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#endif

#include <algorithm>
#include <vector>

namespace kcenon::network::integration {

class NetworkSystemBridge::Impl {
public:
    Impl() = default;

    ~Impl() {
        if (initialized_.load()) {
            shutdown();
        }
    }

    VoidResult initialize(const NetworkSystemBridgeConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_.load()) {
            return error_void(
                error_codes::common_errors::already_exists,
                "NetworkSystemBridge already initialized",
                "NetworkSystemBridge::initialize");
        }

        // Store configuration
        config_ = config;

        // Initialize thread pool bridge if enabled
        if (config.enable_thread_pool && thread_pool_bridge_) {
            BridgeConfig thread_config;
            thread_config.integration_name = config.integration_name;
            thread_config.properties = config.thread_pool_properties;

            auto result = thread_pool_bridge_->initialize(thread_config);
            if (result.is_err()) {
                return error_void(
                    error_codes::common_errors::internal_error,
                    "Failed to initialize thread pool bridge: " + result.error().message,
                    "NetworkSystemBridge::initialize");
            }
            initialized_bridges_.push_back("thread_pool");
        }

        // Future: Initialize logger bridge if enabled
        if (config.enable_logger && logger_) {
            // Logger initialization logic here
            initialized_bridges_.push_back("logger");
        }

        // Future: Initialize monitoring bridge if enabled
        if (config.enable_monitoring && monitoring_) {
            // Monitoring initialization logic here
            initialized_bridges_.push_back("monitoring");
        }

        initialized_.store(true);
        return ok();
    }

    VoidResult shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!initialized_.load()) {
            return ok(); // Idempotent
        }

        // Shutdown in reverse order
        std::reverse(initialized_bridges_.begin(), initialized_bridges_.end());

        for (const auto& bridge_name : initialized_bridges_) {
            if (bridge_name == "thread_pool" && thread_pool_bridge_) {
                thread_pool_bridge_->shutdown();
            }
            // Future: Handle other bridges
        }

        initialized_bridges_.clear();
        initialized_.store(false);
        return ok();
    }

    bool is_initialized() const {
        return initialized_.load();
    }

    BridgeMetrics get_metrics() const {
        std::lock_guard<std::mutex> lock(mutex_);

        BridgeMetrics aggregated;
        aggregated.is_healthy = true;
        aggregated.last_activity = std::chrono::steady_clock::now();

        // Aggregate thread pool metrics
        if (thread_pool_bridge_ && thread_pool_bridge_->is_initialized()) {
            auto thread_metrics = thread_pool_bridge_->get_metrics();
            aggregated.is_healthy &= thread_metrics.is_healthy;

            for (const auto& [key, value] : thread_metrics.custom_metrics) {
                aggregated.custom_metrics["thread_pool." + key] = value;
            }
        }

        // Future: Aggregate logger and monitoring metrics

        return aggregated;
    }

    std::shared_ptr<ThreadPoolBridge> get_thread_pool_bridge() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return thread_pool_bridge_;
    }

    std::shared_ptr<thread_pool_interface> get_thread_pool() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (thread_pool_bridge_) {
            return thread_pool_bridge_->get_thread_pool();
        }
        return nullptr;
    }

    std::shared_ptr<logger_interface> get_logger() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return logger_;
    }

    std::shared_ptr<monitoring_interface> get_monitoring() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return monitoring_;
    }

    VoidResult set_thread_pool_bridge(std::shared_ptr<ThreadPoolBridge> bridge) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_.load()) {
            return error_void(
                error_codes::common_errors::already_exists,
                "Cannot set thread pool bridge after initialization",
                "NetworkSystemBridge::set_thread_pool_bridge");
        }

        if (!bridge) {
            return error_void(
                error_codes::common_errors::invalid_argument,
                "Thread pool bridge cannot be null",
                "NetworkSystemBridge::set_thread_pool_bridge");
        }

        thread_pool_bridge_ = std::move(bridge);
        return ok();
    }

    VoidResult set_logger(std::shared_ptr<logger_interface> logger) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_.load()) {
            return error_void(
                error_codes::common_errors::already_exists,
                "Cannot set logger after initialization",
                "NetworkSystemBridge::set_logger");
        }

        logger_ = std::move(logger);
        return ok();
    }

    VoidResult set_monitoring(std::shared_ptr<monitoring_interface> monitoring) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (initialized_.load()) {
            return error_void(
                error_codes::common_errors::already_exists,
                "Cannot set monitoring after initialization",
                "NetworkSystemBridge::set_monitoring");
        }

        monitoring_ = std::move(monitoring);
        return ok();
    }

private:
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_{false};
    NetworkSystemBridgeConfig config_;
    std::vector<std::string> initialized_bridges_;

    std::shared_ptr<ThreadPoolBridge> thread_pool_bridge_;
    std::shared_ptr<logger_interface> logger_;
    std::shared_ptr<monitoring_interface> monitoring_;
};

// NetworkSystemBridge implementation

NetworkSystemBridge::NetworkSystemBridge()
    : pimpl_(std::make_unique<Impl>()) {
}

NetworkSystemBridge::NetworkSystemBridge(std::shared_ptr<ThreadPoolBridge> thread_pool)
    : pimpl_(std::make_unique<Impl>()) {
    pimpl_->set_thread_pool_bridge(std::move(thread_pool));
}

NetworkSystemBridge::~NetworkSystemBridge() = default;

NetworkSystemBridge::NetworkSystemBridge(NetworkSystemBridge&&) noexcept = default;
NetworkSystemBridge& NetworkSystemBridge::operator=(NetworkSystemBridge&&) noexcept = default;

VoidResult NetworkSystemBridge::initialize(const NetworkSystemBridgeConfig& config) {
    if (!pimpl_) {
        return error_void(
            error_codes::common_errors::not_initialized,
            "Bridge has been moved",
            "NetworkSystemBridge::initialize");
    }
    return pimpl_->initialize(config);
}

VoidResult NetworkSystemBridge::shutdown() {
    if (!pimpl_) {
        return ok(); // Already moved or destroyed
    }
    return pimpl_->shutdown();
}

bool NetworkSystemBridge::is_initialized() const {
    if (!pimpl_) {
        return false;
    }
    return pimpl_->is_initialized();
}

BridgeMetrics NetworkSystemBridge::get_metrics() const {
    if (!pimpl_) {
        return BridgeMetrics{};
    }
    return pimpl_->get_metrics();
}

std::shared_ptr<ThreadPoolBridge> NetworkSystemBridge::get_thread_pool_bridge() const {
    if (!pimpl_) {
        return nullptr;
    }
    return pimpl_->get_thread_pool_bridge();
}

std::shared_ptr<thread_pool_interface> NetworkSystemBridge::get_thread_pool() const {
    if (!pimpl_) {
        return nullptr;
    }
    return pimpl_->get_thread_pool();
}

std::shared_ptr<logger_interface> NetworkSystemBridge::get_logger() const {
    if (!pimpl_) {
        return nullptr;
    }
    return pimpl_->get_logger();
}

std::shared_ptr<monitoring_interface> NetworkSystemBridge::get_monitoring() const {
    if (!pimpl_) {
        return nullptr;
    }
    return pimpl_->get_monitoring();
}

VoidResult NetworkSystemBridge::set_thread_pool_bridge(std::shared_ptr<ThreadPoolBridge> bridge) {
    if (!pimpl_) {
        return error_void(
            error_codes::common_errors::not_initialized,
            "Bridge has been moved",
            "NetworkSystemBridge::set_thread_pool_bridge");
    }
    return pimpl_->set_thread_pool_bridge(std::move(bridge));
}

VoidResult NetworkSystemBridge::set_logger(std::shared_ptr<logger_interface> logger) {
    if (!pimpl_) {
        return error_void(
            error_codes::common_errors::not_initialized,
            "Bridge has been moved",
            "NetworkSystemBridge::set_logger");
    }
    return pimpl_->set_logger(std::move(logger));
}

VoidResult NetworkSystemBridge::set_monitoring(std::shared_ptr<monitoring_interface> monitoring) {
    if (!pimpl_) {
        return error_void(
            error_codes::common_errors::not_initialized,
            "Bridge has been moved",
            "NetworkSystemBridge::set_monitoring");
    }
    return pimpl_->set_monitoring(std::move(monitoring));
}

// Factory Methods

std::shared_ptr<NetworkSystemBridge> NetworkSystemBridge::create_default() {
    auto bridge = std::make_shared<NetworkSystemBridge>();

    // Try to create thread pool from thread_system if available
#if KCENON_WITH_THREAD_SYSTEM
    auto thread_pool_bridge = ThreadPoolBridge::from_thread_system("network_pool");
    bridge->set_thread_pool_bridge(thread_pool_bridge);
#endif

    return bridge;
}

std::shared_ptr<NetworkSystemBridge> NetworkSystemBridge::with_thread_system(
    const std::string& pool_name) {
    auto bridge = std::make_shared<NetworkSystemBridge>();

#if KCENON_WITH_THREAD_SYSTEM
    auto thread_pool_bridge = ThreadPoolBridge::from_thread_system(pool_name);
    bridge->set_thread_pool_bridge(thread_pool_bridge);
#endif

    return bridge;
}

#if KCENON_WITH_COMMON_SYSTEM
std::shared_ptr<NetworkSystemBridge> NetworkSystemBridge::with_common_system(
    std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor,
    std::shared_ptr<::kcenon::common::interfaces::ILogger> logger,
    std::shared_ptr<::kcenon::common::interfaces::IMonitor> monitor) {

    auto bridge = std::make_shared<NetworkSystemBridge>();

    // Set up thread pool bridge
    if (executor) {
        auto thread_pool_bridge = ThreadPoolBridge::from_common_system(executor);
        bridge->set_thread_pool_bridge(thread_pool_bridge);
    }

    // Set up logger
    if (logger) {
        auto logger_adapter = std::make_shared<common_logger_adapter>(logger);
        bridge->set_logger(logger_adapter);
    }

    // Set up monitoring
    if (monitor) {
        auto monitoring_adapter = std::make_shared<common_monitoring_adapter>(monitor);
        bridge->set_monitoring(monitoring_adapter);
    }

    return bridge;
}
#endif

std::shared_ptr<NetworkSystemBridge> NetworkSystemBridge::with_custom(
    std::shared_ptr<thread_pool_interface> thread_pool,
    std::shared_ptr<logger_interface> logger,
    std::shared_ptr<monitoring_interface> monitoring) {

    auto bridge = std::make_shared<NetworkSystemBridge>();

    if (thread_pool) {
        auto thread_pool_bridge = std::make_shared<ThreadPoolBridge>(
            thread_pool, ThreadPoolBridge::BackendType::Custom);
        bridge->set_thread_pool_bridge(thread_pool_bridge);
    }

    if (logger) {
        bridge->set_logger(logger);
    }

    if (monitoring) {
        bridge->set_monitoring(monitoring);
    }

    return bridge;
}

} // namespace kcenon::network::integration
