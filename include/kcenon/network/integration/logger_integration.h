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

/**
 * @file logger_integration.h
 * @brief Logger system integration interface for network_system
 *
 * This file provides logging integration using common_system's ILogger
 * interface and GlobalLoggerRegistry for runtime binding. The NETWORK_LOG_*
 * macros delegate to common_system's LOG_* macros for unified logging.
 *
 * @note Issue #285: Migrated from logger_system to common_system's ILogger.
 *
 * @author kcenon
 * @date 2025-09-20
 */

#include <memory>
#include <string>
#include <string_view>

#include "kcenon/network/integration/thread_integration.h"

// Include common_system logging facilities
#include <kcenon/common/logging/log_macros.h>
#include <kcenon/common/interfaces/global_logger_registry.h>

namespace kcenon::network::integration {

/**
 * @enum log_level
 * @brief Log severity levels matching common_system
 * @deprecated Use kcenon::common::interfaces::log_level instead
 */
enum class log_level : int {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    error = 4,
    fatal = 5
};

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

/**
 * @class logger_interface
 * @brief Abstract interface for logger integration
 * @deprecated Use kcenon::common::interfaces::ILogger instead
 *
 * This interface is maintained for backward compatibility.
 * New code should use common_system's ILogger directly.
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

/**
 * @class basic_logger
 * @brief Basic console logger implementation for standalone use
 * @deprecated Use common_system's logging with NullLogger fallback instead
 *
 * This class is maintained for backward compatibility.
 * The common_system's GlobalLoggerRegistry automatically provides
 * a NullLogger when no logger is registered.
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
 * @deprecated Use common_system's GlobalLoggerRegistry directly
 *
 * This class is maintained for backward compatibility.
 * It now delegates to GlobalLoggerRegistry internally.
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
     * @deprecated Register loggers with GlobalLoggerRegistry instead
     */
    void set_logger(std::shared_ptr<logger_interface> logger);

    /**
     * @brief Get the current logger
     * @return Current logger (uses GlobalLoggerRegistry's default logger)
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

} // namespace kcenon::network::integration

// =============================================================================
// Convenience macros - delegate to common_system's LOG_* macros
//
// Note: NETWORK_LOG_* macros are maintained for backward compatibility.
// New code should use LOG_* macros from common_system directly.
// =============================================================================

#define NETWORK_LOG_TRACE(msg) LOG_TRACE(msg)
#define NETWORK_LOG_DEBUG(msg) LOG_DEBUG(msg)
#define NETWORK_LOG_INFO(msg) LOG_INFO(msg)
#define NETWORK_LOG_WARN(msg) LOG_WARNING(msg)
#define NETWORK_LOG_ERROR(msg) LOG_ERROR(msg)
#define NETWORK_LOG_FATAL(msg) LOG_CRITICAL(msg)

// Backward compatibility namespace alias is defined in thread_integration.h