// BSD 3-Clause License
// Copyright (c) 2024, 🍀☀🌕🌥 🌊
// See the LICENSE file in the project root for full license information.

#pragma once

/*!
 * \file utils.h
 * \brief Unified utility header for network_system
 *
 * This umbrella header provides convenient access to all public utility
 * components in network_system. It includes:
 * - Error handling and result types (result_types.h)
 * - Lifecycle management (lifecycle_manager.h, startable_base.h)
 * - Callback management (callback_manager.h)
 *
 * Usage:
 * \code
 * #include <kcenon/network/utils.h>
 *
 * using namespace kcenon::network;
 *
 * // Use Result<T> for error handling
 * auto result = some_operation();
 * if (result.is_ok()) {
 *     process(result.value());
 * }
 *
 * // Use lifecycle_manager for component management
 * lifecycle_manager lifecycle;
 * lifecycle.start();
 * \endcode
 *
 * \note Internal utilities (buffer_pool, compression_pipeline, etc.) are not
 * included in this header and should be accessed directly from src/internal/
 * when needed for internal implementation.
 *
 * \see docs/architecture/facade-pattern.md
 */

// Core utilities (from detail directory)
#include "kcenon/network/types/result.h"

// Lifecycle management (from detail directory)
#include "kcenon/network/detail/utils/lifecycle_manager.h"
#include "kcenon/network/detail/utils/startable_base.h"

// Callback management (from detail directory)
#include "kcenon/network/detail/utils/callback_manager.h"

/*!
 * \namespace kcenon::network::utils
 * \brief Utility components for network_system
 *
 * This namespace contains essential utility components:
 * - **Result types**: Error handling with Result<T>/VoidResult pattern
 * - **Lifecycle management**: Component start/stop lifecycle patterns
 * - **Callback management**: Type-safe callback registration and invocation
 *
 * All utilities use modern C++ patterns and are designed for:
 * - Type safety with minimal overhead
 * - Thread-safe operations where applicable
 * - Clear error reporting through Result<T>
 */
