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

#include <kcenon/network/config/feature_flags.h>

/**
 * @file logger_integration.h
 * @brief Logger system integration interface for network_system
 *
 * This file provides logging integration with optional common_system support.
 * When KCENON_WITH_COMMON_SYSTEM is enabled, NETWORK_LOG_* macros delegate to
 * common_system's LOG_* macros. Otherwise, they use the built-in basic_logger.
 *
 * @note Issue #285: Supports both common_system ILogger and standalone operation.
 *
 * @author kcenon
 * @date 2025-09-20
 */

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

#include "kcenon/network/integration/thread_integration.h"

// Conditionally include common_system logging facilities
#if KCENON_WITH_COMMON_SYSTEM
#include <kcenon/common/logging/log_macros.h>
#include <kcenon/common/interfaces/global_logger_registry.h>
#endif

namespace kcenon::network::integration {

/**
 * @enum log_level
 * @brief Log severity levels
 */
enum class log_level : int {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
    fatal = 5
};

#if KCENON_WITH_COMMON_SYSTEM
/**
 * @brief Convert network log_level to common_system log_level
 */
inline kcenon::common::interfaces::log_level to_common_level(log_level level) {
    switch (level) {
        case log_level::trace: return kcenon::common::interfaces::log_level::trace;
        case log_level::debug: return kcenon::common::interfaces::log_level::debug;
        case log_level::info: return kcenon::common::interfaces::log_level::info;
        case log_level::warn: return kcenon::common::interfaces::log_level::warning;
        case log_level::error: return kcenon::common::interfaces::log_level::error;
        case log_level::fatal: return kcenon::common::interfaces::log_level::critical;
        default: return kcenon::common::interfaces::log_level::info;
    }
}
#endif // KCENON_WITH_COMMON_SYSTEM

/**
 * @class logger_interface
 * @brief Abstract interface for logger integration
 *
 * This interface allows network_system to work with any logger
 * implementation.
 */
class logger_interface {
public:
    virtual ~logger_interface() = default;

    /**
     * @brief Log a message with specified level
     * @param level Log severity level
     * @param message Message to log
     */
    virtual void log(log_level level, const std::string& message) = 0;

    /**
     * @brief Log a message with source location information
     * @param level Log severity level
     * @param message Message to log
     * @param file Source file name
     * @param line Line number
     * @param function Function name
     */
    virtual void log(log_level level, const std::string& message,
                    const std::string& file, int line,
                    const std::string& function) = 0;

    /**
     * @brief Check if a log level is enabled
     * @param level Log level to check
     * @return true if the level is enabled
     */
    virtual bool is_level_enabled(log_level level) const = 0;

    /**
     * @brief Flush any buffered log messages
     */
    virtual void flush() = 0;
};

#if KCENON_WITH_COMMON_SYSTEM
/**
 * @class common_system_logger_adapter
 * @brief Adapter that wraps common_system's ILogger to logger_interface
 *
 * This adapter bridges the legacy logger_interface with common_system's
 * ILogger, allowing existing code to work with the new logging system.
 */
class common_system_logger_adapter : public logger_interface {
public:
    /**
     * @brief Constructor with optional named logger
     * @param logger_name Name of the logger in GlobalLoggerRegistry (empty for default)
     */
    explicit common_system_logger_adapter(const std::string& logger_name = "");

    ~common_system_logger_adapter() override = default;

    void log(log_level level, const std::string& message) override;
    void log(log_level level, const std::string& message,
            const std::string& file, int line,
            const std::string& function) override;
    bool is_level_enabled(log_level level) const override;
    void flush() override;

private:
    std::string logger_name_;
    std::shared_ptr<kcenon::common::interfaces::ILogger> get_logger() const;
};
#endif // KCENON_WITH_COMMON_SYSTEM

/**
 * @class basic_logger
 * @brief Basic console logger implementation for standalone use
 *
 * This provides a simple logger implementation for when
 * common_system or logger_system is not available.
 */
class basic_logger : public logger_interface {
public:
    /**
     * @brief Constructor with minimum log level
     * @param min_level Minimum level to log (default: info)
     */
    explicit basic_logger(log_level min_level = log_level::info);

    ~basic_logger() override;

    // logger_interface implementation
    void log(log_level level, const std::string& message) override;
    void log(log_level level, const std::string& message,
            const std::string& file, int line,
            const std::string& function) override;
    bool is_level_enabled(log_level level) const override;
    void flush() override;

    /**
     * @brief Set minimum log level
     * @param level New minimum level
     */
    void set_min_level(log_level level);

    /**
     * @brief Get current minimum log level
     * @return Current minimum level
     */
    log_level get_min_level() const;

private:
    class impl;
    std::unique_ptr<impl> pimpl_;
};

/**
 * @class logger_integration_manager
 * @brief Manager for logger system integration
 *
 * This class manages the integration between network_system and
 * logger implementations. When KCENON_WITH_COMMON_SYSTEM is enabled,
 * it delegates to GlobalLoggerRegistry internally.
 */
class logger_integration_manager {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static logger_integration_manager& instance();

    /**
     * @brief Set the logger implementation
     * @param logger Logger to use
     */
    void set_logger(std::shared_ptr<logger_interface> logger);

    /**
     * @brief Get the current logger
     * @return Current logger (creates basic logger if none set)
     */
    std::shared_ptr<logger_interface> get_logger();

    /**
     * @brief Log a message
     * @param level Log level
     * @param message Message to log
     */
    void log(log_level level, const std::string& message);

    /**
     * @brief Log a message with location
     * @param level Log level
     * @param message Message to log
     * @param file Source file
     * @param line Line number
     * @param function Function name
     */
    void log(log_level level, const std::string& message,
            const std::string& file, int line, const std::string& function);

private:
    logger_integration_manager();
    ~logger_integration_manager();

    class impl;
    std::unique_ptr<impl> pimpl_;
};

// =============================================================================
// Static destruction guard
//
// When KCENON_WITH_COMMON_SYSTEM is enabled, LOG_* macros access common_system's
// GlobalLoggerRegistry singleton. During static destruction, this singleton may
// be destroyed before network_system's singletons, causing heap corruption when
// thread pool threads try to log.
//
// This guard prevents logging during static destruction by tracking if the
// logging subsystem is still valid.
// =============================================================================

namespace detail {

/**
 * @brief Guard against logging during static destruction
 *
 * Uses the Schwarz Counter (Nifty Counter) idiom to detect when static
 * destruction has begun. This prevents heap corruption when common_system's
 * GlobalLoggerRegistry is destroyed before network_system's thread pool.
 */
class static_destruction_guard {
public:
    static_destruction_guard() { counter_.fetch_add(1, std::memory_order_relaxed); }
    ~static_destruction_guard() { counter_.fetch_sub(1, std::memory_order_relaxed); }

    /**
     * @brief Check if logging is safe (not in static destruction)
     * @return true if logging is safe, false if in static destruction
     */
    static bool is_logging_safe()
    {
        return counter_.load(std::memory_order_relaxed) > 0;
    }

private:
    static inline std::atomic<int> counter_{0};
};

// This static instance ensures the counter is incremented at program start
// and decremented during static destruction. When counter_ reaches 0,
// we know static destruction has begun.
inline static_destruction_guard global_guard_;

} // namespace detail

} // namespace kcenon::network::integration

// =============================================================================
// Convenience macros for logging with automatic source location
//
// When KCENON_WITH_COMMON_SYSTEM is enabled, delegate to common_system's LOG_* macros.
// Otherwise, use logger_integration_manager for standalone operation.
//
// Note: All macros now check static_destruction_guard to avoid heap corruption
// during static destruction when common_system's GlobalLoggerRegistry is destroyed.
// =============================================================================

#if KCENON_WITH_COMMON_SYSTEM

#define NETWORK_LOG_TRACE(msg) \
    do { if (::kcenon::network::integration::detail::static_destruction_guard::is_logging_safe()) { LOG_TRACE(msg); } } while(0)
#define NETWORK_LOG_DEBUG(msg) \
    do { if (::kcenon::network::integration::detail::static_destruction_guard::is_logging_safe()) { LOG_DEBUG(msg); } } while(0)
#define NETWORK_LOG_INFO(msg) \
    do { if (::kcenon::network::integration::detail::static_destruction_guard::is_logging_safe()) { LOG_INFO(msg); } } while(0)
#define NETWORK_LOG_WARN(msg) \
    do { if (::kcenon::network::integration::detail::static_destruction_guard::is_logging_safe()) { LOG_WARNING(msg); } } while(0)
#define NETWORK_LOG_ERROR(msg) \
    do { if (::kcenon::network::integration::detail::static_destruction_guard::is_logging_safe()) { LOG_ERROR(msg); } } while(0)
#define NETWORK_LOG_FATAL(msg) \
    do { if (::kcenon::network::integration::detail::static_destruction_guard::is_logging_safe()) { LOG_CRITICAL(msg); } } while(0)

#else // KCENON_WITH_COMMON_SYSTEM

#define NETWORK_LOG_TRACE(msg) \
    kcenon::network::integration::logger_integration_manager::instance().log( \
        kcenon::network::integration::log_level::trace, msg, __FILE__, __LINE__, __FUNCTION__)

#define NETWORK_LOG_DEBUG(msg) \
    kcenon::network::integration::logger_integration_manager::instance().log( \
        kcenon::network::integration::log_level::debug, msg, __FILE__, __LINE__, __FUNCTION__)

#define NETWORK_LOG_INFO(msg) \
    kcenon::network::integration::logger_integration_manager::instance().log( \
        kcenon::network::integration::log_level::info, msg, __FILE__, __LINE__, __FUNCTION__)

#define NETWORK_LOG_WARN(msg) \
    kcenon::network::integration::logger_integration_manager::instance().log( \
        kcenon::network::integration::log_level::warn, msg, __FILE__, __LINE__, __FUNCTION__)

#define NETWORK_LOG_ERROR(msg) \
    kcenon::network::integration::logger_integration_manager::instance().log( \
        kcenon::network::integration::log_level::error, msg, __FILE__, __LINE__, __FUNCTION__)

#define NETWORK_LOG_FATAL(msg) \
    kcenon::network::integration::logger_integration_manager::instance().log( \
        kcenon::network::integration::log_level::fatal, msg, __FILE__, __LINE__, __FUNCTION__)

#endif // KCENON_WITH_COMMON_SYSTEM

// Backward compatibility namespace alias is defined in thread_integration.h
