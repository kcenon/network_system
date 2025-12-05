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
 * @file logger_integration.cpp
 * @brief Implementation of logger system integration
 *
 * This file supports both standalone operation (basic_logger) and
 * common_system integration (common_system_logger_adapter).
 *
 * @note Issue #285: Supports both common_system ILogger and standalone operation.
 *
 * @author kcenon
 * @date 2025-09-20
 */

#include "kcenon/network/integration/logger_integration.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace kcenon::network::integration {

// Helper function to convert log level to string
static const char* level_to_string(log_level level) {
    switch (level) {
        case log_level::trace: return "TRACE";
        case log_level::debug: return "DEBUG";
        case log_level::info:  return "INFO ";
        case log_level::warn:  return "WARN ";
        case log_level::error: return "ERROR";
        case log_level::fatal: return "FATAL";
        default: return "UNKN ";
    }
}

// Helper function to get current timestamp
static std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

//============================================================================
// common_system_logger_adapter implementation (only when common_system available)
//============================================================================

#ifdef BUILD_WITH_COMMON_SYSTEM

common_system_logger_adapter::common_system_logger_adapter(const std::string& logger_name)
    : logger_name_(logger_name) {}

std::shared_ptr<kcenon::common::interfaces::ILogger>
common_system_logger_adapter::get_logger() const {
    if (logger_name_.empty()) {
        return kcenon::common::interfaces::get_logger();
    }
    return kcenon::common::interfaces::get_logger(logger_name_);
}

void common_system_logger_adapter::log(log_level level, const std::string& message) {
    auto logger = get_logger();
    if (logger) {
        logger->log(to_common_level(level), message);
    }
}

void common_system_logger_adapter::log(log_level level, const std::string& message,
                                       const std::string& file, int line,
                                       const std::string& function) {
    auto logger = get_logger();
    if (logger) {
        // Use the deprecated interface for backward compatibility
#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4996)
#endif
        logger->log(to_common_level(level), message, file, line, function);
#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
#endif
    }
}

bool common_system_logger_adapter::is_level_enabled(log_level level) const {
    auto logger = get_logger();
    return logger ? logger->is_enabled(to_common_level(level)) : false;
}

void common_system_logger_adapter::flush() {
    auto logger = get_logger();
    if (logger) {
        logger->flush();
    }
}

#endif // BUILD_WITH_COMMON_SYSTEM

//============================================================================
// basic_logger implementation
//============================================================================

class basic_logger::impl {
public:
    explicit impl(log_level min_level) : min_level_(static_cast<int>(min_level)) {}

    void log(log_level level, const std::string& message) {
        if (static_cast<int>(level) < min_level_) return;

        std::lock_guard<std::mutex> lock(mutex_);

        auto timestamp = get_timestamp();
        std::ostringstream oss;
        oss << "[" << timestamp << "] "
            << "[" << level_to_string(level) << "] "
            << "[network_system] "
            << message;

        auto* target = (level >= log_level::error) ? stderr : stdout;
        const auto& record = oss.str();
        std::fwrite(record.data(), 1, record.size(), target);
        std::fputc('\n', target);
        std::fflush(target);
    }

    void log(log_level level, const std::string& message,
            const std::string& file, int line, const std::string& function) {
        if (static_cast<int>(level) < min_level_) return;

        std::lock_guard<std::mutex> lock(mutex_);

        auto timestamp = get_timestamp();
        std::ostringstream oss;
        oss << "[" << timestamp << "] "
            << "[" << level_to_string(level) << "] "
            << "[network_system] "
            << message
            << " (" << file << ":" << line << " in " << function << ")";

        auto* target = (level >= log_level::error) ? stderr : stdout;
        const auto& record = oss.str();
        std::fwrite(record.data(), 1, record.size(), target);
        std::fputc('\n', target);
        std::fflush(target);
    }

    bool is_level_enabled(log_level level) const {
        return static_cast<int>(level) >= min_level_;
    }

    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::fflush(stdout);
        std::fflush(stderr);
    }

    void set_min_level(log_level level) {
        min_level_ = static_cast<int>(level);
    }

    log_level get_min_level() const {
        return static_cast<log_level>(min_level_.load());
    }

private:
    mutable std::mutex mutex_;
    std::atomic<int> min_level_;
};

basic_logger::basic_logger(log_level min_level)
    : pimpl_(std::make_unique<impl>(min_level)) {}

basic_logger::~basic_logger() = default;

void basic_logger::log(log_level level, const std::string& message) {
    pimpl_->log(level, message);
}

void basic_logger::log(log_level level, const std::string& message,
                      const std::string& file, int line, const std::string& function) {
    pimpl_->log(level, message, file, line, function);
}

bool basic_logger::is_level_enabled(log_level level) const {
    return pimpl_->is_level_enabled(level);
}

void basic_logger::flush() {
    pimpl_->flush();
}

void basic_logger::set_min_level(log_level level) {
    pimpl_->set_min_level(level);
}

log_level basic_logger::get_min_level() const {
    return pimpl_->get_min_level();
}

//============================================================================
// logger_integration_manager implementation
//
// When BUILD_WITH_COMMON_SYSTEM is defined, uses common_system_logger_adapter.
// Otherwise, uses basic_logger for standalone operation.
//============================================================================

class logger_integration_manager::impl {
public:
    impl() {
#ifdef BUILD_WITH_COMMON_SYSTEM
        // Use common_system_logger_adapter by default
        // It delegates to GlobalLoggerRegistry which provides NullLogger fallback
        logger_ = std::make_shared<common_system_logger_adapter>();
#else
        // Use basic_logger for standalone operation
        logger_ = std::make_shared<basic_logger>();
#endif
    }

    void set_logger(std::shared_ptr<logger_interface> logger) {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_ = logger;
    }

    std::shared_ptr<logger_interface> get_logger() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!logger_) {
#ifdef BUILD_WITH_COMMON_SYSTEM
            logger_ = std::make_shared<common_system_logger_adapter>();
#else
            logger_ = std::make_shared<basic_logger>();
#endif
        }
        return logger_;
    }

    void log(log_level level, const std::string& message) {
#ifdef BUILD_WITH_COMMON_SYSTEM
        // Delegate directly to common_system's GlobalLoggerRegistry
        auto common_logger = kcenon::common::interfaces::get_logger();
        if (common_logger) {
            common_logger->log(to_common_level(level), message);
        }
#else
        auto logger = get_logger();
        if (logger) {
            logger->log(level, message);
        }
#endif
    }

    void log(log_level level, const std::string& message,
            const std::string& file, int line, const std::string& function) {
#ifdef BUILD_WITH_COMMON_SYSTEM
        // Delegate directly to common_system's GlobalLoggerRegistry
        auto common_logger = kcenon::common::interfaces::get_logger();
        if (common_logger) {
#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4996)
#endif
            common_logger->log(to_common_level(level), message, file, line, function);
#if defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
#endif
        }
#else
        auto logger = get_logger();
        if (logger) {
            logger->log(level, message, file, line, function);
        }
#endif
    }

private:
    mutable std::mutex mutex_;
    std::shared_ptr<logger_interface> logger_;
};

logger_integration_manager& logger_integration_manager::instance() {
    // Intentionally leak the singleton to avoid destruction order issues
    // during sanitizer runs and shutdown hooks.
    static logger_integration_manager* instance = new logger_integration_manager();
    return *instance;
}

logger_integration_manager::logger_integration_manager()
    : pimpl_(std::make_unique<impl>()) {}

logger_integration_manager::~logger_integration_manager() = default;

void logger_integration_manager::set_logger(std::shared_ptr<logger_interface> logger) {
    pimpl_->set_logger(logger);
}

std::shared_ptr<logger_interface> logger_integration_manager::get_logger() {
    return pimpl_->get_logger();
}

void logger_integration_manager::log(log_level level, const std::string& message) {
    pimpl_->log(level, message);
}

void logger_integration_manager::log(log_level level, const std::string& message,
                                    const std::string& file, int line, const std::string& function) {
    pimpl_->log(level, message, file, line, function);
}

} // namespace kcenon::network::integration
