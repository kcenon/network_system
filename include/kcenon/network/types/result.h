// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

/**
 * @file result.h
 * @brief Public header for Result<T> error handling types
 *
 * This is the public API header for result types. Use this header instead of
 * including detail/utils/result_types.h directly.
 *
 * @example
 * @code
 * #include <kcenon/network/types/result.h>
 *
 * auto connect() -> kcenon::network::Result<void> {
 *     // ... connection logic
 *     return kcenon::network::ok();
 * }
 * @endcode
 */

#pragma once

// Include the implementation from detail
// This maintains backward compatibility while providing a public API path
#include "kcenon/network/detail/utils/result_types.h"

namespace kcenon::network
{

/**
 * @brief Result type for operations that can fail
 *
 * Result<T> is the primary error handling mechanism in network_system.
 * It provides a type-safe alternative to exceptions for error handling.
 *
 * @tparam T The success value type
 *
 * @note When KCENON_WITH_COMMON_SYSTEM is enabled, this is an alias to
 *       kcenon::common::Result<T>. Otherwise, a standalone implementation
 *       is provided.
 */
// Result<T> is already defined in detail/utils/result_types.h

/**
 * @brief Alias for Result<void> for operations that return no value
 */
// VoidResult is already defined in detail/utils/result_types.h

/**
 * @brief Error information structure
 *
 * Contains error code and message for failed operations.
 */
// error_info is already defined in detail/utils/result_types.h

} // namespace kcenon::network
