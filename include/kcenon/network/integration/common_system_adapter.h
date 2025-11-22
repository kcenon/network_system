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

#pragma once

/**
 * @file common_system_adapter.h
 * @brief Adapter for common_system integration
 */

#ifdef BUILD_WITH_COMMON_SYSTEM
#include <kcenon/common/interfaces/executor_interface.h>
#include <kcenon/common/interfaces/logger_interface.h>
#include <kcenon/common/interfaces/monitoring_interface.h>
#include <kcenon/common/patterns/result.h>
#include <kcenon/network/integration/thread_integration.h>
#include <kcenon/network/integration/logger_integration.h>
#include <kcenon/network/integration/monitoring_integration.h>
#endif

#include <chrono>
#include <future>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace kcenon::network::integration {

#ifdef BUILD_WITH_COMMON_SYSTEM

namespace detail {
inline std::future<void> make_error_future(const std::string& message) {
    std::promise<void> promise;
    promise.set_exception(std::make_exception_ptr(std::runtime_error(message)));
    auto future = promise.get_future();
    return future;
}
} // namespace detail

class executor_job final : public ::kcenon::common::interfaces::IJob {
public:
    explicit executor_job(std::function<void()> task, std::string name = "network_job")
        : task_(std::move(task)), name_(std::move(name)) {}

    ::kcenon::common::VoidResult execute() override {
        try {
            if (task_) {
                task_();
            }
            return ::kcenon::common::ok();
        } catch (const std::exception& ex) {
            return ::kcenon::common::VoidResult(::kcenon::common::error_info{
                ::kcenon::common::error_codes::INTERNAL_ERROR,
                ex.what(),
                "network_system"});
        }
    }

    std::string get_name() const override { return name_; }

private:
    std::function<void()> task_;
    std::string name_;
};

class common_thread_pool_adapter : public thread_pool_interface {
public:
    explicit common_thread_pool_adapter(
        std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor)
        : executor_(std::move(executor)) {}

    std::future<void> submit(std::function<void()> task) override {
        if (!executor_) {
            auto future = detail::make_error_future("Executor not initialized");
            return future;
        }
        auto result = executor_->execute(std::make_unique<executor_job>(std::move(task)));
        if (result.is_err()) {
            auto future = detail::make_error_future(result.error().message);
            return future;
        }
        auto future = std::move(result.value());
        return future;
    }

    std::future<void> submit_delayed(
        std::function<void()> task,
        std::chrono::milliseconds delay) override {
        if (!executor_) {
            auto future = detail::make_error_future("Executor not initialized");
            return future;
        }
        auto result = executor_->execute_delayed(
            std::make_unique<executor_job>(std::move(task)), delay);
        if (result.is_err()) {
            auto future = detail::make_error_future(result.error().message);
            return future;
        }
        auto future = std::move(result.value());
        return future;
    }

    size_t worker_count() const override {
        return executor_ ? executor_->worker_count() : 0;
    }

    bool is_running() const override {
        return executor_ ? executor_->is_running() : false;
    }

    size_t pending_tasks() const override {
        return executor_ ? executor_->pending_tasks() : 0;
    }

    void shutdown(bool wait_for_tasks = true) {
        if (executor_) {
            executor_->shutdown(wait_for_tasks);
        }
    }

private:
    std::shared_ptr<::kcenon::common::interfaces::IExecutor> executor_;
};

inline ::kcenon::common::interfaces::log_level to_common_log_level(log_level level) {
    using common_level = ::kcenon::common::interfaces::log_level;
    switch (level) {
        case log_level::trace: return common_level::trace;
        case log_level::debug: return common_level::debug;
        case log_level::info: return common_level::info;
        case log_level::warn: return common_level::warning;
        case log_level::error: return common_level::error;
        case log_level::fatal: return common_level::critical;
        default: return common_level::info;
    }
}

class common_logger_adapter : public logger_interface {
public:
    explicit common_logger_adapter(std::shared_ptr<::kcenon::common::interfaces::ILogger> logger)
        : logger_(std::move(logger)) {}

    void log(log_level level, const std::string& message) override {
        if (!logger_) {
            return;
        }
        logger_->log(to_common_log_level(level), message);
    }

    void log(log_level level, const std::string& message,
             const std::string& file, int line,
             const std::string& function) override {
        if (!logger_) {
            return;
        }
        logger_->log(to_common_log_level(level), message, file, line, function);
    }

    bool is_level_enabled(log_level level) const override {
        return logger_
            ? logger_->is_enabled(to_common_log_level(level))
            : false;
    }

    void flush() override {
        if (logger_) {
            logger_->flush();
        }
    }

private:
    std::shared_ptr<::kcenon::common::interfaces::ILogger> logger_;
};

class common_monitoring_adapter : public monitoring_interface {
public:
    explicit common_monitoring_adapter(std::shared_ptr<::kcenon::common::interfaces::IMonitor> monitor)
        : monitor_(std::move(monitor)) {}

    void report_counter(const std::string& name, double value,
                        const std::map<std::string, std::string>& labels = {}) override {
        record_with_type(name, value, "counter", labels);
    }

    void report_gauge(const std::string& name, double value,
                      const std::map<std::string, std::string>& labels = {}) override {
        record_with_type(name, value, "gauge", labels);
    }

    void report_histogram(const std::string& name, double value,
                          const std::map<std::string, std::string>& labels = {}) override {
        record_with_type(name, value, "histogram", labels);
    }

    void report_health(const std::string& connection_id, bool is_alive,
                       double response_time_ms, size_t missed_heartbeats,
                       double packet_loss_rate) override {
        if (!monitor_) {
            return;
        }
        std::unordered_map<std::string, std::string> tags{
            {"connection_id", connection_id}
        };
        monitor_->record_metric("network.connection.alive", is_alive ? 1.0 : 0.0, tags);
        monitor_->record_metric("network.connection.rtt_ms", response_time_ms, tags);
        monitor_->record_metric("network.connection.missed_heartbeats",
                                static_cast<double>(missed_heartbeats), tags);
        monitor_->record_metric("network.connection.packet_loss",
                                packet_loss_rate, tags);
    }

private:
    void record_with_type(const std::string& name,
                          double value,
                          const std::string& type,
                          const std::map<std::string, std::string>& labels) {
        if (!monitor_) {
            return;
        }
        std::unordered_map<std::string, std::string> enriched(labels.begin(), labels.end());
        enriched["metric_type"] = type;
        monitor_->record_metric(name, value, enriched);
    }

    std::shared_ptr<::kcenon::common::interfaces::IMonitor> monitor_;
};

#endif // BUILD_WITH_COMMON_SYSTEM

} // namespace kcenon::network::integration

// Backward compatibility: using declarations for legacy code
// Note: namespace alias is defined in thread_integration.h as:
//   namespace network_system { namespace integration = kcenon::network::integration; }
